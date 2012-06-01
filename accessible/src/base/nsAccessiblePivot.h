/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsAccessiblePivot_H_
#define _nsAccessiblePivot_H_

#include "nsIAccessiblePivot.h"

#include "nsAutoPtr.h"
#include "nsTObserverArray.h"
#include "nsCycleCollectionParticipant.h"

class Accessible;
class nsIAccessibleTraversalRule;

// raised when current pivot's position is needed but it is not in the tree.
#define NS_ERROR_NOT_IN_TREE \
NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GENERAL, 0x26)

/**
 * Class represents an accessible pivot.
 */
class nsAccessiblePivot: public nsIAccessiblePivot
{
public:
  nsAccessiblePivot(Accessible* aRoot);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsAccessiblePivot, nsIAccessiblePivot)

  NS_DECL_NSIACCESSIBLEPIVOT

  /*
   * A simple getter for the pivot's position.
   */
  Accessible* Position() { return mPosition; }

private:
  nsAccessiblePivot() MOZ_DELETE;
  nsAccessiblePivot(const nsAccessiblePivot&) MOZ_DELETE;
  void operator = (const nsAccessiblePivot&) MOZ_DELETE;

  /*
   * Notify all observers on a pivot change.
   */
  void NotifyPivotChanged(Accessible* aOldAccessible,
                          PRInt32 aOldStart, PRInt32 aOldEnd);

  /*
   * Check to see that the given accessible is in the pivot's subtree.
   */
  bool IsRootDescendant(Accessible* aAccessible);


  /*
   * Search in preorder for the first accessible to match the rule.
   */
  Accessible* SearchForward(Accessible* aAccessible,
                            nsIAccessibleTraversalRule* aRule,
                            bool aSearchCurrent,
                            nsresult* aResult);

  /*
   * Reverse search in preorder for the first accessible to match the rule.
   */
  Accessible* SearchBackward(Accessible* aAccessible,
                             nsIAccessibleTraversalRule* aRule,
                             bool aSearchCurrent,
                             nsresult* aResult);

  /*
   * Update the pivot, and notify observers.
   */
  void MovePivotInternal(Accessible* aPosition);

  /*
   * The root accessible.
   */
  nsRefPtr<Accessible> mRoot;

  /*
   * The current pivot position.
   */
  nsRefPtr<Accessible> mPosition;

  /*
   * The text start offset ofthe pivot.
   */
  PRInt32 mStartOffset;

  /*
   * The text end offset ofthe pivot.
   */
  PRInt32 mEndOffset;

  /*
   * The list of pivot-changed observers.
   */
  nsTObserverArray<nsCOMPtr<nsIAccessiblePivotObserver> > mObservers;
};

#endif
