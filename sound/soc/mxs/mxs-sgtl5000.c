/*
 * Copyright 2011 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/soc-dapm.h>
#include <asm/mach-types.h>

#include "../codecs/sgtl5000.h"
#include "mxs-saif.h"

static int mxs_sgtl5000_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int rate = params_rate(params);
	u32 dai_format, mclk;
	int ret;

	/* sgtl5000 does not support 512*rate when in 96000 fs */
	switch (rate) {
	case 96000:
		mclk = 256 * rate;
		break;
	default:
		mclk = 512 * rate;
		break;
	}

	/* Sgtl5000 sysclk should be >= 8MHz and <= 27M */
	if (mclk < 8000000 || mclk > 27000000)
		return -EINVAL;

	/* Set SGTL5000's SYSCLK (provided by SAIF MCLK) */
	ret = snd_soc_dai_set_sysclk(codec_dai, SGTL5000_SYSCLK, mclk, 0);
	if (ret)
		return ret;

	/* The SAIF MCLK should be the same as SGTL5000_SYSCLK */
	ret = snd_soc_dai_set_sysclk(cpu_dai, MXS_SAIF_MCLK, mclk, 0);
	if (ret)
		return ret;

	/* set codec to slave mode */
	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, dai_format);
	if (ret)
		return ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_format);
	if (ret)
		return ret;

	return 0;
}

static struct snd_soc_ops mxs_sgtl5000_hifi_ops = {
	.hw_params = mxs_sgtl5000_hw_params,
};

static struct snd_soc_dai_link mxs_sgtl5000_dai[] = {
	{
		.name		= "HiFi Tx",
		.stream_name	= "HiFi Playback",
		.codec_dai_name	= "sgtl5000",
		.codec_name	= "sgtl5000.0-000a",
		.cpu_dai_name	= "mxs-saif.0",
		.platform_name	= "mxs-pcm-audio.0",
		.ops		= &mxs_sgtl5000_hifi_ops,
	}, {
		.name		= "HiFi Rx",
		.stream_name	= "HiFi Capture",
		.codec_dai_name	= "sgtl5000",
		.codec_name	= "sgtl5000.0-000a",
		.cpu_dai_name	= "mxs-saif.1",
		.platform_name	= "mxs-pcm-audio.1",
		.ops		= &mxs_sgtl5000_hifi_ops,
	},
};

static struct snd_soc_card mxs_sgtl5000 = {
	.name		= "mxs_sgtl5000",
	.dai_link	= mxs_sgtl5000_dai,
	.num_links	= ARRAY_SIZE(mxs_sgtl5000_dai),
};

static int mxs_sgtl5000_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mxs_sgtl5000;
	int ret;

	/*
	 * Set an init clock(11.28Mhz) for sgtl5000 initialization(i2c r/w).
	 * The Sgtl5000 sysclk is derived from saif0 mclk and it's range
	 * should be >= 8MHz and <= 27M.
	 */
	ret = mxs_saif_get_mclk(0, 44100 * 256, 44100);
	if (ret)
		return ret;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		return ret;
	}

	return 0;
}

static int mxs_sgtl5000_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	mxs_saif_put_mclk(0);

	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver mxs_sgtl5000_audio_driver = {
	.driver = {
		.name = "mxs-sgtl5000",
		.owner = THIS_MODULE,
	},
	.probe = mxs_sgtl5000_probe,
	.remove = mxs_sgtl5000_remove,
};

static int __init mxs_sgtl5000_init(void)
{
	return platform_driver_register(&mxs_sgtl5000_audio_driver);
}
module_init(mxs_sgtl5000_init);

static void __exit mxs_sgtl5000_exit(void)
{
	platform_driver_unregister(&mxs_sgtl5000_audio_driver);
}
module_exit(mxs_sgtl5000_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXS ALSA SoC Machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mxs-sgtl5000");
