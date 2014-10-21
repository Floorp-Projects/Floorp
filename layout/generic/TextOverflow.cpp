/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextOverflow.h"
#include <algorithm>

// Please maintain alphabetical order below
#include "nsBlockFrame.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsGfxScrollFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsRenderingContext.h"
#include "nsTextFrame.h"
#include "nsIFrameInlines.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Likely.h"
#include "nsISelection.h"

namespace mozilla {
namespace css {

class LazyReferenceRenderingContextGetterFromFrame MOZ_FINAL :
    public gfxFontGroup::LazyReferenceContextGetter {
public:
  explicit LazyReferenceRenderingContextGetterFromFrame(nsIFrame* aFrame)
    : mFrame(aFrame) {}
  virtual already_AddRefed<gfxContext> GetRefContext() MOZ_OVERRIDE
  {
    nsRefPtr<nsRenderingContext> rc =
      mFrame->PresContext()->PresShell()->CreateReferenceRenderingContext();
    nsRefPtr<gfxContext> ctx = rc->ThebesContext();
    return ctx.forget();
  }
private:
  nsIFrame* mFrame;
};

static gfxTextRun*
GetEllipsisTextRun(nsIFrame* aFrame)
{
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(aFrame));
  LazyReferenceRenderingContextGetterFromFrame lazyRefContextGetter(aFrame);
  return fm->GetThebesFontGroup()->GetEllipsisTextRun(
      aFrame->PresContext()->AppUnitsPerDevPixel(), lazyRefContextGetter);
}

static nsIFrame*
GetSelfOrNearestBlock(nsIFrame* aFrame)
{
  return nsLayoutUtils::GetAsBlock(aFrame) ? aFrame :
         nsLayoutUtils::FindNearestBlockAncestor(aFrame);
}

// Return true if the frame is an atomic inline-level element.
// It's not supposed to be called for block frames since we never
// process block descendants for text-overflow.
static bool
IsAtomicElement(nsIFrame* aFrame, const nsIAtom* aFrameType)
{
  NS_PRECONDITION(!nsLayoutUtils::GetAsBlock(aFrame) ||
                  !aFrame->IsBlockOutside(),
                  "unexpected block frame");
  NS_PRECONDITION(aFrameType != nsGkAtoms::placeholderFrame,
                  "unexpected placeholder frame");
  return !aFrame->IsFrameOfType(nsIFrame::eLineParticipant);
}

static bool
IsFullyClipped(nsTextFrame* aFrame, nscoord aLeft, nscoord aRight,
               nscoord* aSnappedLeft, nscoord* aSnappedRight)
{
  *aSnappedLeft = aLeft;
  *aSnappedRight = aRight;
  if (aLeft <= 0 && aRight <= 0) {
    return false;
  }
  return !aFrame->MeasureCharClippedText(aLeft, aRight,
                                         aSnappedLeft, aSnappedRight);
}

static bool
IsHorizontalOverflowVisible(nsIFrame* aFrame)
{
  NS_PRECONDITION(nsLayoutUtils::GetAsBlock(aFrame) != nullptr,
                  "expected a block frame");

  nsIFrame* f = aFrame;
  while (f && f->StyleContext()->GetPseudo() &&
         f->GetType() != nsGkAtoms::scrollFrame) {
    f = f->GetParent();
  }
  return !f || f->StyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE;
}

static void
ClipMarker(const nsRect&                          aContentArea,
           const nsRect&                          aMarkerRect,
           DisplayListClipState::AutoSaveRestore& aClipState)
{
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

static void
InflateLeft(nsRect* aRect, nscoord aDelta)
{
  nscoord xmost = aRect->XMost();
  aRect->x -= aDelta;
  aRect->width = std::max(xmost - aRect->x, 0);
}

static void
InflateRight(nsRect* aRect, nscoord aDelta)
{
  aRect->width = std::max(aRect->width + aDelta, 0);
}

static bool
IsFrameDescendantOfAny(nsIFrame* aChild,
                       const TextOverflow::FrameHashtable& aSetOfFrames,
                       nsIFrame* aCommonAncestor)
{
  for (nsIFrame* f = aChild; f && f != aCommonAncestor;
       f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    if (aSetOfFrames.GetEntry(f)) {
      return true;
    }
  }
  return false;
}

class nsDisplayTextOverflowMarker : public nsDisplayItem
{
public:
  nsDisplayTextOverflowMarker(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              const nsRect& aRect, nscoord aAscent,
                              const nsStyleTextOverflowSide* aStyle,
                              uint32_t aIndex)
    : nsDisplayItem(aBuilder, aFrame), mRect(aRect),
      mStyle(aStyle), mAscent(aAscent), mIndex(aIndex) {
    MOZ_COUNT_CTOR(nsDisplayTextOverflowMarker);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTextOverflowMarker() {
    MOZ_COUNT_DTOR(nsDisplayTextOverflowMarker);
  }
#endif
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) MOZ_OVERRIDE {
    *aSnap = false;
    nsRect shadowRect =
      nsLayoutUtils::GetTextShadowRectsUnion(mRect, mFrame);
    return mRect.Union(shadowRect);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;

  virtual uint32_t GetPerFrameKey() MOZ_OVERRIDE { 
    return (mIndex << nsDisplayItem::TYPE_BITS) | nsDisplayItem::GetPerFrameKey(); 
  }
  void PaintTextToContext(nsRenderingContext* aCtx,
                          nsPoint aOffsetFromRect);
  NS_DISPLAY_DECL_NAME("TextOverflow", TYPE_TEXT_OVERFLOW)
private:
  nsRect          mRect;   // in reference frame coordinates
  const nsStyleTextOverflowSide* mStyle;
  nscoord         mAscent; // baseline for the marker text in mRect
  uint32_t        mIndex;
};

static void
PaintTextShadowCallback(nsRenderingContext* aCtx,
                        nsPoint aShadowOffset,
                        const nscolor& aShadowColor,
                        void* aData)
{
  reinterpret_cast<nsDisplayTextOverflowMarker*>(aData)->
           PaintTextToContext(aCtx, aShadowOffset);
}

void
nsDisplayTextOverflowMarker::Paint(nsDisplayListBuilder* aBuilder,
                                   nsRenderingContext*   aCtx)
{
  nscolor foregroundColor =
    nsLayoutUtils::GetColor(mFrame, eCSSProperty_color);

  // Paint the text-shadows for the overflow marker
  nsLayoutUtils::PaintTextShadow(mFrame, aCtx, mRect, mVisibleRect,
                                 foregroundColor, PaintTextShadowCallback,
                                 (void*)this);
  aCtx->ThebesContext()->SetColor(foregroundColor);
  PaintTextToContext(aCtx, nsPoint(0, 0));
}

void
nsDisplayTextOverflowMarker::PaintTextToContext(nsRenderingContext* aCtx,
                                                nsPoint aOffsetFromRect)
{
  gfxFloat y = nsLayoutUtils::GetSnappedBaselineY(mFrame, aCtx->ThebesContext(),
                                                  mRect.y, mAscent);
  nsPoint baselinePt(mRect.x, NSToCoordFloor(y));
  nsPoint pt = baselinePt + aOffsetFromRect;

  if (mStyle->mType == NS_STYLE_TEXT_OVERFLOW_ELLIPSIS) {
    gfxTextRun* textRun = GetEllipsisTextRun(mFrame);
    if (textRun) {
      NS_ASSERTION(!textRun->IsRightToLeft(),
                   "Ellipsis textruns should always be LTR!");
      gfxPoint gfxPt(pt.x, pt.y);
      textRun->Draw(aCtx->ThebesContext(), gfxPt, DrawMode::GLYPH_FILL,
                    0, textRun->GetLength(), nullptr, nullptr, nullptr);
    }
  } else {
    nsRefPtr<nsFontMetrics> fm;
    nsLayoutUtils::GetFontMetricsForFrame(mFrame, getter_AddRefs(fm),
      nsLayoutUtils::FontSizeInflationFor(mFrame));
    aCtx->SetFont(fm);
    nsLayoutUtils::DrawString(mFrame, aCtx, mStyle->mString.get(),
                              mStyle->mString.Length(), pt);
  }
}

void
TextOverflow::Init(nsDisplayListBuilder*   aBuilder,
                   nsIFrame*               aBlockFrame)
{
  mBuilder = aBuilder;
  mBlock = aBlockFrame;
  mContentArea = aBlockFrame->GetContentRectRelativeToSelf();
  mScrollableFrame = nsLayoutUtils::GetScrollableFrameFor(aBlockFrame);
  uint8_t direction = aBlockFrame->StyleVisibility()->mDirection;
  mBlockIsRTL = direction == NS_STYLE_DIRECTION_RTL;
  mAdjustForPixelSnapping = false;
#ifdef MOZ_XUL
  if (!mScrollableFrame) {
    nsIAtom* pseudoType = aBlockFrame->StyleContext()->GetPseudo();
    if (pseudoType == nsCSSAnonBoxes::mozXULAnonymousBlock) {
      mScrollableFrame =
        nsLayoutUtils::GetScrollableFrameFor(aBlockFrame->GetParent());
      // nsXULScrollFrame::ClampAndSetBounds rounds to nearest pixels
      // for RTL blocks (also for overflow:hidden), so we need to move
      // the edges 1px outward in ExamineLineFrames to avoid triggering
      // a text-overflow marker in this case.
      mAdjustForPixelSnapping = mBlockIsRTL;
    }
  }
#endif
  mCanHaveHorizontalScrollbar = false;
  if (mScrollableFrame) {
    mCanHaveHorizontalScrollbar =
      mScrollableFrame->GetScrollbarStyles().mHorizontal != NS_STYLE_OVERFLOW_HIDDEN;
    if (!mAdjustForPixelSnapping) {
      // Scrolling to the end position can leave some text still overflowing due
      // to pixel snapping behaviour in our scrolling code.
      mAdjustForPixelSnapping = mCanHaveHorizontalScrollbar;
    }
    mContentArea.MoveBy(mScrollableFrame->GetScrollPosition());
    nsIFrame* scrollFrame = do_QueryFrame(mScrollableFrame);
    scrollFrame->AddStateBits(NS_SCROLLFRAME_INVALIDATE_CONTENTS_ON_SCROLL);
  }
  const nsStyleTextReset* style = aBlockFrame->StyleTextReset();
  mLeft.Init(style->mTextOverflow.GetLeft(direction));
  mRight.Init(style->mTextOverflow.GetRight(direction));
  // The left/right marker string is setup in ExamineLineFrames when a line
  // has overflow on that side.
}

/* static */ TextOverflow*
TextOverflow::WillProcessLines(nsDisplayListBuilder*   aBuilder,
                               nsIFrame*               aBlockFrame)
{
  if (!CanHaveTextOverflow(aBuilder, aBlockFrame)) {
    return nullptr;
  }
  nsAutoPtr<TextOverflow> textOverflow(new TextOverflow);
  textOverflow->Init(aBuilder, aBlockFrame);
  return textOverflow.forget();
}

void
TextOverflow::ExamineFrameSubtree(nsIFrame*       aFrame,
                                  const nsRect&   aContentArea,
                                  const nsRect&   aInsideMarkersArea,
                                  FrameHashtable* aFramesToHide,
                                  AlignmentEdges* aAlignmentEdges,
                                  bool*           aFoundVisibleTextOrAtomic,
                                  InnerClipEdges* aClippedMarkerEdges)
{
  const nsIAtom* frameType = aFrame->GetType();
  if (frameType == nsGkAtoms::brFrame ||
      frameType == nsGkAtoms::placeholderFrame) {
    return;
  }
  const bool isAtomic = IsAtomicElement(aFrame, frameType);
  if (aFrame->StyleVisibility()->IsVisible()) {
    nsRect childRect = aFrame->GetScrollableOverflowRect() +
                       aFrame->GetOffsetTo(mBlock);
    bool overflowLeft = childRect.x < aContentArea.x;
    bool overflowRight = childRect.XMost() > aContentArea.XMost();
    if (overflowLeft) {
      mLeft.mHasOverflow = true;
    }
    if (overflowRight) {
      mRight.mHasOverflow = true;
    }
    if (isAtomic && ((mLeft.mActive && overflowLeft) ||
                     (mRight.mActive && overflowRight))) {
      aFramesToHide->PutEntry(aFrame);
    } else if (isAtomic || frameType == nsGkAtoms::textFrame) {
      AnalyzeMarkerEdges(aFrame, frameType, aInsideMarkersArea,
                         aFramesToHide, aAlignmentEdges,
                         aFoundVisibleTextOrAtomic,
                         aClippedMarkerEdges);
    }
  }
  if (isAtomic) {
    return;
  }

  nsIFrame* child = aFrame->GetFirstPrincipalChild();
  while (child) {
    ExamineFrameSubtree(child, aContentArea, aInsideMarkersArea,
                        aFramesToHide, aAlignmentEdges,
                        aFoundVisibleTextOrAtomic,
                        aClippedMarkerEdges);
    child = child->GetNextSibling();
  }
}

void
TextOverflow::AnalyzeMarkerEdges(nsIFrame*       aFrame,
                                 const nsIAtom*  aFrameType,
                                 const nsRect&   aInsideMarkersArea,
                                 FrameHashtable* aFramesToHide,
                                 AlignmentEdges* aAlignmentEdges,
                                 bool*           aFoundVisibleTextOrAtomic,
                                 InnerClipEdges* aClippedMarkerEdges)
{
  nsRect borderRect(aFrame->GetOffsetTo(mBlock), aFrame->GetSize());
  nscoord leftOverlap =
    std::max(aInsideMarkersArea.x - borderRect.x, 0);
  nscoord rightOverlap =
    std::max(borderRect.XMost() - aInsideMarkersArea.XMost(), 0);
  bool insideLeftEdge = aInsideMarkersArea.x <= borderRect.XMost();
  bool insideRightEdge = borderRect.x <= aInsideMarkersArea.XMost();

  if (leftOverlap > 0) {
    aClippedMarkerEdges->AccumulateLeft(borderRect);
    if (!mLeft.mActive) {
      leftOverlap = 0;
    }
  }
  if (rightOverlap > 0) {
    aClippedMarkerEdges->AccumulateRight(borderRect);
    if (!mRight.mActive) {
      rightOverlap = 0;
    }
  }

  if ((leftOverlap > 0 && insideLeftEdge) ||
      (rightOverlap > 0 && insideRightEdge)) {
    if (aFrameType == nsGkAtoms::textFrame) {
      if (aInsideMarkersArea.x < aInsideMarkersArea.XMost()) {
        // a clipped text frame and there is some room between the markers
        nscoord snappedLeft, snappedRight;
        bool isFullyClipped =
          IsFullyClipped(static_cast<nsTextFrame*>(aFrame),
                         leftOverlap, rightOverlap, &snappedLeft, &snappedRight);
        if (!isFullyClipped) {
          nsRect snappedRect = borderRect;
          if (leftOverlap > 0) {
            snappedRect.x += snappedLeft;
            snappedRect.width -= snappedLeft;
          }
          if (rightOverlap > 0) {
            snappedRect.width -= snappedRight;
          }
          aAlignmentEdges->Accumulate(snappedRect);
          *aFoundVisibleTextOrAtomic = true;
        }
      }
    } else {
      aFramesToHide->PutEntry(aFrame);
    }
  } else if (!insideLeftEdge || !insideRightEdge) {
    // frame is outside
    if (IsAtomicElement(aFrame, aFrameType)) {
      aFramesToHide->PutEntry(aFrame);
    }
  } else {
    // frame is inside
    aAlignmentEdges->Accumulate(borderRect);
    *aFoundVisibleTextOrAtomic = true;
  }
}

void
TextOverflow::ExamineLineFrames(nsLineBox*      aLine,
                                FrameHashtable* aFramesToHide,
                                AlignmentEdges* aAlignmentEdges)
{
  // No ellipsing for 'clip' style.
  bool suppressLeft = mLeft.mStyle->mType == NS_STYLE_TEXT_OVERFLOW_CLIP;
  bool suppressRight = mRight.mStyle->mType == NS_STYLE_TEXT_OVERFLOW_CLIP;
  if (mCanHaveHorizontalScrollbar) {
    nsPoint pos = mScrollableFrame->GetScrollPosition();
    nsRect scrollRange = mScrollableFrame->GetScrollRange();
    // No ellipsing when nothing to scroll to on that side (this includes
    // overflow:auto that doesn't trigger a horizontal scrollbar).
    if (pos.x <= scrollRange.x) {
      suppressLeft = true;
    }
    if (pos.x >= scrollRange.XMost()) {
      suppressRight = true;
    }
  }

  nsRect contentArea = mContentArea;
  const nscoord scrollAdjust = mAdjustForPixelSnapping ?
    mBlock->PresContext()->AppUnitsPerDevPixel() : 0;
  InflateLeft(&contentArea, scrollAdjust);
  InflateRight(&contentArea, scrollAdjust);
  nsRect lineRect = aLine->GetScrollableOverflowArea();
  const bool leftOverflow =
    !suppressLeft && lineRect.x < contentArea.x;
  const bool rightOverflow =
    !suppressRight && lineRect.XMost() > contentArea.XMost();
  if (!leftOverflow && !rightOverflow) {
    // The line does not overflow on a side we should ellipsize.
    return;
  }

  int pass = 0;
  bool retryEmptyLine = true;
  bool guessLeft = leftOverflow;
  bool guessRight = rightOverflow;
  mLeft.mActive = leftOverflow;
  mRight.mActive = rightOverflow;
  bool clippedLeftMarker = false;
  bool clippedRightMarker = false;
  do {
    // Setup marker strings as needed.
    if (guessLeft) {
      mLeft.SetupString(mBlock);
    }
    if (guessRight) {
      mRight.SetupString(mBlock);
    }
    
    // If there is insufficient space for both markers then keep the one on the
    // end side per the block's 'direction'.
    nscoord rightMarkerWidth = mRight.mActive ? mRight.mWidth : 0;
    nscoord leftMarkerWidth = mLeft.mActive ? mLeft.mWidth : 0;
    if (leftMarkerWidth && rightMarkerWidth &&
        leftMarkerWidth + rightMarkerWidth > contentArea.width) {
      if (mBlockIsRTL) {
        rightMarkerWidth = 0;
      } else {
        leftMarkerWidth = 0;
      }
    }

    // Calculate the area between the potential markers aligned at the
    // block's edge.
    nsRect insideMarkersArea = mContentArea;
    if (guessLeft) {
      InflateLeft(&insideMarkersArea, -leftMarkerWidth);
    }
    if (guessRight) {
      InflateRight(&insideMarkersArea, -rightMarkerWidth);
    }

    // Analyze the frames on aLine for the overflow situation at the content
    // edges and at the edges of the area between the markers.
    bool foundVisibleTextOrAtomic = false;
    int32_t n = aLine->GetChildCount();
    nsIFrame* child = aLine->mFirstChild;
    InnerClipEdges clippedMarkerEdges;
    for (; n-- > 0; child = child->GetNextSibling()) {
      ExamineFrameSubtree(child, contentArea, insideMarkersArea,
                          aFramesToHide, aAlignmentEdges,
                          &foundVisibleTextOrAtomic,
                          &clippedMarkerEdges);
    }
    if (!foundVisibleTextOrAtomic && retryEmptyLine) {
      aAlignmentEdges->mAssigned = false;
      aFramesToHide->Clear();
      pass = -1;
      if (mLeft.IsNeeded() && mLeft.mActive && !clippedLeftMarker) {
        if (clippedMarkerEdges.mAssignedLeft &&
            clippedMarkerEdges.mLeft - mContentArea.X() > 0) {
          mLeft.mWidth = clippedMarkerEdges.mLeft - mContentArea.X();
          NS_ASSERTION(mLeft.mWidth < mLeft.mIntrinsicISize,
                       "clipping a marker should make it strictly smaller");
          clippedLeftMarker = true;
        } else {
          mLeft.mActive = guessLeft = false;
        }
        continue;
      }
      if (mRight.IsNeeded() && mRight.mActive && !clippedRightMarker) {
        if (clippedMarkerEdges.mAssignedRight &&
            mContentArea.XMost() - clippedMarkerEdges.mRight > 0) {
          mRight.mWidth = mContentArea.XMost() - clippedMarkerEdges.mRight;
          NS_ASSERTION(mRight.mWidth < mRight.mIntrinsicISize,
                       "clipping a marker should make it strictly smaller");
          clippedRightMarker = true;
        } else {
          mRight.mActive = guessRight = false;
        }
        continue;
      }
      // The line simply has no visible content even without markers,
      // so examine the line again without suppressing markers.
      retryEmptyLine = false;
      mLeft.mWidth = mLeft.mIntrinsicISize;
      mLeft.mActive = guessLeft = leftOverflow;
      mRight.mWidth = mRight.mIntrinsicISize;
      mRight.mActive = guessRight = rightOverflow;
      continue;
    }
    if (guessLeft == (mLeft.mActive && mLeft.IsNeeded()) &&
        guessRight == (mRight.mActive && mRight.IsNeeded())) {
      break;
    } else {
      guessLeft = mLeft.mActive && mLeft.IsNeeded();
      guessRight = mRight.mActive && mRight.IsNeeded();
      mLeft.Reset();
      mRight.Reset();
      aFramesToHide->Clear();
    }
    NS_ASSERTION(pass == 0, "2nd pass should never guess wrong");
  } while (++pass != 2);
  if (!leftOverflow || !mLeft.mActive) {
    mLeft.Reset();
  }
  if (!rightOverflow || !mRight.mActive) {
    mRight.Reset();
  }
}

void
TextOverflow::ProcessLine(const nsDisplayListSet& aLists,
                          nsLineBox*              aLine)
{
  NS_ASSERTION(mLeft.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP ||
               mRight.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP,
               "TextOverflow with 'clip' for both sides");
  mLeft.Reset();
  mLeft.mActive = mLeft.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP;
  mRight.Reset();
  mRight.mActive = mRight.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP;

  FrameHashtable framesToHide(64);
  AlignmentEdges alignmentEdges;
  ExamineLineFrames(aLine, &framesToHide, &alignmentEdges);
  bool needLeft = mLeft.IsNeeded();
  bool needRight = mRight.IsNeeded();
  if (!needLeft && !needRight) {
    return;
  }
  NS_ASSERTION(mLeft.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP ||
               !needLeft, "left marker for 'clip'");
  NS_ASSERTION(mRight.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP ||
               !needRight, "right marker for 'clip'");

  // If there is insufficient space for both markers then keep the one on the
  // end side per the block's 'direction'.
  if (needLeft && needRight &&
      mLeft.mWidth + mRight.mWidth > mContentArea.width) {
    if (mBlockIsRTL) {
      needRight = false;
    } else {
      needLeft = false;
    }
  }
  nsRect insideMarkersArea = mContentArea;
  if (needLeft) {
    InflateLeft(&insideMarkersArea, -mLeft.mWidth);
  }
  if (needRight) {
    InflateRight(&insideMarkersArea, -mRight.mWidth);
  }
  if (!mCanHaveHorizontalScrollbar && alignmentEdges.mAssigned) {
    nsRect alignmentRect = nsRect(alignmentEdges.x, insideMarkersArea.y,
                                  alignmentEdges.Width(), 1);
    insideMarkersArea.IntersectRect(insideMarkersArea, alignmentRect);
  }

  // Clip and remove display items as needed at the final marker edges.
  nsDisplayList* lists[] = { aLists.Content(), aLists.PositionedDescendants() };
  for (uint32_t i = 0; i < ArrayLength(lists); ++i) {
    PruneDisplayListContents(lists[i], framesToHide, insideMarkersArea);
  }
  CreateMarkers(aLine, needLeft, needRight, insideMarkersArea);
}

void
TextOverflow::PruneDisplayListContents(nsDisplayList*        aList,
                                       const FrameHashtable& aFramesToHide,
                                       const nsRect&         aInsideMarkersArea)
{
  nsDisplayList saved;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    nsIFrame* itemFrame = item->Frame();
    if (IsFrameDescendantOfAny(itemFrame, aFramesToHide, mBlock)) {
      item->~nsDisplayItem();
      continue;
    }

    nsDisplayList* wrapper = item->GetSameCoordinateSystemChildren();
    if (wrapper) {
      if (!itemFrame || GetSelfOrNearestBlock(itemFrame) == mBlock) {
        PruneDisplayListContents(wrapper, aFramesToHide, aInsideMarkersArea);
      }
    }

    nsCharClipDisplayItem* charClip = itemFrame ? 
      nsCharClipDisplayItem::CheckCast(item) : nullptr;
    if (charClip && GetSelfOrNearestBlock(itemFrame) == mBlock) {
      nsRect rect = itemFrame->GetScrollableOverflowRect() +
                    itemFrame->GetOffsetTo(mBlock);
      if (mLeft.IsNeeded() && rect.x < aInsideMarkersArea.x) {
        nscoord left = aInsideMarkersArea.x - rect.x;
        if (MOZ_UNLIKELY(left < 0)) {
          item->~nsDisplayItem();
          continue;
        }
        charClip->mLeftEdge = left;
      }
      if (mRight.IsNeeded() && rect.XMost() > aInsideMarkersArea.XMost()) {
        nscoord right = rect.XMost() - aInsideMarkersArea.XMost();
        if (MOZ_UNLIKELY(right < 0)) {
          item->~nsDisplayItem();
          continue;
        }
        charClip->mRightEdge = right;
      }
    }

    saved.AppendToTop(item);
  }
  aList->AppendToTop(&saved);
}

/* static */ bool
TextOverflow::CanHaveTextOverflow(nsDisplayListBuilder* aBuilder,
                                  nsIFrame*             aBlockFrame)
{
  const nsStyleTextReset* style = aBlockFrame->StyleTextReset();
  // Nothing to do for text-overflow:clip or if 'overflow-x:visible' or if
  // we're just building items for event processing or image visibility.
  if ((style->mTextOverflow.mLeft.mType == NS_STYLE_TEXT_OVERFLOW_CLIP &&
       style->mTextOverflow.mRight.mType == NS_STYLE_TEXT_OVERFLOW_CLIP) ||
      IsHorizontalOverflowVisible(aBlockFrame) ||
      aBuilder->IsForEventDelivery() || aBuilder->IsForImageVisibility()) {
    return false;
  }

  // Skip ComboboxControlFrame because it would clip the drop-down arrow.
  // Its anon block inherits 'text-overflow' and does what is expected.
  if (aBlockFrame->GetType() == nsGkAtoms::comboboxControlFrame) {
    return false;
  }

  // Inhibit the markers if a descendant content owns the caret.
  nsRefPtr<nsCaret> caret = aBlockFrame->PresContext()->PresShell()->GetCaret();
  if (caret && caret->IsVisible()) {
    nsCOMPtr<nsISelection> domSelection = caret->GetSelection();
    if (domSelection) {
      nsCOMPtr<nsIDOMNode> node;
      domSelection->GetFocusNode(getter_AddRefs(node));
      nsCOMPtr<nsIContent> content = do_QueryInterface(node);
      if (content && nsContentUtils::ContentIsDescendantOf(content,
                       aBlockFrame->GetContent())) {
        return false;
      }
    }
  }
  return true;
}

void
TextOverflow::CreateMarkers(const nsLineBox* aLine,
                            bool             aCreateLeft,
                            bool             aCreateRight,
                            const nsRect&    aInsideMarkersArea)
{
  if (aCreateLeft) {
    DisplayListClipState::AutoSaveRestore clipState(mBuilder);

    //XXX Needs vertical text love
    nsRect markerRect = nsRect(aInsideMarkersArea.x - mLeft.mIntrinsicISize,
                               aLine->BStart(),
                               mLeft.mIntrinsicISize, aLine->BSize());
    markerRect += mBuilder->ToReferenceFrame(mBlock);
    ClipMarker(mContentArea + mBuilder->ToReferenceFrame(mBlock),
               markerRect, clipState);
    nsDisplayItem* marker = new (mBuilder)
      nsDisplayTextOverflowMarker(mBuilder, mBlock, markerRect,
                                  aLine->GetLogicalAscent(), mLeft.mStyle, 0);
    mMarkerList.AppendNewToTop(marker);
  }

  if (aCreateRight) {
    DisplayListClipState::AutoSaveRestore clipState(mBuilder);

    nsRect markerRect = nsRect(aInsideMarkersArea.XMost(),
                               aLine->BStart(),
                               mRight.mIntrinsicISize, aLine->BSize());
    markerRect += mBuilder->ToReferenceFrame(mBlock);
    ClipMarker(mContentArea + mBuilder->ToReferenceFrame(mBlock),
               markerRect, clipState);
    nsDisplayItem* marker = new (mBuilder)
      nsDisplayTextOverflowMarker(mBuilder, mBlock, markerRect,
                                  aLine->GetLogicalAscent(), mRight.mStyle, 1);
    mMarkerList.AppendNewToTop(marker);
  }
}

void
TextOverflow::Marker::SetupString(nsIFrame* aFrame)
{
  if (mInitialized) {
    return;
  }

  if (mStyle->mType == NS_STYLE_TEXT_OVERFLOW_ELLIPSIS) {
    gfxTextRun* textRun = GetEllipsisTextRun(aFrame);
    if (textRun) {
      mWidth = textRun->GetAdvanceWidth(0, textRun->GetLength(), nullptr);
    } else {
      mWidth = 0;
    }
  } else {
    nsRefPtr<nsRenderingContext> rc =
      aFrame->PresContext()->PresShell()->CreateReferenceRenderingContext();
    nsRefPtr<nsFontMetrics> fm;
    nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm),
      nsLayoutUtils::FontSizeInflationFor(aFrame));
    rc->SetFont(fm);
    mWidth = nsLayoutUtils::GetStringWidth(aFrame, rc, mStyle->mString.get(),
                                           mStyle->mString.Length());
  }
  mIntrinsicISize = mWidth;
  mInitialized = true;
}

}  // namespace css
}  // namespace mozilla
