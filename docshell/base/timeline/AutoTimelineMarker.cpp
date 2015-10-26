/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoTimelineMarker.h"

#include "TimelineConsumers.h"
#include "MainThreadUtils.h"

namespace mozilla {

AutoTimelineMarker::AutoTimelineMarker(nsIDocShell* aDocShell, const char* aName
                                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mName(aName)
  , mDocShell(nullptr)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  MOZ_ASSERT(NS_IsMainThread());

  if (!aDocShell) {
    return;
  }

  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  if (!timelines || !timelines->HasConsumer(aDocShell)) {
    return;
  }

  mDocShell = aDocShell;
  timelines->AddMarkerForDocShell(mDocShell, mName, MarkerTracingType::START);
}

AutoTimelineMarker::~AutoTimelineMarker()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDocShell) {
    return;
  }

  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  if (!timelines || !timelines->HasConsumer(mDocShell)) {
    return;
  }

  timelines->AddMarkerForDocShell(mDocShell, mName, MarkerTracingType::END);
}

} // namespace mozilla
