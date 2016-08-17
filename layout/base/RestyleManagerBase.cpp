/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RestyleManagerBase.h"
#include "mozilla/StyleSetHandle.h"
#include "nsIFrame.h"

namespace mozilla {

RestyleManagerBase::RestyleManagerBase(nsPresContext* aPresContext)
  : mPresContext(aPresContext)
  , mRestyleGeneration(1)
  , mHoverGeneration(0)
  , mObservingRefreshDriver(false)
  , mInStyleRefresh(false)
{
  MOZ_ASSERT(mPresContext);
}

/**
 * Calculates the change hint and the restyle hint for a given content state
 * change.
 *
 * This is called from both Restyle managers.
 */
void
RestyleManagerBase::ContentStateChangedInternal(Element* aElement,
                                                EventStates aStateMask,
                                                nsChangeHint* aOutChangeHint,
                                                nsRestyleHint* aOutRestyleHint)
{
  MOZ_ASSERT(aOutChangeHint);
  MOZ_ASSERT(aOutRestyleHint);

  StyleSetHandle styleSet = PresContext()->StyleSet();
  NS_ASSERTION(styleSet, "couldn't get style set");

  *aOutChangeHint = nsChangeHint(0);
  // Any change to a content state that affects which frames we construct
  // must lead to a frame reconstruct here if we already have a frame.
  // Note that we never decide through non-CSS means to not create frames
  // based on content states, so if we already don't have a frame we don't
  // need to force a reframe -- if it's needed, the HasStateDependentStyle
  // call will handle things.
  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  CSSPseudoElementType pseudoType = CSSPseudoElementType::NotPseudo;
  if (primaryFrame) {
    // If it's generated content, ignore LOADING/etc state changes on it.
    if (!primaryFrame->IsGeneratedContentFrame() &&
        aStateMask.HasAtLeastOneOfStates(NS_EVENT_STATE_BROKEN |
                                         NS_EVENT_STATE_USERDISABLED |
                                         NS_EVENT_STATE_SUPPRESSED |
                                         NS_EVENT_STATE_LOADING)) {
      *aOutChangeHint = nsChangeHint_ReconstructFrame;
    } else {
      uint8_t app = primaryFrame->StyleDisplay()->mAppearance;
      if (app) {
        nsITheme* theme = PresContext()->GetTheme();
        if (theme &&
            theme->ThemeSupportsWidget(PresContext(), primaryFrame, app)) {
          bool repaint = false;
          theme->WidgetStateChanged(primaryFrame, app, nullptr, &repaint,
                                    nullptr);
          if (repaint) {
            *aOutChangeHint |= nsChangeHint_RepaintFrame;
          }
        }
      }
    }

    pseudoType = primaryFrame->StyleContext()->GetPseudoType();

    primaryFrame->ContentStatesChanged(aStateMask);
  }

  if (pseudoType >= CSSPseudoElementType::Count) {
    *aOutRestyleHint = styleSet->HasStateDependentStyle(aElement, aStateMask);
  } else if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(
               pseudoType)) {
    // If aElement is a pseudo-element, we want to check to see whether there
    // are any state-dependent rules applying to that pseudo.
    Element* ancestor =
      ElementForStyleContext(nullptr, primaryFrame, pseudoType);
    *aOutRestyleHint = styleSet->HasStateDependentStyle(ancestor, pseudoType,
                                                        aElement, aStateMask);
  } else {
    *aOutRestyleHint = nsRestyleHint(0);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_HOVER) && *aOutRestyleHint != 0) {
    IncrementHoverGeneration();
  }

  if (aStateMask.HasState(NS_EVENT_STATE_VISITED)) {
    // Exposing information to the page about whether the link is
    // visited or not isn't really something we can worry about here.
    // FIXME: We could probably do this a bit better.
    *aOutChangeHint |= nsChangeHint_RepaintFrame;
  }
}

/* static */ nsCString
RestyleManagerBase::RestyleHintToString(nsRestyleHint aHint)
{
  nsCString result;
  bool any = false;
  const char* names[] = {
    "Self", "SomeDescendants", "Subtree", "LaterSiblings", "CSSTransitions",
    "CSSAnimations", "SVGAttrAnimations", "StyleAttribute",
    "StyleAttribute_Animations", "Force", "ForceDescendants"
  };
  uint32_t hint = aHint & ((1 << ArrayLength(names)) - 1);
  uint32_t rest = aHint & ~((1 << ArrayLength(names)) - 1);
  for (uint32_t i = 0; i < ArrayLength(names); i++) {
    if (hint & (1 << i)) {
      if (any) {
        result.AppendLiteral(" | ");
      }
      result.AppendPrintf("eRestyle_%s", names[i]);
      any = true;
    }
  }
  if (rest) {
    if (any) {
      result.AppendLiteral(" | ");
    }
    result.AppendPrintf("0x%0x", rest);
  } else {
    if (!any) {
      result.AppendLiteral("0");
    }
  }
  return result;
}

#ifdef DEBUG
/* static */ nsCString
RestyleManagerBase::ChangeHintToString(nsChangeHint aHint)
{
  nsCString result;
  bool any = false;
  const char* names[] = {
    "RepaintFrame", "NeedReflow", "ClearAncestorIntrinsics",
    "ClearDescendantIntrinsics", "NeedDirtyReflow", "SyncFrameView",
    "UpdateCursor", "UpdateEffects", "UpdateOpacityLayer",
    "UpdateTransformLayer", "ReconstructFrame", "UpdateOverflow",
    "UpdateSubtreeOverflow", "UpdatePostTransformOverflow",
    "UpdateParentOverflow",
    "ChildrenOnlyTransform", "RecomputePosition", "AddOrRemoveTransform",
    "BorderStyleNoneChange", "UpdateTextPath", "SchedulePaint",
    "NeutralChange", "InvalidateRenderingObservers",
    "ReflowChangesSizeOrPosition", "UpdateComputedBSize",
    "UpdateUsesOpacity", "UpdateBackgroundPosition"
  };
  static_assert(nsChangeHint_AllHints == (1 << ArrayLength(names)) - 1,
                "Name list doesn't match change hints.");
  uint32_t hint = aHint & ((1 << ArrayLength(names)) - 1);
  uint32_t rest = aHint & ~((1 << ArrayLength(names)) - 1);
  if (hint == nsChangeHint_Hints_NotHandledForDescendants) {
    result.AppendLiteral("nsChangeHint_Hints_NotHandledForDescendants");
    hint = 0;
    any = true;
  } else {
    if ((hint & NS_STYLE_HINT_REFLOW) == NS_STYLE_HINT_REFLOW) {
      result.AppendLiteral("NS_STYLE_HINT_REFLOW");
      hint = hint & ~NS_STYLE_HINT_REFLOW;
      any = true;
    } else if ((hint & nsChangeHint_AllReflowHints) == nsChangeHint_AllReflowHints) {
      result.AppendLiteral("nsChangeHint_AllReflowHints");
      hint = hint & ~nsChangeHint_AllReflowHints;
      any = true;
    } else if ((hint & NS_STYLE_HINT_VISUAL) == NS_STYLE_HINT_VISUAL) {
      result.AppendLiteral("NS_STYLE_HINT_VISUAL");
      hint = hint & ~NS_STYLE_HINT_VISUAL;
      any = true;
    }
  }
  for (uint32_t i = 0; i < ArrayLength(names); i++) {
    if (hint & (1 << i)) {
      if (any) {
        result.AppendLiteral(" | ");
      }
      result.AppendPrintf("nsChangeHint_%s", names[i]);
      any = true;
    }
  }
  if (rest) {
    if (any) {
      result.AppendLiteral(" | ");
    }
    result.AppendPrintf("0x%0x", rest);
  } else {
    if (!any) {
      result.AppendLiteral("nsChangeHint(0)");
    }
  }
  return result;
}
#endif

void
RestyleManagerBase::PostRestyleEventInternal(bool aForLazyConstruction)
{
  // Make sure we're not in a style refresh; if we are, we still have
  // a call to ProcessPendingRestyles coming and there's no need to
  // add ourselves as a refresh observer until then.
  bool inRefresh = !aForLazyConstruction && mInStyleRefresh;
  nsIPresShell* presShell = PresContext()->PresShell();
  if (!ObservingRefreshDriver() && !inRefresh) {
    SetObservingRefreshDriver(PresContext()->RefreshDriver()->
        AddStyleFlushObserver(presShell));
  }

  // Unconditionally flag our document as needing a flush.  The other
  // option here would be a dedicated boolean to track whether we need
  // to do so (set here and unset in ProcessPendingRestyles).
  presShell->GetDocument()->SetNeedStyleFlush();
}

/**
 * Frame construction helpers follow.
 */
#ifdef DEBUG
static bool gInApplyRenderingChangeToTree = false;
#endif

#ifdef DEBUG
static void
DumpContext(nsIFrame* aFrame, nsStyleContext* aContext)
{
  if (aFrame) {
    fputs("frame: ", stdout);
    nsAutoString name;
    aFrame->GetFrameName(name);
    fputs(NS_LossyConvertUTF16toASCII(name).get(), stdout);
    fprintf(stdout, " (%p)", static_cast<void*>(aFrame));
  }
  if (aContext) {
    fprintf(stdout, " style: %p ", static_cast<void*>(aContext));

    nsIAtom* pseudoTag = aContext->GetPseudo();
    if (pseudoTag) {
      nsAutoString buffer;
      pseudoTag->ToString(buffer);
      fputs(NS_LossyConvertUTF16toASCII(buffer).get(), stdout);
      fputs(" ", stdout);
    }
    fputs("{}\n", stdout);
  }
}

static void
VerifySameTree(nsStyleContext* aContext1, nsStyleContext* aContext2)
{
  nsStyleContext* top1 = aContext1;
  nsStyleContext* top2 = aContext2;
  nsStyleContext* parent;
  for (;;) {
    parent = top1->GetParent();
    if (!parent)
      break;
    top1 = parent;
  }
  for (;;) {
    parent = top2->GetParent();
    if (!parent)
      break;
    top2 = parent;
  }
  NS_ASSERTION(top1 == top2,
               "Style contexts are not in the same style context tree");
}

static void
VerifyContextParent(nsIFrame* aFrame, nsStyleContext* aContext,
                    nsStyleContext* aParentContext)
{
  // get the contexts not provided
  if (!aContext) {
    aContext = aFrame->StyleContext();
  }

  if (!aParentContext) {
    nsIFrame* providerFrame;
    aParentContext = aFrame->GetParentStyleContext(&providerFrame);
    // aParentContext could still be null
  }

  NS_ASSERTION(aContext, "Failure to get required contexts");
  nsStyleContext* actualParentContext = aContext->GetParent();

  if (aParentContext) {
    if (aParentContext != actualParentContext) {
      DumpContext(aFrame, aContext);
      if (aContext == aParentContext) {
        NS_ERROR("Using parent's style context");
      } else {
        NS_ERROR("Wrong parent style context");
        fputs("Wrong parent style context: ", stdout);
        DumpContext(nullptr, actualParentContext);
        fputs("should be using: ", stdout);
        DumpContext(nullptr, aParentContext);
        VerifySameTree(actualParentContext, aParentContext);
        fputs("\n", stdout);
      }
    }

  } else {
    if (actualParentContext) {
      NS_ERROR("Have parent context and shouldn't");
      DumpContext(aFrame, aContext);
      fputs("Has parent context: ", stdout);
      DumpContext(nullptr, actualParentContext);
      fputs("Should be null\n\n", stdout);
    }
  }

  nsStyleContext* childStyleIfVisited = aContext->GetStyleIfVisited();
  // Either childStyleIfVisited has aContext->GetParent()->GetStyleIfVisited()
  // as the parent or it has a different rulenode from aContext _and_ has
  // aContext->GetParent() as the parent.
  if (childStyleIfVisited &&
      !((childStyleIfVisited->RuleNode() != aContext->RuleNode() &&
         childStyleIfVisited->GetParent() == aContext->GetParent()) ||
        childStyleIfVisited->GetParent() ==
          aContext->GetParent()->GetStyleIfVisited())) {
    NS_ERROR("Visited style has wrong parent");
    DumpContext(aFrame, aContext);
    fputs("\n", stdout);
  }
}

static void
VerifyStyleTree(nsIFrame* aFrame)
{
  nsStyleContext* context = aFrame->StyleContext();
  VerifyContextParent(aFrame, context, nullptr);

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    for (nsIFrame* child : lists.CurrentList()) {
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        if (nsGkAtoms::placeholderFrame == child->GetType()) {
          // placeholder: first recurse and verify the out of flow frame,
          // then verify the placeholder's context
          nsIFrame* outOfFlowFrame =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(child);

          // recurse to out of flow frame, letting the parent context get resolved
          do {
            VerifyStyleTree(outOfFlowFrame);
          } while ((outOfFlowFrame = outOfFlowFrame->GetNextContinuation()));

          // verify placeholder using the parent frame's context as
          // parent context
          VerifyContextParent(child, nullptr, nullptr);
        } else { // regular frame
          VerifyStyleTree(child);
        }
      }
    }
  }

  // do additional contexts
  int32_t contextIndex = 0;
  for (nsStyleContext* extraContext;
       (extraContext = aFrame->GetAdditionalStyleContext(contextIndex));
       ++contextIndex) {
    VerifyContextParent(aFrame, extraContext, context);
  }
}

void
RestyleManagerBase::DebugVerifyStyleTree(nsIFrame* aFrame)
{
  if (aFrame) {
    VerifyStyleTree(aFrame);
  }
}

#endif // DEBUG

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(ChangeListProperty, bool)

/**
 * Sync views on aFrame and all of aFrame's descendants (following placeholders),
 * if aChange has nsChangeHint_SyncFrameView.
 * Calls DoApplyRenderingChangeToTree on all aFrame's out-of-flow descendants
 * (following placeholders), if aChange has nsChangeHint_RepaintFrame.
 * aFrame should be some combination of nsChangeHint_SyncFrameView,
 * nsChangeHint_RepaintFrame, nsChangeHint_UpdateOpacityLayer and
 * nsChangeHint_SchedulePaint, nothing else.
*/
static void SyncViewsAndInvalidateDescendants(nsIFrame* aFrame,
                                              nsChangeHint aChange);

static void StyleChangeReflow(nsIFrame* aFrame, nsChangeHint aHint);

/**
 * To handle nsChangeHint_ChildrenOnlyTransform we must iterate over the child
 * frames of the SVG frame concerned. This helper function is used to find that
 * SVG frame when we encounter nsChangeHint_ChildrenOnlyTransform to ensure
 * that we iterate over the intended children, since sometimes we end up
 * handling that hint while processing hints for one of the SVG frame's
 * ancestor frames.
 *
 * The reason that we sometimes end up trying to process the hint for an
 * ancestor of the SVG frame that the hint is intended for is due to the way we
 * process restyle events. ApplyRenderingChangeToTree adjusts the frame from
 * the restyled element's principle frame to one of its ancestor frames based
 * on what nsCSSRendering::FindBackground returns, since the background style
 * may have been propagated up to an ancestor frame. Processing hints using an
 * ancestor frame is fine in general, but nsChangeHint_ChildrenOnlyTransform is
 * a special case since it is intended to update the children of a specific
 * frame.
 */
static nsIFrame*
GetFrameForChildrenOnlyTransformHint(nsIFrame* aFrame)
{
  if (aFrame->GetType() == nsGkAtoms::viewportFrame) {
    // This happens if the root-<svg> is fixed positioned, in which case we
    // can't use aFrame->GetContent() to find the primary frame, since
    // GetContent() returns nullptr for ViewportFrame.
    aFrame = aFrame->PrincipalChildList().FirstChild();
  }
  // For an nsHTMLScrollFrame, this will get the SVG frame that has the
  // children-only transforms:
  aFrame = aFrame->GetContent()->GetPrimaryFrame();
  if (aFrame->GetType() == nsGkAtoms::svgOuterSVGFrame) {
    aFrame = aFrame->PrincipalChildList().FirstChild();
    MOZ_ASSERT(aFrame->GetType() == nsGkAtoms::svgOuterSVGAnonChildFrame,
               "Where is the nsSVGOuterSVGFrame's anon child??");
  }
  MOZ_ASSERT(aFrame->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer),
             "Children-only transforms only expected on SVG frames");
  return aFrame;
}

// Returns true if this function managed to successfully move a frame, and
// false if it could not process the position change, and a reflow should
// be performed instead.
bool
RecomputePosition(nsIFrame* aFrame)
{
  // Don't process position changes on table frames, since we already handle
  // the dynamic position change on the table wrapper frame, and the
  // reflow-based fallback code path also ignores positions on inner table
  // frames.
  if (aFrame->GetType() == nsGkAtoms::tableFrame) {
    return true;
  }

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  // Changes to the offsets of a non-positioned element can safely be ignored.
  if (display->mPosition == NS_STYLE_POSITION_STATIC) {
    return true;
  }

  // Don't process position changes on frames which have views or the ones which
  // have a view somewhere in their descendants, because the corresponding view
  // needs to be repositioned properly as well.
  if (aFrame->HasView() ||
      (aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    StyleChangeReflow(aFrame, nsChangeHint_NeedReflow);
    return false;
  }

  aFrame->SchedulePaint();

  // For relative positioning, we can simply update the frame rect
  if (display->IsRelativelyPositionedStyle()) {
    // Move the frame
    if (display->mPosition == NS_STYLE_POSITION_STICKY) {
      if (display->IsInnerTableStyle()) {
        // We don't currently support sticky positioning of inner table
        // elements (bug 975644). Bail.
        //
        // When this is fixed, remove the null-check for the computed
        // offsets in nsTableRowFrame::ReflowChildren.
        return true;
      }

      // Update sticky positioning for an entire element at once, starting with
      // the first continuation or ib-split sibling.
      // It's rare that the frame we already have isn't already the first
      // continuation or ib-split sibling, but it can happen when styles differ
      // across continuations such as ::first-line or ::first-letter, and in
      // those cases we will generally (but maybe not always) do the work twice.
      nsIFrame* firstContinuation =
        nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);

      StickyScrollContainer::ComputeStickyOffsets(firstContinuation);
      StickyScrollContainer* ssc =
        StickyScrollContainer::GetStickyScrollContainerForFrame(
          firstContinuation);
      if (ssc) {
        ssc->PositionContinuations(firstContinuation);
      }
    } else {
      MOZ_ASSERT(NS_STYLE_POSITION_RELATIVE == display->mPosition,
                 "Unexpected type of positioning");
      for (nsIFrame* cont = aFrame; cont;
           cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
        nsIFrame* cb = cont->GetContainingBlock();
        nsMargin newOffsets;
        WritingMode wm = cb->GetWritingMode();
        const LogicalSize size(wm, cb->GetContentRectRelativeToSelf().Size());

        ReflowInput::ComputeRelativeOffsets(wm, cont, size, newOffsets);
        NS_ASSERTION(newOffsets.left == -newOffsets.right &&
                     newOffsets.top == -newOffsets.bottom,
                     "ComputeRelativeOffsets should return valid results");

        // ReflowInput::ApplyRelativePositioning would work here, but
        // since we've already checked mPosition and aren't changing the frame's
        // normal position, go ahead and add the offsets directly.
        cont->SetPosition(cont->GetNormalPosition() +
                          nsPoint(newOffsets.left, newOffsets.top));
      }
    }

    return true;
  }

  // For the absolute positioning case, set up a fake HTML reflow state for
  // the frame, and then get the offsets and size from it. If the frame's size
  // doesn't need to change, we can simply update the frame position. Otherwise
  // we fall back to a reflow.
  nsRenderingContext rc(
    aFrame->PresContext()->PresShell()->CreateReferenceRenderingContext());

  // Construct a bogus parent reflow state so that there's a usable
  // containing block reflow state.
  nsIFrame* parentFrame = aFrame->GetParent();
  WritingMode parentWM = parentFrame->GetWritingMode();
  WritingMode frameWM = aFrame->GetWritingMode();
  LogicalSize parentSize = parentFrame->GetLogicalSize();

  nsFrameState savedState = parentFrame->GetStateBits();
  ReflowInput parentReflowInput(aFrame->PresContext(), parentFrame, &rc,
                                parentSize);
  parentFrame->RemoveStateBits(~nsFrameState(0));
  parentFrame->AddStateBits(savedState);

  // The bogus parent state here was created with no parent state of its own,
  // and therefore it won't have an mCBReflowInput set up.
  // But we may need one (for InitCBReflowInput in a child state), so let's
  // try to create one here for the cases where it will be needed.
  Maybe<ReflowInput> cbReflowInput;
  nsIFrame* cbFrame = parentFrame->GetContainingBlock();
  if (cbFrame && (aFrame->GetContainingBlock() != parentFrame ||
                  parentFrame->GetType() == nsGkAtoms::tableFrame)) {
    LogicalSize cbSize = cbFrame->GetLogicalSize();
    cbReflowInput.emplace(cbFrame->PresContext(), cbFrame, &rc, cbSize);
    cbReflowInput->ComputedPhysicalMargin() = cbFrame->GetUsedMargin();
    cbReflowInput->ComputedPhysicalPadding() = cbFrame->GetUsedPadding();
    cbReflowInput->ComputedPhysicalBorderPadding() =
      cbFrame->GetUsedBorderAndPadding();
    parentReflowInput.mCBReflowInput = cbReflowInput.ptr();
  }

  NS_WARN_IF_FALSE(parentSize.ISize(parentWM) != NS_INTRINSICSIZE &&
                   parentSize.BSize(parentWM) != NS_INTRINSICSIZE,
                   "parentSize should be valid");
  parentReflowInput.SetComputedISize(std::max(parentSize.ISize(parentWM), 0));
  parentReflowInput.SetComputedBSize(std::max(parentSize.BSize(parentWM), 0));
  parentReflowInput.ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);

  parentReflowInput.ComputedPhysicalPadding() = parentFrame->GetUsedPadding();
  parentReflowInput.ComputedPhysicalBorderPadding() =
    parentFrame->GetUsedBorderAndPadding();
  LogicalSize availSize = parentSize.ConvertTo(frameWM, parentWM);
  availSize.BSize(frameWM) = NS_INTRINSICSIZE;

  ViewportFrame* viewport = do_QueryFrame(parentFrame);
  nsSize cbSize = viewport ?
    viewport->AdjustReflowInputAsContainingBlock(&parentReflowInput).Size()
    : aFrame->GetContainingBlock()->GetSize();
  const nsMargin& parentBorder =
    parentReflowInput.mStyleBorder->GetComputedBorder();
  cbSize -= nsSize(parentBorder.LeftRight(), parentBorder.TopBottom());
  LogicalSize lcbSize(frameWM, cbSize);
  ReflowInput reflowInput(aFrame->PresContext(), parentReflowInput, aFrame,
                          availSize, &lcbSize);
  nsSize computedSize(reflowInput.ComputedWidth(),
                      reflowInput.ComputedHeight());
  computedSize.width += reflowInput.ComputedPhysicalBorderPadding().LeftRight();
  if (computedSize.height != NS_INTRINSICSIZE) {
    computedSize.height +=
      reflowInput.ComputedPhysicalBorderPadding().TopBottom();
  }
  nsSize size = aFrame->GetSize();
  // The RecomputePosition hint is not used if any offset changed between auto
  // and non-auto. If computedSize.height == NS_INTRINSICSIZE then the new
  // element height will be its intrinsic height, and since 'top' and 'bottom''s
  // auto-ness hasn't changed, the old height must also be its intrinsic
  // height, which we can assume hasn't changed (or reflow would have
  // been triggered).
  if (computedSize.width == size.width &&
      (computedSize.height == NS_INTRINSICSIZE || computedSize.height == size.height)) {
    // If we're solving for 'left' or 'top', then compute it here, in order to
    // match the reflow code path.
    if (NS_AUTOOFFSET == reflowInput.ComputedPhysicalOffsets().left) {
      reflowInput.ComputedPhysicalOffsets().left = cbSize.width -
                                          reflowInput.ComputedPhysicalOffsets().right -
                                          reflowInput.ComputedPhysicalMargin().right -
                                          size.width -
                                          reflowInput.ComputedPhysicalMargin().left;
    }

    if (NS_AUTOOFFSET == reflowInput.ComputedPhysicalOffsets().top) {
      reflowInput.ComputedPhysicalOffsets().top = cbSize.height -
                                         reflowInput.ComputedPhysicalOffsets().bottom -
                                         reflowInput.ComputedPhysicalMargin().bottom -
                                         size.height -
                                         reflowInput.ComputedPhysicalMargin().top;
    }

    // Move the frame
    nsPoint pos(parentBorder.left + reflowInput.ComputedPhysicalOffsets().left +
                reflowInput.ComputedPhysicalMargin().left,
                parentBorder.top + reflowInput.ComputedPhysicalOffsets().top +
                reflowInput.ComputedPhysicalMargin().top);
    aFrame->SetPosition(pos);

    return true;
  }

  // Fall back to a reflow
  StyleChangeReflow(aFrame, nsChangeHint_NeedReflow);
  return false;
}

static bool
HasBoxAncestor(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame; f; f = f->GetParent()) {
    if (f->IsXULBoxFrame()) {
      return true;
    }
  }
  return false;
}

/**
 * Return true if aFrame's subtree has placeholders for out-of-flow content
 * whose 'position' style's bit in aPositionMask is set.
 */
static bool
FrameHasPositionedPlaceholderDescendants(nsIFrame* aFrame,
                                         uint32_t aPositionMask)
{
  const nsIFrame::ChildListIDs skip(nsIFrame::kAbsoluteList |
                                    nsIFrame::kFixedList);
  for (nsIFrame::ChildListIterator lists(aFrame); !lists.IsDone(); lists.Next()) {
    if (!skip.Contains(lists.CurrentID())) {
      for (nsIFrame* f : lists.CurrentList()) {
        if (f->GetType() == nsGkAtoms::placeholderFrame) {
          nsIFrame* outOfFlow =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
          // If SVG text frames could appear here, they could confuse us since
          // they ignore their position style ... but they can't.
          NS_ASSERTION(!outOfFlow->IsSVGText(),
                       "SVG text frames can't be out of flow");
          if (aPositionMask & (1 << outOfFlow->StyleDisplay()->mPosition)) {
            return true;
          }
        }
        if (FrameHasPositionedPlaceholderDescendants(f, aPositionMask)) {
          return true;
        }
      }
    }
  }
  return false;
}

static bool
NeedToReframeForAddingOrRemovingTransform(nsIFrame* aFrame)
{
  static_assert(0 <= NS_STYLE_POSITION_ABSOLUTE &&
                NS_STYLE_POSITION_ABSOLUTE < 32, "Style constant out of range");
  static_assert(0 <= NS_STYLE_POSITION_FIXED &&
                NS_STYLE_POSITION_FIXED < 32, "Style constant out of range");

  uint32_t positionMask;
  // Don't call aFrame->IsPositioned here, since that returns true if
  // the frame already has a transform, and we want to ignore that here
  if (aFrame->IsAbsolutelyPositioned() || aFrame->IsRelativelyPositioned()) {
    // This frame is a container for abs-pos descendants whether or not it
    // has a transform.
    // So abs-pos descendants are no problem; we only need to reframe if
    // we have fixed-pos descendants.
    positionMask = 1 << NS_STYLE_POSITION_FIXED;
  } else {
    // This frame may not be a container for abs-pos descendants already.
    // So reframe if we have abs-pos or fixed-pos descendants.
    positionMask =
      (1 << NS_STYLE_POSITION_FIXED) | (1 << NS_STYLE_POSITION_ABSOLUTE);
  }
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(f)) {
    if (FrameHasPositionedPlaceholderDescendants(f, positionMask)) {
      return true;
    }
  }
  return false;
}

/* static */ nsIFrame*
RestyleManagerBase::GetNearestAncestorFrame(nsIContent* aContent)
{
  nsIFrame* ancestorFrame = nullptr;
  for (nsIContent* ancestor = aContent->GetParent();
       ancestor && !ancestorFrame;
       ancestor = ancestor->GetParent()) {
    ancestorFrame = ancestor->GetPrimaryFrame();
  }
  return ancestorFrame;
}

/* static */ nsIFrame*
RestyleManagerBase::GetNextBlockInInlineSibling(FramePropertyTable* aPropTable,
                                                nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(),
               "must start with the first continuation");
  // Might we have ib-split siblings?
  if (!(aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
    // nothing more to do here
    return nullptr;
  }

  return static_cast<nsIFrame*>
    (aPropTable->Get(aFrame, nsIFrame::IBSplitSibling()));
}

static void
DoApplyRenderingChangeToTree(nsIFrame* aFrame,
                             nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");

  for ( ; aFrame; aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame)) {
    // Invalidate and sync views on all descendant frames, following placeholders.
    // We don't need to update transforms in SyncViewsAndInvalidateDescendants, because
    // there can't be any out-of-flows or popups that need to be transformed;
    // all out-of-flow descendants of the transformed element must also be
    // descendants of the transformed frame.
    SyncViewsAndInvalidateDescendants(aFrame,
      nsChangeHint(aChange & (nsChangeHint_RepaintFrame |
                              nsChangeHint_SyncFrameView |
                              nsChangeHint_UpdateOpacityLayer |
                              nsChangeHint_SchedulePaint)));
    // This must be set to true if the rendering change needs to
    // invalidate content.  If it's false, a composite-only paint
    // (empty transaction) will be scheduled.
    bool needInvalidatingPaint = false;

    // if frame has view, will already be invalidated
    if (aChange & nsChangeHint_RepaintFrame) {
      // Note that this whole block will be skipped when painting is suppressed
      // (due to our caller ApplyRendingChangeToTree() discarding the
      // nsChangeHint_RepaintFrame hint).  If you add handling for any other
      // hints within this block, be sure that they too should be ignored when
      // painting is suppressed.
      needInvalidatingPaint = true;
      aFrame->InvalidateFrameSubtree();
      if ((aChange & nsChangeHint_UpdateEffects) &&
          aFrame->IsFrameOfType(nsIFrame::eSVG) &&
          !(aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
        // Need to update our overflow rects:
        nsSVGUtils::ScheduleReflowSVG(aFrame);
      }
    }
    if (aChange & nsChangeHint_UpdateTextPath) {
      if (aFrame->IsSVGText()) {
        // Invalidate and reflow the entire SVGTextFrame:
        NS_ASSERTION(aFrame->GetContent()->IsSVGElement(nsGkAtoms::textPath),
                     "expected frame for a <textPath> element");
        nsIFrame* text =
          nsLayoutUtils::GetClosestFrameOfType(aFrame, nsGkAtoms::svgTextFrame);
        NS_ASSERTION(text, "expected to find an ancestor SVGTextFrame");
        static_cast<SVGTextFrame*>(text)->NotifyGlyphMetricsChange();
      } else {
        MOZ_ASSERT(false, "unexpected frame got nsChangeHint_UpdateTextPath");
      }
    }
    if (aChange & nsChangeHint_UpdateOpacityLayer) {
      // FIXME/bug 796697: we can get away with empty transactions for
      // opacity updates in many cases.
      needInvalidatingPaint = true;

      ActiveLayerTracker::NotifyRestyle(aFrame, eCSSProperty_opacity);
      if (nsSVGIntegrationUtils::UsingEffectsForFrame(aFrame)) {
        // SVG effects paints the opacity without using
        // nsDisplayOpacity. We need to invalidate manually.
        aFrame->InvalidateFrameSubtree();
      }
    }
    if ((aChange & nsChangeHint_UpdateTransformLayer) &&
        aFrame->IsTransformed()) {
      ActiveLayerTracker::NotifyRestyle(aFrame, eCSSProperty_transform);
      // If we're not already going to do an invalidating paint, see
      // if we can get away with only updating the transform on a
      // layer for this frame, and not scheduling an invalidating
      // paint.
      if (!needInvalidatingPaint) {
        Layer* layer;
        needInvalidatingPaint |= !aFrame->TryUpdateTransformOnly(&layer);

        if (!needInvalidatingPaint) {
          // Since we're not going to paint, we need to resend animation
          // data to the layer.
          MOZ_ASSERT(layer, "this can't happen if there's no layer");
          nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(
            layer, nullptr, nullptr, aFrame, eCSSProperty_transform);
        }
      }
    }
    if (aChange & nsChangeHint_ChildrenOnlyTransform) {
      needInvalidatingPaint = true;
      nsIFrame* childFrame =
        GetFrameForChildrenOnlyTransformHint(aFrame)->PrincipalChildList().FirstChild();
      for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
        ActiveLayerTracker::NotifyRestyle(childFrame, eCSSProperty_transform);
      }
    }
    if (aChange & nsChangeHint_SchedulePaint) {
      needInvalidatingPaint = true;
    }
    aFrame->SchedulePaint(needInvalidatingPaint
                            ? nsIFrame::PAINT_DEFAULT
                            : nsIFrame::PAINT_COMPOSITE_ONLY);
  }
}

static void
SyncViewsAndInvalidateDescendants(nsIFrame* aFrame, nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");
  NS_ASSERTION(nsChangeHint_size_t(aChange) ==
                          (aChange & (nsChangeHint_RepaintFrame |
                                      nsChangeHint_SyncFrameView |
                                      nsChangeHint_UpdateOpacityLayer |
                                      nsChangeHint_SchedulePaint)),
               "Invalid change flag");

  nsView* view = aFrame->GetView();
  if (view) {
    if (aChange & nsChangeHint_SyncFrameView) {
      nsContainerFrame::SyncFrameViewProperties(aFrame->PresContext(), aFrame,
                                                nullptr, view);
    }
  }

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    for (nsIFrame* child : lists.CurrentList()) {
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that don't have placeholders
        if (nsGkAtoms::placeholderFrame == child->GetType()) {
          // do the out-of-flow frame and its continuations
          nsIFrame* outOfFlowFrame =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(child);
          DoApplyRenderingChangeToTree(outOfFlowFrame, aChange);
        } else if (lists.CurrentID() == nsIFrame::kPopupList) {
          DoApplyRenderingChangeToTree(child, aChange);
        } else { // regular frame
          SyncViewsAndInvalidateDescendants(child, aChange);
        }
      }
    }
  }
}

static void
ApplyRenderingChangeToTree(nsIPresShell* aPresShell,
                           nsIFrame* aFrame,
                           nsChangeHint aChange)
{
  // We check StyleDisplay()->HasTransformStyle() in addition to checking
  // IsTransformed() since we can get here for some frames that don't support
  // CSS transforms.
  NS_ASSERTION(!(aChange & nsChangeHint_UpdateTransformLayer) ||
               aFrame->IsTransformed() ||
               aFrame->StyleDisplay()->HasTransformStyle(),
               "Unexpected UpdateTransformLayer hint");

  if (aPresShell->IsPaintingSuppressed()) {
    // Don't allow synchronous rendering changes when painting is turned off.
    aChange &= ~nsChangeHint_RepaintFrame;
    if (!aChange) {
      return;
    }
  }

// Trigger rendering updates by damaging this frame and any
// continuations of this frame.
#ifdef DEBUG
  gInApplyRenderingChangeToTree = true;
#endif
  if (aChange & nsChangeHint_RepaintFrame) {
    // If the frame's background is propagated to an ancestor, walk up to
    // that ancestor and apply the RepaintFrame change hint to it.
    nsStyleContext* bgSC;
    nsIFrame* propagatedFrame = aFrame;
    while (!nsCSSRendering::FindBackground(propagatedFrame, &bgSC)) {
      propagatedFrame = propagatedFrame->GetParent();
      NS_ASSERTION(aFrame, "root frame must paint");
    }

    if (propagatedFrame != aFrame) {
      DoApplyRenderingChangeToTree(propagatedFrame, nsChangeHint_RepaintFrame);
      aChange &= ~nsChangeHint_RepaintFrame;
      if (!aChange) {
        return;
      }
    }
  }
  DoApplyRenderingChangeToTree(aFrame, aChange);
#ifdef DEBUG
  gInApplyRenderingChangeToTree = false;
#endif
}

static void
AddSubtreeToOverflowTracker(nsIFrame* aFrame,
                            OverflowChangedTracker& aOverflowChangedTracker)
{
  if (aFrame->FrameMaintainsOverflow()) {
    aOverflowChangedTracker.AddFrame(aFrame,
                                     OverflowChangedTracker::CHILDREN_CHANGED);
  }
  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    for (nsIFrame* child : lists.CurrentList()) {
      AddSubtreeToOverflowTracker(child, aOverflowChangedTracker);
    }
  }
}

static void
StyleChangeReflow(nsIFrame* aFrame, nsChangeHint aHint)
{
  nsIPresShell::IntrinsicDirty dirtyType;
  if (aHint & nsChangeHint_ClearDescendantIntrinsics) {
    NS_ASSERTION(aHint & nsChangeHint_ClearAncestorIntrinsics,
                 "Please read the comments in nsChangeHint.h");
    NS_ASSERTION(aHint & nsChangeHint_NeedDirtyReflow,
                 "ClearDescendantIntrinsics requires NeedDirtyReflow");
    dirtyType = nsIPresShell::eStyleChange;
  } else if ((aHint & nsChangeHint_UpdateComputedBSize) &&
             aFrame->HasAnyStateBits(
               NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE)) {
    dirtyType = nsIPresShell::eStyleChange;
  } else if (aHint & nsChangeHint_ClearAncestorIntrinsics) {
    dirtyType = nsIPresShell::eTreeChange;
  } else if ((aHint & nsChangeHint_UpdateComputedBSize) &&
             HasBoxAncestor(aFrame)) {
    // The frame's computed BSize is changing, and we have a box ancestor
    // whose cached intrinsic height may need to be updated.
    dirtyType = nsIPresShell::eTreeChange;
  } else {
    dirtyType = nsIPresShell::eResize;
  }

  nsFrameState dirtyBits;
  if (aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    dirtyBits = nsFrameState(0);
  } else if ((aHint & nsChangeHint_NeedDirtyReflow) ||
             dirtyType == nsIPresShell::eStyleChange) {
    dirtyBits = NS_FRAME_IS_DIRTY;
  } else {
    dirtyBits = NS_FRAME_HAS_DIRTY_CHILDREN;
  }

  // If we're not going to clear any intrinsic sizes on the frames, and
  // there are no dirty bits to set, then there's nothing to do.
  if (dirtyType == nsIPresShell::eResize && !dirtyBits)
    return;

  nsIPresShell::ReflowRootHandling rootHandling;
  if (aHint & nsChangeHint_ReflowChangesSizeOrPosition) {
    rootHandling = nsIPresShell::ePositionOrSizeChange;
  } else {
    rootHandling = nsIPresShell::eNoPositionOrSizeChange;
  }

  do {
    aFrame->PresContext()->PresShell()->FrameNeedsReflow(
      aFrame, dirtyType, dirtyBits, rootHandling);
    aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame);
  } while (aFrame);
}

/* static */ nsIFrame*
RestyleManagerBase::GetNextContinuationWithSameStyle(
  nsIFrame* aFrame, nsStyleContext* aOldStyleContext,
  bool* aHaveMoreContinuations)
{
  // See GetPrevContinuationWithSameStyle about {ib} splits.

  nsIFrame* nextContinuation = aFrame->GetNextContinuation();
  if (!nextContinuation &&
      (aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
    // We're the last continuation, so we have to hop back to the first
    // before getting the frame property
    nextContinuation =
      aFrame->FirstContinuation()->Properties().Get(nsIFrame::IBSplitSibling());
    if (nextContinuation) {
      nextContinuation =
        nextContinuation->Properties().Get(nsIFrame::IBSplitSibling());
    }
  }

  if (!nextContinuation) {
    return nullptr;
  }

  NS_ASSERTION(nextContinuation->GetContent() == aFrame->GetContent(),
               "unexpected content mismatch");

  nsStyleContext* nextStyle = nextContinuation->StyleContext();
  if (nextStyle != aOldStyleContext) {
    NS_ASSERTION(aOldStyleContext->GetPseudo() != nextStyle->GetPseudo() ||
                 aOldStyleContext->GetParent() != nextStyle->GetParent(),
                 "continuations should have the same style context");
    nextContinuation = nullptr;
    if (aHaveMoreContinuations) {
      *aHaveMoreContinuations = true;
    }
  }
  return nextContinuation;
}

nsresult
RestyleManagerBase::ProcessRestyledFrames(nsStyleChangeList& aChangeList)
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "Someone forgot a script blocker");
  int32_t count = aChangeList.Count();
  if (!count)
    return NS_OK;

  PROFILER_LABEL("RestyleManager", "ProcessRestyledFrames",
                 js::ProfileEntry::Category::CSS);

  nsPresContext* presContext = PresContext();
  FramePropertyTable* propTable = presContext->PropertyTable();
  nsCSSFrameConstructor* frameConstructor = presContext->FrameConstructor();

  // Make sure to not rebuild quote or counter lists while we're
  // processing restyles
  frameConstructor->BeginUpdate();

  // Mark frames so that we skip frames that die along the way, bug 123049.
  // A frame can be in the list multiple times with different hints. Further
  // optmization is possible if nsStyleChangeList::AppendChange could coalesce
  int32_t index = count;

  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->Set(changeData->mFrame, ChangeListProperty(), true);
    }
  }

  index = count;

  bool didUpdateCursor = false;

  while (0 <= --index) {
    nsIFrame* frame;
    nsIContent* content;
    bool didReflowThisFrame = false;
    nsChangeHint hint;
    aChangeList.ChangeAt(index, frame, content, hint);

    NS_ASSERTION(!(hint & nsChangeHint_AllReflowHints) ||
                 (hint & nsChangeHint_NeedReflow),
                 "Reflow hint bits set without actually asking for a reflow");

    // skip any frame that has been destroyed due to a ripple effect
    if (frame && !propTable->Get(frame, ChangeListProperty())) {
      continue;
    }

    if (frame && frame->GetContent() != content) {
      // XXXbz this is due to image maps messing with the primary frame of
      // <area>s.  See bug 135040.  Remove this block once that's fixed.
      frame = nullptr;
      if (!(hint & nsChangeHint_ReconstructFrame)) {
        continue;
      }
    }

    if ((hint & nsChangeHint_UpdateContainingBlock) && frame &&
        !(hint & nsChangeHint_ReconstructFrame)) {
      if (NeedToReframeForAddingOrRemovingTransform(frame) ||
          frame->GetType() == nsGkAtoms::fieldSetFrame ||
          frame->GetContentInsertionFrame() != frame) {
        // The frame has positioned children that need to be reparented, or
        // it can't easily be converted to/from being an abs-pos container correctly.
        hint |= nsChangeHint_ReconstructFrame;
      } else {
        for (nsIFrame* cont = frame; cont;
             cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
          // Normally frame construction would set state bits as needed,
          // but we're not going to reconstruct the frame so we need to set them.
          // It's because we need to set this state on each affected frame
          // that we can't coalesce nsChangeHint_UpdateContainingBlock hints up
          // to ancestors (i.e. it can't be an inherited change hint).
          if (cont->IsAbsPosContainingBlock()) {
            if (cont->StyleDisplay()->HasTransform(cont)) {
              cont->AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
            }
            if (!cont->IsAbsoluteContainer() &&
                (cont->GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN)) {
              cont->MarkAsAbsoluteContainingBlock();
            }
          } else {
            // Don't remove NS_FRAME_MAY_BE_TRANSFORMED since it may still by
            // transformed by other means. It's OK to have the bit even if it's
            // not needed.
            if (cont->IsAbsoluteContainer()) {
              cont->MarkAsNotAbsoluteContainingBlock();
            }
          }
        }
      }
    }
    if (hint & nsChangeHint_ReconstructFrame) {
      // If we ever start passing true here, be careful of restyles
      // that involve a reframe and animations.  In particular, if the
      // restyle we're processing here is an animation restyle, but
      // the style resolution we will do for the frame construction
      // happens async when we're not in an animation restyle already,
      // problems could arise.
      // We could also have problems with triggering of CSS transitions
      // on elements whose frames are reconstructed, since we depend on
      // the reconstruction happening synchronously.
      frameConstructor->RecreateFramesForContent(content, false,
        nsCSSFrameConstructor::REMOVE_FOR_RECONSTRUCTION, nullptr);
    } else {
      NS_ASSERTION(frame, "This shouldn't happen");

      if (!frame->FrameMaintainsOverflow()) {
        // frame does not maintain overflow rects, so avoid calling
        // FinishAndStoreOverflow on it:
        hint &= ~(nsChangeHint_UpdateOverflow |
                  nsChangeHint_ChildrenOnlyTransform |
                  nsChangeHint_UpdatePostTransformOverflow |
                  nsChangeHint_UpdateParentOverflow);
      }

      if (!(frame->GetStateBits() & NS_FRAME_MAY_BE_TRANSFORMED)) {
        // Frame can not be transformed, and thus a change in transform will
        // have no effect and we should not use the
        // nsChangeHint_UpdatePostTransformOverflow hint.
        hint &= ~nsChangeHint_UpdatePostTransformOverflow;
      }

      if (hint & nsChangeHint_UpdateEffects) {
        for (nsIFrame* cont = frame; cont;
             cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
          nsSVGEffects::UpdateEffects(cont);
        }
      }
      if ((hint & nsChangeHint_InvalidateRenderingObservers) ||
          ((hint & nsChangeHint_UpdateOpacityLayer) &&
           frame->IsFrameOfType(nsIFrame::eSVG) &&
           !(frame->GetStateBits() & NS_STATE_IS_OUTER_SVG))) {
        nsSVGEffects::InvalidateRenderingObservers(frame);
      }
      if (hint & nsChangeHint_NeedReflow) {
        StyleChangeReflow(frame, hint);
        didReflowThisFrame = true;
      }

      if ((hint & nsChangeHint_UpdateUsesOpacity) &&
          frame->IsFrameOfType(nsIFrame::eTablePart)) {
        NS_ASSERTION(hint & nsChangeHint_UpdateOpacityLayer,
                     "should only return UpdateUsesOpacity hint "
                     "when also returning UpdateOpacityLayer hint");
        // When an internal table part (including cells) changes between
        // having opacity 1 and non-1, it changes whether its
        // backgrounds (and those of table parts inside of it) are
        // painted as part of the table's nsDisplayTableBorderBackground
        // display item, or part of its own display item.  That requires
        // invalidation, so change UpdateOpacityLayer to RepaintFrame.
        hint &= ~nsChangeHint_UpdateOpacityLayer;
        hint |= nsChangeHint_RepaintFrame;
      }

      if (hint & nsChangeHint_UpdateBackgroundPosition) {
        // For most frame types, DLBI can detect background position changes,
        // so we only need to schedule a paint.
        hint |= nsChangeHint_SchedulePaint;
        if (frame->IsFrameOfType(nsIFrame::eTablePart) ||
            frame->IsFrameOfType(nsIFrame::eMathML)) {
          // Table parts and MathML frames don't build display items for their
          // backgrounds, so DLBI can't detect background-position changes for
          // these frames. Repaint the whole frame.
          hint |= nsChangeHint_RepaintFrame;
        }
      }

      if (hint & (nsChangeHint_RepaintFrame | nsChangeHint_SyncFrameView |
                  nsChangeHint_UpdateOpacityLayer | nsChangeHint_UpdateTransformLayer |
                  nsChangeHint_ChildrenOnlyTransform | nsChangeHint_SchedulePaint)) {
        ApplyRenderingChangeToTree(presContext->PresShell(), frame, hint);
      }
      if ((hint & nsChangeHint_RecomputePosition) && !didReflowThisFrame) {
        ActiveLayerTracker::NotifyOffsetRestyle(frame);
        // It is possible for this to fall back to a reflow
        if (!RecomputePosition(frame)) {
          didReflowThisFrame = true;
        }
      }
      NS_ASSERTION(!(hint & nsChangeHint_ChildrenOnlyTransform) ||
                   (hint & nsChangeHint_UpdateOverflow),
                   "nsChangeHint_UpdateOverflow should be passed too");
      if (!didReflowThisFrame &&
          (hint & (nsChangeHint_UpdateOverflow |
                   nsChangeHint_UpdatePostTransformOverflow |
                   nsChangeHint_UpdateParentOverflow |
                   nsChangeHint_UpdateSubtreeOverflow))) {
        if (hint & nsChangeHint_UpdateSubtreeOverflow) {
          for (nsIFrame* cont = frame; cont; cont =
                 nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
            AddSubtreeToOverflowTracker(cont, mOverflowChangedTracker);
          }
          // The work we just did in AddSubtreeToOverflowTracker
          // subsumes some of the other hints:
          hint &= ~(nsChangeHint_UpdateOverflow |
                    nsChangeHint_UpdatePostTransformOverflow);
        }
        if (hint & nsChangeHint_ChildrenOnlyTransform) {
          // The overflow areas of the child frames need to be updated:
          nsIFrame* hintFrame = GetFrameForChildrenOnlyTransformHint(frame);
          nsIFrame* childFrame = hintFrame->PrincipalChildList().FirstChild();
          NS_ASSERTION(!nsLayoutUtils::GetNextContinuationOrIBSplitSibling(frame),
                       "SVG frames should not have continuations "
                       "or ib-split siblings");
          NS_ASSERTION(!nsLayoutUtils::GetNextContinuationOrIBSplitSibling(hintFrame),
                       "SVG frames should not have continuations "
                       "or ib-split siblings");
          for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
            MOZ_ASSERT(childFrame->IsFrameOfType(nsIFrame::eSVG),
                       "Not expecting non-SVG children");
            // If |childFrame| is dirty or has dirty children, we don't bother
            // updating overflows since that will happen when it's reflowed.
            if (!(childFrame->GetStateBits() &
                  (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
              mOverflowChangedTracker.AddFrame(childFrame,
                                        OverflowChangedTracker::CHILDREN_CHANGED);
            }
            NS_ASSERTION(!nsLayoutUtils::GetNextContinuationOrIBSplitSibling(childFrame),
                         "SVG frames should not have continuations "
                         "or ib-split siblings");
            NS_ASSERTION(childFrame->GetParent() == hintFrame,
                         "SVG child frame not expected to have different parent");
          }
        }
        // If |frame| is dirty or has dirty children, we don't bother updating
        // overflows since that will happen when it's reflowed.
        if (!(frame->GetStateBits() &
              (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
          if (hint & (nsChangeHint_UpdateOverflow |
                      nsChangeHint_UpdatePostTransformOverflow)) {
            OverflowChangedTracker::ChangeKind changeKind;
            // If we have both nsChangeHint_UpdateOverflow and
            // nsChangeHint_UpdatePostTransformOverflow,
            // CHILDREN_CHANGED is selected as it is
            // strictly stronger.
            if (hint & nsChangeHint_UpdateOverflow) {
              changeKind = OverflowChangedTracker::CHILDREN_CHANGED;
            } else {
              changeKind = OverflowChangedTracker::TRANSFORM_CHANGED;
            }
            for (nsIFrame* cont = frame; cont; cont =
                   nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
              mOverflowChangedTracker.AddFrame(cont, changeKind);
            }
          }
          // UpdateParentOverflow hints need to be processed in addition
          // to the above, since if the processing of the above hints
          // yields no change, the update will not propagate to the
          // parent.
          if (hint & nsChangeHint_UpdateParentOverflow) {
            MOZ_ASSERT(frame->GetParent(),
                       "shouldn't get style hints for the root frame");
            for (nsIFrame* cont = frame; cont; cont =
                   nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
              mOverflowChangedTracker.AddFrame(cont->GetParent(),
                                   OverflowChangedTracker::CHILDREN_CHANGED);
            }
          }
        }
      }
      if ((hint & nsChangeHint_UpdateCursor) && !didUpdateCursor) {
        presContext->PresShell()->SynthesizeMouseMove(false);
        didUpdateCursor = true;
      }
    }
  }

  frameConstructor->EndUpdate();

  // cleanup references and verify the style tree.  Note that the latter needs
  // to happen once we've processed the whole list, since until then the tree
  // is not in fact in a consistent state.
  index = count;
  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->Delete(changeData->mFrame, ChangeListProperty());
    }

#ifdef DEBUG
    // reget frame from content since it may have been regenerated...
    if (changeData->mContent) {
      nsIFrame* frame = changeData->mContent->GetPrimaryFrame();
      if (frame) {
        DebugVerifyStyleTree(frame);
      }
    } else if (!changeData->mFrame ||
               changeData->mFrame->GetType() != nsGkAtoms::viewportFrame) {
      NS_WARNING("Unable to test style tree integrity -- no content node "
                 "(and not a viewport frame)");
    }
#endif
  }

  aChangeList.Clear();
  return NS_OK;
}

} // namespace mozilla
