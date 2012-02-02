/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eitan Isaacson <eitan@monotonous.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsAccessiblePivot_H_
#define _nsAccessiblePivot_H_

#include "nsIAccessiblePivot.h"

#include "nsAutoPtr.h"
#include "nsTObserverArray.h"
#include "nsCycleCollectionParticipant.h"

class nsAccessible;
class nsIAccessibleTraversalRule;

/**
 * Class represents an accessible pivot.
 */
class nsAccessiblePivot: public nsIAccessiblePivot
{
public:
  nsAccessiblePivot(nsAccessible* aRoot);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsAccessiblePivot, nsIAccessiblePivot)

  NS_DECL_NSIACCESSIBLEPIVOT

  /*
   * A simple getter for the pivot's position.
   */
  nsAccessible* Position() { return mPosition; }

private:
  nsAccessiblePivot() MOZ_DELETE;
  nsAccessiblePivot(const nsAccessiblePivot&) MOZ_DELETE;
  void operator = (const nsAccessiblePivot&) MOZ_DELETE;

  /*
   * Notify all observers on a pivot change.
   */
  void NotifyPivotChanged(nsAccessible* aOldAccessible,
                          PRInt32 aOldStart, PRInt32 aOldEnd);

  /*
   * Check to see that the given accessible is in the pivot's subtree.
   */
  bool IsRootDescendant(nsAccessible* aAccessible);


  /*
   * Search in preorder for the first accessible to match the rule.
   */
  nsAccessible* SearchForward(nsAccessible* aAccessible,
                              nsIAccessibleTraversalRule* aRule,
                              bool searchCurrent,
                              nsresult* rv);

  /*
   * Reverse search in preorder for the first accessible to match the rule.
   */
  nsAccessible* SearchBackward(nsAccessible* aAccessible,
                               nsIAccessibleTraversalRule* aRule,
                               bool searchCurrent,
                               nsresult* rv);

  /*
   * Update the pivot, and notify observers.
   */
  void MovePivotInternal(nsAccessible* aPosition);

  /*
   * The root accessible.
   */
  nsRefPtr<nsAccessible> mRoot;

  /*
   * The current pivot position.
   */
  nsRefPtr<nsAccessible> mPosition;

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
