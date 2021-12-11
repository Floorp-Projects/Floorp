/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextOverflow.h"
#include <algorithm>

// Please maintain alphabetical order below
#include "gfxContext.h"
#include "nsBlockFrame.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsFontMetrics.h"
#include "nsGfxScrollFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsTextFrame.h"
#include "nsIFrameInlines.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Likely.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Selection.h"
#include "TextDrawTarget.h"

using mozilla::layout::TextDrawTarget;

namespace mozilla {
namespace css {

class LazyReferenceRenderingDrawTargetGetterFromFrame final
    : public gfxFontGroup::LazyReferenceDrawTargetGetter {
 public:
  typedef mozilla::gfx::DrawTarget DrawTarget;

  explicit LazyReferenceRenderingDrawTargetGetterFromFrame(nsIFrame* aFrame)
      : mFrame(aFrame) {}
  virtual already_AddRefed<DrawTarget> GetRefDrawTarget() override {
    RefPtr<gfxContext> ctx =
        mFrame->PresShell()->CreateReferenceRenderingContext();
    RefPtr<DrawTarget> dt = ctx->GetDrawTarget();
    return dt.forget();
  }

 private:
  nsIFrame* mFrame;
};

static gfxTextRun* GetEllipsisTextRun(nsIFrame* aFrame) {
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(aFrame);
  LazyReferenceRenderingDrawTargetGetterFromFrame lazyRefDrawTargetGetter(
      aFrame);
  return fm->GetThebesFontGroup()->GetEllipsisTextRun(
      aFrame->PresContext()->AppUnitsPerDevPixel(),
      nsLayoutUtils::GetTextRunOrientFlagsForStyle(aFrame->Style()),
      lazyRefDrawTargetGetter);
}

static nsIFrame* GetSelfOrNearestBlock(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame);
  return aFrame->IsBlockFrameOrSubclass()
             ? aFrame
             : nsLayoutUtils::FindNearestBlockAncestor(aFrame);
}

// Return true if the frame is an atomic inline-level element.
// It's not supposed to be called for block frames since we never
// process block descendants for text-overflow.
static bool IsAtomicElement(nsIFrame* aFrame, LayoutFrameType aFrameType) {
  MOZ_ASSERT(!aFrame->IsBlockFrameOrSubclass() || !aFrame->IsBlockOutside(),
             "unexpected block frame");
  MOZ_ASSERT(aFrameType != LayoutFrameType::Placeholder,
             "unexpected placeholder frame");
  return !aFrame->IsFrameOfType(nsIFrame::eLineParticipant);
}

static bool IsFullyClipped(nsTextFrame* aFrame, nscoord aLeft, nscoord aRight,
                           nscoord* aSnappedLeft, nscoord* aSnappedRight) {
  *aSnappedLeft = aLeft;
  *aSnappedRight = aRight;
  if (aLeft <= 0 && aRight <= 0) {
    return false;
  }
  return !aFrame->MeasureCharClippedText(aLeft, aRight, aSnappedLeft,
                                         aSnappedRight);
}

static bool IsInlineAxisOverflowVisible(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame && aFrame->IsBlockFrameOrSubclass(),
             "expected a block frame");

  nsIFrame* f = aFrame;
  while (f && f->Style()->IsAnonBox() && !f->IsScrollFrame()) {
    f = f->GetParent();
  }
  if (!f) {
    return true;
  }
  auto overflow = aFrame->GetWritingMode().IsVertical()
                      ? f->StyleDisplay()->mOverflowY
                      : f->StyleDisplay()->mOverflowX;
  return overflow == StyleOverflow::Visible;
}

static void ClipMarker(const nsRect& aContentArea, const nsRect& aMarkerRect,
                       DisplayListClipState::AutoSaveRestore& aClipState) {
  nscoord rightOverflow = aMarkerRect.XMost() - aContentArea.XMost();
  nsRect markerRect = aMarkerRect;
  if (rightOverflow > 0) {
    // Marker overflows on the right side (content width < marker width).
    markerRect.width -= rightOverflow;
    aClipState.ClipContentDescendants(markerRect);
  } else {
    nscoord leftOverflow = aContentArea.x - aMarkerRect.x;
    if (leftOverflow > 0) {
      // Marker overflows on the left side
      markerRect.width -= leftOverflow;
      markerRect.x += leftOverflow;
      aClipState.ClipContentDescendants(markerRect);
    }
  }
}

static void InflateIStart(WritingMode aWM, LogicalRect* aRect, nscoord aDelta) {
  nscoord iend = aRect->IEnd(aWM);
  aRect->IStart(aWM) -= aDelta;
  aRect->ISize(aWM) = std::max(iend - aRect->IStart(aWM), 0);
}

static void InflateIEnd(WritingMode aWM, LogicalRect* aRect, nscoord aDelta) {
  aRect->ISize(aWM) = std::max(aRect->ISize(aWM) + aDelta, 0);
}

static bool IsFrameDescendantOfAny(
    nsIFrame* aChild, const TextOverflow::FrameHashtable& aSetOfFrames,
    nsIFrame* aCommonAncestor) {
  for (nsIFrame* f = aChild; f && f != aCommonAncestor;
       f = nsLayoutUtils::GetCrossDocParentFrameInProcess(f)) {
    if (aSetOfFrames.Contains(f)) {
      return true;
    }
  }
  return false;
}

class nsDisplayTextOverflowMarker final : public nsPaintedDisplayItem {
 public:
  nsDisplayTextOverflowMarker(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              const nsRect& aRect, nscoord aAscent,
                              const StyleTextOverflowSide& aStyle)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mRect(aRect),
        mStyle(aStyle),
        mAscent(aAscent) {
    MOZ_COUNT_CTOR(nsDisplayTextOverflowMarker);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayTextOverflowMarker)

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = false;
    nsRect shadowRect = nsLayoutUtils::GetTextShadowRectsUnion(mRect, mFrame);
    return mRect.Union(shadowRect);
  }

  virtual nsRect GetComponentAlphaBounds(
      nsDisplayListBuilder* aBuilder) const override {
    if (gfxPlatform::GetPlatform()->RespectsFontStyleSmoothing()) {
      // On OS X, web authors can turn off subpixel text rendering using the
      // CSS property -moz-osx-font-smoothing. If they do that, we don't need
      // to use component alpha layers for the affected text.
      if (mFrame->StyleFont()->mFont.smoothing == NS_FONT_SMOOTHING_GRAYSCALE) {
        return nsRect();
      }
    }
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  void PaintTextToContext(gfxContext* aCtx, nsPoint aOffsetFromRect);

  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  NS_DISPLAY_DECL_NAME("TextOverflow", TYPE_TEXT_OVERFLOW)
 private:
  nsRect mRect;  // in reference frame coordinates
  const StyleTextOverflowSide mStyle;
  nscoord mAscent;  // baseline for the marker text in mRect
};

static void PaintTextShadowCallback(gfxContext* aCtx, nsPoint aShadowOffset,
                                    const nscolor& aShadowColor, void* aData) {
  reinterpret_cast<nsDisplayTextOverflowMarker*>(aData)->PaintTextToContext(
      aCtx, aShadowOffset);
}

void nsDisplayTextOverflowMarker::Paint(nsDisplayListBuilder* aBuilder,
                                        gfxContext* aCtx) {
  nscolor foregroundColor =
      nsLayoutUtils::GetColor(mFrame, &nsStyleText::mWebkitTextFillColor);

  // Paint the text-shadows for the overflow marker
  nsLayoutUtils::PaintTextShadow(mFrame, aCtx, mRect,
                                 GetPaintRect(aBuilder, aCtx), foregroundColor,
                                 PaintTextShadowCallback, (void*)this);
  aCtx->SetColor(gfx::sRGBColor::FromABGR(foregroundColor));
  PaintTextToContext(aCtx, nsPoint(0, 0));
}

void nsDisplayTextOverflowMarker::PaintTextToContext(gfxContext* aCtx,
                                                     nsPoint aOffsetFromRect) {
  WritingMode wm = mFrame->GetWritingMode();
  nsPoint pt(mRect.x, mRect.y);
  if (wm.IsVertical()) {
    if (wm.IsVerticalLR()) {
      pt.x = NSToCoordFloor(
          nsLayoutUtils::GetSnappedBaselineX(mFrame, aCtx, pt.x, mAscent));
    } else {
      pt.x = NSToCoordFloor(nsLayoutUtils::GetSnappedBaselineX(
          mFrame, aCtx, pt.x + mRect.width, -mAscent));
    }
  } else {
    pt.y = NSToCoordFloor(
        nsLayoutUtils::GetSnappedBaselineY(mFrame, aCtx, pt.y, mAscent));
  }
  pt += aOffsetFromRect;

  if (mStyle.IsEllipsis()) {
    gfxTextRun* textRun = GetEllipsisTextRun(mFrame);
    if (textRun) {
      NS_ASSERTION(!textRun->IsRightToLeft(),
                   "Ellipsis textruns should always be LTR!");
      gfx::Point gfxPt(pt.x, pt.y);
      textRun->Draw(gfxTextRun::Range(textRun), gfxPt,
                    gfxTextRun::DrawParams(aCtx));
    }
  } else {
    RefPtr<nsFontMetrics> fm =
        nsLayoutUtils::GetInflatedFontMetricsForFrame(mFrame);
    NS_ConvertUTF8toUTF16 str16{mStyle.AsString().AsString()};
    nsLayoutUtils::DrawString(mFrame, *fm, aCtx, str16.get(), str16.Length(),
                              pt);
  }
}

bool nsDisplayTextOverflowMarker::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  bool snap;
  nsRect bounds = GetBounds(aDisplayListBuilder, &snap);
  if (bounds.IsEmpty()) {
    return true;
  }

  // Run the rendering algorithm to capture the glyphs and shadows
  RefPtr<TextDrawTarget> textDrawer =
      new TextDrawTarget(aBuilder, aResources, aSc, aManager, this, bounds);
  RefPtr<gfxContext> captureCtx = gfxContext::CreateOrNull(textDrawer);
  Paint(aDisplayListBuilder, captureCtx);
  textDrawer->TerminateShadows();

  return textDrawer->Finish();
}

TextOverflow::TextOverflow(nsDisplayListBuilder* aBuilder,
                           nsIFrame* aBlockFrame)
    : mContentArea(aBlockFrame->GetWritingMode(),
                   aBlockFrame->GetContentRectRelativeToSelf(),
                   aBlockFrame->GetSize()),
      mBuilder(aBuilder),
      mBlock(aBlockFrame),
      mScrollableFrame(nsLayoutUtils::GetScrollableFrameFor(aBlockFrame)),
      mBlockSize(aBlockFrame->GetSize()),
      mBlockWM(aBlockFrame->GetWritingMode()),
      mAdjustForPixelSnapping(false) {
#ifdef MOZ_XUL
  if (!mScrollableFrame) {
    auto pseudoType = aBlockFrame->Style()->GetPseudoType();
    if (pseudoType == PseudoStyleType::mozXULAnonymousBlock) {
      mScrollableFrame =
          nsLayoutUtils::GetScrollableFrameFor(aBlockFrame->GetParent());
      // nsXULScrollFrame::ClampAndSetBounds rounds to nearest pixels
      // for RTL blocks (also for overflow:hidden), so we need to move
      // the edges 1px outward in ExamineLineFrames to avoid triggering
      // a text-overflow marker in this case.
      mAdjustForPixelSnapping = mBlockWM.IsBidiRTL();
    }
  }
#endif
  mCanHaveInlineAxisScrollbar = false;
  if (mScrollableFrame) {
    auto scrollbarStyle = mBlockWM.IsVertical()
                              ? mScrollableFrame->GetScrollStyles().mVertical
                              : mScrollableFrame->GetScrollStyles().mHorizontal;
    mCanHaveInlineAxisScrollbar = scrollbarStyle != StyleOverflow::Hidden;
    if (!mAdjustForPixelSnapping) {
      // Scrolling to the end position can leave some text still overflowing due
      // to pixel snapping behaviour in our scrolling code.
      mAdjustForPixelSnapping = mCanHaveInlineAxisScrollbar;
    }
    // Use a null containerSize to convert a vector from logical to physical.
    const nsSize nullContainerSize;
    mContentArea.MoveBy(
        mBlockWM, LogicalPoint(mBlockWM, mScrollableFrame->GetScrollPosition(),
                               nullContainerSize));
  }
  StyleDirection direction = aBlockFrame->StyleVisibility()->mDirection;
  const nsStyleTextReset* style = aBlockFrame->StyleTextReset();

  const auto& textOverflow = style->mTextOverflow;
  bool shouldToggleDirection =
      textOverflow.sides_are_logical && (direction == StyleDirection::Rtl);
  const auto& leftSide =
      shouldToggleDirection ? textOverflow.second : textOverflow.first;
  const auto& rightSide =
      shouldToggleDirection ? textOverflow.first : textOverflow.second;

  if (mBlockWM.IsBidiLTR()) {
    mIStart.Init(leftSide);
    mIEnd.Init(rightSide);
  } else {
    mIStart.Init(rightSide);
    mIEnd.Init(leftSide);
  }
  // The left/right marker string is setup in ExamineLineFrames when a line
  // has overflow on that side.
}

/* static */
Maybe<TextOverflow> TextOverflow::WillProcessLines(
    nsDisplayListBuilder* aBuilder, nsIFrame* aBlockFrame) {
  // Ignore text-overflow and -webkit-line-clamp for event and frame visibility
  // processing.
  if (aBuilder->IsForEventDelivery() || aBuilder->IsForFrameVisibility() ||
      !CanHaveOverflowMarkers(aBlockFrame)) {
    return Nothing();
  }
  nsIScrollableFrame* scrollableFrame =
      nsLayoutUtils::GetScrollableFrameFor(aBlockFrame);
  if (scrollableFrame && scrollableFrame->IsTransformingByAPZ()) {
    // If the APZ is actively scrolling this, don't bother with markers.
    return Nothing();
  }
  return Some(TextOverflow(aBuilder, aBlockFrame));
}

void TextOverflow::ExamineFrameSubtree(nsIFrame* aFrame,
                                       const LogicalRect& aContentArea,
                                       const LogicalRect& aInsideMarkersArea,
                                       FrameHashtable* aFramesToHide,
                                       AlignmentEdges* aAlignmentEdges,
                                       bool* aFoundVisibleTextOrAtomic,
                                       InnerClipEdges* aClippedMarkerEdges) {
  const LayoutFrameType frameType = aFrame->Type();
  if (frameType == LayoutFrameType::Br ||
      frameType == LayoutFrameType::Placeholder) {
    return;
  }
  const bool isAtomic = IsAtomicElement(aFrame, frameType);
  if (aFrame->StyleVisibility()->IsVisible()) {
    LogicalRect childRect =
        GetLogicalScrollableOverflowRectRelativeToBlock(aFrame);
    bool overflowIStart =
        childRect.IStart(mBlockWM) < aContentArea.IStart(mBlockWM);
    bool overflowIEnd = childRect.IEnd(mBlockWM) > aContentArea.IEnd(mBlockWM);
    if (overflowIStart) {
      mIStart.mHasOverflow = true;
    }
    if (overflowIEnd) {
      mIEnd.mHasOverflow = true;
    }
    if (isAtomic && ((mIStart.mActive && overflowIStart) ||
                     (mIEnd.mActive && overflowIEnd))) {
      aFramesToHide->Insert(aFrame);
    } else if (isAtomic || frameType == LayoutFrameType::Text) {
      AnalyzeMarkerEdges(aFrame, frameType, aInsideMarkersArea, aFramesToHide,
                         aAlignmentEdges, aFoundVisibleTextOrAtomic,
                         aClippedMarkerEdges);
    }
  }
  if (isAtomic) {
    return;
  }

  for (nsIFrame* child : aFrame->PrincipalChildList()) {
    ExamineFrameSubtree(child, aContentArea, aInsideMarkersArea, aFramesToHide,
                        aAlignmentEdges, aFoundVisibleTextOrAtomic,
                        aClippedMarkerEdges);
  }
}

void TextOverflow::AnalyzeMarkerEdges(nsIFrame* aFrame,
                                      LayoutFrameType aFrameType,
                                      const LogicalRect& aInsideMarkersArea,
                                      FrameHashtable* aFramesToHide,
                                      AlignmentEdges* aAlignmentEdges,
                                      bool* aFoundVisibleTextOrAtomic,
                                      InnerClipEdges* aClippedMarkerEdges) {
  MOZ_ASSERT(aFrameType == LayoutFrameType::Text ||
             IsAtomicElement(aFrame, aFrameType));
  LogicalRect borderRect(mBlockWM,
                         nsRect(aFrame->GetOffsetTo(mBlock), aFrame->GetSize()),
                         mBlockSize);
  nscoord istartOverlap = std::max(
      aInsideMarkersArea.IStart(mBlockWM) - borderRect.IStart(mBlockWM), 0);
  nscoord iendOverlap = std::max(
      borderRect.IEnd(mBlockWM) - aInsideMarkersArea.IEnd(mBlockWM), 0);
  bool insideIStartEdge =
      aInsideMarkersArea.IStart(mBlockWM) <= borderRect.IEnd(mBlockWM);
  bool insideIEndEdge =
      borderRect.IStart(mBlockWM) <= aInsideMarkersArea.IEnd(mBlockWM);

  if (istartOverlap > 0) {
    aClippedMarkerEdges->AccumulateIStart(mBlockWM, borderRect);
    if (!mIStart.mActive) {
      istartOverlap = 0;
    }
  }
  if (iendOverlap > 0) {
    aClippedMarkerEdges->AccumulateIEnd(mBlockWM, borderRect);
    if (!mIEnd.mActive) {
      iendOverlap = 0;
    }
  }

  if ((istartOverlap > 0 && insideIStartEdge) ||
      (iendOverlap > 0 && insideIEndEdge)) {
    if (aFrameType == LayoutFrameType::Text) {
      auto textFrame = static_cast<nsTextFrame*>(aFrame);
      if ((aInsideMarkersArea.IStart(mBlockWM) <
           aInsideMarkersArea.IEnd(mBlockWM)) &&
          textFrame->HasNonSuppressedText()) {
        // a clipped text frame and there is some room between the markers
        nscoord snappedIStart, snappedIEnd;
        bool isFullyClipped =
            mBlockWM.IsBidiLTR()
                ? IsFullyClipped(textFrame, istartOverlap, iendOverlap,
                                 &snappedIStart, &snappedIEnd)
                : IsFullyClipped(textFrame, iendOverlap, istartOverlap,
                                 &snappedIEnd, &snappedIStart);
        if (!isFullyClipped) {
          LogicalRect snappedRect = borderRect;
          if (istartOverlap > 0) {
            snappedRect.IStart(mBlockWM) += snappedIStart;
            snappedRect.ISize(mBlockWM) -= snappedIStart;
          }
          if (iendOverlap > 0) {
            snappedRect.ISize(mBlockWM) -= snappedIEnd;
          }
          aAlignmentEdges->AccumulateInner(mBlockWM, snappedRect);
          *aFoundVisibleTextOrAtomic = true;
        }
      }
    } else {
      aFramesToHide->Insert(aFrame);
    }
  } else if (!insideIStartEdge || !insideIEndEdge) {
    // frame is outside
    if (!insideIStartEdge) {
      aAlignmentEdges->AccumulateOuter(mBlockWM, borderRect);
    }
    if (IsAtomicElement(aFrame, aFrameType)) {
      aFramesToHide->Insert(aFrame);
    }
  } else {
    // frame is inside
    aAlignmentEdges->AccumulateInner(mBlockWM, borderRect);
    if (aFrameType == LayoutFrameType::Text) {
      auto textFrame = static_cast<nsTextFrame*>(aFrame);
      if (textFrame->HasNonSuppressedText()) {
        *aFoundVisibleTextOrAtomic = true;
      }
    } else {
      *aFoundVisibleTextOrAtomic = true;
    }
  }
}

LogicalRect TextOverflow::ExamineLineFrames(nsLineBox* aLine,
                                            FrameHashtable* aFramesToHide,
                                            AlignmentEdges* aAlignmentEdges) {
  // No ellipsing for 'clip' style.
  bool suppressIStart = mIStart.IsSuppressed();
  bool suppressIEnd = mIEnd.IsSuppressed();
  if (mCanHaveInlineAxisScrollbar) {
    LogicalPoint pos(mBlockWM, mScrollableFrame->GetScrollPosition(),
                     mBlockSize);
    LogicalRect scrollRange(mBlockWM, mScrollableFrame->GetScrollRange(),
                            mBlockSize);
    // No ellipsing when nothing to scroll to on that side (this includes
    // overflow:auto that doesn't trigger a horizontal scrollbar).
    if (pos.I(mBlockWM) <= scrollRange.IStart(mBlockWM)) {
      suppressIStart = true;
    }
    if (pos.I(mBlockWM) >= scrollRange.IEnd(mBlockWM)) {
      // Except that we always want to display a -webkit-line-clamp ellipsis.
      if (!mIEnd.mHasBlockEllipsis) {
        suppressIEnd = true;
      }
    }
  }

  LogicalRect contentArea = mContentArea;
  bool snapStart = true, snapEnd = true;
  nscoord startEdge, endEdge;
  if (aLine->GetFloatEdges(&startEdge, &endEdge)) {
    // Narrow the |contentArea| to account for any floats on this line, and
    // don't bother with the snapping quirk on whichever side(s) we narrow.
    nscoord delta = endEdge - contentArea.IEnd(mBlockWM);
    if (delta < 0) {
      nscoord newSize = contentArea.ISize(mBlockWM) + delta;
      contentArea.ISize(mBlockWM) = std::max(nscoord(0), newSize);
      snapEnd = false;
    }
    delta = startEdge - contentArea.IStart(mBlockWM);
    if (delta > 0) {
      contentArea.IStart(mBlockWM) = startEdge;
      nscoord newSize = contentArea.ISize(mBlockWM) - delta;
      contentArea.ISize(mBlockWM) = std::max(nscoord(0), newSize);
      snapStart = false;
    }
  }
  // Save the non-snapped area since that's what we want to use when placing
  // the markers (our return value).  The snapped area is only for analysis.
  LogicalRect nonSnappedContentArea = contentArea;
  if (mAdjustForPixelSnapping) {
    const nscoord scrollAdjust = mBlock->PresContext()->AppUnitsPerDevPixel();
    if (snapStart) {
      InflateIStart(mBlockWM, &contentArea, scrollAdjust);
    }
    if (snapEnd) {
      InflateIEnd(mBlockWM, &contentArea, scrollAdjust);
    }
  }

  LogicalRect lineRect(mBlockWM, aLine->ScrollableOverflowRect(), mBlockSize);
  const bool istartWantsMarker =
      !suppressIStart &&
      lineRect.IStart(mBlockWM) < contentArea.IStart(mBlockWM);
  const bool iendWantsTextOverflowMarker =
      !suppressIEnd && lineRect.IEnd(mBlockWM) > contentArea.IEnd(mBlockWM);
  const bool iendWantsBlockEllipsisMarker =
      !suppressIEnd && mIEnd.mHasBlockEllipsis;
  const bool iendWantsMarker =
      iendWantsTextOverflowMarker || iendWantsBlockEllipsisMarker;
  if (!istartWantsMarker && !iendWantsMarker) {
    // We don't need any markers on this line.
    return nonSnappedContentArea;
  }

  int pass = 0;
  bool retryEmptyLine = true;
  bool guessIStart = istartWantsMarker;
  bool guessIEnd = iendWantsMarker;
  mIStart.mActive = istartWantsMarker;
  mIEnd.mActive = iendWantsMarker;
  mIStart.mEdgeAligned = mCanHaveInlineAxisScrollbar && istartWantsMarker;
  mIEnd.mEdgeAligned =
      mCanHaveInlineAxisScrollbar && iendWantsTextOverflowMarker;
  bool clippedIStartMarker = false;
  bool clippedIEndMarker = false;
  do {
    // Setup marker strings as needed.
    if (guessIStart) {
      mIStart.SetupString(mBlock);
    }
    if (guessIEnd) {
      mIEnd.SetupString(mBlock);
    }

    // If there is insufficient space for both markers then keep the one on the
    // end side per the block's 'direction'.
    nscoord istartMarkerISize = mIStart.mActive ? mIStart.mISize : 0;
    nscoord iendMarkerISize = mIEnd.mActive ? mIEnd.mISize : 0;
    if (istartMarkerISize && iendMarkerISize &&
        istartMarkerISize + iendMarkerISize > contentArea.ISize(mBlockWM)) {
      istartMarkerISize = 0;
    }

    // Calculate the area between the potential markers aligned at the
    // block's edge.
    LogicalRect insideMarkersArea = nonSnappedContentArea;
    if (guessIStart) {
      InflateIStart(mBlockWM, &insideMarkersArea, -istartMarkerISize);
    }
    if (guessIEnd) {
      InflateIEnd(mBlockWM, &insideMarkersArea, -iendMarkerISize);
    }

    // Analyze the frames on aLine for the overflow situation at the content
    // edges and at the edges of the area between the markers.
    bool foundVisibleTextOrAtomic = false;
    int32_t n = aLine->GetChildCount();
    nsIFrame* child = aLine->mFirstChild;
    InnerClipEdges clippedMarkerEdges;
    for (; n-- > 0; child = child->GetNextSibling()) {
      ExamineFrameSubtree(child, contentArea, insideMarkersArea, aFramesToHide,
                          aAlignmentEdges, &foundVisibleTextOrAtomic,
                          &clippedMarkerEdges);
    }
    if (!foundVisibleTextOrAtomic && retryEmptyLine) {
      aAlignmentEdges->mAssignedInner = false;
      aAlignmentEdges->mIEndOuter = 0;
      aFramesToHide->Clear();
      pass = -1;
      if (mIStart.IsNeeded() && mIStart.mActive && !clippedIStartMarker) {
        if (clippedMarkerEdges.mAssignedIStart &&
            clippedMarkerEdges.mIStart >
                nonSnappedContentArea.IStart(mBlockWM)) {
          mIStart.mISize = clippedMarkerEdges.mIStart -
                           nonSnappedContentArea.IStart(mBlockWM);
          NS_ASSERTION(mIStart.mISize < mIStart.mIntrinsicISize,
                       "clipping a marker should make it strictly smaller");
          clippedIStartMarker = true;
        } else {
          mIStart.mActive = guessIStart = false;
        }
        continue;
      }
      if (mIEnd.IsNeeded() && mIEnd.mActive && !clippedIEndMarker) {
        if (clippedMarkerEdges.mAssignedIEnd &&
            nonSnappedContentArea.IEnd(mBlockWM) > clippedMarkerEdges.mIEnd) {
          mIEnd.mISize =
              nonSnappedContentArea.IEnd(mBlockWM) - clippedMarkerEdges.mIEnd;
          NS_ASSERTION(mIEnd.mISize < mIEnd.mIntrinsicISize,
                       "clipping a marker should make it strictly smaller");
          clippedIEndMarker = true;
        } else {
          mIEnd.mActive = guessIEnd = false;
        }
        continue;
      }
      // The line simply has no visible content even without markers,
      // so examine the line again without suppressing markers.
      retryEmptyLine = false;
      mIStart.mISize = mIStart.mIntrinsicISize;
      mIStart.mActive = guessIStart = istartWantsMarker;
      mIEnd.mISize = mIEnd.mIntrinsicISize;
      mIEnd.mActive = guessIEnd = iendWantsMarker;
      // If we wanted to place a block ellipsis but didn't, due to not having
      // any visible content to align to or the line's content being scrolled
      // out of view, then clip the ellipsis so that it looks like it is aligned
      // with the out of view content.
      if (mIEnd.IsNeeded() && mIEnd.mActive && mIEnd.mHasBlockEllipsis) {
        NS_ASSERTION(nonSnappedContentArea.IStart(mBlockWM) >
                         aAlignmentEdges->mIEndOuter,
                     "Expected the alignment edge for the out of view content "
                     "to be before the start of the content area");
        mIEnd.mISize = std::max(
            mIEnd.mIntrinsicISize - (nonSnappedContentArea.IStart(mBlockWM) -
                                     aAlignmentEdges->mIEndOuter),
            0);
      }
      continue;
    }
    if (guessIStart == (mIStart.mActive && mIStart.IsNeeded()) &&
        guessIEnd == (mIEnd.mActive && mIEnd.IsNeeded())) {
      break;
    } else {
      guessIStart = mIStart.mActive && mIStart.IsNeeded();
      guessIEnd = mIEnd.mActive && mIEnd.IsNeeded();
      mIStart.Reset();
      mIEnd.Reset();
      aFramesToHide->Clear();
    }
    NS_ASSERTION(pass == 0, "2nd pass should never guess wrong");
  } while (++pass != 2);
  if (!istartWantsMarker || !mIStart.mActive) {
    mIStart.Reset();
  }
  if (!iendWantsMarker || !mIEnd.mActive) {
    mIEnd.Reset();
  }
  return nonSnappedContentArea;
}

void TextOverflow::ProcessLine(const nsDisplayListSet& aLists, nsLineBox* aLine,
                               uint32_t aLineNumber) {
  if (mIStart.mStyle->IsClip() && mIEnd.mStyle->IsClip() &&
      !aLine->HasLineClampEllipsis()) {
    return;
  }

  mIStart.Reset();
  mIStart.mActive = !mIStart.mStyle->IsClip();
  mIEnd.Reset();
  mIEnd.mHasBlockEllipsis = aLine->HasLineClampEllipsis();
  mIEnd.mActive = !mIEnd.mStyle->IsClip() || aLine->HasLineClampEllipsis();

  FrameHashtable framesToHide(64);
  AlignmentEdges alignmentEdges;
  const LogicalRect contentArea =
      ExamineLineFrames(aLine, &framesToHide, &alignmentEdges);
  bool needIStart = mIStart.IsNeeded();
  bool needIEnd = mIEnd.IsNeeded();
  if (!needIStart && !needIEnd) {
    return;
  }
  NS_ASSERTION(!mIStart.IsSuppressed() || !needIStart,
               "left marker when not needed");
  NS_ASSERTION(!mIEnd.IsSuppressed() || !needIEnd,
               "right marker when not needed");

  // If there is insufficient space for both markers then keep the one on the
  // end side per the block's 'direction'.
  if (needIStart && needIEnd &&
      mIStart.mISize + mIEnd.mISize > contentArea.ISize(mBlockWM)) {
    needIStart = false;
  }
  LogicalRect insideMarkersArea = contentArea;
  if (needIStart) {
    InflateIStart(mBlockWM, &insideMarkersArea, -mIStart.mISize);
  }
  if (needIEnd) {
    InflateIEnd(mBlockWM, &insideMarkersArea, -mIEnd.mISize);
  }

  if (alignmentEdges.mAssignedInner) {
    if (mIStart.mEdgeAligned) {
      alignmentEdges.mIStart = insideMarkersArea.IStart(mBlockWM);
    }
    if (mIEnd.mEdgeAligned) {
      alignmentEdges.mIEnd = insideMarkersArea.IEnd(mBlockWM);
    }
    LogicalRect alignmentRect(mBlockWM, alignmentEdges.mIStart,
                              insideMarkersArea.BStart(mBlockWM),
                              alignmentEdges.ISize(), 1);
    insideMarkersArea.IntersectRect(insideMarkersArea, alignmentRect);
  } else {
    // There was no content on the line that was visible at the current scolled
    // position.  If we wanted to place a block ellipsis but failed due to
    // having no visible content to align it to, we still need to ensure it
    // is displayed.  It goes at the start of the line, even though it's an
    // IEnd marker, since that is the side of the line that the content has
    // been scrolled past.  We set the insideMarkersArea to a zero-sized
    // rectangle placed next to the scrolled-out-of-view content.
    if (mIEnd.mHasBlockEllipsis) {
      insideMarkersArea = LogicalRect(mBlockWM, alignmentEdges.mIEndOuter,
                                      insideMarkersArea.BStart(mBlockWM), 0, 1);
    }
  }

  // Clip and remove display items as needed at the final marker edges.
  nsDisplayList* lists[] = {aLists.Content(), aLists.PositionedDescendants()};
  for (uint32_t i = 0; i < ArrayLength(lists); ++i) {
    PruneDisplayListContents(lists[i], framesToHide, insideMarkersArea);
  }
  CreateMarkers(aLine, needIStart, needIEnd, insideMarkersArea, contentArea,
                aLineNumber);
}

void TextOverflow::PruneDisplayListContents(
    nsDisplayList* aList, const FrameHashtable& aFramesToHide,
    const LogicalRect& aInsideMarkersArea) {
  nsDisplayList saved;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    nsIFrame* itemFrame = item->Frame();
    if (IsFrameDescendantOfAny(itemFrame, aFramesToHide, mBlock)) {
      item->Destroy(mBuilder);
      continue;
    }

    nsDisplayList* wrapper = item->GetSameCoordinateSystemChildren();
    if (wrapper) {
      if (!itemFrame || GetSelfOrNearestBlock(itemFrame) == mBlock) {
        PruneDisplayListContents(wrapper, aFramesToHide, aInsideMarkersArea);
      }
    }

    nsDisplayText* textItem =
        itemFrame ? nsDisplayText::CheckCast(item) : nullptr;
    if (textItem && GetSelfOrNearestBlock(itemFrame) == mBlock) {
      LogicalRect rect =
          GetLogicalScrollableOverflowRectRelativeToBlock(itemFrame);
      if (mIStart.IsNeeded()) {
        nscoord istart =
            aInsideMarkersArea.IStart(mBlockWM) - rect.IStart(mBlockWM);
        if (istart > 0) {
          (mBlockWM.IsBidiLTR() ? textItem->VisIStartEdge()
                                : textItem->VisIEndEdge()) = istart;
        }
      }
      if (mIEnd.IsNeeded()) {
        nscoord iend = rect.IEnd(mBlockWM) - aInsideMarkersArea.IEnd(mBlockWM);
        if (iend > 0) {
          (mBlockWM.IsBidiLTR() ? textItem->VisIEndEdge()
                                : textItem->VisIStartEdge()) = iend;
        }
      }
    }

    saved.AppendToTop(item);
  }
  aList->AppendToTop(&saved);
}

/* static */
bool TextOverflow::HasClippedTextOverflow(nsIFrame* aBlockFrame) {
  const nsStyleTextReset* style = aBlockFrame->StyleTextReset();
  return style->mTextOverflow.first.IsClip() &&
         style->mTextOverflow.second.IsClip();
}

/* static */
bool TextOverflow::HasBlockEllipsis(nsIFrame* aBlockFrame) {
  nsBlockFrame* f = do_QueryFrame(aBlockFrame);
  return f && f->HasAnyStateBits(NS_BLOCK_HAS_LINE_CLAMP_ELLIPSIS);
}

/* static */
bool TextOverflow::CanHaveOverflowMarkers(nsIFrame* aBlockFrame) {
  // Treat a line with a -webkit-line-clamp ellipsis as a kind of text
  // overflow.
  if (aBlockFrame->HasAnyStateBits(NS_BLOCK_HAS_LINE_CLAMP_ELLIPSIS)) {
    return true;
  }

  // Nothing to do for text-overflow:clip or if 'overflow-x/y:visible'.
  if (HasClippedTextOverflow(aBlockFrame) ||
      IsInlineAxisOverflowVisible(aBlockFrame)) {
    return false;
  }

  // Skip ComboboxControlFrame because it would clip the drop-down arrow.
  // Its anon block inherits 'text-overflow' and does what is expected.
  if (aBlockFrame->IsComboboxControlFrame()) {
    return false;
  }

  // Inhibit the markers if a descendant content owns the caret.
  RefPtr<nsCaret> caret = aBlockFrame->PresShell()->GetCaret();
  if (caret && caret->IsVisible()) {
    RefPtr<dom::Selection> domSelection = caret->GetSelection();
    if (domSelection) {
      nsCOMPtr<nsIContent> content =
          nsIContent::FromNodeOrNull(domSelection->GetFocusNode());
      if (content &&
          content->IsInclusiveDescendantOf(aBlockFrame->GetContent())) {
        return false;
      }
    }
  }
  return true;
}

void TextOverflow::CreateMarkers(const nsLineBox* aLine, bool aCreateIStart,
                                 bool aCreateIEnd,
                                 const LogicalRect& aInsideMarkersArea,
                                 const LogicalRect& aContentArea,
                                 uint32_t aLineNumber) {
  if (!mBlock->IsVisibleForPainting()) {
    return;
  }

  if (aCreateIStart) {
    DisplayListClipState::AutoSaveRestore clipState(mBuilder);

    LogicalRect markerLogicalRect(
        mBlockWM, aInsideMarkersArea.IStart(mBlockWM) - mIStart.mIntrinsicISize,
        aLine->BStart(), mIStart.mIntrinsicISize, aLine->BSize());
    nsPoint offset = mBuilder->ToReferenceFrame(mBlock);
    nsRect markerRect =
        markerLogicalRect.GetPhysicalRect(mBlockWM, mBlockSize) + offset;
    ClipMarker(aContentArea.GetPhysicalRect(mBlockWM, mBlockSize) + offset,
               markerRect, clipState);

    mMarkerList.AppendNewToTopWithIndex<nsDisplayTextOverflowMarker>(
        mBuilder, mBlock, /* aIndex = */ (aLineNumber << 1) + 0, markerRect,
        aLine->GetLogicalAscent(), *mIStart.mStyle);
  }

  if (aCreateIEnd) {
    DisplayListClipState::AutoSaveRestore clipState(mBuilder);

    LogicalRect markerLogicalRect(mBlockWM, aInsideMarkersArea.IEnd(mBlockWM),
                                  aLine->BStart(), mIEnd.mIntrinsicISize,
                                  aLine->BSize());
    nsPoint offset = mBuilder->ToReferenceFrame(mBlock);
    nsRect markerRect =
        markerLogicalRect.GetPhysicalRect(mBlockWM, mBlockSize) + offset;
    ClipMarker(aContentArea.GetPhysicalRect(mBlockWM, mBlockSize) + offset,
               markerRect, clipState);

    mMarkerList.AppendNewToTopWithIndex<nsDisplayTextOverflowMarker>(
        mBuilder, mBlock, /* aIndex = */ (aLineNumber << 1) + 1, markerRect,
        aLine->GetLogicalAscent(),
        mIEnd.mHasBlockEllipsis ? StyleTextOverflowSide::Ellipsis()
                                : *mIEnd.mStyle);
  }
}

void TextOverflow::Marker::SetupString(nsIFrame* aFrame) {
  if (mInitialized) {
    return;
  }

  // A limitation here is that at the IEnd of a line, we only ever render one of
  // a text-overflow marker and a -webkit-line-clamp block ellipsis.  Since we
  // don't track the block ellipsis string and the text-overflow marker string
  // separately, if both apply to the element, we will always use "…" as the
  // string for text-overflow.
  if (HasBlockEllipsis(aFrame) || mStyle->IsEllipsis()) {
    gfxTextRun* textRun = GetEllipsisTextRun(aFrame);
    if (textRun) {
      mISize = textRun->GetAdvanceWidth();
    } else {
      mISize = 0;
    }
  } else {
    RefPtr<gfxContext> rc =
        aFrame->PresShell()->CreateReferenceRenderingContext();
    RefPtr<nsFontMetrics> fm =
        nsLayoutUtils::GetInflatedFontMetricsForFrame(aFrame);
    mISize = nsLayoutUtils::AppUnitWidthOfStringBidi(
        NS_ConvertUTF8toUTF16(mStyle->AsString().AsString()), aFrame, *fm, *rc);
  }
  mIntrinsicISize = mISize;
  mInitialized = true;
}

}  // namespace css
}  // namespace mozilla
