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
#include "nsSliderFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "mozilla/LookAndFeel.h"
#include "nsThemeConstants.h"
#include "nsIContent.h"
#include "nsIDOMMutationEvent.h"

using namespace mozilla;

//
// NS_NewScrollbarFrame
//
// Creates a new scrollbar frame and returns it
//
nsIFrame*
NS_NewScrollbarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsScrollbarFrame(aContext);
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
  if (!mScrollbarMediator) {
    return nullptr;
  }
  nsIFrame* f = mScrollbarMediator->GetPrimaryFrame();
  nsIScrollableFrame* scrollFrame = do_QueryFrame(f);
  nsIScrollbarMediator* sbm;

  if (scrollFrame) {
    nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
    sbm = do_QueryFrame(scrolledFrame);
    if (sbm) {
      return sbm;
    }
  }
  sbm = do_QueryFrame(f);
  if (f && !sbm) {
    f = f->PresContext()->PresShell()->GetRootScrollFrame();
    if (f && f->GetContent() == mScrollbarMediator) {
      return do_QueryFrame(f);
    }
  }
  return sbm;
}

nsresult
nsScrollbarFrame::GetScrollbarMargin(
  nsMargin& aMargin,
  mozilla::ScrollFrameHelper::eScrollbarSide aSide)
{
  nsresult rv = NS_ERROR_FAILURE;
  aMargin.SizeTo(0,0,0,0);

  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    nsPresContext* presContext = PresContext();
    nsITheme* theme = presContext->GetTheme();
    if (theme) {
      LayoutDeviceIntSize size;
      bool isOverridable;
      theme->GetMinimumWidgetSize(presContext, this, NS_THEME_SCROLLBAR, &size,
                                  &isOverridable);
      if (IsHorizontal()) {
        aMargin.top = -presContext->DevPixelsToAppUnits(size.height);
      }
      else {
        aMargin.left = -presContext->DevPixelsToAppUnits(size.width);
      }
      rv = NS_OK;
    }
  }

  if (NS_FAILED(rv)) {
    rv = nsBox::GetMargin(aMargin);
  }

  if (NS_SUCCEEDED(rv) && aSide == ScrollFrameHelper::eScrollbarOnLeft) {
    Swap(aMargin.left, aMargin.right);
  }

  return rv;
}

void
nsScrollbarFrame::SetIncrementToLine(int32_t aDirection)
{
  // get the scrollbar's content node
  nsIContent* content = GetContent();
  mSmoothScroll = true;
  mIncrement = aDirection * nsSliderFrame::GetIncrement(content);
}

void
nsScrollbarFrame::SetIncrementToPage(int32_t aDirection)
{
  // get the scrollbar's content node
  nsIContent* content = GetContent();
  mSmoothScroll = true;
  mIncrement = aDirection * nsSliderFrame::GetPageIncrement(content);
}

void
nsScrollbarFrame::SetIncrementToWhole(int32_t aDirection)
{
  // get the scrollbar's content node
  nsIContent* content = GetContent();
  if (aDirection == -1)
    mIncrement = -nsSliderFrame::GetCurrentPosition(content);
  else
    mIncrement = nsSliderFrame::GetMaxPosition(content) -
                 nsSliderFrame::GetCurrentPosition(content);
  // Don't repeat or use smooth scrolling if scrolling to beginning or end
  // of a page.
  mSmoothScroll = false;
}

int32_t
nsScrollbarFrame::MoveToNewPosition()
{
  // get the scrollbar's content node
  nsCOMPtr<nsIContent> content = GetContent();

  // get the current pos
  int32_t curpos = nsSliderFrame::GetCurrentPosition(content);

  // get the max pos
  int32_t maxpos = nsSliderFrame::GetMaxPosition(content);

  // increment the given amount
  if (mIncrement) {
    curpos += mIncrement;
  }

  // make sure the current position is between the current and max positions
  if (curpos < 0) {
    curpos = 0;
  } else if (curpos > maxpos) {
    curpos = maxpos;
  }

  // set the current position of the slider.
  nsAutoString curposStr;
  curposStr.AppendInt(curpos);

  nsWeakFrame weakFrame(this);
  if (mSmoothScroll) {
    content->SetAttr(kNameSpaceID_None, nsGkAtoms::smooth, NS_LITERAL_STRING("true"), false);
  }
  content->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, curposStr, false);
  // notify the nsScrollbarFrame of the change
  AttributeChanged(kNameSpaceID_None, nsGkAtoms::curpos, nsIDOMMutationEvent::MODIFICATION);
  if (!weakFrame.IsAlive()) {
    return curpos;
  }
  // notify all nsSliderFrames of the change
  nsIFrame::ChildListIterator childLists(this);
  for (; !childLists.IsDone(); childLists.Next()) {
    nsFrameList::Enumerator childFrames(childLists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* f = childFrames.get();
      nsSliderFrame* sliderFrame = do_QueryFrame(f);
      if (sliderFrame) {
        sliderFrame->AttributeChanged(kNameSpaceID_None, nsGkAtoms::curpos, nsIDOMMutationEvent::MODIFICATION);
        if (!weakFrame.IsAlive()) {
          return curpos;
        }
      }
    }
  }
  // See if we have appearance information for a theme.
  const nsStyleDisplay* disp = StyleDisplay();
  nsPresContext* presContext = PresContext();
  if (disp->mAppearance) {
    nsITheme *theme = presContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(presContext, this, disp->mAppearance)) {
      bool repaint;
      theme->WidgetStateChanged(this, disp->mAppearance, nsGkAtoms::curpos, &repaint);
    }
  }
  content->UnsetAttr(kNameSpaceID_None, nsGkAtoms::smooth, false);
  return curpos;
}
