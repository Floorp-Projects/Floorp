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
  NS_IMETHODIMP RegisterVisitedCallback(nsIURI*, dom::Link*) final;
  NS_IMETHODIMP UnregisterVisitedCallback(nsIURI*, dom::Link*) final;
  NS_IMETHODIMP NotifyVisited(nsIURI* aURI) final;

 protected:
  static constexpr const size_t kTrackedUrisInitialSize = 64;

  BaseHistory() : mTrackedURIs(kTrackedUrisInitialSize) {}

  // Starts a visited query, that eventually could call NotifyVisited if
  // appropriate.
  virtual Result<Ok, nsresult> StartVisitedQuery(nsIURI*) = 0;

  // Cancels a visited query, if it is at all possible, because we know we won't
  // use the results anymore.
  virtual void CancelVisitedQueryIfPossible(nsIURI*) = 0;

  static dom::Document* GetLinkDocument(dom::Link&);

  using ObserverArray = nsTObserverArray<dom::Link*>;
  struct TrackedURI {
    ObserverArray mLinks;
    bool mVisited = false;

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
  nsDataHashtable<nsURIHashKey, TrackedURI> mTrackedURIs;
};

}  // namespace mozilla

#endif
