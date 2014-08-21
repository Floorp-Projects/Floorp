/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "AppleCMLinker.h"
#include "MainThreadUtils.h"
#include "nsDebug.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetAppleMediaLog();
#define LOG(...) PR_LOG(GetAppleMediaLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

namespace mozilla {

AppleCMLinker::LinkStatus
AppleCMLinker::sLinkStatus = LinkStatus_INIT;

void* AppleCMLinker::sLink = nullptr;
nsrefcnt AppleCMLinker::sRefCount = 0;

#define LINK_FUNC(func) typeof(func) func;
#include "AppleCMFunctions.h"
#undef LINK_FUNC

/* static */ bool
AppleCMLinker::Link()
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
    "/System/Library/Frameworks/CoreMedia.framework/CoreMedia";
  if (!(sLink = dlopen(dlname, RTLD_NOW | RTLD_LOCAL))) {
    NS_WARNING("Couldn't load CoreMedia framework");
    goto fail;
  }

#define LINK_FUNC(func)                                        \
  func = (typeof(func))dlsym(sLink, #func);                    \
  if (!func) {                                                 \
    NS_WARNING("Couldn't load CoreMedia function " #func ); \
    goto fail;                                                 \
  }
#include "AppleCMFunctions.h"
#undef LINK_FUNC

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
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sLink && sRefCount > 0, "Unbalanced Unlink()");
  --sRefCount;
  if (sLink && sRefCount < 1) {
    LOG("Unlinking CoreMedia framework.");
    dlclose(sLink);
    sLink = nullptr;
  }
}

} // namespace mozilla
