/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object that is the root of the frame tree, which contains
 * the document's scrollbars and contains fixed-positioned elements
 */

#include "mozilla/ViewportFrame.h"

#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/RestyleManager.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsCanvasFrame.h"
#include "nsAbsoluteContainingBlock.h"
#include "GeckoProfiler.h"
#include "nsIMozBrowserFrame.h"
#include "nsPlaceholderFrame.h"

using namespace mozilla;
typedef nsAbsoluteContainingBlock::AbsPosReflowFlags AbsPosReflowFlags;

ViewportFrame*
NS_NewViewportFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) ViewportFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(ViewportFrame)
NS_QUERYFRAME_HEAD(ViewportFrame)
  NS_QUERYFRAME_ENTRY(ViewportFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void
ViewportFrame::Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow)
{
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
  // No need to call CreateView() here - the frame ctor will call SetView()
  // with the ViewManager's root view, so we'll assign it in SetViewInternal().

  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(this);
  if (parent) {
    nsFrameState state = parent->GetStateBits();

    AddStateBits(state & (NS_FRAME_IN_POPUP));
  }
}

void
ViewportFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists)
{
  AUTO_PROFILER_LABEL("ViewportFrame::BuildDisplayList", GRAPHICS);

  if (nsIFrame* kid = mFrames.FirstChild()) {
    // make the kid's BorderBackground our own. This ensures that the canvas
    // frame's background becomes our own background and therefore appears
    // below negative z-index elements.
    BuildDisplayListForChild(aBuilder, kid, aLists);
  }

  nsDisplayList topLayerList;
  BuildDisplayListForTopLayer(aBuilder, &topLayerList);
  if (!topLayerList.IsEmpty()) {
    // Wrap the whole top layer in a single item with maximum z-index,
    // and append it at the very end, so that it stays at the topmost.
    nsDisplayWrapList* wrapList =
      MakeDisplayItem<nsDisplayWrapList>(aBuilder, this, &topLayerList);
    wrapList->SetOverrideZIndex(
      std::numeric_limits<decltype(wrapList->ZIndex())>::max());
    aLists.PositionedDescendants()->AppendToTop(wrapList);
  }
}

#ifdef DEBUG
/**
 * Returns whether we are going to put an element in the top layer for
 * fullscreen. This function should matches the CSS rule in ua.css.
 */
static bool
ShouldInTopLayerForFullscreen(Element* aElement)
{
  if (!aElement->GetParent()) {
    return false;
  }
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(aElement);
  if (browserFrame && browserFrame->GetReallyIsBrowser()) {
    return false;
  }
  return true;
}
#endif // DEBUG

static void
BuildDisplayListForTopLayerFrame(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame,
                                 nsDisplayList* aList)
{
  nsRect visible;
  nsRect dirty;
  DisplayListClipState::AutoClipMultiple clipState(aBuilder);
  nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(aBuilder);
  nsDisplayListBuilder::OutOfFlowDisplayData*
    savedOutOfFlowData = nsDisplayListBuilder::GetOutOfFlowData(aFrame);
  if (savedOutOfFlowData) {
    visible = savedOutOfFlowData->GetVisibleRectForFrame(aBuilder, aFrame, &dirty);
    // This function is called after we've finished building display items for
    // the root scroll frame. That means that the content clip from the root
    // scroll frame is no longer on aBuilder. However, we need to make sure
    // that the display items we build in this function have finite clipped
    // bounds with respect to the root ASR, so we restore the *combined clip*
    // that we saved earlier. The combined clip will include the clip from the
    // root scroll frame.
    clipState.SetClipChainForContainingBlockDescendants(
      savedOutOfFlowData->mCombinedClipChain);
    clipState.ClipContainingBlockDescendantsExtra(
      visible + aBuilder->ToReferenceFrame(aFrame), nullptr);
    asrSetter.SetCurrentActiveScrolledRoot(
      savedOutOfFlowData->mContainingBlockActiveScrolledRoot);
  }
  nsDisplayListBuilder::AutoBuildingDisplayList
    buildingForChild(aBuilder, aFrame, visible, dirty,
                     aBuilder->IsAtRootOfPseudoStackingContext());

  nsDisplayList list;
  aFrame->BuildDisplayListForStackingContext(aBuilder, &list);
  aList->AppendToTop(&list);
}

void
ViewportFrame::BuildDisplayListForTopLayer(nsDisplayListBuilder* aBuilder,
                                           nsDisplayList* aList)
{
  nsIDocument* doc = PresContext()->Document();
  nsTArray<Element*> fullscreenStack = doc->GetFullscreenStack();
  for (Element* elem : fullscreenStack) {
    if (nsIFrame* frame = elem->GetPrimaryFrame()) {
      // There are two cases where an element in fullscreen is not in
      // the top layer:
      // 1. When building display list for purpose other than painting,
      //    it is possible that there is inconsistency between the style
      //    info and the content tree.
      // 2. This is an element which we are not going to put in the top
      //    layer for fullscreen. See ShouldInTopLayerForFullscreen().
      // In both cases, we want to skip the frame here and paint it in
      // the normal path.
      if (frame->StyleDisplay()->mTopLayer == NS_STYLE_TOP_LAYER_NONE) {
        MOZ_ASSERT(!aBuilder->IsForPainting() ||
                   !ShouldInTopLayerForFullscreen(elem));
        continue;
      }
      MOZ_ASSERT(ShouldInTopLayerForFullscreen(elem));
      // Inner SVG, MathML elements, as well as children of some XUL
      // elements are not allowed to be out-of-flow. They should not
      // be handled as top layer element here.
      if (!(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        MOZ_ASSERT(!elem->GetParent()->IsHTMLElement(), "HTML element "
                   "should always be out-of-flow if in the top layer");
        continue;
      }
      if (nsIFrame* backdropPh =
          frame->GetChildList(kBackdropList).FirstChild()) {
        MOZ_ASSERT(backdropPh->IsPlaceholderFrame());
        MOZ_ASSERT(!backdropPh->GetNextSibling(), "more than one ::backdrop?");
        MOZ_ASSERT(backdropPh->HasAnyStateBits(NS_FRAME_FIRST_REFLOW),
                   "did you intend to reflow ::backdrop placeholders?");
        nsIFrame* backdropFrame =
          static_cast<nsPlaceholderFrame*>(backdropPh)->GetOutOfFlowFrame();
        MOZ_ASSERT(backdropFrame);
        BuildDisplayListForTopLayerFrame(aBuilder, backdropFrame, aList);
      }
      BuildDisplayListForTopLayerFrame(aBuilder, frame, aList);
    }
  }

  nsIPresShell* shell = PresShell();
  if (nsCanvasFrame* canvasFrame = shell->GetCanvasFrame()) {
    if (Element* container = canvasFrame->GetCustomContentContainer()) {
      if (nsIFrame* frame = container->GetPrimaryFrame()) {
        MOZ_ASSERT(frame->StyleDisplay()->mTopLayer != NS_STYLE_TOP_LAYER_NONE,
                   "ua.css should ensure this");
        MOZ_ASSERT(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW);
        BuildDisplayListForTopLayerFrame(aBuilder, frame, aList);
      }
    }
  }
}

#ifdef DEBUG
void
ViewportFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  nsContainerFrame::AppendFrames(aListID, aFrameList);
}

void
ViewportFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  nsContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

void
ViewportFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}
#endif

/* virtual */ nscoord
ViewportFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinISize(aRenderingContext);

  return result;
}

/* virtual */ nscoord
ViewportFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefISize(aRenderingContext);

  return result;
}

nsPoint
ViewportFrame::AdjustReflowInputForScrollbars(ReflowInput* aReflowInput) const
{
  // Get our prinicpal child frame and see if we're scrollable
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsIScrollableFrame* scrollingFrame = do_QueryFrame(kidFrame);

  if (scrollingFrame) {
    WritingMode wm = aReflowInput->GetWritingMode();
    LogicalMargin scrollbars(wm, scrollingFrame->GetActualScrollbarSizes());
    aReflowInput->SetComputedISize(aReflowInput->ComputedISize() -
                                   scrollbars.IStartEnd(wm));
    aReflowInput->AvailableISize() -= scrollbars.IStartEnd(wm);
    aReflowInput->SetComputedBSizeWithoutResettingResizeFlags(
      aReflowInput->ComputedBSize() - scrollbars.BStartEnd(wm));
    return nsPoint(scrollbars.Left(wm), scrollbars.Top(wm));
  }
  return nsPoint(0, 0);
}

nsRect
ViewportFrame::AdjustReflowInputAsContainingBlock(ReflowInput* aReflowInput) const
{
#ifdef DEBUG
  nsPoint offset =
#endif
    AdjustReflowInputForScrollbars(aReflowInput);

  NS_ASSERTION(GetAbsoluteContainingBlock()->GetChildList().IsEmpty() ||
               (offset.x == 0 && offset.y == 0),
               "We don't handle correct positioning of fixed frames with "
               "scrollbars in odd positions");

  // If a scroll position clamping scroll-port size has been set, layout
  // fixed position elements to this size instead of the computed size.
  nsRect rect(0, 0, aReflowInput->ComputedWidth(), aReflowInput->ComputedHeight());
  nsIPresShell* ps = PresShell();
  if (ps->IsScrollPositionClampingScrollPortSizeSet()) {
    rect.SizeTo(ps->GetScrollPositionClampingScrollPortSize());
  }

  return rect;
}

void
ViewportFrame::Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("ViewportFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");

  // Because |Reflow| sets ComputedBSize() on the child to our
  // ComputedBSize().
  AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowInput.ComputedWidth(), aReflowInput.ComputedHeight()));

  // Reflow the main content first so that the placeholders of the
  // fixed-position frames will be in the right places on an initial
  // reflow.
  nscoord kidBSize = 0;
  WritingMode wm = aReflowInput.GetWritingMode();

  if (mFrames.NotEmpty()) {
    // Deal with a non-incremental reflow or an incremental reflow
    // targeted at our one-and-only principal child frame.
    if (aReflowInput.ShouldReflowAllKids() ||
        aReflowInput.IsBResize() ||
        NS_SUBTREE_DIRTY(mFrames.FirstChild())) {
      // Reflow our one-and-only principal child frame
      nsIFrame*           kidFrame = mFrames.FirstChild();
      ReflowOutput kidDesiredSize(aReflowInput);
      WritingMode         wm = kidFrame->GetWritingMode();
      LogicalSize         availableSpace = aReflowInput.AvailableSize(wm);
      ReflowInput   kidReflowInput(aPresContext, aReflowInput,
                                         kidFrame, availableSpace);

      // Reflow the frame
      kidReflowInput.SetComputedBSize(aReflowInput.ComputedBSize());
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowInput,
                  0, 0, 0, aStatus);
      kidBSize = kidDesiredSize.BSize(wm);

      FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, nullptr, 0, 0, 0);
    } else {
      kidBSize = LogicalSize(wm, mFrames.FirstChild()->GetSize()).BSize(wm);
    }
  }

  NS_ASSERTION(aReflowInput.AvailableISize() != NS_UNCONSTRAINEDSIZE,
               "shouldn't happen anymore");

  // Return the max size as our desired size
  LogicalSize maxSize(wm, aReflowInput.AvailableISize(),
                      // Being flowed initially at an unconstrained block size
                      // means we should return our child's intrinsic size.
                      aReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE
                        ? aReflowInput.ComputedBSize()
                        : kidBSize);
  aDesiredSize.SetSize(wm, maxSize);
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (HasAbsolutelyPositionedChildren()) {
    // Make a copy of the reflow state and change the computed width and height
    // to reflect the available space for the fixed items
    ReflowInput reflowInput(aReflowInput);

    if (reflowInput.AvailableBSize() == NS_UNCONSTRAINEDSIZE) {
      // We have an intrinsic-height document with abs-pos/fixed-pos children.
      // Set the available height and mComputedHeight to our chosen height.
      reflowInput.AvailableBSize() = maxSize.BSize(wm);
      // Not having border/padding simplifies things
      NS_ASSERTION(reflowInput.ComputedPhysicalBorderPadding() == nsMargin(0,0,0,0),
                   "Viewports can't have border/padding");
      reflowInput.SetComputedBSize(maxSize.BSize(wm));
    }

    nsRect rect = AdjustReflowInputAsContainingBlock(&reflowInput);
    nsOverflowAreas* overflowAreas = &aDesiredSize.mOverflowAreas;
    nsIScrollableFrame* rootScrollFrame =
                    aPresContext->PresShell()->GetRootScrollFrameAsScrollable();
    if (rootScrollFrame && !rootScrollFrame->IsIgnoringViewportClipping()) {
      overflowAreas = nullptr;
    }
    AbsPosReflowFlags flags =
      AbsPosReflowFlags::eCBWidthAndHeightChanged; // XXX could be optimized
    GetAbsoluteContainingBlock()->Reflow(this, aPresContext, reflowInput, aStatus,
                                         rect, flags, overflowAreas);
  }

  if (mFrames.NotEmpty()) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, mFrames.FirstChild());
  }

  // If we were dirty then do a repaint
  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    InvalidateFrame();
  }

  // Clipping is handled by the document container (e.g., nsSubDocumentFrame),
  // so we don't need to change our overflow areas.
  bool overflowChanged = FinishAndStoreOverflow(&aDesiredSize);
  if (overflowChanged) {
    // We may need to alert our container to get it to pick up the
    // overflow change.
    nsSubDocumentFrame* container = static_cast<nsSubDocumentFrame*>
      (nsLayoutUtils::GetCrossDocParentFrame(this));
    if (container && !container->ShouldClipSubdocument()) {
      container->PresShell()->
        FrameNeedsReflow(container, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    }
  }

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

bool
ViewportFrame::ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas)
{
  nsIScrollableFrame* rootScrollFrame =
    PresShell()->GetRootScrollFrameAsScrollable();
  if (rootScrollFrame && !rootScrollFrame->IsIgnoringViewportClipping()) {
    return false;
  }

  return nsContainerFrame::ComputeCustomOverflow(aOverflowAreas);
}

void
ViewportFrame::UpdateStyle(ServoRestyleState& aRestyleState)
{
 nsAtom* pseudo = Style()->GetPseudo();
  RefPtr<ComputedStyle> newStyle =
    aRestyleState.StyleSet().ResolveInheritingAnonymousBoxStyle(pseudo, nullptr);

  // We're special because we have a null GetContent(), so don't call things
  // like UpdateStyleOfOwnedChildFrame that try to append changes for the
  // content to the change list.  Nor do we computed a changehint, since we have
  // no way to apply it anyway.
  newStyle->ResolveSameStructsAs(Style());

  MOZ_ASSERT(!GetNextContinuation(), "Viewport has continuations?");
  SetComputedStyle(newStyle);

  UpdateStyleOfOwnedAnonBoxes(aRestyleState);
}

void
ViewportFrame::AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult)
{
  if (mFrames.NotEmpty()) {
    aResult.AppendElement(mFrames.FirstChild());
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult
ViewportFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Viewport"), aResult);
}
#endif
