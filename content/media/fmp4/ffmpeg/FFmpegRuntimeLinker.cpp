/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "FFmpegRuntimeLinker.h"
#include "mozilla/ArrayUtils.h"
#include "FFmpegLog.h"

#define NUM_ELEMENTS(X) (sizeof(X) / sizeof((X)[0]))

namespace mozilla
{

FFmpegRuntimeLinker::LinkStatus FFmpegRuntimeLinker::sLinkStatus =
  LinkStatus_INIT;

struct AvFormatLib
{
  const char* Name;
  PlatformDecoderModule* (*Factory)();
};

template <int V> class FFmpegDecoderModule
{
public:
  static PlatformDecoderModule* Create();
};

static const AvFormatLib sLibs[] = {
  { "libavformat.so.55", FFmpegDecoderModule<55>::Create },
  { "libavformat.so.54", FFmpegDecoderModule<54>::Create },
  { "libavformat.so.53", FFmpegDecoderModule<53>::Create },
};

void* FFmpegRuntimeLinker::sLinkedLib = nullptr;
const AvFormatLib* FFmpegRuntimeLinker::sLib = nullptr;

#define AV_FUNC(func) void (*func)();
#include "FFmpegFunctionList.h"
#undef AV_FUNC

/* static */ bool
FFmpegRuntimeLinker::Link()
{
  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  for (size_t i = 0; i < ArrayLength(sLibs); i++) {
    const AvFormatLib* lib = &sLibs[i];
    sLinkedLib = dlopen(lib->Name, RTLD_NOW | RTLD_LOCAL);
    if (sLinkedLib) {
      if (Bind(lib->Name)) {
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
  return nullptr;
}

/* static */ bool
FFmpegRuntimeLinker::Bind(const char* aLibName)
{
#define AV_FUNC(func)                                                          \
  if (!(func = (typeof(func))dlsym(sLinkedLib, #func))) {                      \
    FFMPEG_LOG("Couldn't load function " #func " from %s.", aLibName);         \
    return false;                                                              \
  }
#include "FFmpegFunctionList.h"
#undef AV_FUNC
  return true;
}

/* static */ PlatformDecoderModule*
FFmpegRuntimeLinker::CreateDecoderModule()
{
  PlatformDecoderModule* module = sLib->Factory();
  return module;
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
