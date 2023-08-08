/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegLibs_h__
#define __FFmpegLibs_h__

extern "C" {
#ifdef __GNUC__
#  pragma GCC visibility push(default)
#endif
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/mem.h"
#ifdef MOZ_USE_HWDECODE
#  include "libavutil/hwcontext_vaapi.h"
#  include "libavutil/hwcontext_drm.h"
#endif
#ifdef __GNUC__
#  pragma GCC visibility pop
#endif
}

#if LIBAVCODEC_VERSION_MAJOR >= 58
#  include "libavcodec/codec_desc.h"
#endif  // LIBAVCODEC_VERSION_MAJOR >= 58

#if LIBAVCODEC_VERSION_MAJOR < 55
// This value is not defined in older version of libavcodec
#  define CODEC_ID_OPUS 86076
#  define AV_CODEC_ID_VP6F CODEC_ID_VP6F
#  define AV_CODEC_ID_H264 CODEC_ID_H264
#  define AV_CODEC_ID_AAC CODEC_ID_AAC
#  define AV_CODEC_ID_VORBIS CODEC_ID_VORBIS
#  define AV_CODEC_ID_OPUS CODEC_ID_OPUS
#  define AV_CODEC_ID_MP3 CODEC_ID_MP3
#  define AV_CODEC_ID_VP8 CODEC_ID_VP8
#  define AV_CODEC_ID_NONE CODEC_ID_NONE
#  define AV_CODEC_ID_FLAC CODEC_ID_FLAC
typedef CodecID AVCodecID;
#endif
#if LIBAVCODEC_VERSION_MAJOR <= 55
#  define AV_CODEC_FLAG_LOW_DELAY CODEC_FLAG_LOW_DELAY
#endif

#ifdef FFVPX_VERSION
enum { LIBAV_VER = FFVPX_VERSION };
#else
enum { LIBAV_VER = LIBAVCODEC_VERSION_MAJOR };
#endif

#endif  // __FFmpegLibs_h__
