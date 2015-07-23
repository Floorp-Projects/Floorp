/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ObservedDocShell_h_
#define ObservedDocShell_h_

#include "GeckoProfiler.h"
#include "nsTArray.h"
#include "nsRefPtr.h"

class nsDocShell;
class TimelineMarker;

namespace mozilla {

// # ObservedDocShell
//
// A wrapper around a docshell for which docshell-specific markers are
// allowed to exist. See TimelineConsumers for register/unregister logic.
class ObservedDocShell : public LinkedListElement<ObservedDocShell>
{
private:
  nsRefPtr<nsDocShell> mDocShell;

public:
  // FIXME: make this private once all marker-specific logic has been
  // moved out of nsDocShell.
  nsTArray<UniquePtr<TimelineMarker>> mTimelineMarkers;

  explicit ObservedDocShell(nsDocShell* aDocShell);
  nsDocShell* operator*() const { return mDocShell.get(); }

  void AddMarker(const char* aName, TracingMetadata aMetaData);
  void AddMarker(UniquePtr<TimelineMarker>&& aMarker);
  void ClearMarkers();
};

} // namespace mozilla

#endif /* ObservedDocShell_h_ */
