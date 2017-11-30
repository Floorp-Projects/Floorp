/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingFunctions.h"

#include "nsJSEnvironment.h"
#include "js/GCAPI.h"
#include "nsIAccessibilityService.h"
#include "xpcAccessibilityService.h"

namespace mozilla {
namespace dom {

/* static */ void
FuzzingFunctions::GarbageCollect(const GlobalObject&)
{
  nsJSContext::GarbageCollectNow(JS::gcreason::COMPONENT_UTILS,
                                 nsJSContext::NonIncrementalGC,
                                 nsJSContext::NonShrinkingGC);
}

/* static */ void
FuzzingFunctions::CycleCollect(const GlobalObject&)
{
  nsJSContext::CycleCollectNow();
}

/* static */ void
FuzzingFunctions::EnableAccessibility(const GlobalObject&,
                                      ErrorResult& aRv)
{
  RefPtr<nsIAccessibilityService> a11y;
  nsresult rv;

  rv = NS_GetAccessibilityService(getter_AddRefs(a11y));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

} // namespace dom
} // namespace mozilla
