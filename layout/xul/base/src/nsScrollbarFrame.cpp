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

#include "nsScrollbarFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewScrollbarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsScrollbarFrame (aPresShell, aContext);
} // NS_NewScrollbarFrame

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsScrollbarFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


NS_IMETHODIMP
nsScrollbarFrame::Init(nsIContent* aContent,
                       nsIFrame*   aParent,
                       nsIFrame*   aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // We want to be a reflow root since we use reflows to move the
  // slider.  Any reflow inside the scrollbar frame will be a reflow to
  // move the slider and will thus not change anything outside of the
  // scrollbar or change the size of the scrollbar frame.
  mState |= NS_FRAME_REFLOW_ROOT;

  return rv;
}

NS_IMETHODIMP
nsScrollbarFrame::Reflow(nsPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  // nsGfxScrollFrame may have told us to shrink to nothing. If so, make sure our
  // desired size agrees.
  if (aReflowState.availableWidth == 0) {
    aDesiredSize.width = 0;
  }
  if (aReflowState.availableHeight == 0) {
    aDesiredSize.height = 0;
  }

  return NS_OK;
}

/* virtual */ PRBool
nsScrollbarFrame::IsContainingBlock() const
{
  // Return true so that the nsHTMLReflowState code is happy with us
  // being a reflow root.
  return PR_TRUE;
}

NS_IMETHODIMP
nsScrollbarFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);

  // if the current position changes, notify any nsGfxScrollFrame
  // parent we may have
  if (aAttribute != nsGkAtoms::curpos)
    return rv;

  nsIFrame* parent = GetParent();
  if (!parent)
    return rv;

  nsIScrollableFrame* scrollable = nsnull;
  parent->QueryInterface(NS_GET_IID(nsIScrollableFrame), (void**)&scrollable);
  if (!scrollable)
    return rv;

  scrollable->CurPosAttributeChanged(mContent, aModType);
  return rv;
}

NS_IMETHODIMP
nsScrollbarFrame::HandlePress(nsPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::HandleMultiplePress(nsPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleDrag(nsPresContext* aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleRelease(nsPresContext* aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

void
nsScrollbarFrame::SetScrollbarMediatorContent(nsIContent* aMediator)
{
  mScrollbarMediator = aMediator;
}

nsIScrollbarMediator*
nsScrollbarFrame::GetScrollbarMediator()
{
  if (!mScrollbarMediator)
    return nsnull;
  nsIFrame* f =
    GetPresContext()->PresShell()->GetPrimaryFrameFor(mScrollbarMediator);
  if (!f)
    return nsnull;

  // check if the frame is a scroll frame. If so, get the scrollable frame
  // inside it.
  nsIScrollableFrame* scrollFrame;
  CallQueryInterface(f, &scrollFrame);
  if (scrollFrame) {
    f = scrollFrame->GetScrolledFrame();
    if (!f)
      return nsnull;
  }

  nsIScrollbarMediator* sbm;
  CallQueryInterface(f, &sbm);
  return sbm;
}
