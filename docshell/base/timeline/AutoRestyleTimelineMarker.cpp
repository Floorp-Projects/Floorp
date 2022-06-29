/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoRestyleTimelineMarker.h"

#include "TimelineConsumers.h"
#include "MainThreadUtils.h"
#include "nsIDocShell.h"
#include "RestyleTimelineMarker.h"

namespace mozilla {

AutoRestyleTimelineMarker::AutoRestyleTimelineMarker(nsIDocShell* aDocShell,
                                                     bool aIsAnimationOnly)
    : mDocShell(nullptr), mIsAnimationOnly(aIsAnimationOnly) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aDocShell) {
    return;
  }

  if (!TimelineConsumers::HasConsumer(aDocShell)) {
    return;
  }

  mDocShell = aDocShell;
  TimelineConsumers::AddMarkerForDocShell(
      mDocShell, MakeUnique<RestyleTimelineMarker>(mIsAnimationOnly,
                                                   MarkerTracingType::START));
}

AutoRestyleTimelineMarker::~AutoRestyleTimelineMarker() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDocShell) {
    return;
  }

  if (!TimelineConsumers::HasConsumer(mDocShell)) {
    return;
  }

  TimelineConsumers::AddMarkerForDocShell(
      mDocShell, MakeUnique<RestyleTimelineMarker>(mIsAnimationOnly,
                                                   MarkerTracingType::END));
}

}  // namespace mozilla
