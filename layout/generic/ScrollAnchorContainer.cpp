/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollAnchorContainer.h"
#include <cstddef>

#include "mozilla/dom/Text.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/ToString.h"
#include "nsBlockFrame.h"
#include "nsGfxScrollFrame.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"

using namespace mozilla::dom;

#ifdef DEBUG
static mozilla::LazyLogModule sAnchorLog("scrollanchor");

#  define ANCHOR_LOG_WITH(anchor_, fmt, ...)              \
    MOZ_LOG(sAnchorLog, LogLevel::Debug,                  \
            ("ANCHOR(%p, %s, root: %d): " fmt, (anchor_), \
             (anchor_)                                    \
                 ->Frame()                                \
                 ->PresContext()                          \
                 ->Document()                             \
                 ->GetDocumentURI()                       \
                 ->GetSpecOrDefault()                     \
                 .get(),                                  \
             (anchor_)->Frame()->mIsRoot, ##__VA_ARGS__));

#  define ANCHOR_LOG(fmt, ...) ANCHOR_LOG_WITH(this, fmt, ##__VA_ARGS__)
#else
#  define ANCHOR_LOG(...)
#  define ANCHOR_LOG_WITH(...)
#endif

namespace mozilla::layout {

nsHTMLScrollFrame* ScrollAnchorContainer::Frame() const {
  return reinterpret_cast<nsHTMLScrollFrame*>(
      ((char*)this) - offsetof(nsHTMLScrollFrame, mAnchor));
}

ScrollAnchorContainer::ScrollAnchorContainer(nsHTMLScrollFrame* aScrollFrame)
    : mDisabled(false),
      mAnchorMightBeSubOptimal(false),
      mAnchorNodeIsDirty(true),
      mApplyingAnchorAdjustment(false),
      mSuppressAnchorAdjustment(false) {
  MOZ_ASSERT(aScrollFrame == Frame());
}

ScrollAnchorContainer::~ScrollAnchorContainer() = default;

ScrollAnchorContainer* ScrollAnchorContainer::FindFor(nsIFrame* aFrame) {
  aFrame = aFrame->GetParent();
  if (!aFrame) {
    return nullptr;
  }
  nsIScrollableFrame* nearest = nsLayoutUtils::GetNearestScrollableFrame(
      aFrame, nsLayoutUtils::SCROLLABLE_SAME_DOC |
                  nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
  if (nearest) {
    return nearest->Anchor();
  }
  return nullptr;
}

nsIScrollableFrame* ScrollAnchorContainer::ScrollableFrame() const {
  return Frame()->GetScrollTargetFrame();
}

/**
 * Set the appropriate frame flags for a frame that has become or is no longer
 * an anchor node.
 */
static void SetAnchorFlags(const nsIFrame* aScrolledFrame,
                           nsIFrame* aAnchorNode, bool aInScrollAnchorChain) {
  nsIFrame* frame = aAnchorNode;
  while (frame && frame != aScrolledFrame) {
    // TODO(emilio, bug 1629280): This commented out assertion below should
    // hold, but it may not in the case of reparenting-during-reflow (due to
    // inline fragmentation or such). That looks fishy!
    //
    // We should either invalidate the anchor when reparenting any frame on the
    // chain, or fix up the chain flags.
    //
    // MOZ_DIAGNOSTIC_ASSERT(frame->IsInScrollAnchorChain() !=
    //                       aInScrollAnchorChain);
    frame->SetInScrollAnchorChain(aInScrollAnchorChain);
    frame = frame->GetParent();
  }
  MOZ_ASSERT(frame,
             "The anchor node should be a descendant of the scrolled frame");
  // If needed, invalidate the frame so that we start/stop highlighting the
  // anchor
  if (StaticPrefs::layout_css_scroll_anchoring_highlight()) {
    for (nsIFrame* frame = aAnchorNode->FirstContinuation(); !!frame;
         frame = frame->GetNextContinuation()) {
      frame->InvalidateFrame();
    }
  }
}

/**
 * Compute the scrollable overflow rect [1] of aCandidate relative to
 * aScrollFrame with all transforms applied.
 *
 * The specification is ambiguous about what can be selected as a scroll anchor,
 * which makes the scroll anchoring bounding rect partially undefined [2]. This
 * code attempts to match the implementation in Blink.
 *
 * An additional unspecified behavior is that any scrollable overflow before the
 * border start edge in the block axis of aScrollFrame should be clamped. This
 * is to prevent absolutely positioned descendant elements from being able to
 * trigger scroll adjustments [3].
 *
 * [1]
 * https://drafts.csswg.org/css-scroll-anchoring-1/#scroll-anchoring-bounding-rect
 * [2] https://github.com/w3c/csswg-drafts/issues/3478
 * [3] https://bugzilla.mozilla.org/show_bug.cgi?id=1519541
 */
static nsRect FindScrollAnchoringBoundingRect(const nsIFrame* aScrollFrame,
                                              nsIFrame* aCandidate) {
  MOZ_ASSERT(nsLayoutUtils::IsProperAncestorFrame(aScrollFrame, aCandidate));
  if (!!Text::FromNodeOrNull(aCandidate->GetContent())) {
    // This is a frame for a text node. The spec says we need to accumulate the
    // union of all line boxes in the coordinate space of the scroll frame
    // accounting for transforms.
    //
    // To do this, we translate and accumulate the overflow rect for each text
    // continuation to the coordinate space of the nearest ancestor block
    // frame. Then we transform the resulting rect into the coordinate space of
    // the scroll frame.
    //
    // Transforms aren't allowed on non-replaced inline boxes, so we can assume
    // that these text node continuations will have the same transform as their
    // nearest block ancestor. And it should be faster to transform their union
    // rather than individually transforming each overflow rect
    //
    // XXX for fragmented blocks, blockAncestor will be an ancestor only to the
    //     text continuations in the first block continuation. GetOffsetTo
    //     should continue to work, but is it correct with transforms or a
    //     performance hazard?
    nsIFrame* blockAncestor =
        nsLayoutUtils::FindNearestBlockAncestor(aCandidate);
    MOZ_ASSERT(
        nsLayoutUtils::IsProperAncestorFrame(aScrollFrame, blockAncestor));
    nsRect bounding;
    for (nsIFrame* continuation = aCandidate->FirstContinuation(); continuation;
         continuation = continuation->GetNextContinuation()) {
      nsRect overflowRect =
          continuation->ScrollableOverflowRectRelativeToSelf();
      overflowRect += continuation->GetOffsetTo(blockAncestor);
      bounding = bounding.Union(overflowRect);
    }
    return nsLayoutUtils::TransformFrameRectToAncestor(blockAncestor, bounding,
                                                       aScrollFrame);
  }

  nsRect borderRect = aCandidate->GetRectRelativeToSelf();
  nsRect overflowRect = aCandidate->ScrollableOverflowRectRelativeToSelf();

  NS_ASSERTION(overflowRect.Contains(borderRect),
               "overflow rect must include border rect, and the clamping logic "
               "here depends on that");

  // Clamp the scrollable overflow rect to the border start edge on the block
  // axis of the scroll frame
  WritingMode writingMode = aScrollFrame->GetWritingMode();
  switch (writingMode.GetBlockDir()) {
    case WritingMode::eBlockTB: {
      overflowRect.SetBoxY(borderRect.Y(), overflowRect.YMost());
      break;
    }
    case WritingMode::eBlockLR: {
      overflowRect.SetBoxX(borderRect.X(), overflowRect.XMost());
      break;
    }
    case WritingMode::eBlockRL: {
      overflowRect.SetBoxX(overflowRect.X(), borderRect.XMost());
      break;
    }
  }

  nsRect transformed = nsLayoutUtils::TransformFrameRectToAncestor(
      aCandidate, overflowRect, aScrollFrame);
  return transformed;
}

/**
 * Compute the offset between the scrollable overflow rect start edge of
 * aCandidate and the scroll-port start edge of aScrollFrame, in the block axis
 * of aScrollFrame.
 */
static nscoord FindScrollAnchoringBoundingOffset(
    const nsHTMLScrollFrame* aScrollFrame, nsIFrame* aCandidate) {
  WritingMode writingMode = aScrollFrame->GetWritingMode();
  nsRect physicalBounding =
      FindScrollAnchoringBoundingRect(aScrollFrame, aCandidate);
  LogicalRect logicalBounding(writingMode, physicalBounding,
                              aScrollFrame->mScrolledFrame->GetSize());
  return logicalBounding.BStart(writingMode);
}

bool ScrollAnchorContainer::CanMaintainAnchor() const {
  if (!StaticPrefs::layout_css_scroll_anchoring_enabled()) {
    return false;
  }

  // If we've been disabled due to heuristics, we don't anchor anymore.
  if (mDisabled) {
    return false;
  }

  const nsStyleDisplay& disp = *Frame()->StyleDisplay();
  // Don't select a scroll anchor if the scroll frame has `overflow-anchor:
  // none`.
  if (disp.mOverflowAnchor != mozilla::StyleOverflowAnchor::Auto) {
    return false;
  }

  // Or if the scroll frame has not been scrolled from the logical origin. This
  // is not in the specification [1], but Blink does this.
  //
  // [1] https://github.com/w3c/csswg-drafts/issues/3319
  if (Frame()->GetLogicalScrollPosition() == nsPoint()) {
    return false;
  }

  // Or if there is perspective that could affect the scrollable overflow rect
  // for descendant frames. This is not in the specification as Blink doesn't
  // share this behavior with perspective [1].
  //
  // [1] https://github.com/w3c/csswg-drafts/issues/3322
  if (Frame()->ChildrenHavePerspective()) {
    return false;
  }

  return true;
}

void ScrollAnchorContainer::SelectAnchor() {
  MOZ_ASSERT(Frame()->mScrolledFrame);
  MOZ_ASSERT(mAnchorNodeIsDirty);

  AUTO_PROFILER_LABEL("ScrollAnchorContainer::SelectAnchor", LAYOUT);
  ANCHOR_LOG("Selecting anchor with scroll-port=%s.\n",
             mozilla::ToString(Frame()->GetVisualOptimalViewingRect()).c_str());

  // Select a new scroll anchor
  nsIFrame* oldAnchor = mAnchorNode;
  if (CanMaintainAnchor()) {
    MOZ_DIAGNOSTIC_ASSERT(
        !Frame()->mScrolledFrame->IsInScrollAnchorChain(),
        "Our scrolled frame can't serve as or contain an anchor for an "
        "ancestor if it can maintain its own anchor");
    ANCHOR_LOG("Beginning selection.\n");
    mAnchorNode = FindAnchorIn(Frame()->mScrolledFrame);
  } else {
    ANCHOR_LOG("Skipping selection, doesn't maintain a scroll anchor.\n");
    mAnchorNode = nullptr;
  }
  mAnchorMightBeSubOptimal =
      mAnchorNode && mAnchorNode->HasAnyStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

  // Update the anchor flags if needed
  if (oldAnchor != mAnchorNode) {
    ANCHOR_LOG("Anchor node has changed from (%p) to (%p).\n", oldAnchor,
               mAnchorNode);

    // Unset all flags for the old scroll anchor
    if (oldAnchor) {
      SetAnchorFlags(Frame()->mScrolledFrame, oldAnchor, false);
    }

    // Set all flags for the new scroll anchor
    if (mAnchorNode) {
      // Anchor selection will never select a descendant of a nested scroll
      // frame which maintains an anchor, so we can set flags without
      // conflicting with other scroll anchor containers.
      SetAnchorFlags(Frame()->mScrolledFrame, mAnchorNode, true);
    }
  } else {
    ANCHOR_LOG("Anchor node has remained (%p).\n", mAnchorNode);
  }

  // Calculate the position to use for scroll adjustments
  if (mAnchorNode) {
    mLastAnchorOffset = FindScrollAnchoringBoundingOffset(Frame(), mAnchorNode);
    ANCHOR_LOG("Using last anchor offset = %d.\n", mLastAnchorOffset);
  } else {
    mLastAnchorOffset = 0;
  }

  mAnchorNodeIsDirty = false;
}

void ScrollAnchorContainer::UserScrolled() {
  if (mApplyingAnchorAdjustment) {
    return;
  }
  InvalidateAnchor();

  if (!StaticPrefs::
          layout_css_scroll_anchoring_reset_heuristic_during_animation() &&
      Frame()->ScrollAnimationState().contains(
          nsIScrollableFrame::AnimationState::APZInProgress)) {
    // We'd want to skip resetting our heuristic while APZ is running an async
    // scroll because this UserScrolled function gets called on every refresh
    // driver's tick during running the async scroll, thus it will clobber the
    // heuristic.
    return;
  }

  mHeuristic.Reset();
}

void ScrollAnchorContainer::DisablingHeuristic::Reset() {
  mConsecutiveScrollAnchoringAdjustments = SaturateUint32(0);
  mConsecutiveScrollAnchoringAdjustmentLength = 0;
  mTimeStamp = {};
}

void ScrollAnchorContainer::AdjustmentMade(nscoord aAdjustment) {
  MOZ_ASSERT(!mDisabled, "How?");
  mDisabled = mHeuristic.AdjustmentMade(*this, aAdjustment);
}

bool ScrollAnchorContainer::DisablingHeuristic::AdjustmentMade(
    const ScrollAnchorContainer& aAnchor, nscoord aAdjustment) {
  // A reasonably large number of times that we want to check for this. If we
  // haven't hit this limit after these many attempts we assume we'll never hit
  // it.
  //
  // This is to prevent the number getting too large and making the limit round
  // to zero by mere precision error.
  //
  // 100k should be enough for anyone :)
  static const uint32_t kAnchorCheckCountLimit = 100000;

  // Zero-length adjustments are common & don't have side effects, so we don't
  // want them to consider them here; they'd bias our average towards 0.
  MOZ_ASSERT(aAdjustment, "Don't call this API for zero-length adjustments");

  const uint32_t maxConsecutiveAdjustments =
      StaticPrefs::layout_css_scroll_anchoring_max_consecutive_adjustments();

  if (!maxConsecutiveAdjustments) {
    return false;
  }

  // We don't high resolution for this timestamp.
  const auto now = TimeStamp::NowLoRes();
  if (mConsecutiveScrollAnchoringAdjustments++ == 0) {
    MOZ_ASSERT(mTimeStamp.IsNull());
    mTimeStamp = now;
  } else if (
      const auto timeoutMs = StaticPrefs::
          layout_css_scroll_anchoring_max_consecutive_adjustments_timeout_ms();
      timeoutMs && (now - mTimeStamp).ToMilliseconds() > timeoutMs) {
    Reset();
    return false;
  }

  mConsecutiveScrollAnchoringAdjustmentLength = NSCoordSaturatingAdd(
      mConsecutiveScrollAnchoringAdjustmentLength, aAdjustment);

  uint32_t consecutiveAdjustments =
      mConsecutiveScrollAnchoringAdjustments.value();
  if (consecutiveAdjustments < maxConsecutiveAdjustments ||
      consecutiveAdjustments > kAnchorCheckCountLimit) {
    return false;
  }

  auto cssPixels =
      CSSPixel::FromAppUnits(mConsecutiveScrollAnchoringAdjustmentLength);
  double average = double(cssPixels) / consecutiveAdjustments;
  uint32_t minAverage = StaticPrefs::
      layout_css_scroll_anchoring_min_average_adjustment_threshold();
  if (MOZ_LIKELY(std::abs(average) >= double(minAverage))) {
    return false;
  }

  ANCHOR_LOG_WITH(&aAnchor,
                  "Disabled scroll anchoring for container: "
                  "%f average, %f total out of %u consecutive adjustments\n",
                  average, float(cssPixels), consecutiveAdjustments);

  AutoTArray<nsString, 3> arguments;
  arguments.AppendElement()->AppendInt(consecutiveAdjustments);
  arguments.AppendElement()->AppendFloat(average);
  arguments.AppendElement()->AppendFloat(cssPixels);

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "Layout"_ns,
                                  aAnchor.Frame()->PresContext()->Document(),
                                  nsContentUtils::eLAYOUT_PROPERTIES,
                                  "ScrollAnchoringDisabledInContainer",
                                  arguments);
  return true;
}

void ScrollAnchorContainer::SuppressAdjustments() {
  ANCHOR_LOG("Received a scroll anchor suppression for %p.\n", this);
  mSuppressAnchorAdjustment = true;

  // Forward to our parent if appropriate, that is, if we don't maintain an
  // anchor, and we can't maintain one.
  //
  // Note that we need to check !CanMaintainAnchor(), instead of just whether
  // our frame is in the anchor chain of our ancestor as InvalidateAnchor()
  // does, given some suppression triggers apply even for nodes that are not in
  // the anchor chain.
  if (!mAnchorNode && !CanMaintainAnchor()) {
    if (ScrollAnchorContainer* container = FindFor(Frame())) {
      ANCHOR_LOG(" > Forwarding to parent anchor\n");
      container->SuppressAdjustments();
    }
  }
}

void ScrollAnchorContainer::InvalidateAnchor(ScheduleSelection aSchedule) {
  ANCHOR_LOG("Invalidating scroll anchor %p for %p.\n", mAnchorNode, this);

  if (mAnchorNode) {
    SetAnchorFlags(Frame()->mScrolledFrame, mAnchorNode, false);
  } else if (Frame()->mScrolledFrame->IsInScrollAnchorChain()) {
    ANCHOR_LOG(" > Forwarding to parent anchor\n");
    // We don't maintain an anchor, and our scrolled frame is in the anchor
    // chain of an ancestor. Invalidate that anchor.
    //
    // NOTE: Intentionally not forwarding aSchedule: Scheduling is always safe
    // and not doing so is just an optimization.
    FindFor(Frame())->InvalidateAnchor();
  }
  mAnchorNode = nullptr;
  mAnchorMightBeSubOptimal = false;
  mAnchorNodeIsDirty = true;
  mLastAnchorOffset = 0;

  if (!CanMaintainAnchor() || aSchedule == ScheduleSelection::No) {
    return;
  }

  Frame()->PresShell()->PostPendingScrollAnchorSelection(this);
}

void ScrollAnchorContainer::Destroy() {
  InvalidateAnchor(ScheduleSelection::No);
}

void ScrollAnchorContainer::ApplyAdjustments() {
  if (!mAnchorNode || mAnchorNodeIsDirty || mDisabled ||
      Frame()->HasPendingScrollRestoration() ||
      (StaticPrefs::
           layout_css_scroll_anchoring_reset_heuristic_during_animation() &&
       Frame()->IsProcessingScrollEvent()) ||
      Frame()->ScrollAnimationState().contains(
          nsIScrollableFrame::AnimationState::TriggeredByScript) ||
      Frame()->GetScrollPosition() == nsPoint()) {
    ANCHOR_LOG(
        "Ignoring post-reflow (anchor=%p, dirty=%d, disabled=%d, "
        "pendingRestoration=%d, scrollevent=%d, scriptAnimating=%d, "
        "zeroScrollPos=%d pendingSuppression=%d, "
        "container=%p).\n",
        mAnchorNode, mAnchorNodeIsDirty, mDisabled,
        Frame()->HasPendingScrollRestoration(),
        Frame()->IsProcessingScrollEvent(),
        Frame()->ScrollAnimationState().contains(
            nsIScrollableFrame::AnimationState::TriggeredByScript),
        Frame()->GetScrollPosition() == nsPoint(), mSuppressAnchorAdjustment,
        this);
    if (mSuppressAnchorAdjustment) {
      mSuppressAnchorAdjustment = false;
      InvalidateAnchor();
    }
    return;
  }

  nscoord current = FindScrollAnchoringBoundingOffset(Frame(), mAnchorNode);
  nscoord logicalAdjustment = current - mLastAnchorOffset;
  WritingMode writingMode = Frame()->GetWritingMode();

  ANCHOR_LOG("Anchor has moved from %d to %d.\n", mLastAnchorOffset, current);

  auto maybeInvalidate = MakeScopeExit([&] {
    if (mAnchorMightBeSubOptimal &&
        StaticPrefs::layout_css_scroll_anchoring_reselect_if_suboptimal()) {
      ANCHOR_LOG(
          "Anchor might be suboptimal, invalidating to try finding a better "
          "one\n");
      InvalidateAnchor();
    }
  });

  if (logicalAdjustment == 0) {
    ANCHOR_LOG("Ignoring zero delta anchor adjustment for %p.\n", this);
    mSuppressAnchorAdjustment = false;
    return;
  }

  if (mSuppressAnchorAdjustment) {
    ANCHOR_LOG("Applying anchor adjustment suppression for %p.\n", this);
    mSuppressAnchorAdjustment = false;
    InvalidateAnchor();
    return;
  }

  ANCHOR_LOG("Applying anchor adjustment of %d in %s with anchor %p.\n",
             logicalAdjustment, ToString(writingMode).c_str(), mAnchorNode);

  AdjustmentMade(logicalAdjustment);

  nsPoint physicalAdjustment;
  switch (writingMode.GetBlockDir()) {
    case WritingMode::eBlockTB: {
      physicalAdjustment.y = logicalAdjustment;
      break;
    }
    case WritingMode::eBlockLR: {
      physicalAdjustment.x = logicalAdjustment;
      break;
    }
    case WritingMode::eBlockRL: {
      physicalAdjustment.x = -logicalAdjustment;
      break;
    }
  }

  MOZ_RELEASE_ASSERT(!mApplyingAnchorAdjustment);
  // We should use AutoRestore here, but that doesn't work with bitfields
  mApplyingAnchorAdjustment = true;
  Frame()->ScrollToInternal(
      Frame()->GetScrollPosition() + physicalAdjustment, ScrollMode::Instant,
      StaticPrefs::layout_css_scroll_anchoring_absolute_update()
          ? ScrollOrigin::AnchorAdjustment
          : ScrollOrigin::Relative);
  mApplyingAnchorAdjustment = false;

  nsPresContext* pc = Frame()->PresContext();
  if (Frame()->mIsRoot) {
    pc->PresShell()->RootScrollFrameAdjusted(physicalAdjustment.y);
  }

  // The anchor position may not be in the same relative position after
  // adjustment. Update ourselves so we have consistent state.
  mLastAnchorOffset = FindScrollAnchoringBoundingOffset(Frame(), mAnchorNode);
}

ScrollAnchorContainer::ExamineResult
ScrollAnchorContainer::ExamineAnchorCandidate(nsIFrame* aFrame) const {
#ifdef DEBUG_FRAME_DUMP
  nsCString tag = aFrame->ListTag();
  ANCHOR_LOG("\tVisiting frame=%s (%p).\n", tag.get(), aFrame);
#else
  ANCHOR_LOG("\t\tVisiting frame=%p.\n", aFrame);
#endif
  bool isText = !!Text::FromNodeOrNull(aFrame->GetContent());
  bool isContinuation = !!aFrame->GetPrevContinuation();

  if (isText && isContinuation) {
    ANCHOR_LOG("\t\tExcluding continuation text node.\n");
    return ExamineResult::Exclude;
  }

  // Check if the author has opted out of scroll anchoring for this frame
  // and its descendants.
  const nsStyleDisplay* disp = aFrame->StyleDisplay();
  if (disp->mOverflowAnchor == mozilla::StyleOverflowAnchor::None) {
    ANCHOR_LOG("\t\tExcluding `overflow-anchor: none`.\n");
    return ExamineResult::Exclude;
  }

  // Sticky positioned elements can move with the scroll frame, making them
  // unsuitable scroll anchors. This isn't in the specification yet [1], but
  // matches Blink's implementation.
  //
  // [1] https://github.com/w3c/csswg-drafts/issues/3319
  if (aFrame->IsStickyPositioned()) {
    ANCHOR_LOG("\t\tExcluding `position: sticky`.\n");
    return ExamineResult::Exclude;
  }

  // The frame for a <br> element has a non-zero area, but Blink treats them
  // as if they have no area, so exclude them specially.
  if (aFrame->IsBrFrame()) {
    ANCHOR_LOG("\t\tExcluding <br>.\n");
    return ExamineResult::Exclude;
  }

  // Exclude frames that aren't accessible to content.
  bool isChrome =
      aFrame->GetContent() && aFrame->GetContent()->ChromeOnlyAccess();
  bool isPseudo = aFrame->Style()->IsPseudoElement();
  if (isChrome && !isPseudo) {
    ANCHOR_LOG("\t\tExcluding chrome only content.\n");
    return ExamineResult::Exclude;
  }

  const bool isReplaced = aFrame->IsFrameOfType(nsIFrame::eReplaced);

  const bool isNonReplacedInline =
      aFrame->StyleDisplay()->IsInlineInsideStyle() && !isReplaced;

  const bool isAnonBox = aFrame->Style()->IsAnonBox();

  // See if this frame has or could maintain its own anchor node.
  const bool isScrollableWithAnchor = [&] {
    nsIScrollableFrame* scrollable = do_QueryFrame(aFrame);
    if (!scrollable) {
      return false;
    }
    auto* anchor = scrollable->Anchor();
    return anchor->AnchorNode() || anchor->CanMaintainAnchor();
  }();

  // We don't allow scroll anchors to be selected inside of nested scrollable
  // frames which maintain an anchor node as it's not clear how an anchor
  // adjustment should apply to multiple scrollable frames.
  //
  // It is important to descend into _some_ scrollable frames, specially
  // overflow: hidden, as those don't generally maintain their own anchors, and
  // it is a common case in the wild where scroll anchoring ought to work.
  //
  // We also don't allow scroll anchors to be selected inside of replaced
  // elements (like <img>, <video>, <svg>...) as they behave atomically. SVG
  // uses a different layout model than CSS, and the specification doesn't say
  // it should apply anyway.
  //
  // [1] https://github.com/w3c/csswg-drafts/issues/3477
  const bool canDescend = !isScrollableWithAnchor && !isReplaced;

  // Non-replaced inline boxes (including ruby frames) and anon boxes are not
  // acceptable anchors, so we descend if possible, or otherwise exclude them
  // altogether.
  if (!isText && (isNonReplacedInline || isAnonBox)) {
    ANCHOR_LOG(
        "\t\tSearching descendants of anon or non-replaced inline box (a=%d, "
        "i=%d).\n",
        isAnonBox, isNonReplacedInline);
    if (canDescend) {
      return ExamineResult::PassThrough;
    }
    return ExamineResult::Exclude;
  }

  // Find the scroll anchoring bounding rect.
  nsRect rect = FindScrollAnchoringBoundingRect(Frame(), aFrame);
  ANCHOR_LOG("\t\trect = [%d %d x %d %d].\n", rect.x, rect.y, rect.width,
             rect.height);

  // Check if this frame is visible in the scroll port. This will exclude rects
  // with zero sized area. The specification is ambiguous about this [1], but
  // this matches Blink's implementation.
  //
  // [1] https://github.com/w3c/csswg-drafts/issues/3483
  nsRect visibleRect;
  if (!visibleRect.IntersectRect(rect,
                                 Frame()->GetVisualOptimalViewingRect())) {
    return ExamineResult::Exclude;
  }

  // It's not clear what the scroll anchoring bounding rect is, for elements
  // fragmented in the block direction (e.g. across column or page breaks).
  //
  // Inline-fragmented elements other than text shouldn't get here because of
  // the isNonReplacedInline check.
  //
  // For text nodes that are fragmented, it's specified that we need to consider
  // the union of its line boxes.
  //
  // So for text nodes we handle them by including the union of line boxes in
  // the bounding rect of the primary frame, and not selecting any
  // continuations.
  //
  // For block-outside elements we choose to consider the bounding rect of each
  // frame individually, allowing ourselves to descend into any frame, but only
  // selecting a frame if it's not a continuation.
  if (canDescend && isContinuation) {
    ANCHOR_LOG("\t\tSearching descendants of a continuation.\n");
    return ExamineResult::PassThrough;
  }

  // If this frame is fully visible, then select it as the scroll anchor.
  if (visibleRect.IsEqualEdges(rect)) {
    ANCHOR_LOG("\t\tFully visible, taking.\n");
    return ExamineResult::Accept;
  }

  // If we can't descend into this frame, then select it as the scroll anchor.
  if (!canDescend) {
    ANCHOR_LOG("\t\tIntersects a frame that we can't descend into, taking.\n");
    return ExamineResult::Accept;
  }

  // It must be partially visible and we can descend into this frame. Examine
  // its children for a better scroll anchor or fall back to this one.
  ANCHOR_LOG("\t\tIntersects valid candidate, checking descendants.\n");
  return ExamineResult::Traverse;
}

nsIFrame* ScrollAnchorContainer::FindAnchorIn(nsIFrame* aFrame) const {
  // Visit the child lists of this frame
  for (const auto& [list, listID] : aFrame->ChildLists()) {
    // Skip child lists that contain out-of-flow frames, we'll visit them by
    // following placeholders in the in-flow lists so that we visit these
    // frames in DOM order.
    // XXX do we actually need to exclude FrameChildListID::OverflowOutOfFlow
    // too?
    if (listID == FrameChildListID::Absolute ||
        listID == FrameChildListID::Fixed ||
        listID == FrameChildListID::Float ||
        listID == FrameChildListID::OverflowOutOfFlow) {
      continue;
    }

    // Search the child list, and return if we selected an anchor
    if (nsIFrame* anchor = FindAnchorInList(list)) {
      return anchor;
    }
  }

  // The spec requires us to do an extra pass to visit absolutely positioned
  // frames a second time after all the children of their containing block have
  // been visited.
  //
  // It's not clear why this is needed [1], but it matches Blink's
  // implementation, and is needed for a WPT test.
  //
  // [1] https://github.com/w3c/csswg-drafts/issues/3465
  const nsFrameList& absPosList =
      aFrame->GetChildList(FrameChildListID::Absolute);
  if (nsIFrame* anchor = FindAnchorInList(absPosList)) {
    return anchor;
  }

  return nullptr;
}

nsIFrame* ScrollAnchorContainer::FindAnchorInList(
    const nsFrameList& aFrameList) const {
  for (nsIFrame* child : aFrameList) {
    // If this is a placeholder, try to follow it to the out of flow frame.
    nsIFrame* realFrame = nsPlaceholderFrame::GetRealFrameFor(child);
    if (child != realFrame) {
      // If the out of flow frame is not a descendant of our scroll frame,
      // then it must have a different containing block and cannot be an
      // anchor node.
      if (!nsLayoutUtils::IsProperAncestorFrame(Frame(), realFrame)) {
        ANCHOR_LOG(
            "\t\tSkipping out of flow frame that is not a descendant of the "
            "scroll frame.\n");
        continue;
      }
      ANCHOR_LOG("\t\tFollowing placeholder to out of flow frame.\n");
      child = realFrame;
    }

    // Perform the candidate examination algorithm
    ExamineResult examine = ExamineAnchorCandidate(child);

    // See the comment before the definition of `ExamineResult` in
    // `ScrollAnchorContainer.h` for an explanation of this behavior.
    switch (examine) {
      case ExamineResult::Exclude: {
        continue;
      }
      case ExamineResult::PassThrough: {
        nsIFrame* candidate = FindAnchorIn(child);
        if (!candidate) {
          continue;
        }
        return candidate;
      }
      case ExamineResult::Traverse: {
        nsIFrame* candidate = FindAnchorIn(child);
        if (!candidate) {
          return child;
        }
        return candidate;
      }
      case ExamineResult::Accept: {
        return child;
      }
    }
  }
  return nullptr;
}

}  // namespace mozilla::layout
