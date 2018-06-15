/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

using namespace mozilla;

nsListItemFrame::nsListItemFrame(ComputedStyle* aStyle,
                                 bool aIsRoot,
                                 nsBoxLayout* aLayoutManager)
  : nsGridRowLeafFrame(aStyle, aIsRoot, aLayoutManager, kClassID)
{
}

nsListItemFrame::~nsListItemFrame()
{
}

nsSize
nsListItemFrame::GetXULPrefSize(nsBoxLayoutState& aState)
{
  nsSize size = nsBoxFrame::GetXULPrefSize(aState);
  DISPLAY_PREF_SIZE(this, size);

  // guarantee that our preferred height doesn't exceed the standard
  // listbox row height
  size.height = std::max(mRect.height, size.height);
  return size;
}

void
nsListItemFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                             const nsDisplayListSet& aLists)
{
  if (aBuilder->IsForEventDelivery()) {
    if (!mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                            nsGkAtoms::allowevents,
                                            nsGkAtoms::_true, eCaseMatters))
      return;
  }

  nsGridRowLeafFrame::BuildDisplayListForChildren(aBuilder, aLists);
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

already_AddRefed<nsBoxLayout> NS_NewGridRowLeafLayout();

nsIFrame*
NS_NewListItemFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  nsCOMPtr<nsBoxLayout> layout = NS_NewGridRowLeafLayout();
  if (!layout) {
    return nullptr;
  }

  return new (aPresShell) nsListItemFrame(aStyle, false, layout);
}

NS_IMPL_FRAMEARENA_HELPERS(nsListItemFrame)
