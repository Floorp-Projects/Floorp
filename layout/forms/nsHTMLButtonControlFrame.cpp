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
#include "nsNameSpaceManager.h"
#include "nsStyleSet.h"
#include "nsDisplayList.h"
#include <algorithm>

using namespace mozilla;

nsContainerFrame*
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
nsHTMLButtonControlFrame::Init(nsIContent*       aContent,
                               nsContainerFrame* aParent,
                               nsIFrame*         aPrevInFlow)
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

nsresult
nsHTMLButtonControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                      WidgetGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
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
nsHTMLButtonControlFrame::GetMinISize(nsRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                kid,
                                                nsLayoutUtils::MIN_ISIZE);

  result += mRenderer.GetAddedButtonBorderAndPadding().LeftRight();

  return result;
}

nscoord
nsHTMLButtonControlFrame::GetPrefISize(nsRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  
  nsIFrame* kid = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                kid,
                                                nsLayoutUtils::PREF_ISIZE);
  result += mRenderer.GetAddedButtonBorderAndPadding().LeftRight();
  return result;
}

void
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

  WritingMode wm = aFirstKid->GetWritingMode();
  LogicalSize availSize = aButtonReflowState.ComputedSize(GetWritingMode());
  availSize.BSize(wm) = NS_INTRINSICSIZE;

  // Indent the child inside us by the focus border. We must do this separate
  // from the regular border.
  availSize.ISize(wm) -= LogicalMargin(wm, focusPadding).IStartEnd(wm);

  // See whether out availSize's width is big enough.  If it's smaller than our
  // intrinsic min width, that means that the kid wouldn't really fit; for a
  // better look in such cases we adjust the available width and our left
  // offset to allow the kid to spill left into our padding.
  nscoord xoffset = focusPadding.left +
    aButtonReflowState.ComputedPhysicalBorderPadding().left;
  nscoord extrawidth = GetMinISize(aButtonReflowState.rendContext) -
    aButtonReflowState.ComputedWidth();
  if (extrawidth > 0) {
    nscoord extraleft = extrawidth / 2;
    nscoord extraright = extrawidth - extraleft;
    NS_ASSERTION(extraright >=0, "How'd that happen?");

    // Do not allow the extras to be bigger than the relevant padding
    extraleft = std::min(extraleft, aButtonReflowState.ComputedPhysicalPadding().left);
    extraright = std::min(extraright, aButtonReflowState.ComputedPhysicalPadding().right);
    xoffset -= extraleft;
    availSize.Width(wm) = availSize.Width(wm) + extraleft + extraright;
  }
  availSize.Width(wm) = std::max(availSize.Width(wm), 0);

  // Give child a clone of the button's reflow state, with height/width reduced
  // by focusPadding, so that descendants with height:100% don't protrude.
  nsHTMLReflowState adjustedButtonReflowState =
    CloneReflowStateWithReducedContentBox(aButtonReflowState, focusPadding);

  nsHTMLReflowState contentsReflowState(aPresContext,
                                        adjustedButtonReflowState,
                                        aFirstKid, availSize);

  nsReflowStatus contentsReflowStatus;
  nsHTMLReflowMetrics contentsDesiredSize(aButtonReflowState);
  ReflowChild(aFirstKid, aPresContext,
              contentsDesiredSize, contentsReflowState,
              xoffset,
              focusPadding.top + aButtonReflowState.ComputedPhysicalBorderPadding().top,
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
      contentsDesiredSize.Height() + focusPadding.TopBottom();

    // Make sure we obey min/max-height in the case when we're doing intrinsic
    // sizing (we get it for free when we have a non-intrinsic
    // aButtonReflowState.ComputedHeight()).  Note that we do this before
    // adjusting for borderpadding, since mComputedMaxHeight and
    // mComputedMinHeight are content heights.
    buttonContentBoxHeight =
      NS_CSS_MINMAX(buttonContentBoxHeight,
                    aButtonReflowState.ComputedMinHeight(),
                    aButtonReflowState.ComputedMaxHeight());
  }

  // Center child vertically in the button
  // (technically, inside of the button's focus-padding area)
  nscoord extraSpace =
    buttonContentBoxHeight - focusPadding.TopBottom() -
    contentsDesiredSize.Height();

  nscoord yoffset = std::max(0, extraSpace / 2);

  // Adjust yoffset to be in terms of the button's frame-rect, instead of
  // its focus-padding rect:
  yoffset += focusPadding.top + aButtonReflowState.ComputedPhysicalBorderPadding().top;

  // Place the child
  FinishReflowChild(aFirstKid, aPresContext,
                    contentsDesiredSize, &contentsReflowState,
                    xoffset, yoffset, 0);

  // Make sure we have a useful 'ascent' value for the child
  if (contentsDesiredSize.BlockStartAscent() ==
      nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
    WritingMode wm = aButtonReflowState.GetWritingMode();
    contentsDesiredSize.SetBlockStartAscent(aFirstKid->GetLogicalBaseline(wm));
  }

  // OK, we're done with the child frame.
  // Use what we learned to populate the button frame's reflow metrics.
  //  * Button's height & width are content-box size + border-box contribution:
  aButtonDesiredSize.Width() = aButtonReflowState.ComputedWidth() +
    aButtonReflowState.ComputedPhysicalBorderPadding().LeftRight();

  aButtonDesiredSize.Height() = buttonContentBoxHeight +
    aButtonReflowState.ComputedPhysicalBorderPadding().TopBottom();

  //  * Button's ascent is its child's ascent, plus the child's y-offset
  // within our frame:
  aButtonDesiredSize.SetBlockStartAscent(contentsDesiredSize.BlockStartAscent() +
                                         yoffset);

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

#ifdef DEBUG
void
nsHTMLButtonControlFrame::AppendFrames(ChildListID     aListID,
                                       nsFrameList&    aFrameList)
{
  MOZ_CRASH("unsupported operation");
}

void
nsHTMLButtonControlFrame::InsertFrames(ChildListID     aListID,
                                       nsIFrame*       aPrevFrame,
                                       nsFrameList&    aFrameList)
{
  MOZ_CRASH("unsupported operation");
}

void
nsHTMLButtonControlFrame::RemoveFrame(ChildListID     aListID,
                                      nsIFrame*       aOldFrame)
{
  MOZ_CRASH("unsupported operation");
}
#endif
