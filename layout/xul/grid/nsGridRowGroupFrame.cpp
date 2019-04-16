/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "mozilla/PresShell.h"
#include "nsGridRowGroupFrame.h"
#include "nsGridRowLeafLayout.h"
#include "nsGridRow.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"

using namespace mozilla;

already_AddRefed<nsBoxLayout> NS_NewGridRowGroupLayout();

nsIFrame* NS_NewGridRowGroupFrame(PresShell* aPresShell,
                                  ComputedStyle* aStyle) {
  nsCOMPtr<nsBoxLayout> layout = NS_NewGridRowGroupLayout();
  return new (aPresShell)
      nsGridRowGroupFrame(aStyle, aPresShell->GetPresContext(), layout);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGridRowGroupFrame)

/**
 * This is redefined because row groups have a funny property. If they are
 * flexible then their flex must be equal to the sum of their children's flexes.
 */
nscoord nsGridRowGroupFrame::GetXULFlex() {
  // if we are flexible out flexibility is determined by our columns.
  // so first get the our flex. If not 0 then our flex is the sum of
  // our columns flexes.

  if (!DoesNeedRecalc(mFlex)) return mFlex;

  if (nsBoxFrame::GetXULFlex() == 0) return 0;

  // ok we are flexible add up our children
  nscoord totalFlex = 0;
  nsIFrame* child = nsBox::GetChildXULBox(this);
  while (child) {
    totalFlex += child->GetXULFlex();
    child = GetNextXULBox(child);
  }

  mFlex = totalFlex;

  return totalFlex;
}
