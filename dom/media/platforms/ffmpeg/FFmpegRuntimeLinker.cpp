/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegRuntimeLinker.h"
#include "mozilla/ArrayUtils.h"
#include "FFmpegLog.h"
#include "mozilla/Types.h"
#include "prlink.h"

namespace mozilla
{

FFmpegRuntimeLinker::LinkStatus FFmpegRuntimeLinker::sLinkStatus =
  LinkStatus_INIT;

template <int V> class FFmpegDecoderModule
{
public:
  static already_AddRefed<PlatformDecoderModule> Create();
};

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

PRLibrary* FFmpegRuntimeLinker::sLinkedLib = nullptr;
PRLibrary* FFmpegRuntimeLinker::sLinkedUtilLib = nullptr;
static unsigned (*avcodec_version)() = nullptr;

#if !defined(XP_WIN)
#ifdef __GNUC__
#define AV_FUNC(func, ver) void (*func)();
#define LIBAVCODEC_ALLVERSION
#else
#define AV_FUNC(func, ver) decltype(func)* func;
#endif
#include "FFmpegFunctionList.h"
#undef AV_FUNC
#endif

static PRLibrary*
MozAVLink(const char* aName)
{
  PRLibSpec lspec;
  lspec.type = PR_LibSpec_Pathname;
  lspec.value.pathname = aName;
  return PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
}

/* static */ bool
FFmpegRuntimeLinker::Link()
{
  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }
#if !defined(XP_WIN)
  MOZ_ASSERT(NS_IsMainThread());

  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    const char* lib = sLibs[i];
    sLinkedLib = MozAVLink(lib);
    if (sLinkedLib) {
      sLinkedUtilLib = sLinkedLib;
      if (Bind(lib)) {
        sLinkStatus = LinkStatus_SUCCEEDED;
        return true;
      }
      // Shouldn't happen but if it does then we try the next lib..
      Unlink();
    }
  }

  FFMPEG_LOG("H264/AAC codecs unsupported without [");
  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    FFMPEG_LOG("%s %s", i ? "," : "", sLibs[i]);
  }
  FFMPEG_LOG(" ]\n");

  Unlink();
#endif

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ bool
FFmpegRuntimeLinker::Bind(const char* aLibName)
{
#if defined(XP_WIN)
  return false;
#else
  avcodec_version = (decltype(avcodec_version))PR_FindSymbol(sLinkedLib,
                                                           "avcodec_version");
  uint32_t major, minor, micro;
  if (!GetVersion(major, minor, micro)) {
    return false;
  }

  int version;
  switch (major) {
    case 53:
      version = AV_FUNC_53;
      break;
    case 54:
      version = AV_FUNC_54;
      break;
    case 56:
      // We use libavcodec 55 code instead. Fallback
    case 55:
      version = AV_FUNC_55;
      break;
    case 57:
      if (micro != 100) {
        // a micro version of 100 indicates that it's FFmpeg (as opposed to LibAV.
        // Due to current AVCodecContext binary incompatibility we can only
        // support FFmpeg at this stage.
        return false;
      }
      version = AV_FUNC_57;
      break;
    default:
      // Not supported at this stage.
      return false;
  }

#define AV_FUNC(func, ver)                                                     \
  if ((ver) & version) {                                                       \
    if (!(func = (decltype(func))PR_FindSymbol(((ver) & AV_FUNC_AVUTIL_MASK) ? sLinkedUtilLib : sLinkedLib, #func))) { \
      FFMPEG_LOG("Couldn't load function " #func " from %s.", aLibName);       \
      return false;                                                            \
    }                                                                          \
  } else {                                                                     \
    func = (decltype(func))nullptr;                                            \
  }
#include "FFmpegFunctionList.h"
#undef AV_FUNC
  return true;
#endif
}

/* static */ already_AddRefed<PlatformDecoderModule>
FFmpegRuntimeLinker::CreateDecoderModule()
{
#if defined(XP_WIN)
  return nullptr;
#else
  if (!Link()) {
    return nullptr;
  }
  uint32_t major, minor, micro;
  if (!GetVersion(major, minor, micro)) {
    return  nullptr;
  }

  RefPtr<PlatformDecoderModule> module;
  switch (major) {
    case 53: module = FFmpegDecoderModule<53>::Create(); break;
    case 54: module = FFmpegDecoderModule<54>::Create(); break;
    case 55:
    case 56: module = FFmpegDecoderModule<55>::Create(); break;
    case 57: module = FFmpegDecoderModule<57>::Create(); break;
    default: module = nullptr;
  }
  return module.forget();
#endif
}

/* static */ void
FFmpegRuntimeLinker::Unlink()
{
  if (sLinkedUtilLib && sLinkedUtilLib != sLinkedLib) {
    PR_UnloadLibrary(sLinkedUtilLib);
  }
  if (sLinkedLib) {
    PR_UnloadLibrary(sLinkedLib);
    sLinkedLib = nullptr;
    sLinkStatus = LinkStatus_INIT;
    avcodec_version = nullptr;
  }
  sLinkedUtilLib = nullptr;
}

/* static */ bool
FFmpegRuntimeLinker::GetVersion(uint32_t& aMajor, uint32_t& aMinor, uint32_t& aMicro)
{
  if (!avcodec_version) {
    return false;
  }
  uint32_t version = avcodec_version();
  aMajor = (version >> 16) & 0xff;
  aMinor = (version >> 8) & 0xff;
  aMicro = version & 0xff;
  return true;
}

#undef LIBAVCODEC_ALLVERSION
} // namespace mozilla
