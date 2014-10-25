/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "AppleVDALinker.h"
#include "MainThreadUtils.h"
#include "nsDebug.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetAppleMediaLog();
#define LOG(...) PR_LOG(GetAppleMediaLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

namespace mozilla {

AppleVDALinker::LinkStatus
AppleVDALinker::sLinkStatus = LinkStatus_INIT;

void* AppleVDALinker::sLink = nullptr;
nsrefcnt AppleVDALinker::sRefCount = 0;
CFStringRef AppleVDALinker::skPropWidth = nullptr;
CFStringRef AppleVDALinker::skPropHeight = nullptr;
CFStringRef AppleVDALinker::skPropSourceFormat = nullptr;
CFStringRef AppleVDALinker::skPropAVCCData = nullptr;

#define LINK_FUNC(func) typeof(func) func;
#include "AppleVDAFunctions.h"
#undef LINK_FUNC

/* static */ bool
AppleVDALinker::Link()
{
  // Bump our reference count every time we're called.
  // Add a lock or change the thread assertion if
  // you need to call this off the main thread.
  MOZ_ASSERT(NS_IsMainThread());
  ++sRefCount;

  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  const char* dlname =
    "/System/Library/Frameworks/VideoDecodeAcceleration.framework/VideoDecodeAcceleration";

  if (!(sLink = dlopen(dlname, RTLD_NOW | RTLD_LOCAL))) {
    NS_WARNING("Couldn't load VideoDecodeAcceleration framework");
    goto fail;
  }

#define LINK_FUNC(func)                                                   \
  func = (typeof(func))dlsym(sLink, #func);                               \
  if (!func) {                                                            \
    NS_WARNING("Couldn't load VideoDecodeAcceleration function " #func ); \
    goto fail;                                                            \
  }
#include "AppleVDAFunctions.h"
#undef LINK_FUNC

  skPropWidth = GetIOConst("kVDADecoderConfiguration_Width");
  skPropHeight = GetIOConst("kVDADecoderConfiguration_Height");
  skPropSourceFormat = GetIOConst("kVDADecoderConfiguration_SourceFormat");
  skPropAVCCData = GetIOConst("kVDADecoderConfiguration_avcCData");

  if (!skPropWidth || !skPropHeight || !skPropSourceFormat || !skPropAVCCData) {
    goto fail;
  }

  LOG("Loaded VideoDecodeAcceleration framework.");
  sLinkStatus = LinkStatus_SUCCEEDED;
  return true;

fail:
  Unlink();

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ void
AppleVDALinker::Unlink()
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
    dlclose(sLink);
    sLink = nullptr;
    skPropWidth = nullptr;
    skPropHeight = nullptr;
    skPropSourceFormat = nullptr;
    skPropAVCCData = nullptr;
    sLinkStatus = LinkStatus_INIT;
  }
}

/* static */ CFStringRef
AppleVDALinker::GetIOConst(const char* symbol)
{
  CFStringRef* address = (CFStringRef*)dlsym(sLink, symbol);
  if (!address) {
    return nullptr;
  }

  return *address;
}

} // namespace mozilla
