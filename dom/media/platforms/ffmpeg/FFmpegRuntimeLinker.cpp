/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "FFmpegRuntimeLinker.h"
#include "mozilla/ArrayUtils.h"
#include "FFmpegLog.h"
#include "mozilla/Preferences.h"

#define NUM_ELEMENTS(X) (sizeof(X) / sizeof((X)[0]))

namespace mozilla
{

bool FFmpegRuntimeLinker::sFFmpegDecoderEnabled = false;

FFmpegRuntimeLinker::LinkStatus FFmpegRuntimeLinker::sLinkStatus =
  LinkStatus_INIT;

struct AvCodecLib
{
  const char* Name;
  already_AddRefed<PlatformDecoderModule> (*Factory)();
  uint32_t Version;
};

template <int V> class FFmpegDecoderModule
{
public:
  static already_AddRefed<PlatformDecoderModule> Create();
};

static const AvCodecLib sLibs[] = {
  { "libavcodec-ffmpeg.so.56", FFmpegDecoderModule<55>::Create, 55 },
  { "libavcodec.so.56", FFmpegDecoderModule<55>::Create, 55 },
  { "libavcodec.so.55", FFmpegDecoderModule<55>::Create, 55 },
  { "libavcodec.so.54", FFmpegDecoderModule<54>::Create, 54 },
  { "libavcodec.so.53", FFmpegDecoderModule<53>::Create, 53 },
  { "libavcodec.56.dylib", FFmpegDecoderModule<55>::Create, 55 },
  { "libavcodec.55.dylib", FFmpegDecoderModule<55>::Create, 55 },
  { "libavcodec.54.dylib", FFmpegDecoderModule<54>::Create, 54 },
  { "libavcodec.53.dylib", FFmpegDecoderModule<53>::Create, 53 },
};

void* FFmpegRuntimeLinker::sLinkedLib = nullptr;
const AvCodecLib* FFmpegRuntimeLinker::sLib = nullptr;

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

  Preferences::AddBoolVarCache(&sFFmpegDecoderEnabled,
                               "media.fragmented-mp4.ffmpeg.enabled", false);

  MOZ_ASSERT(NS_IsMainThread());

  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    const AvCodecLib* lib = &sLibs[i];
    sLinkedLib = dlopen(lib->Name, RTLD_NOW | RTLD_LOCAL);
    if (sLinkedLib) {
      if (Bind(lib->Name, lib->Version)) {
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
    FFMPEG_LOG("%s %s", i ? "," : "", sLibs[i].Name);
  }
  FFMPEG_LOG(" ]\n");

  Unlink();

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ bool
FFmpegRuntimeLinker::Bind(const char* aLibName, uint32_t Version)
{
#define LIBAVCODEC_ALLVERSION
#define AV_FUNC(func, ver)                                                     \
  if (ver == 0 || ver == Version) {                                            \
    if (!(func = (typeof(func))dlsym(sLinkedLib, #func))) {                    \
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
  nsRefPtr<PlatformDecoderModule> module = sLib->Factory();
  return module.forget();
}

/* static */ void
FFmpegRuntimeLinker::Unlink()
{
  if (sLinkedLib) {
    dlclose(sLinkedLib);
    sLinkedLib = nullptr;
    sLib = nullptr;
    sLinkStatus = LinkStatus_INIT;
  }
}

} // namespace mozilla
