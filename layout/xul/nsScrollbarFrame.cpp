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

#include "nsScrollbarFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "mozilla/LookAndFeel.h"
#include "nsThemeConstants.h"
#include "nsRenderingContext.h"
#include "nsIContent.h"

using namespace mozilla;

//
// NS_NewScrollbarFrame
//
// Creates a new scrollbar frame and returns it
//
nsIFrame*
NS_NewScrollbarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsScrollbarFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsScrollbarFrame)

NS_QUERYFRAME_HEAD(nsScrollbarFrame)
  NS_QUERYFRAME_ENTRY(nsScrollbarFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

void
nsScrollbarFrame::Init(nsIContent*       aContent,
                       nsContainerFrame* aParent,
                       nsIFrame*         aPrevInFlow)
{
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // We want to be a reflow root since we use reflows to move the
  // slider.  Any reflow inside the scrollbar frame will be a reflow to
  // move the slider and will thus not change anything outside of the
  // scrollbar or change the size of the scrollbar frame.
  mState |= NS_FRAME_REFLOW_ROOT;
}

void
nsScrollbarFrame::Reflow(nsPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // nsGfxScrollFrame may have told us to shrink to nothing. If so, make sure our
  // desired size agrees.
  if (aReflowState.AvailableWidth() == 0) {
    aDesiredSize.Width() = 0;
  }
  if (aReflowState.AvailableHeight() == 0) {
    aDesiredSize.Height() = 0;
  }
}

nsIAtom*
nsScrollbarFrame::GetType() const
{
  return nsGkAtoms::scrollbarFrame;
}

nsresult
nsScrollbarFrame::AttributeChanged(int32_t aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   int32_t aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);

  // if the current position changes, notify any nsGfxScrollFrame
  // parent we may have
  if (aAttribute != nsGkAtoms::curpos)
    return rv;

  nsIScrollableFrame* scrollable = do_QueryFrame(GetParent());
  if (!scrollable)
    return rv;

  nsCOMPtr<nsIContent> kungFuDeathGrip(mContent);
  scrollable->CurPosAttributeChanged(mContent);
  return rv;
}

NS_IMETHODIMP
nsScrollbarFrame::HandlePress(nsPresContext* aPresContext,
                              WidgetGUIEvent* aEvent,
                              nsEventStatus* aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::HandleMultiplePress(nsPresContext* aPresContext,
                                      WidgetGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus,
                                      bool aControlHeld)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleDrag(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleRelease(nsPresContext* aPresContext,
                                WidgetGUIEvent* aEvent,
                                nsEventStatus* aEventStatus)
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
    return nullptr;
  nsIFrame* f = mScrollbarMediator->GetPrimaryFrame();

  // check if the frame is a scroll frame. If so, get the scrollable frame
  // inside it.
  nsIScrollableFrame* scrollFrame = do_QueryFrame(f);
  if (scrollFrame) {
    f = scrollFrame->GetScrolledFrame();
  }

  nsIScrollbarMediator* sbm = do_QueryFrame(f);
  return sbm;
}

nsresult
nsScrollbarFrame::GetMargin(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);

  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    nsPresContext* presContext = PresContext();
    nsITheme* theme = presContext->GetTheme();
    if (theme) {
      nsIntSize size;
      bool isOverridable;
      nsRefPtr<nsRenderingContext> rc =
        presContext->PresShell()->CreateReferenceRenderingContext();
      theme->GetMinimumWidgetSize(rc, this, NS_THEME_SCROLLBAR, &size,
                                  &isOverridable);
      if (IsHorizontal()) {
        aMargin.top = -presContext->DevPixelsToAppUnits(size.height);
      }
      else {
        if (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
          aMargin.right = -presContext->DevPixelsToAppUnits(size.width);
        }
        else {
          aMargin.left = -presContext->DevPixelsToAppUnits(size.width);
        }
      }
      return NS_OK;
    }
  }

  return nsBox::GetMargin(aMargin);
}
