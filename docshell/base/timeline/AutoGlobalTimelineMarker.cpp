/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoGlobalTimelineMarker.h"

#include "TimelineConsumers.h"
#include "MainThreadUtils.h"

namespace mozilla {

AutoGlobalTimelineMarker::AutoGlobalTimelineMarker(
    const char* aName, MarkerStackRequest aStackRequest /* = STACK */
    )
    : mName(aName), mStackRequest(aStackRequest) {
  MOZ_ASSERT(NS_IsMainThread());

  if (TimelineConsumers::IsEmpty()) {
    return;
  }

  TimelineConsumers::AddMarkerForAllObservedDocShells(
      mName, MarkerTracingType::START, mStackRequest);
}

AutoGlobalTimelineMarker::~AutoGlobalTimelineMarker() {
  MOZ_ASSERT(NS_IsMainThread());

  if (TimelineConsumers::IsEmpty()) {
    return;
  }

  TimelineConsumers::AddMarkerForAllObservedDocShells(
      mName, MarkerTracingType::END, mStackRequest);
}

}  // namespace mozilla
