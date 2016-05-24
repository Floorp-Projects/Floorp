/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "AppleCMLinker.h"
#include "mozilla/ArrayUtils.h"
#include "nsDebug.h"

#ifndef MOZ_WIDGET_UIKIT
#include "nsCocoaFeatures.h"
#endif

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

AppleCMLinker::LinkStatus
AppleCMLinker::sLinkStatus = LinkStatus_INIT;

void* AppleCMLinker::sLink = nullptr;
CFStringRef AppleCMLinker::skPropExtensionAtoms = nullptr;
CFStringRef AppleCMLinker::skPropFullRangeVideo = nullptr;

#define LINK_FUNC(func) typeof(CM ## func) CM ## func;
#include "AppleCMFunctions.h"
#undef LINK_FUNC

/* static */ bool
AppleCMLinker::Link()
{
  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  const char* dlnames[] =
    { "/System/Library/Frameworks/CoreMedia.framework/CoreMedia",
      "/System/Library/PrivateFrameworks/CoreMedia.framework/CoreMedia" };
  bool dlfound = false;
  for (size_t i = 0; i < ArrayLength(dlnames); i++) {
    if ((sLink = dlopen(dlnames[i], RTLD_NOW | RTLD_LOCAL))) {
      dlfound = true;
      break;
    }
  }
  if (!dlfound) {
    NS_WARNING("Couldn't load CoreMedia framework");
    goto fail;
  }

#ifdef MOZ_WIDGET_UIKIT
  if (true) {
#else
  if (nsCocoaFeatures::OnLionOrLater()) {
#endif
#define LINK_FUNC2(func)                                       \
  func = (typeof(func))dlsym(sLink, #func);                    \
  if (!func) {                                                 \
    NS_WARNING("Couldn't load CoreMedia function " #func );    \
    goto fail;                                                 \
  }
#define LINK_FUNC(func) LINK_FUNC2(CM ## func)
#include "AppleCMFunctions.h"
#undef LINK_FUNC
#undef LINK_FUNC2

    skPropExtensionAtoms =
      GetIOConst("kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms");

    skPropFullRangeVideo =
      GetIOConst("kCMFormatDescriptionExtension_FullRangeVideo");

  } else {
#define LINK_FUNC2(cm, fig)                                    \
  cm = (typeof(cm))dlsym(sLink, #fig);                         \
  if (!cm) {                                                   \
    NS_WARNING("Couldn't load CoreMedia function " #fig );     \
    goto fail;                                                 \
  }
#define LINK_FUNC(func) LINK_FUNC2(CM ## func, Fig ## func)
#include "AppleCMFunctions.h"
#undef LINK_FUNC
#undef LINK_FUNC2

    skPropExtensionAtoms =
      GetIOConst("kFigFormatDescriptionExtension_SampleDescriptionExtensionAtoms");
  }

  if (!skPropExtensionAtoms) {
    goto fail;
  }

  LOG("Loaded CoreMedia framework.");
  sLinkStatus = LinkStatus_SUCCEEDED;
  return true;

fail:
  Unlink();

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ void
AppleCMLinker::Unlink()
{
  if (sLink) {
    LOG("Unlinking CoreMedia framework.");
    dlclose(sLink);
    sLink = nullptr;
    sLinkStatus = LinkStatus_INIT;
  }
}

/* static */ CFStringRef
AppleCMLinker::GetIOConst(const char* symbol)
{
  CFStringRef* address = (CFStringRef*)dlsym(sLink, symbol);
  if (!address) {
    return nullptr;
  }

  return *address;
}

} // namespace mozilla
