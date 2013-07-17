/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLButtonControlFrame.h"

#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsISupports.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsStyleConsts.h"
#include "nsIComponentManager.h"
#include "nsButtonFrameRenderer.h"
#include "nsFormControlFrame.h"
#include "nsFrameManager.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsStyleSet.h"
#include "nsDisplayList.h"
#include <algorithm>

using namespace mozilla;

nsIFrame*
NS_NewHTMLButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsHTMLButtonControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLButtonControlFrame)

nsHTMLButtonControlFrame::nsHTMLButtonControlFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
{
}

nsHTMLButtonControlFrame::~nsHTMLButtonControlFrame()
{
}

void
nsHTMLButtonControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

void
nsHTMLButtonControlFrame::Init(
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIFrame*        aPrevInFlow)
{
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
  mRenderer.SetFrame(this, PresContext());
}

NS_QUERYFRAME_HEAD(nsHTMLButtonControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsHTMLButtonControlFrame::AccessibleType()
{
  return a11y::eHTMLButtonType;
}
#endif

nsIAtom*
nsHTMLButtonControlFrame::GetType() const
{
  return nsGkAtoms::HTMLButtonControlFrame;
}

void 
nsHTMLButtonControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent*     aEvent,
                                      nsEventStatus*  aEventStatus)
{
  // if disabled do nothing
  if (mRenderer.isDisabled()) {
    return NS_OK;
  }

  // mouse clicks are handled by content
  // we don't want our children to get any events. So just pass it to frame.
  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


void
nsHTMLButtonControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists)
{
  nsDisplayList onTop;
  if (IsVisibleForPainting(aBuilder)) {
    mRenderer.DisplayButton(aBuilder, aLists.BorderBackground(), &onTop);
  }

  nsDisplayListCollection set;

  // Do not allow the child subtree to receive events.
  if (!aBuilder->IsForEventDelivery()) {
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);

    if (IsInput() || StyleDisplay()->mOverflowX != NS_STYLE_OVERFLOW_VISIBLE) {
      nsMargin border = StyleBorder()->GetComputedBorder();
      nsRect rect(aBuilder->ToReferenceFrame(this), GetSize());
      rect.Deflate(border);
      nscoord radii[8];
      bool hasRadii = GetPaddingBoxBorderRadii(radii);
      clipState.ClipContainingBlockDescendants(rect, hasRadii ? radii : nullptr);
    }

    BuildDisplayListForChild(aBuilder, mFrames.FirstChild(), aDirtyRect, set,
                             DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT);
    // That should put the display items in set.Content()
  }
  
  // Put the foreground outline and focus rects on top of the children
  set.Content()->AppendToTop(&onTop);
  set.MoveTo(aLists);
  
  DisplayOutline(aBuilder, aLists);

  // to draw border when selected in editor
  DisplaySelectionOverlay(aBuilder, aLists.Content());
}

nscoord
nsHTMLButtonControlFrame::GetMinWidth(nsRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                kid,
                                                nsLayoutUtils::MIN_WIDTH);

  result += mRenderer.GetAddedButtonBorderAndPadding().LeftRight();

  return result;
}

nscoord
nsHTMLButtonControlFrame::GetPrefWidth(nsRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  
  nsIFrame* kid = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                kid,
                                                nsLayoutUtils::PREF_WIDTH);
  result += mRenderer.GetAddedButtonBorderAndPadding().LeftRight();
  return result;
}

NS_IMETHODIMP 
nsHTMLButtonControlFrame::Reflow(nsPresContext* aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLButtonControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_PRECONDITION(aReflowState.ComputedWidth() != NS_INTRINSICSIZE,
                  "Should have real computed width by now");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), true);
  }

  // Reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();

  // XXXbz Eventually we may want to check-and-bail if
  // !aReflowState.ShouldReflowAllKids() &&
  // !NS_SUBTREE_DIRTY(firstKid).
  // We'd need to cache our ascent for that, of course.
  
  nsMargin focusPadding = mRenderer.GetAddedButtonBorderAndPadding();
  
  // Reflow the contents of the button.
  ReflowButtonContents(aPresContext, aDesiredSize, aReflowState, firstKid,
                       focusPadding, aStatus);

  aDesiredSize.width = aReflowState.ComputedWidth();

  aDesiredSize.width += aReflowState.mComputedBorderPadding.LeftRight();
  aDesiredSize.height += aReflowState.mComputedBorderPadding.TopBottom();

  aDesiredSize.ascent +=
    aReflowState.mComputedBorderPadding.top + focusPadding.top;

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, firstKid);
  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowState, aStatus);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

void
nsHTMLButtonControlFrame::ReflowButtonContents(nsPresContext* aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               const nsHTMLReflowState& aReflowState,
                                               nsIFrame* aFirstKid,
                                               nsMargin aFocusPadding,
                                               nsReflowStatus& aStatus)
{
  nsSize availSize(aReflowState.ComputedWidth(), NS_INTRINSICSIZE);

  // Indent the child inside us by the focus border. We must do this separate
  // from the regular border.
  availSize.width -= aFocusPadding.LeftRight();
  
  // See whether out availSize's width is big enough.  If it's smaller than our
  // intrinsic min width, that means that the kid wouldn't really fit; for a
  // better look in such cases we adjust the available width and our left
  // offset to allow the kid to spill left into our padding.
  nscoord xoffset = aFocusPadding.left + aReflowState.mComputedBorderPadding.left;
  nscoord extrawidth = GetMinWidth(aReflowState.rendContext) -
    aReflowState.ComputedWidth();
  if (extrawidth > 0) {
    nscoord extraleft = extrawidth / 2;
    nscoord extraright = extrawidth - extraleft;
    NS_ASSERTION(extraright >=0, "How'd that happen?");
    
    // Do not allow the extras to be bigger than the relevant padding
    extraleft = std::min(extraleft, aReflowState.mComputedPadding.left);
    extraright = std::min(extraright, aReflowState.mComputedPadding.right);
    xoffset -= extraleft;
    availSize.width += extraleft + extraright;
  }
  availSize.width = std::max(availSize.width,0);
  
  nsHTMLReflowState reflowState(aPresContext, aReflowState, aFirstKid,
                                availSize);

  ReflowChild(aFirstKid, aPresContext, aDesiredSize, reflowState,
              xoffset,
              aFocusPadding.top + aReflowState.mComputedBorderPadding.top,
              0, aStatus);
  
  // calculate the min internal height so the contents gets centered correctly.
  // XXXbz this assumes border-box sizing.
  nscoord minInternalHeight = aReflowState.mComputedMinHeight -
    aReflowState.mComputedBorderPadding.TopBottom();
  minInternalHeight = std::max(minInternalHeight, 0);

  // Compute our desired height before vertically centering our children
  if (aReflowState.ComputedHeight() != NS_INTRINSICSIZE) {
    aDesiredSize.height = aReflowState.ComputedHeight();
  } else {
    aDesiredSize.height += aFocusPadding.TopBottom();

    // Make sure we obey min/max-height in the case when we're doing intrinsic
    // sizing (we get it for free when we have a non-intrinsic
    // aReflowState.ComputedHeight()).  Note that we do this before adjusting
    // for borderpadding, since mComputedMaxHeight and mComputedMinHeight are
    // content heights.
    aDesiredSize.height = NS_CSS_MINMAX(aDesiredSize.height,
                                        aReflowState.mComputedMinHeight,
                                        aReflowState.mComputedMaxHeight);
  }

  // center child vertically
  nscoord yoff = 0;
  if (aReflowState.ComputedHeight() != NS_INTRINSICSIZE) {
    yoff = (aReflowState.ComputedHeight() - aDesiredSize.height)/2;
    if (yoff < 0) {
      yoff = 0;
    }
  } else if (aDesiredSize.height < minInternalHeight) {
    yoff = (minInternalHeight - aDesiredSize.height) / 2;
  }

  // Place the child
  FinishReflowChild(aFirstKid, aPresContext, &reflowState, aDesiredSize,
                    xoffset,
                    yoff + aFocusPadding.top + aReflowState.mComputedBorderPadding.top, 0);

  if (aDesiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE)
    aDesiredSize.ascent = aFirstKid->GetBaseline();

  // Adjust the baseline by our offset (since we moved the child's
  // baseline by that much).
  aDesiredSize.ascent += yoff;
}

nsresult nsHTMLButtonControlFrame::SetFormProperty(nsIAtom* aName, const nsAString& aValue)
{
  if (nsGkAtoms::value == aName) {
    return mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::value,
                             aValue, true);
  }
  return NS_OK;
}

nsStyleContext*
nsHTMLButtonControlFrame::GetAdditionalStyleContext(int32_t aIndex) const
{
  return mRenderer.GetStyleContext(aIndex);
}

void
nsHTMLButtonControlFrame::SetAdditionalStyleContext(int32_t aIndex, 
                                                    nsStyleContext* aStyleContext)
{
  mRenderer.SetStyleContext(aIndex, aStyleContext);
}

NS_IMETHODIMP 
nsHTMLButtonControlFrame::AppendFrames(ChildListID     aListID,
                                       nsFrameList&    aFrameList)
{
  NS_NOTREACHED("unsupported operation");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::InsertFrames(ChildListID     aListID,
                                       nsIFrame*       aPrevFrame,
                                       nsFrameList&    aFrameList)
{
  NS_NOTREACHED("unsupported operation");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::RemoveFrame(ChildListID     aListID,
                                      nsIFrame*       aOldFrame)
{
  NS_NOTREACHED("unsupported operation");
  return NS_ERROR_UNEXPECTED;
}
