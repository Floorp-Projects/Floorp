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
  void NotifyVisited(nsIURI*, VisitedStatus) final;

 protected:
  static constexpr const size_t kTrackedUrisInitialSize = 64;

  BaseHistory() : mTrackedURIs(kTrackedUrisInitialSize) {}

  using ObserverArray = nsTObserverArray<dom::Link*>;
  struct ObservingLinks {
    ObserverArray mLinks;
    VisitedStatus mStatus = VisitedStatus::Unknown;

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
      return mLinks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
  };

  using PendingVisitedQueries = nsTHashtable<nsURIHashKey>;

  // Starts all the queries in the pending queries list, potentially at the same
  // time.
  virtual void StartPendingVisitedQueries(const PendingVisitedQueries&) = 0;

 private:
  /**
   * Mark all links for the given URI in the given document as visited. Used
   * within NotifyVisited.
   */
  void NotifyVisitedForDocument(nsIURI*, dom::Document*, VisitedStatus);

  void ScheduleVisitedQuery(nsIURI*);

  // Cancels a visited query, if it is at all possible, because we know we won't
  // use the results anymore.
  void CancelVisitedQueryIfPossible(nsIURI*);

  /**
   * Dispatch a runnable for the document passed in which will call
   * NotifyVisitedForDocument with the correct URI and Document.
   */
  void DispatchNotifyVisited(nsIURI*, dom::Document*, VisitedStatus);

 protected:
  // A map from URI to links that depend on that URI, and whether that URI is
  // known-to-be-visited-or-unvisited already.
  nsDataHashtable<nsURIHashKey, ObservingLinks> mTrackedURIs;

 private:
  // The set of pending URIs that we haven't queried yet but need to.
  PendingVisitedQueries mPendingQueries;
  // Whether we've successfully scheduled a runnable to call
  // StartPendingVisitedQueries already.
  bool mStartPendingVisitedQueriesScheduled = false;
};

}  // namespace mozilla

#endif
