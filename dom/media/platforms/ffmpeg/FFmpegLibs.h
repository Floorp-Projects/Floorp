/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegLibs_h__
#define __FFmpegLibs_h__

extern "C" {
#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/mem.h"
#ifdef __GNUC__
#pragma GCC visibility pop
#endif
}

#if LIBAVCODEC_VERSION_MAJOR < 55
#define AV_CODEC_ID_VP6F CODEC_ID_VP6F
#define AV_CODEC_ID_H264 CODEC_ID_H264
#define AV_CODEC_ID_AAC CODEC_ID_AAC
#define AV_CODEC_ID_MP3 CODEC_ID_MP3
#define AV_CODEC_ID_VP8 CODEC_ID_VP8
#define AV_CODEC_ID_NONE CODEC_ID_NONE
#define AV_CODEC_ID_FLAC CODEC_ID_FLAC
typedef CodecID AVCodecID;
#endif

#ifdef FFVPX_VERSION
enum { LIBAV_VER = FFVPX_VERSION };
#else
enum { LIBAV_VER = LIBAVCODEC_VERSION_MAJOR };
#endif

#endif // __FFmpegLibs_h__
