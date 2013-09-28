/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLButtonControlFrame.h"

#include "nsContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsButtonFrameRenderer.h"
#include "nsCSSAnonBoxes.h"
#include "nsFormControlFrame.h"
#include "nsINameSpaceManager.h"
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

  MOZ_ASSERT(firstKid, "Button should have a child frame for its contents");
  MOZ_ASSERT(!firstKid->GetNextSibling(),
             "Button should have exactly one child frame");
  MOZ_ASSERT(firstKid->StyleContext()->GetPseudo() ==
             nsCSSAnonBoxes::buttonContent,
             "Button's child frame has unexpected pseudo type!");

  // XXXbz Eventually we may want to check-and-bail if
  // !aReflowState.ShouldReflowAllKids() &&
  // !NS_SUBTREE_DIRTY(firstKid).
  // We'd need to cache our ascent for that, of course.

  // Reflow the contents of the button.
  // (This populates our aDesiredSize, too.)
  ReflowButtonContents(aPresContext, aDesiredSize,
                       aReflowState, firstKid);

  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, firstKid);

  aStatus = NS_FRAME_COMPLETE;
  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize,
                                 aReflowState, aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

// Helper-function that lets us clone the button's reflow state, but with its
// ComputedWidth and ComputedHeight reduced by the amount of renderer-specific
// focus border and padding that we're using. (This lets us provide a more
// appropriate content-box size for descendents' percent sizes to resolve
// against.)
static nsHTMLReflowState
CloneReflowStateWithReducedContentBox(
  const nsHTMLReflowState& aButtonReflowState,
  const nsMargin& aFocusPadding)
{
  nscoord adjustedWidth =
    aButtonReflowState.ComputedWidth() - aFocusPadding.LeftRight();
  adjustedWidth = std::max(0, adjustedWidth);

  // (Only adjust height if it's an actual length.)
  nscoord adjustedHeight = aButtonReflowState.ComputedHeight();
  if (adjustedHeight != NS_INTRINSICSIZE) {
    adjustedHeight -= aFocusPadding.TopBottom();
    adjustedHeight = std::max(0, adjustedHeight);
  }

  nsHTMLReflowState clone(aButtonReflowState);
  clone.SetComputedWidth(adjustedWidth);
  clone.SetComputedHeight(adjustedHeight);

  return clone;
}

void
nsHTMLButtonControlFrame::ReflowButtonContents(nsPresContext* aPresContext,
                                               nsHTMLReflowMetrics& aButtonDesiredSize,
                                               const nsHTMLReflowState& aButtonReflowState,
                                               nsIFrame* aFirstKid)
{
  // Buttons have some bonus renderer-determined border/padding,
  // which occupies part of the button's content-box area:
  const nsMargin focusPadding = mRenderer.GetAddedButtonBorderAndPadding();

  nsSize availSize(aButtonReflowState.ComputedWidth(), NS_INTRINSICSIZE);

  // Indent the child inside us by the focus border. We must do this separate
  // from the regular border.
  availSize.width -= focusPadding.LeftRight();
  
  // See whether out availSize's width is big enough.  If it's smaller than our
  // intrinsic min width, that means that the kid wouldn't really fit; for a
  // better look in such cases we adjust the available width and our left
  // offset to allow the kid to spill left into our padding.
  nscoord xoffset = focusPadding.left +
    aButtonReflowState.mComputedBorderPadding.left;
  nscoord extrawidth = GetMinWidth(aButtonReflowState.rendContext) -
    aButtonReflowState.ComputedWidth();
  if (extrawidth > 0) {
    nscoord extraleft = extrawidth / 2;
    nscoord extraright = extrawidth - extraleft;
    NS_ASSERTION(extraright >=0, "How'd that happen?");
    
    // Do not allow the extras to be bigger than the relevant padding
    extraleft = std::min(extraleft, aButtonReflowState.mComputedPadding.left);
    extraright = std::min(extraright, aButtonReflowState.mComputedPadding.right);
    xoffset -= extraleft;
    availSize.width += extraleft + extraright;
  }
  availSize.width = std::max(availSize.width,0);
  
  // Give child a clone of the button's reflow state, with height/width reduced
  // by focusPadding, so that descendants with height:100% don't protrude.
  nsHTMLReflowState adjustedButtonReflowState =
    CloneReflowStateWithReducedContentBox(aButtonReflowState, focusPadding);

  nsHTMLReflowState contentsReflowState(aPresContext,
                                        adjustedButtonReflowState,
                                        aFirstKid, availSize);

  nsReflowStatus contentsReflowStatus;
  nsHTMLReflowMetrics contentsDesiredSize;
  ReflowChild(aFirstKid, aPresContext,
              contentsDesiredSize, contentsReflowState,
              xoffset,
              focusPadding.top + aButtonReflowState.mComputedBorderPadding.top,
              0, contentsReflowStatus);
  MOZ_ASSERT(NS_FRAME_IS_COMPLETE(contentsReflowStatus),
             "We gave button-contents frame unconstrained available height, "
             "so it should be complete");

  // Compute the button's content-box height:
  nscoord buttonContentBoxHeight = 0;
  if (aButtonReflowState.ComputedHeight() != NS_INTRINSICSIZE) {
    // Button has a fixed height -- that's its content-box height.
    buttonContentBoxHeight = aButtonReflowState.ComputedHeight();
  } else {
    // Button is intrinsically sized -- it should shrinkwrap the
    // button-contents' height, plus any focus-padding space:
    buttonContentBoxHeight =
      contentsDesiredSize.height + focusPadding.TopBottom();

    // Make sure we obey min/max-height in the case when we're doing intrinsic
    // sizing (we get it for free when we have a non-intrinsic
    // aButtonReflowState.ComputedHeight()).  Note that we do this before
    // adjusting for borderpadding, since mComputedMaxHeight and
    // mComputedMinHeight are content heights.
    buttonContentBoxHeight =
      NS_CSS_MINMAX(buttonContentBoxHeight,
                    aButtonReflowState.mComputedMinHeight,
                    aButtonReflowState.mComputedMaxHeight);
  }

  // Center child vertically in the button
  // (technically, inside of the button's focus-padding area)
  nscoord extraSpace =
    buttonContentBoxHeight - focusPadding.TopBottom() -
    contentsDesiredSize.height;

  nscoord yoffset = std::max(0, extraSpace / 2);

  // Adjust yoffset to be in terms of the button's frame-rect, instead of
  // its focus-padding rect:
  yoffset += focusPadding.top + aButtonReflowState.mComputedBorderPadding.top;

  // Place the child
  FinishReflowChild(aFirstKid, aPresContext,
                    &contentsReflowState, contentsDesiredSize,
                    xoffset, yoffset, 0);

  // Make sure we have a useful 'ascent' value for the child
  if (contentsDesiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
    contentsDesiredSize.ascent = aFirstKid->GetBaseline();
  }

  // OK, we're done with the child frame.
  // Use what we learned to populate the button frame's reflow metrics.
  //  * Button's height & width are content-box size + border-box contribution:
  aButtonDesiredSize.width = aButtonReflowState.ComputedWidth() +
    aButtonReflowState.mComputedBorderPadding.LeftRight();

  aButtonDesiredSize.height = buttonContentBoxHeight +
    aButtonReflowState.mComputedBorderPadding.TopBottom();

  //  * Button's ascent is its child's ascent, plus the child's y-offset
  // within our frame:
  aButtonDesiredSize.ascent = contentsDesiredSize.ascent + yoffset;

  aButtonDesiredSize.SetOverflowAreasToDesiredBounds();
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
