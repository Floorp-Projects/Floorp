/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeSelection_h__
#define nsTreeSelection_h__

#include "nsITreeSelection.h"
#include "nsITimer.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "XULTreeElement.h"

class nsTreeColumn;
struct nsTreeRange;

class nsTreeSelection final : public nsINativeTreeSelection {
 public:
  explicit nsTreeSelection(mozilla::dom::XULTreeElement* aTree);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsTreeSelection)
  NS_DECL_NSITREESELECTION

  // nsINativeTreeSelection: Untrusted code can use us
  NS_IMETHOD EnsureNative() override { return NS_OK; }

  friend struct nsTreeRange;

 protected:
  ~nsTreeSelection();

  nsresult FireOnSelectHandler();
  static void SelectCallback(nsITimer* aTimer, void* aClosure);

 protected:
  // The tree will hold on to us through the view and let go when it dies.
  RefPtr<mozilla::dom::XULTreeElement> mTree;

  bool mSuppressed;       // Whether or not we should be firing onselect events.
  int32_t mCurrentIndex;  // The item to draw the rect around. The last one
                          // clicked, etc.
  int32_t mShiftSelectPivot;  // Used when multiple SHIFT+selects are performed
                              // to pivot on.

  nsTreeRange* mFirstRange;  // Our list of ranges.

  nsCOMPtr<nsITimer> mSelectTimer;
};

nsresult NS_NewTreeSelection(mozilla::dom::XULTreeElement* aTree,
                             nsITreeSelection** aResult);

#endif
