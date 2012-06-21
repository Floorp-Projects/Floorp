/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implements DOM interface for querying and observing media queries */

#ifndef nsDOMMediaQueryList_h_
#define nsDOMMediaQueryList_h_

#include "nsIDOMMediaQueryList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "prclist.h"
#include "mozilla/Attributes.h"

class nsPresContext;
class nsMediaList;

class nsDOMMediaQueryList MOZ_FINAL : public nsIDOMMediaQueryList,
                                      public PRCList
{
public:
  // The caller who constructs is responsible for calling Evaluate
  // before calling any other methods.
  nsDOMMediaQueryList(nsPresContext *aPresContext,
                      const nsAString &aMediaQueryList);
private:
  ~nsDOMMediaQueryList();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMMediaQueryList)

  NS_DECL_NSIDOMMEDIAQUERYLIST

  struct HandleChangeData {
    nsRefPtr<nsDOMMediaQueryList> mql;
    nsCOMPtr<nsIDOMMediaQueryListListener> listener;
  };

  typedef FallibleTArray< nsCOMPtr<nsIDOMMediaQueryListListener> > ListenerList;
  typedef FallibleTArray<HandleChangeData> NotifyList;

  // Appends listeners that need notification to aListenersToNotify
  void MediumFeaturesChanged(NotifyList &aListenersToNotify);

  bool HasListeners() const { return !mListeners.IsEmpty(); }

  void RemoveAllListeners();

private:
  void RecomputeMatches();

  // We only need a pointer to the pres context to support lazy
  // reevaluation following dynamic changes.  However, this lazy
  // reevaluation is perhaps somewhat important, since some usage
  // patterns may involve the creation of large numbers of
  // MediaQueryList objects which almost immediately become garbage
  // (after a single call to the .matches getter).
  //
  // This pointer does make us a little more dependent on cycle
  // collection.
  //
  // We have a non-null mPresContext for our entire lifetime except
  // after cycle collection unlinking.  Having a non-null mPresContext
  // is equivalent to being in that pres context's mDOMMediaQueryLists
  // linked list.
  nsRefPtr<nsPresContext> mPresContext;

  nsRefPtr<nsMediaList> mMediaList;
  bool mMatches;
  bool mMatchesValid;
  ListenerList mListeners;
};

#endif /* !defined(nsDOMMediaQueryList_h_) */
