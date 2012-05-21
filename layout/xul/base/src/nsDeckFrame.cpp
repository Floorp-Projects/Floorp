/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsDeckFrame.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIViewManager.h"
#include "nsBoxLayoutState.h"
#include "nsStackLayout.h"
#include "nsDisplayList.h"
#include "nsContainerFrame.h"

nsIFrame*
NS_NewDeckFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsDeckFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsDeckFrame)

NS_QUERYFRAME_HEAD(nsDeckFrame)
  NS_QUERYFRAME_ENTRY(nsDeckFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)


nsDeckFrame::nsDeckFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
  : nsBoxFrame(aPresShell, aContext), mIndex(0)
{
  nsCOMPtr<nsBoxLayout> layout;
  NS_NewStackLayout(aPresShell, layout);
  SetLayoutManager(layout);
}

nsIAtom*
nsDeckFrame::GetType() const
{
  return nsGkAtoms::deckFrame;
}

NS_IMETHODIMP
nsDeckFrame::AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);


   // if the index changed hide the old element and make the new element visible
  if (aAttribute == nsGkAtoms::selectedIndex) {
    IndexChanged();
  }

  return rv;
}

NS_IMETHODIMP
nsDeckFrame::Init(nsIContent*     aContent,
                  nsIFrame*       aParent,
                  nsIFrame*       aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  mIndex = GetSelectedIndex();

  return rv;
}

void
nsDeckFrame::HideBox(nsIBox* aBox)
{
  nsIPresShell::ClearMouseCapture(aBox);
}

void
nsDeckFrame::IndexChanged()
{
  //did the index change?
  PRInt32 index = GetSelectedIndex();
  if (index == mIndex)
    return;

  // redraw
  InvalidateOverflowRect();

  // hide the currently showing box
  nsIBox* currentBox = GetSelectedBox();
  if (currentBox) // only hide if it exists
    HideBox(currentBox);

  mIndex = index;
}

PRInt32
nsDeckFrame::GetSelectedIndex()
{
  // default index is 0
  PRInt32 index = 0;

  // get the index attribute
  nsAutoString value;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::selectedIndex, value))
  {
    PRInt32 error;

    // convert it to an integer
    index = value.ToInteger(&error);
  }

  return index;
}

nsIFrame* 
nsDeckFrame::GetSelectedBox()
{
  return (mIndex >= 0) ? mFrames.FrameAt(mIndex) : nsnull; 
}

NS_IMETHODIMP
nsDeckFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists)
{
  // if a tab is hidden all its children are too.
  if (!GetStyleVisibility()->mVisible)
    return NS_OK;
    
  // REVIEW: The old code skipped painting of background/borders/outline for this
  // frame and painting of debug boxes ... I've put it back.
  return nsBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsDeckFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  // only paint the selected box
  nsIBox* box = GetSelectedBox();
  if (!box)
    return NS_OK;

  // Putting the child in the background list. This is a little weird but
  // it matches what we were doing before.
  nsDisplayListSet set(aLists, aLists.BlockBorderBackgrounds());
  return BuildDisplayListForChild(aBuilder, box, aDirtyRect, set);
}

NS_IMETHODIMP
nsDeckFrame::DoLayout(nsBoxLayoutState& aState)
{
  // Make sure we tweak the state so it does not resize our children.
  // We will do that.
  PRUint32 oldFlags = aState.LayoutFlags();
  aState.SetLayoutFlags(NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_VISIBILITY);

  // do a normal layout
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // run though each child. Hide all but the selected one
  nsIBox* box = GetChildBox();

  nscoord count = 0;
  while (box) 
  {
    // make collapsed children not show up
    if (count != mIndex) 
      HideBox(box);

    box = box->GetNextBox();
    count++;
  }

  aState.SetLayoutFlags(oldFlags);

  return rv;
}

