/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "AppleVDALinker.h"
#include "nsDebug.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

AppleVDALinker::LinkStatus
AppleVDALinker::sLinkStatus = LinkStatus_INIT;

void* AppleVDALinker::sLink = nullptr;
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
  if (sLink) {
    LOG("Unlinking VideoDecodeAcceleration framework.");
#define LINK_FUNC(func)                                                   \
    func = nullptr;
#include "AppleVDAFunctions.h"
#undef LINK_FUNC
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
