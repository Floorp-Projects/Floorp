/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BaseHistory_h
#define mozilla_BaseHistory_h

#include "IHistory.h"
#include "mozilla/Result.h"

/* A base class for history implementations that implement link coloring. */

namespace mozilla {

class BaseHistory : public IHistory {
 public:
  nsresult RegisterVisitedCallback(nsIURI*, dom::Link*) final;
  void UnregisterVisitedCallback(nsIURI*, dom::Link*) final;
  void NotifyVisited(nsIURI* aURI) final;

 protected:
  static constexpr const size_t kTrackedUrisInitialSize = 64;

  BaseHistory() : mTrackedURIs(kTrackedUrisInitialSize) {}

  // Starts a visited query, that eventually could call NotifyVisited if
  // appropriate.
  virtual Result<Ok, nsresult> StartVisitedQuery(nsIURI*) = 0;

  // Cancels a visited query, if it is at all possible, because we know we won't
  // use the results anymore.
  virtual void CancelVisitedQueryIfPossible(nsIURI*) = 0;

  using ObserverArray = nsTObserverArray<dom::Link*>;
  struct ObservingLinks {
    ObserverArray mLinks;
    bool mKnownVisited = false;

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
      return mLinks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
  };

 private:
  /**
   * Mark all links for the given URI in the given document as visited. Used
   * within NotifyVisited.
   */
  void NotifyVisitedForDocument(nsIURI*, dom::Document*);

  /**
   * Dispatch a runnable for the document passed in which will call
   * NotifyVisitedForDocument with the correct URI and Document.
   */
  void DispatchNotifyVisited(nsIURI*, dom::Document*);

 protected:
  // A map from URI to links that depend on that URI, and whether that URI is
  // known-to-be-visited already.
  //
  // FIXME(emilio, bug 1506842): The known-to-be-visited stuff seems it could
  // cause timeable differences, probably we should store whether it's
  // known-to-be-unvisited too.
  nsDataHashtable<nsURIHashKey, ObservingLinks> mTrackedURIs;
};

}  // namespace mozilla

#endif
