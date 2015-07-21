/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AutoTimelineMarker.h"

#include "MainThreadUtils.h"
#include "nsDocShell.h"

namespace mozilla {

AutoTimelineMarker::AutoTimelineMarker(nsIDocShell* aDocShell, const char* aName
                                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mName(aName)
  , mDocShell(nullptr)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  MOZ_ASSERT(NS_IsMainThread());

  if (TimelineConsumers::IsEmpty()) {
    return;
  }

  bool isRecordingEnabledForDocShell = false;
  nsDocShell* docShell = static_cast<nsDocShell*>(aDocShell);
  aDocShell->GetRecordProfileTimelineMarkers(&isRecordingEnabledForDocShell);

  if (isRecordingEnabledForDocShell) {
    mDocShell = docShell;
    mDocShell->AddProfileTimelineMarker(mName, TRACING_INTERVAL_START);
  }
}

AutoTimelineMarker::~AutoTimelineMarker()
{
  if (mDocShell) {
    mDocShell->AddProfileTimelineMarker(mName, TRACING_INTERVAL_END);
  }
}

} // namespace mozilla
