/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegRuntimeLinker.h"
#include "mozilla/ArrayUtils.h"
#include "FFmpegLog.h"
#include "mozilla/Preferences.h"
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
  "libavcodec.56.dylib",
  "libavcodec.55.dylib",
  "libavcodec.54.dylib",
  "libavcodec.53.dylib",
#else
  "libavcodec-ffmpeg.so.56",
  "libavcodec.so.56",
  "libavcodec.so.55",
  "libavcodec.so.54",
  "libavcodec.so.53",
#endif
};

PRLibrary* FFmpegRuntimeLinker::sLinkedLib = nullptr;
const char* FFmpegRuntimeLinker::sLib = nullptr;
static unsigned (*avcodec_version)() = nullptr;

#define AV_FUNC(func, ver) void (*func)();

#define LIBAVCODEC_ALLVERSION
#include "FFmpegFunctionList.h"
#undef LIBAVCODEC_ALLVERSION
#undef AV_FUNC

/* static */ bool
FFmpegRuntimeLinker::Link()
{
  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  MOZ_ASSERT(NS_IsMainThread());

  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    const char* lib = sLibs[i];
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = lib;
    sLinkedLib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
    if (sLinkedLib) {
      if (Bind(lib)) {
        sLib = lib;
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

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ bool
FFmpegRuntimeLinker::Bind(const char* aLibName)
{
  avcodec_version = (typeof(avcodec_version))PR_FindSymbol(sLinkedLib,
                                                           "avcodec_version");
  uint32_t major, minor;
  if (!GetVersion(major, minor)) {
    return false;
  }
  if (major > 55) {
    // All major greater than 56 currently use the same ABI as 55.
    major = 55;
  }

#define LIBAVCODEC_ALLVERSION
#define AV_FUNC(func, ver)                                                     \
  if (ver == 0 || ver == major) {                                              \
    if (!(func = (typeof(func))PR_FindSymbol(sLinkedLib, #func))) {            \
      FFMPEG_LOG("Couldn't load function " #func " from %s.", aLibName);       \
      return false;                                                            \
    }                                                                          \
  }
#include "FFmpegFunctionList.h"
#undef AV_FUNC
#undef LIBAVCODEC_ALLVERSION
  return true;
}

/* static */ already_AddRefed<PlatformDecoderModule>
FFmpegRuntimeLinker::CreateDecoderModule()
{
  if (!Link()) {
    return nullptr;
  }
  uint32_t major, minor;
  if (!GetVersion(major, minor)) {
    return  nullptr;
  }

  RefPtr<PlatformDecoderModule> module;
  switch (major) {
    case 53: module = FFmpegDecoderModule<53>::Create(); break;
    case 54: module = FFmpegDecoderModule<54>::Create(); break;
    default: module = FFmpegDecoderModule<55>::Create(); break;
  }
  return module.forget();
}

/* static */ void
FFmpegRuntimeLinker::Unlink()
{
  if (sLinkedLib) {
    PR_UnloadLibrary(sLinkedLib);
    sLinkedLib = nullptr;
    sLib = nullptr;
    sLinkStatus = LinkStatus_INIT;
    avcodec_version = nullptr;
  }
}

/* static */ bool
FFmpegRuntimeLinker::GetVersion(uint32_t& aMajor, uint32_t& aMinor)
{
  if (!avcodec_version) {
    return false;
  }
  uint32_t version = avcodec_version();
  aMajor = (version >> 16) & 0xff;
  aMinor = (version >> 8) & 0xff;
  return true;
}

} // namespace mozilla
