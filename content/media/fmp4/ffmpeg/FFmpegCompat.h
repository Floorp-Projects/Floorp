/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegCompat_h__
#define __FFmpegCompat_h__

#include <libavcodec/version.h>

#if LIBAVCODEC_VERSION_MAJOR < 55
#define AV_CODEC_ID_H264 CODEC_ID_H264
#define AV_CODEC_ID_AAC CODEC_ID_AAC
typedef CodecID AVCodecID;
#endif

#endif // __FFmpegCompat_h__
