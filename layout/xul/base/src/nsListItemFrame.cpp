/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsListItemFrame.h"

#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h" 
#include "nsGkAtoms.h"
#include "nsDisplayList.h"
#include "nsBoxLayout.h"

nsListItemFrame::nsListItemFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext,
                                 bool aIsRoot,
                                 nsBoxLayout* aLayoutManager)
  : nsGridRowLeafFrame(aPresShell, aContext, aIsRoot, aLayoutManager) 
{
}

nsListItemFrame::~nsListItemFrame()
{
}

nsSize
nsListItemFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize size = nsBoxFrame::GetPrefSize(aState);  
  DISPLAY_PREF_SIZE(this, size);

  // guarantee that our preferred height doesn't exceed the standard
  // listbox row height
  size.height = NS_MAX(mRect.height, size.height);
  return size;
}

NS_IMETHODIMP
nsListItemFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                             const nsRect&           aDirtyRect,
                                             const nsDisplayListSet& aLists)
{
  if (aBuilder->IsForEventDelivery()) {
    if (!mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::allowevents,
                               nsGkAtoms::_true, eCaseMatters))
      return NS_OK;
  }
  
  return nsGridRowLeafFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

already_AddRefed<nsBoxLayout> NS_NewGridRowLeafLayout();

nsIFrame*
NS_NewListItemFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  nsCOMPtr<nsBoxLayout> layout = NS_NewGridRowLeafLayout();
  if (!layout) {
    return nullptr;
  }
  
  return new (aPresShell) nsListItemFrame(aPresShell, aContext, false, layout);
}

NS_IMPL_FRAMEARENA_HELPERS(nsListItemFrame)
