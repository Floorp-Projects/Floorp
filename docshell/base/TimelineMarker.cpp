/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShell.h"
#include "TimelineMarker.h"

TimelineMarker::TimelineMarker(nsDocShell* aDocShell, const char* aName,
                               TracingMetadata aMetaData)
  : mName(aName)
  , mMetaData(aMetaData)
{
  MOZ_COUNT_CTOR(TimelineMarker);
  MOZ_ASSERT(aName);
  aDocShell->Now(&mTime);
  if (aMetaData == TRACING_INTERVAL_START) {
    CaptureStack();
  }
}

TimelineMarker::TimelineMarker(nsDocShell* aDocShell, const char* aName,
                               TracingMetadata aMetaData,
                               const nsAString& aCause)
  : mName(aName)
  , mMetaData(aMetaData)
  , mCause(aCause)
{
  MOZ_COUNT_CTOR(TimelineMarker);
  MOZ_ASSERT(aName);
  aDocShell->Now(&mTime);
  if (aMetaData == TRACING_INTERVAL_START) {
    CaptureStack();
  }
}

TimelineMarker::~TimelineMarker()
{
  MOZ_COUNT_DTOR(TimelineMarker);
}
