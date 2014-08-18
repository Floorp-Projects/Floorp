/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegLibs_h__
#define __FFmpegLibs_h__

extern "C" {
#pragma GCC visibility push(default)
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#pragma GCC visibility pop
}

#if LIBAVCODEC_VERSION_MAJOR < 55
#define AV_CODEC_ID_H264 CODEC_ID_H264
#define AV_CODEC_ID_AAC CODEC_ID_AAC
#define AV_CODEC_ID_MP3 CODEC_ID_MP3
#define AV_CODEC_ID_NONE CODEC_ID_NONE
typedef CodecID AVCodecID;
#endif

enum { LIBAV_VER = LIBAVFORMAT_VERSION_MAJOR };

namespace mozilla {

#define AV_FUNC(func, ver) extern typeof(func)* func;
#include "FFmpegFunctionList.h"
#undef AV_FUNC

}

#endif // __FFmpegLibs_h__
