/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoTimelineMarker.h"

#include "nsIDocShell.h"
#include "TimelineConsumers.h"
#include "MainThreadUtils.h"

namespace mozilla {

AutoTimelineMarker::AutoTimelineMarker(nsIDocShell* aDocShell,
                                       const char* aName)
    : mName(aName), mDocShell(nullptr) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aDocShell) {
    return;
  }

  if (!TimelineConsumers::HasConsumer(aDocShell)) {
    return;
  }

  mDocShell = aDocShell;
  TimelineConsumers::AddMarkerForDocShell(mDocShell, mName,
                                          MarkerTracingType::START);
}

AutoTimelineMarker::~AutoTimelineMarker() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDocShell) {
    return;
  }

  if (!TimelineConsumers::HasConsumer(mDocShell)) {
    return;
  }

  TimelineConsumers::AddMarkerForDocShell(mDocShell, mName,
                                          MarkerTracingType::END);
}

}  // namespace mozilla
