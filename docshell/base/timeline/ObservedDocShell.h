/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ObservedDocShell_h_
#define mozilla_ObservedDocShell_h_

#include "OTMTMarkerReceiver.h"
#include "nsTArray.h"
#include "mozilla/nsRefPtr.h"

class nsDocShell;

namespace mozilla {
class AbstractTimelineMarker;

namespace dom {
struct ProfileTimelineMarker;
}

// # ObservedDocShell
//
// A wrapper around a docshell for which docshell-specific markers are
// allowed to exist. See TimelineConsumers for register/unregister logic.
class ObservedDocShell : public LinkedListElement<ObservedDocShell>,
                         public OTMTMarkerReceiver
{
private:
  nsRefPtr<nsDocShell> mDocShell;
  nsTArray<UniquePtr<AbstractTimelineMarker>> mTimelineMarkers;

public:
  explicit ObservedDocShell(nsDocShell* aDocShell);
  nsDocShell* operator*() const { return mDocShell.get(); }

  void AddMarker(UniquePtr<AbstractTimelineMarker>&& aMarker);
  void AddOTMTMarkerClone(UniquePtr<AbstractTimelineMarker>& aMarker) override;

  void ClearMarkers();
  void PopMarkers(JSContext* aCx, nsTArray<dom::ProfileTimelineMarker>& aStore);
};

} // namespace mozilla

#endif /* mozilla_ObservedDocShell_h_ */
