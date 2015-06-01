/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "AppleVTLinker.h"
#include "MainThreadUtils.h"
#include "mozilla/ArrayUtils.h"
#include "nsDebug.h"

PRLogModuleInfo* GetAppleMediaLog();
#define LOG(...) MOZ_LOG(GetAppleMediaLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

AppleVTLinker::LinkStatus
AppleVTLinker::sLinkStatus = LinkStatus_INIT;

void* AppleVTLinker::sLink = nullptr;
nsrefcnt AppleVTLinker::sRefCount = 0;
CFStringRef AppleVTLinker::skPropEnableHWAccel = nullptr;
CFStringRef AppleVTLinker::skPropUsingHWAccel = nullptr;

#define LINK_FUNC(func) typeof(func) func;
#include "AppleVTFunctions.h"
#undef LINK_FUNC

/* static */ bool
AppleVTLinker::Link()
{
  // Bump our reference count every time we're called.
  // Add a lock or change the thread assertion if
  // you need to call this off the main thread.
  MOZ_ASSERT(NS_IsMainThread());
  ++sRefCount;

  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  const char* dlnames[] =
    { "/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox",
      "/System/Library/PrivateFrameworks/VideoToolbox.framework/VideoToolbox" };
  bool dlfound = false;
  for (size_t i = 0; i < ArrayLength(dlnames); i++) {
    if ((sLink = dlopen(dlnames[i], RTLD_NOW | RTLD_LOCAL))) {
      dlfound = true;
      break;
    }
  }
  if (!dlfound) {
    NS_WARNING("Couldn't load VideoToolbox framework");
    goto fail;
  }

#define LINK_FUNC(func)                                        \
  func = (typeof(func))dlsym(sLink, #func);                    \
  if (!func) {                                                 \
    NS_WARNING("Couldn't load VideoToolbox function " #func ); \
    goto fail;                                                 \
  }
#include "AppleVTFunctions.h"
#undef LINK_FUNC

  // Will only resolve in 10.9 and later.
  skPropEnableHWAccel =
    GetIOConst("kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder");
  skPropUsingHWAccel =
    GetIOConst("kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder");

  LOG("Loaded VideoToolbox framework.");
  sLinkStatus = LinkStatus_SUCCEEDED;
  return true;

fail:
  Unlink();

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ void
AppleVTLinker::Unlink()
{
  // We'll be called by multiple Decoders, one intantiated for
  // each media element. Therefore we receive must maintain a
  // reference count to avoidunloading our symbols when other
  // instances still need them.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sRefCount > 0, "Unbalanced Unlink()");
  --sRefCount;
  if (sLink && sRefCount < 1) {
    LOG("Unlinking VideoToolbox framework.");
#define LINK_FUNC(func)                                                   \
    func = nullptr;
#include "AppleVTFunctions.h"
#undef LINK_FUNC
    dlclose(sLink);
    sLink = nullptr;
    skPropEnableHWAccel = nullptr;
    skPropUsingHWAccel = nullptr;
    sLinkStatus = LinkStatus_INIT;
  }
}

/* static */ CFStringRef
AppleVTLinker::GetIOConst(const char* symbol)
{
  CFStringRef* address = (CFStringRef*)dlsym(sLink, symbol);
  if (!address) {
    return nullptr;
  }

  return *address;
}

} // namespace mozilla
