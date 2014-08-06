/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsListItemFrame.h"

#include <algorithm>

#include "nsCOMPtr.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsDisplayList.h"
#include "nsBoxLayout.h"
#include "nsIContent.h"

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
  size.height = std::max(mRect.height, size.height);
  return size;
}

void
nsListItemFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                             const nsRect&           aDirtyRect,
                                             const nsDisplayListSet& aLists)
{
  if (aBuilder->IsForEventDelivery()) {
    if (!mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::allowevents,
                               nsGkAtoms::_true, eCaseMatters))
      return;
  }
  
  nsGridRowLeafFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
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
