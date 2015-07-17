/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AutoGlobalTimelineMarker.h"

#include "MainThreadUtils.h"
#include "nsDocShell.h"

namespace mozilla {

void
AutoGlobalTimelineMarker::PopulateDocShells()
{
  const LinkedList<nsDocShell::ObservedDocShell>& docShells =
    nsDocShell::GetObservedDocShells();
  MOZ_ASSERT(!docShells.isEmpty());

  for (const nsDocShell::ObservedDocShell* ds = docShells.getFirst();
       ds;
       ds = ds->getNext()) {
    mOk = mDocShells.append(**ds);
    if (!mOk) {
      return;
    }
  }
}

AutoGlobalTimelineMarker::AutoGlobalTimelineMarker(const char* aName
                                                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mOk(true)
  , mDocShells()
  , mName(aName)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  MOZ_ASSERT(NS_IsMainThread());

  if (nsDocShell::gProfileTimelineRecordingsCount == 0) {
    return;
  }

  PopulateDocShells();
  if (!mOk) {
    // If we don't successfully populate our vector with *all* docshells being
    // observed, don't add markers to *any* of them.
    return;
  }

  for (Vector<nsRefPtr<nsDocShell>>::Range range = mDocShells.all();
       !range.empty();
       range.popFront()) {
    range.front()->AddProfileTimelineMarker(mName, TRACING_INTERVAL_START);
  }
}

AutoGlobalTimelineMarker::~AutoGlobalTimelineMarker()
{
  if (!mOk) {
    return;
  }

  for (Vector<nsRefPtr<nsDocShell>>::Range range = mDocShells.all();
       !range.empty();
       range.popFront()) {
    range.front()->AddProfileTimelineMarker(mName, TRACING_INTERVAL_END);
  }
}

} // namespace mozilla
