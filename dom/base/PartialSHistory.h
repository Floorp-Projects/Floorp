/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PartialSHistory_h
#define PartialSHistory_h

#include "nsCycleCollectionParticipant.h"
#include "nsFrameLoader.h"
#include "nsIGroupedSHistory.h"
#include "nsIPartialSHistoryListener.h"
#include "nsIPartialSHistory.h"
#include "nsISHistory.h"
#include "nsISHistoryListener.h"
#include "TabParent.h"

namespace mozilla {
namespace dom {

class PartialSHistory final : public nsIPartialSHistory,
                              public nsISHistoryListener,
                              public nsIPartialSHistoryListener,
                              public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PartialSHistory, nsIPartialSHistory)
  NS_DECL_NSIPARTIALSHISTORY
  NS_DECL_NSIPARTIALSHISTORYLISTENER
  NS_DECL_NSISHISTORYLISTENER

  /**
   * Note that PartialSHistory must be constructed after frameloader has
   * created a valid docshell or tabparent.
   */
  explicit PartialSHistory(nsIFrameLoader* aOwnerFrameLoader);

private:
  ~PartialSHistory() {}
  already_AddRefed<nsISHistory> GetSessionHistory();
  already_AddRefed<TabParent> GetTabParent();

  // The cache of number of entries in corresponding nsISHistory. It's only
  // used for remote process case. If nsISHistory is in-process, mCount will not
  // be used at all.
  uint32_t mCount;

  // The cache of globalIndexOffset in corresponding nsISHistory. It's only
  // used for remote process case.
  uint32_t mGlobalIndexOffset;

  // The frameloader which owns this PartialSHistory.
  nsCOMPtr<nsIFrameLoader> mOwnerFrameLoader;
};

} // namespace dom
} // namespace mozilla

#endif /* PartialSHistory_h */
