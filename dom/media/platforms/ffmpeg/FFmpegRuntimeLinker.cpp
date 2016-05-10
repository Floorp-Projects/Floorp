/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegRuntimeLinker.h"
#include "FFmpegLibWrapper.h"
#include "mozilla/ArrayUtils.h"
#include "FFmpegLog.h"
#include "prlink.h"

namespace mozilla
{

FFmpegRuntimeLinker::LinkStatus FFmpegRuntimeLinker::sLinkStatus =
  LinkStatus_INIT;

template <int V> class FFmpegDecoderModule
{
public:
  static already_AddRefed<PlatformDecoderModule> Create(FFmpegLibWrapper*);
};

static FFmpegLibWrapper sLibAV;

static const char* sLibs[] = {
#if defined(XP_DARWIN)
  "libavcodec.57.dylib",
  "libavcodec.56.dylib",
  "libavcodec.55.dylib",
  "libavcodec.54.dylib",
  "libavcodec.53.dylib",
#else
  "libavcodec-ffmpeg.so.57",
  "libavcodec-ffmpeg.so.56",
  "libavcodec.so.57",
  "libavcodec.so.56",
  "libavcodec.so.55",
  "libavcodec.so.54",
  "libavcodec.so.53",
#endif
};

/* static */ bool
FFmpegRuntimeLinker::Init()
{
  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    const char* lib = sLibs[i];
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = lib;
    sLibAV.mAVCodecLib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
    if (sLibAV.mAVCodecLib) {
      sLibAV.mAVUtilLib = sLibAV.mAVCodecLib;
      if (sLibAV.Link()) {
        sLinkStatus = LinkStatus_SUCCEEDED;
        return true;
      }
    }
  }

  FFMPEG_LOG("H264/AAC codecs unsupported without [");
  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    FFMPEG_LOG("%s %s", i ? "," : " ", sLibs[i]);
  }
  FFMPEG_LOG(" ]\n");

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ already_AddRefed<PlatformDecoderModule>
FFmpegRuntimeLinker::CreateDecoderModule()
{
  if (!Init()) {
    return nullptr;
  }
  RefPtr<PlatformDecoderModule> module;
  switch (sLibAV.mVersion) {
    case 53: module = FFmpegDecoderModule<53>::Create(&sLibAV); break;
    case 54: module = FFmpegDecoderModule<54>::Create(&sLibAV); break;
    case 55:
    case 56: module = FFmpegDecoderModule<55>::Create(&sLibAV); break;
    case 57: module = FFmpegDecoderModule<57>::Create(&sLibAV); break;
    default: module = nullptr;
  }
  return module.forget();
}

} // namespace mozilla
