/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsHTMLContainerFrame.h"

nsIFrame*
NS_NewDeckFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsDeckFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsDeckFrame)


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
    IndexChanged(PresContext());
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

static void
CreateViewsForFrames(const nsFrameList& aFrames)
{
  for (nsFrameList::Enumerator f(aFrames); !f.AtEnd(); f.Next()) {
    nsContainerFrame::CreateViewForFrame(f.get(), PR_TRUE);
  }
}

NS_IMETHODIMP
nsDeckFrame::SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList)
{
  CreateViewsForFrames(aChildList);
  return nsBoxFrame::SetInitialChildList(aListID, aChildList);
}

NS_IMETHODIMP
nsDeckFrame::AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList)
{
  CreateViewsForFrames(aFrameList);
  return nsBoxFrame::AppendFrames(aListID, aFrameList);
}

NS_IMETHODIMP
nsDeckFrame::InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList)
{
  CreateViewsForFrames(aFrameList);
  return nsBoxFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

void
nsDeckFrame::HideBox(nsPresContext* aPresContext, nsIBox* aBox)
{
  nsIView* view = aBox->GetView();

  if (view) {
    nsIViewManager* viewManager = view->GetViewManager();
    viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
    viewManager->ResizeView(view, nsRect(0, 0, 0, 0));
  }
}

void
nsDeckFrame::ShowBox(nsPresContext* aPresContext, nsIBox* aBox)
{
  nsRect rect = aBox->GetRect();
  nsIView* view = aBox->GetView();
  if (view) {
    nsIViewManager* viewManager = view->GetViewManager();
    rect.x = rect.y = 0;
    viewManager->ResizeView(view, rect);
    viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
  }
}

void
nsDeckFrame::IndexChanged(nsPresContext* aPresContext)
{
  //did the index change?
  PRInt32 index = GetSelectedIndex();
  if (index == mIndex)
    return;

  // redraw
  nsBoxLayoutState state(aPresContext);
  Redraw(state);

  // hide the currently showing box
  nsIBox* currentBox = GetSelectedBox();
  if (currentBox) // only hide if it exists
     HideBox(aPresContext, currentBox);

  mIndex = index;

  // show the new box
  nsIBox* newBox = GetSelectedBox();
  if (newBox) // only show if it exists
     ShowBox(aPresContext, newBox);
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

nsIBox* 
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
    if (count == mIndex) 
      ShowBox(aState.PresContext(), box);
    else
      HideBox(aState.PresContext(), box);

    box = box->GetNextBox();
    count++;
  }

  aState.SetLayoutFlags(oldFlags);

  return rv;
}

