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

namespace mozilla {

FFmpegRuntimeLinker::LinkStatus FFmpegRuntimeLinker::sLinkStatus =
    LinkStatus_INIT;
const char* FFmpegRuntimeLinker::sLinkStatusLibraryName = "";

template <int V>
class FFmpegDecoderModule {
 public:
  static already_AddRefed<PlatformDecoderModule> Create(FFmpegLibWrapper*);
};

static FFmpegLibWrapper sLibAV;

static const char* sLibs[] = {
// clang-format off
#if defined(XP_DARWIN)
  "libavcodec.60.dylib",
  "libavcodec.59.dylib",
  "libavcodec.58.dylib",
  "libavcodec.57.dylib",
  "libavcodec.56.dylib",
  "libavcodec.55.dylib",
  "libavcodec.54.dylib",
  "libavcodec.53.dylib",
#elif defined(XP_OPENBSD)
  "libavcodec.so", // OpenBSD hardly controls the major/minor library version
                   // of ffmpeg and update it regulary on ABI/API changes
#else
  "libavcodec.so.60",
  "libavcodec.so.59",
  "libavcodec.so.58",
  "libavcodec-ffmpeg.so.58",
  "libavcodec-ffmpeg.so.57",
  "libavcodec-ffmpeg.so.56",
  "libavcodec.so.57",
  "libavcodec.so.56",
  "libavcodec.so.55",
  "libavcodec.so.54",
  "libavcodec.so.53",
#endif
    // clang-format on
};

/* static */
bool FFmpegRuntimeLinker::Init() {
  if (sLinkStatus != LinkStatus_INIT) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

#ifdef MOZ_WIDGET_GTK
  sLibAV.LinkVAAPILibs();
#endif

  // While going through all possible libs, this status will be updated with a
  // more precise error if possible.
  sLinkStatus = LinkStatus_NOT_FOUND;

  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    const char* lib = sLibs[i];
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = lib;
    sLibAV.mAVCodecLib =
        PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
    if (sLibAV.mAVCodecLib) {
      sLibAV.mAVUtilLib = sLibAV.mAVCodecLib;
      switch (sLibAV.Link()) {
        case FFmpegLibWrapper::LinkResult::Success:
          sLinkStatus = LinkStatus_SUCCEEDED;
          sLinkStatusLibraryName = lib;
          return true;
        case FFmpegLibWrapper::LinkResult::NoProvidedLib:
          MOZ_ASSERT_UNREACHABLE("Incorrectly-setup sLibAV");
          break;
        case FFmpegLibWrapper::LinkResult::NoAVCodecVersion:
          if (sLinkStatus > LinkStatus_INVALID_CANDIDATE) {
            sLinkStatus = LinkStatus_INVALID_CANDIDATE;
            sLinkStatusLibraryName = lib;
          }
          break;
        case FFmpegLibWrapper::LinkResult::CannotUseLibAV57:
          if (sLinkStatus > LinkStatus_UNUSABLE_LIBAV57) {
            sLinkStatus = LinkStatus_UNUSABLE_LIBAV57;
            sLinkStatusLibraryName = lib;
          }
          break;
        case FFmpegLibWrapper::LinkResult::BlockedOldLibAVVersion:
          if (sLinkStatus > LinkStatus_OBSOLETE_LIBAV) {
            sLinkStatus = LinkStatus_OBSOLETE_LIBAV;
            sLinkStatusLibraryName = lib;
          }
          break;
        case FFmpegLibWrapper::LinkResult::UnknownFutureLibAVVersion:
        case FFmpegLibWrapper::LinkResult::MissingLibAVFunction:
          if (sLinkStatus > LinkStatus_INVALID_LIBAV_CANDIDATE) {
            sLinkStatus = LinkStatus_INVALID_LIBAV_CANDIDATE;
            sLinkStatusLibraryName = lib;
          }
          break;
        case FFmpegLibWrapper::LinkResult::UnknownFutureFFMpegVersion:
        case FFmpegLibWrapper::LinkResult::MissingFFMpegFunction:
          if (sLinkStatus > LinkStatus_INVALID_FFMPEG_CANDIDATE) {
            sLinkStatus = LinkStatus_INVALID_FFMPEG_CANDIDATE;
            sLinkStatusLibraryName = lib;
          }
          break;
        case FFmpegLibWrapper::LinkResult::UnknownOlderFFMpegVersion:
          if (sLinkStatus > LinkStatus_OBSOLETE_FFMPEG) {
            sLinkStatus = LinkStatus_OBSOLETE_FFMPEG;
            sLinkStatusLibraryName = lib;
          }
          break;
      }
    }
  }

  FFMPEGV_LOG("H264/AAC codecs unsupported without [");
  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    FFMPEGV_LOG("%s %s", i ? "," : " ", sLibs[i]);
  }
  FFMPEGV_LOG(" ]\n");

  return false;
}

/* static */
already_AddRefed<PlatformDecoderModule> FFmpegRuntimeLinker::Create() {
  if (!Init()) {
    return nullptr;
  }
  RefPtr<PlatformDecoderModule> module;
  switch (sLibAV.mVersion) {
    case 53:
      module = FFmpegDecoderModule<53>::Create(&sLibAV);
      break;
    case 54:
      module = FFmpegDecoderModule<54>::Create(&sLibAV);
      break;
    case 55:
    case 56:
      module = FFmpegDecoderModule<55>::Create(&sLibAV);
      break;
    case 57:
      module = FFmpegDecoderModule<57>::Create(&sLibAV);
      break;
    case 58:
      module = FFmpegDecoderModule<58>::Create(&sLibAV);
      break;
    case 59:
      module = FFmpegDecoderModule<59>::Create(&sLibAV);
      break;
    case 60:
      module = FFmpegDecoderModule<60>::Create(&sLibAV);
      break;
    default:
      module = nullptr;
  }
  return module.forget();
}

/* static */ const char* FFmpegRuntimeLinker::LinkStatusString() {
  switch (sLinkStatus) {
    case LinkStatus_INIT:
      return "Libavcodec not initialized yet";
    case LinkStatus_SUCCEEDED:
      return "Libavcodec linking succeeded";
    case LinkStatus_INVALID_FFMPEG_CANDIDATE:
      return "Invalid FFMpeg libavcodec candidate";
    case LinkStatus_UNUSABLE_LIBAV57:
      return "Unusable LibAV's libavcodec 57";
    case LinkStatus_INVALID_LIBAV_CANDIDATE:
      return "Invalid LibAV libavcodec candidate";
    case LinkStatus_OBSOLETE_FFMPEG:
      return "Obsolete FFMpeg libavcodec candidate";
    case LinkStatus_OBSOLETE_LIBAV:
      return "Obsolete LibAV libavcodec candidate";
    case LinkStatus_INVALID_CANDIDATE:
      return "Invalid libavcodec candidate";
    case LinkStatus_NOT_FOUND:
      return "Libavcodec not found";
  }
  MOZ_ASSERT_UNREACHABLE("Unknown sLinkStatus value");
  return "?";
}

}  // namespace mozilla
