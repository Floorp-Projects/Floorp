/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObservedDocShell.h"

#include "TimelineMarker.h"
#include "mozilla/Move.h"

namespace mozilla {

ObservedDocShell::ObservedDocShell(nsDocShell* aDocShell)
  : mDocShell(aDocShell)
{}

void
ObservedDocShell::AddMarker(const char* aName, TracingMetadata aMetaData)
{
  TimelineMarker* marker = new TimelineMarker(mDocShell, aName, aMetaData);
  mTimelineMarkers.AppendElement(marker);
}

void
ObservedDocShell::AddMarker(UniquePtr<TimelineMarker>&& aMarker)
{
  mTimelineMarkers.AppendElement(Move(aMarker));
}

void
ObservedDocShell::ClearMarkers()
{
  mTimelineMarkers.Clear();
}

} // namespace mozilla
