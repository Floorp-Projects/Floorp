/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingFunctions.h"

#include "nsJSEnvironment.h"
#include "js/GCAPI.h"

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

} // namespace dom
} // namespace mozilla
