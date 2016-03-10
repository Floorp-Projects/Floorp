/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ObservedDocShell_h_
#define mozilla_ObservedDocShell_h_

#include "MarkersStorage.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

class nsIDocShell;

namespace mozilla {
class AbstractTimelineMarker;

namespace dom {
struct ProfileTimelineMarker;
}

// # ObservedDocShell
//
// A wrapper around a docshell for which docshell-specific markers are
// allowed to exist. See TimelineConsumers for register/unregister logic.
class ObservedDocShell : public MarkersStorage
{
private:
  RefPtr<nsIDocShell> mDocShell;

  // Main thread only.
  nsTArray<UniquePtr<AbstractTimelineMarker>> mTimelineMarkers;
  bool mPopping;

  // Off the main thread only.
  nsTArray<UniquePtr<AbstractTimelineMarker>> mOffTheMainThreadTimelineMarkers;

public:
  explicit ObservedDocShell(nsIDocShell* aDocShell);
  nsIDocShell* operator*() const { return mDocShell.get(); }

  void AddMarker(UniquePtr<AbstractTimelineMarker>&& aMarker) override;
  void AddOTMTMarker(UniquePtr<AbstractTimelineMarker>&& aMarker) override;
  void ClearMarkers() override;
  void PopMarkers(JSContext* aCx, nsTArray<dom::ProfileTimelineMarker>& aStore) override;
};

} // namespace mozilla

#endif /* mozilla_ObservedDocShell_h_ */
