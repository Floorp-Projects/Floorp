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
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIViewManager.h"
#include "nsBoxLayoutState.h"
#include "nsStackLayout.h"

nsresult
NS_NewDeckFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame,
                nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (!aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDeckFrame* it = new (aPresShell) nsDeckFrame(aPresShell, aLayoutManager);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewDeckFrame


nsDeckFrame::nsDeckFrame(nsIPresShell* aPresShell,
                         nsIBoxLayout* aLayoutManager)
  : nsBoxFrame(aPresShell), mIndex(0)
{
     // if no layout manager specified us the static sprocket layout
  nsCOMPtr<nsIBoxLayout> layout = aLayoutManager;

  if (!layout) {
    NS_NewStackLayout(aPresShell, layout);
  }

  SetLayoutManager(layout);
}

/**
 * Hack for deck who requires that all its children has widgets
 */
NS_IMETHODIMP
nsDeckFrame::ChildrenMustHaveWidgets(PRBool& aMust)
{
  aMust = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsDeckFrame::AttributeChanged(nsPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                             aNameSpaceID, aAttribute,
                                             aModType);


   // if the index changed hide the old element and make the now element visible
  if (aAttribute == nsXULAtoms::selectedIndex) {
    IndexChanged(aPresContext);
  }

  return rv;
}

NS_IMETHODIMP
nsDeckFrame::Init(nsPresContext* aPresContext,
                  nsIContent*     aContent,
                  nsIFrame*       aParent,
                  nsStyleContext* aStyleContext,
                  nsIFrame*       aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aPresContext, aContent,
                                 aParent, aStyleContext,
                                 aPrevInFlow);

  mIndex = GetSelectedIndex();

  return rv;
}

void
nsDeckFrame::HideBox(nsPresContext* aPresContext, nsIBox* aBox)
{
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);

  nsIView* view = frame->GetView();

  if (view) {
    nsIViewManager* viewManager = view->GetViewManager();
    viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
    viewManager->ResizeView(view, nsRect(0, 0, 0, 0));
  }
}

void
nsDeckFrame::ShowBox(nsPresContext* aPresContext, nsIBox* aBox)
{
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);

  nsRect rect = frame->GetRect();
  nsIView* view = frame->GetView();
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
  nsIBox* currentBox = GetBoxAt(mIndex);
  if (currentBox) // only hide if it exists
     HideBox(aPresContext, currentBox);

  // show the new box
  nsIBox* newBox = GetBoxAt(index);
  if (newBox) // only show if it exists
     ShowBox(aPresContext, newBox);

  mIndex = index;
}

PRInt32
nsDeckFrame::GetSelectedIndex()
{
  // default index is 0
  PRInt32 index = 0;

  // get the index attribute
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::selectedIndex, value))
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
 // ok we want to paint only the child that as at the given index
  PRInt32 index = GetSelectedIndex();
 
  // get the child at that index. 
  return GetBoxAt(index); 
}


NS_IMETHODIMP
nsDeckFrame::Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags)
{
  // if a tab is hidden all its children are too.

  if (!GetStyleVisibility()->mVisible)
    return NS_OK;

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
  }

  // only paint the seleced box
  nsIBox* box = GetSelectedBox();
  if (box) {
    nsIFrame* frame = nsnull;
    box->GetFrame(&frame);

    if (frame)
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, frame, aWhichLayer);
  }

  return NS_OK;

}


NS_IMETHODIMP
nsDeckFrame::GetFrameForPoint(nsPresContext*   aPresContext,
                              const nsPoint&    aPoint, 
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**        aFrame)
{
  // if it is not inside us fail
  if (!mRect.Contains(aPoint)) {
    return NS_ERROR_FAILURE;
  }

  // get the selected frame and see if the point is in it.
  nsIBox* selectedBox = GetSelectedBox();
  if (selectedBox) {
    nsIFrame* selectedFrame = nsnull;
    selectedBox->GetFrame(&selectedFrame);

    nsPoint tmp(aPoint.x - mRect.x, aPoint.y - mRect.y);

    if (NS_SUCCEEDED(selectedFrame->GetFrameForPoint(aPresContext, tmp,
                                                     aWhichLayer, aFrame)))
      return NS_OK;
  }
    
  // if its not in our child just return us.
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND) {
      *aFrame = this;
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
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
  nsIBox* box = nsnull;
  GetChildBox(&box);

  nscoord count = 0;
  while (box) 
  {
    // make collapsed children not show up
    if (count == mIndex) 
      ShowBox(aState.PresContext(), box);
    else
      HideBox(aState.PresContext(), box);

    nsresult rv2 = box->GetNextBox(&box);
    NS_ASSERTION(NS_SUCCEEDED(rv2), "failed to get next child");
    count++;
  }

  aState.SetLayoutFlags(oldFlags);

  return rv;
}

