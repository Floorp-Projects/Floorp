/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeSelection_h__
#define nsTreeSelection_h__

#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsITimer.h"
#include "nsCycleCollectionParticipant.h"

class nsITreeBoxObject;
struct nsTreeRange;

class nsTreeSelection : public nsINativeTreeSelection
{
public:
  nsTreeSelection(nsITreeBoxObject* aTree);
  ~nsTreeSelection();
   
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsTreeSelection)
  NS_DECL_NSITREESELECTION

  // nsINativeTreeSelection: Untrusted code can use us
  NS_IMETHOD EnsureNative() { return NS_OK; }

  friend struct nsTreeRange;

protected:
  nsresult FireOnSelectHandler();
  static void SelectCallback(nsITimer *aTimer, void *aClosure);

protected:
  // Members
  nsCOMPtr<nsITreeBoxObject> mTree; // The tree will hold on to us through the view and let go when it dies.

  bool mSuppressed; // Whether or not we should be firing onselect events.
  PRInt32 mCurrentIndex; // The item to draw the rect around. The last one clicked, etc.
  nsCOMPtr<nsITreeColumn> mCurrentColumn;
  PRInt32 mShiftSelectPivot; // Used when multiple SHIFT+selects are performed to pivot on.

  nsTreeRange* mFirstRange; // Our list of ranges.

  nsCOMPtr<nsITimer> mSelectTimer;
};

nsresult
NS_NewTreeSelection(nsITreeBoxObject* aTree, nsITreeSelection** aResult);

#endif
