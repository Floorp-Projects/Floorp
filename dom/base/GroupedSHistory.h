/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GroupedSHistory_h
#define GroupedSHistory_h

#include "nsIFrameLoader.h"
#include "nsIGroupedSHistory.h"
#include "nsIPartialSHistory.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace dom {


/**
 * GroupedSHistory connects session histories across multiple frameloaders.
 * Each frameloader has a PartialSHistory, and GroupedSHistory has an array
 * refering to all participating PartialSHistory(s).
 *
 * The following figure illustrates the idea. In this case, the GroupedSHistory
 * is composed of 3 frameloaders, and the active one is frameloader 1.
 * GroupedSHistory is always attached to the active frameloader.
 *
 *            +----------------------------------------------------+
 *            |                                                    |
 *            |                                                    v
 *  +------------------+      +-------------------+       +-----------------+
 *  |  FrameLoader 1   |      | PartialSHistory 1 |       | GroupedSHistory |
 *  |     (active)     |----->|     (active)      |<--+---|                 |
 *  +------------------+      +-------------------+   |   +-----------------+
 *                                                    |
 *  +------------------+      +-------------------+   |
 *  |  FrameLoader 2   |      | PartialSHistory 2 |   |
 *  |    (inactive)    |----->|    (inactive)     |<--+
 *  +------------------+      +-------------------+   |
 *                                                    |
 *  +------------------+      +-------------------+   |
 *  |  FrameLoader 3   |      | PartialSHistory 3 |   |
 *  |    (inactive)    |----->|    (inactive)     |<--+
 *  +------------------+      +-------------------+
 *
 * If a history navigation leads to frameloader 3, it becomes the active one,
 * and GroupedSHistory is re-attached to frameloader 3.
 *
 *  +------------------+      +-------------------+
 *  |  FrameLoader 1   |      | PartialSHistory 1 |
 *  |    (inactive)    |----->|    (inactive)     |<--+
 *  +------------------+      +-------------------+   |
 *                                                    |
 *  +------------------+      +-------------------+   |
 *  |  FrameLoader 2   |      | PartialSHistory 2 |   |
 *  |    (inactive)    |----->|    (inactive)     |<--+
 *  +------------------+      +-------------------+   |
 *                                                    |
 *  +------------------+      +-------------------+   |   +-----------------+
 *  |  FrameLoader 3   |      | PartialSHistory 3 |   |   | GroupedSHistory |
 *  |     (active)     |----->|     (active)      |<--+---|                 |
 *  +------------------+      +-------------------+       +-----------------+
 *            |                                                    ^
 *            |                                                    |
 *            +----------------------------------------------------+
 */
class GroupedSHistory final : public nsIGroupedSHistory
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(GroupedSHistory)
  NS_DECL_NSIGROUPEDSHISTORY
  GroupedSHistory();

  /**
   * Get the value of preference "browser.groupedhistory.enabled" to determine
   * if grouped session history should be enabled.
   */
  static bool GroupedHistoryEnabled();

private:
  ~GroupedSHistory() {}

  /**
   * Remove all partial histories and close tabs after the given index (of
   * mPartialHistories, not the index of session history entries).
   */
  void PurgePartialHistories(uint32_t aLastPartialIndexToKeep);

  /**
   * Remove the frameloaders which are owned by the prerendering history, and
   * remove them from mPrerenderingHistories.
   */
  void PurgePrerendering();

  // The total number of entries in all partial histories.
  uint32_t mCount;

  // The index of currently active partial history in mPartialHistories.
  // Use int32_t as we have invalid index and nsCOMArray also uses int32_t.
  int32_t mIndexOfActivePartialHistory;

  // All participating nsIPartialSHistory objects.
  nsCOMArray<nsIPartialSHistory> mPartialHistories;

  // All nsIPartialSHistories which are being prerendered.
  struct PrerenderingHistory
  {
    nsCOMPtr<nsIPartialSHistory> mPartialHistory;
    int32_t mId;
  };
  nsTArray<PrerenderingHistory> mPrerenderingHistories;
};

} // namespace dom
} // namespace mozilla

#endif /* GroupedSHistory_h */
