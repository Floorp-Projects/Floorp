/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BaseHistory_h
#define mozilla_BaseHistory_h

#include "IHistory.h"

/* A base class for history implementations that implement link coloring. */

namespace mozilla {

class BaseHistory : public IHistory {
 protected:
  static constexpr const size_t kTrackedUrisInitialSize = 64;

  BaseHistory()
    : mTrackedURIs(kTrackedUrisInitialSize) {}

  /**
   * Mark all links for the given URI in the given document as visited. Used
   * within NotifyVisited.
   */
  void NotifyVisitedForDocument(nsIURI*,
                                dom::Document*);

  /**
   * Dispatch a runnable for the document passed in which will call
   * NotifyVisitedForDocument with the correct URI and Document.
   */
  void DispatchNotifyVisited(nsIURI*, dom::Document*);


  static dom::Document* GetLinkDocument(dom::Link&);

  using ObserverArray = nsTObserverArray<dom::Link*>;
  struct TrackedURI {
    ObserverArray mLinks;
    bool mVisited = false;

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
      return mLinks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
  };

  nsDataHashtable<nsURIHashKey, TrackedURI> mTrackedURIs;

};

} // namespace mozilla

#endif
