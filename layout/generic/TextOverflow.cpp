/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is an implementation of CSS3 text-overflow.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <matspal@gmail.com> (original author)
 *   Michael Ventnor <m.ventnor@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TextOverflow.h"

// Please maintain alphabetical order below
#include "nsBlockFrame.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsRenderingContext.h"
#include "nsTextFrame.h"

namespace mozilla {
namespace css {

static const PRUnichar kEllipsisChar[] = { 0x2026, 0x0 };
static const PRUnichar kASCIIPeriodsChar[] = { '.', '.', '.', 0x0 };

// Return an ellipsis if the font supports it,
// otherwise use three ASCII periods as fallback.
static nsDependentString GetEllipsis(nsIFrame* aFrame)
{
  // Check if the first font supports Unicode ellipsis.
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm));
  gfxFontGroup* fontGroup = fm->GetThebesFontGroup();
  gfxFont* firstFont = fontGroup->GetFontAt(0);
  return firstFont && firstFont->HasCharacter(kEllipsisChar[0])
    ? nsDependentString(kEllipsisChar,
                        NS_ARRAY_LENGTH(kEllipsisChar) - 1)
    : nsDependentString(kASCIIPeriodsChar,
                        NS_ARRAY_LENGTH(kASCIIPeriodsChar) - 1);
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
                  !aFrame->GetStyleDisplay()->IsBlockOutside(),
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
  nsRefPtr<nsRenderingContext> rc =
    aFrame->PresContext()->PresShell()->GetReferenceRenderingContext();
  return rc &&
    !aFrame->MeasureCharClippedText(rc->ThebesContext(), aLeft, aRight,
                                    aSnappedLeft, aSnappedRight);
}

static bool
IsHorizontalOverflowVisible(nsIFrame* aFrame)
{
  NS_PRECONDITION(nsLayoutUtils::GetAsBlock(aFrame) != nsnull,
                  "expected a block frame");

  nsIFrame* f = aFrame;
  while (f && f->GetStyleContext()->GetPseudo()) {
    f = f->GetParent();
  }
  return !f || f->GetStyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE;
}

static nsDisplayItem*
ClipMarker(nsDisplayListBuilder* aBuilder,
           nsIFrame*             aFrame,
           nsDisplayItem*        aMarker,
           const nsRect&         aContentArea,
           nsRect*               aMarkerRect)
{
  nsDisplayItem* item = aMarker;
  nscoord rightOverflow = aMarkerRect->XMost() - aContentArea.XMost();
  if (rightOverflow > 0) {
    // Marker overflows on the right side (content width < marker width).
    aMarkerRect->width -= rightOverflow;
    item = new (aBuilder)
      nsDisplayClip(aBuilder, aFrame, aMarker, *aMarkerRect);
  } else {
    nscoord leftOverflow = aContentArea.x - aMarkerRect->x;
    if (leftOverflow > 0) {
      // Marker overflows on the left side
      aMarkerRect->width -= leftOverflow;
      aMarkerRect->x += leftOverflow;
      item = new (aBuilder)
        nsDisplayClip(aBuilder, aFrame, aMarker, *aMarkerRect);
    }
  }
  return item;
}

static void
InflateLeft(nsRect* aRect, bool aInfinity, nscoord aDelta)
{
  nscoord xmost = aRect->XMost();
  if (aInfinity) {
    aRect->x = nscoord_MIN;
  } else {
    aRect->x -= aDelta;
  }
  aRect->width = NS_MAX(xmost - aRect->x, 0);
}

static void
InflateRight(nsRect* aRect, bool aInfinity, nscoord aDelta)
{
  if (aInfinity) {
    aRect->width = nscoord_MAX;
  } else {
    aRect->width = NS_MAX(aRect->width + aDelta, 0);
  }
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
                              const nsString& aString)
    : nsDisplayItem(aBuilder, aFrame), mRect(aRect), mString(aString),
      mAscent(aAscent) {
    MOZ_COUNT_CTOR(nsDisplayTextOverflowMarker);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTextOverflowMarker() {
    MOZ_COUNT_DTOR(nsDisplayTextOverflowMarker);
  }
#endif
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) {
    nsRect shadowRect =
      nsLayoutUtils::GetTextShadowRectsUnion(mRect, mFrame);
    return mRect.Union(shadowRect);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  void PaintTextToContext(nsRenderingContext* aCtx,
                          nsPoint aOffsetFromRect);
  NS_DISPLAY_DECL_NAME("TextOverflow", TYPE_TEXT_OVERFLOW)
private:
  nsRect          mRect;   // in reference frame coordinates
  const nsString  mString; // the marker text
  nscoord         mAscent; // baseline for the marker text in mRect
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
  aCtx->SetColor(foregroundColor);
  PaintTextToContext(aCtx, nsPoint(0, 0));
}

void
nsDisplayTextOverflowMarker::PaintTextToContext(nsRenderingContext* aCtx,
                                                nsPoint aOffsetFromRect)
{
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(mFrame, getter_AddRefs(fm));
  aCtx->SetFont(fm);
  gfxFloat y = nsLayoutUtils::GetSnappedBaselineY(mFrame, aCtx->ThebesContext(),
                                                  mRect.y, mAscent);
  nsPoint baselinePt(mRect.x, NSToCoordFloor(y));
  nsLayoutUtils::DrawString(mFrame, aCtx, mString.get(),
                            mString.Length(), baselinePt + aOffsetFromRect);
}

/* static */ TextOverflow*
TextOverflow::WillProcessLines(nsDisplayListBuilder*   aBuilder,
                               const nsDisplayListSet& aLists,
                               nsIFrame*               aBlockFrame)
{
  if (!CanHaveTextOverflow(aBuilder, aBlockFrame)) {
    return nsnull;
  }

  nsAutoPtr<TextOverflow> textOverflow(new TextOverflow);
  textOverflow->mBuilder = aBuilder;
  textOverflow->mBlock = aBlockFrame;
  textOverflow->mMarkerList = aLists.PositionedDescendants();
  textOverflow->mContentArea = aBlockFrame->GetContentRectRelativeToSelf();
  nsIScrollableFrame* scroll =
    nsLayoutUtils::GetScrollableFrameFor(aBlockFrame);
  textOverflow->mCanHaveHorizontalScrollbar = false;
  if (scroll) {
    textOverflow->mCanHaveHorizontalScrollbar =
      scroll->GetScrollbarStyles().mHorizontal != NS_STYLE_OVERFLOW_HIDDEN;
    textOverflow->mContentArea.MoveBy(scroll->GetScrollPosition());
  }
  textOverflow->mBlockIsRTL =
    aBlockFrame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  const nsStyleTextReset* style = aBlockFrame->GetStyleTextReset();
  textOverflow->mLeft.Init(style->mTextOverflow.mLeft);
  textOverflow->mRight.Init(style->mTextOverflow.mRight);
  // The left/right marker string is setup in ExamineLineFrames when a line
  // has overflow on that side.

  return textOverflow.forget();
}

void
TextOverflow::DidProcessLines()
{
  nsIScrollableFrame* scroll = nsLayoutUtils::GetScrollableFrameFor(mBlock);
  if (scroll) {
    // Create a dummy item covering the entire area, it doesn't paint
    // but reports true for IsVaryingRelativeToMovingFrame().
    nsIFrame* scrollFrame = do_QueryFrame(scroll);
    nsDisplayItem* marker = new (mBuilder)
      nsDisplayForcePaintOnScroll(mBuilder, scrollFrame);
    if (marker) {
      mMarkerList->AppendNewToBottom(marker);
      mBlock->PresContext()->SetHasFixedBackgroundFrame();
    }
  }
}

void
TextOverflow::ExamineFrameSubtree(nsIFrame*       aFrame,
                                  const nsRect&   aContentArea,
                                  const nsRect&   aInsideMarkersArea,
                                  FrameHashtable* aFramesToHide,
                                  AlignmentEdges* aAlignmentEdges)
{
  const nsIAtom* frameType = aFrame->GetType();
  if (frameType == nsGkAtoms::brFrame ||
      frameType == nsGkAtoms::placeholderFrame) {
    return;
  }
  const bool isAtomic = IsAtomicElement(aFrame, frameType);
  if (aFrame->GetStyleVisibility()->IsVisible()) {
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
    if (isAtomic && (overflowLeft || overflowRight)) {
      aFramesToHide->PutEntry(aFrame);
    } else if (isAtomic || frameType == nsGkAtoms::textFrame) {
      AnalyzeMarkerEdges(aFrame, frameType, aInsideMarkersArea,
                         aFramesToHide, aAlignmentEdges);
    }
  }
  if (isAtomic) {
    return;
  }

  nsIFrame* child = aFrame->GetFirstPrincipalChild();
  while (child) {
    ExamineFrameSubtree(child, aContentArea, aInsideMarkersArea,
                        aFramesToHide, aAlignmentEdges);
    child = child->GetNextSibling();
  }
}

void
TextOverflow::AnalyzeMarkerEdges(nsIFrame*       aFrame,
                                 const nsIAtom*  aFrameType,
                                 const nsRect&   aInsideMarkersArea,
                                 FrameHashtable* aFramesToHide,
                                 AlignmentEdges* aAlignmentEdges)
{
  nsRect borderRect(aFrame->GetOffsetTo(mBlock), aFrame->GetSize());
  nscoord leftOverlap =
    NS_MAX(aInsideMarkersArea.x - borderRect.x, 0);
  nscoord rightOverlap =
    NS_MAX(borderRect.XMost() - aInsideMarkersArea.XMost(), 0);
  bool insideLeftEdge = aInsideMarkersArea.x <= borderRect.XMost();
  bool insideRightEdge = borderRect.x <= aInsideMarkersArea.XMost();

  if ((leftOverlap > 0 && insideLeftEdge) ||
      (rightOverlap > 0 && insideRightEdge)) {
    if (aFrameType == nsGkAtoms::textFrame &&
        aInsideMarkersArea.x < aInsideMarkersArea.XMost()) {
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
      }
    } else if (IsAtomicElement(aFrame, aFrameType)) {
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
  }
}

void
TextOverflow::ExamineLineFrames(nsLineBox*      aLine,
                                FrameHashtable* aFramesToHide,
                                AlignmentEdges* aAlignmentEdges)
{
  // Scrolling to the end position can leave some text still overflowing due to
  // pixel snapping behaviour in our scrolling code so we move the edges 1px
  // outward to avoid triggering a text-overflow marker for such overflow.
  nsRect contentArea = mContentArea;
  const nscoord scrollAdjust = mCanHaveHorizontalScrollbar ?
    mBlock->PresContext()->AppUnitsPerDevPixel() : 0;
  InflateLeft(&contentArea,
              mLeft.mStyle->mType == NS_STYLE_TEXT_OVERFLOW_CLIP,
              scrollAdjust);
  InflateRight(&contentArea,
               mRight.mStyle->mType == NS_STYLE_TEXT_OVERFLOW_CLIP,
               scrollAdjust);
  nsRect lineRect = aLine->GetScrollableOverflowArea();
  const bool leftOverflow = lineRect.x < contentArea.x;
  const bool rightOverflow = lineRect.XMost() > contentArea.XMost();
  if (!leftOverflow && !rightOverflow) {
    // The line does not overflow - no need to traverse the frame tree.
    return;
  }

  PRUint32 pass = 0;
  bool guessLeft =
    mLeft.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP && leftOverflow;
  bool guessRight =
    mRight.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP && rightOverflow;
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
    nscoord rightMarkerWidth = mRight.mWidth;
    nscoord leftMarkerWidth = mLeft.mWidth;
    if (leftOverflow && rightOverflow &&
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
    InflateLeft(&insideMarkersArea, !guessLeft, -leftMarkerWidth);
    InflateRight(&insideMarkersArea, !guessRight, -rightMarkerWidth);

    // Analyze the frames on aLine for the overflow situation at the content
    // edges and at the edges of the area between the markers.
    PRInt32 n = aLine->GetChildCount();
    nsIFrame* child = aLine->mFirstChild;
    for (; n-- > 0; child = child->GetNextSibling()) {
      ExamineFrameSubtree(child, contentArea, insideMarkersArea,
                          aFramesToHide, aAlignmentEdges);
    }
    if (guessLeft == mLeft.IsNeeded() && guessRight == mRight.IsNeeded()) {
      break;
    } else {
      guessLeft = mLeft.IsNeeded();
      guessRight = mRight.IsNeeded();
      mLeft.Reset();
      mRight.Reset();
      aFramesToHide->Clear();
    }
    NS_ASSERTION(pass == 0, "2nd pass should never guess wrong");
  } while (++pass != 2);
}

void
TextOverflow::ProcessLine(const nsDisplayListSet& aLists,
                          nsLineBox*              aLine)
{
  NS_ASSERTION(mLeft.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP ||
               mRight.mStyle->mType != NS_STYLE_TEXT_OVERFLOW_CLIP,
               "TextOverflow with 'clip' for both sides");
  mLeft.Reset();
  mRight.Reset();
  FrameHashtable framesToHide;
  if (!framesToHide.Init(100)) {
    return;
  }
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
    InflateLeft(&insideMarkersArea, false, -mLeft.mWidth);
  }
  if (needRight) {
    InflateRight(&insideMarkersArea, false, -mRight.mWidth);
  }
  if (!mCanHaveHorizontalScrollbar && alignmentEdges.mAssigned) {
    nsRect alignmentRect = nsRect(alignmentEdges.x, insideMarkersArea.y,
                                  alignmentEdges.Width(), 1);
    insideMarkersArea.IntersectRect(insideMarkersArea, alignmentRect);
  }

  // Clip and remove display items as needed at the final marker edges.
  nsDisplayList* lists[] = { aLists.Content(), aLists.PositionedDescendants() };
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(lists); ++i) {
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
    nsIFrame* itemFrame = item->GetUnderlyingFrame();
    if (itemFrame && IsFrameDescendantOfAny(itemFrame, aFramesToHide, mBlock)) {
      item->~nsDisplayItem();
      continue;
    }

    nsDisplayList* wrapper = item->GetList();
    if (wrapper) {
      if (!itemFrame || GetSelfOrNearestBlock(itemFrame) == mBlock) {
        PruneDisplayListContents(wrapper, aFramesToHide, aInsideMarkersArea);
      }
    }

    nsCharClipDisplayItem* charClip = itemFrame ? 
      nsCharClipDisplayItem::CheckCast(item) : nsnull;
    if (charClip && GetSelfOrNearestBlock(itemFrame) == mBlock) {
      nsRect rect = itemFrame->GetScrollableOverflowRect() +
                    itemFrame->GetOffsetTo(mBlock);
      if (mLeft.IsNeeded() && rect.x < aInsideMarkersArea.x) {
        nscoord left = aInsideMarkersArea.x - rect.x;
        if (NS_UNLIKELY(left < 0)) {
          item->~nsDisplayItem();
          continue;
        }
        charClip->mLeftEdge = left;
      }
      if (mRight.IsNeeded() && rect.XMost() > aInsideMarkersArea.XMost()) {
        nscoord right = rect.XMost() - aInsideMarkersArea.XMost();
        if (NS_UNLIKELY(right < 0)) {
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
  const nsStyleTextReset* style = aBlockFrame->GetStyleTextReset();
  // Nothing to do for text-overflow:clip or if 'overflow-x:visible'
  // or if we're just building items for event processing.
  if ((style->mTextOverflow.mLeft.mType == NS_STYLE_TEXT_OVERFLOW_CLIP &&
       style->mTextOverflow.mRight.mType == NS_STYLE_TEXT_OVERFLOW_CLIP) ||
      IsHorizontalOverflowVisible(aBlockFrame) ||
      aBuilder->IsForEventDelivery()) {
    return false;
  }

  // Inhibit the markers if a descendant content owns the caret.
  nsRefPtr<nsCaret> caret = aBlockFrame->PresContext()->PresShell()->GetCaret();
  PRBool visible = PR_FALSE;
  if (caret && NS_SUCCEEDED(caret->GetCaretVisible(&visible)) && visible) {
    nsCOMPtr<nsISelection> domSelection = caret->GetCaretDOMSelection();
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
                            const nsRect&    aInsideMarkersArea) const
{
  if (aCreateLeft) {
    nsRect markerRect = nsRect(aInsideMarkersArea.x - mLeft.mWidth,
                               aLine->mBounds.y,
                               mLeft.mWidth, aLine->mBounds.height);
    markerRect += mBuilder->ToReferenceFrame(mBlock);
    nsDisplayItem* marker = new (mBuilder)
      nsDisplayTextOverflowMarker(mBuilder, mBlock, markerRect,
                                  aLine->GetAscent(), mLeft.mMarkerString);
    if (marker) {
      marker = ClipMarker(mBuilder, mBlock, marker,
                          mContentArea + mBuilder->ToReferenceFrame(mBlock),
                          &markerRect);
    }
    mMarkerList->AppendNewToTop(marker);
  }

  if (aCreateRight) {
    nsRect markerRect = nsRect(aInsideMarkersArea.XMost(),
                               aLine->mBounds.y,
                               mRight.mWidth, aLine->mBounds.height);
    markerRect += mBuilder->ToReferenceFrame(mBlock);
    nsDisplayItem* marker = new (mBuilder)
      nsDisplayTextOverflowMarker(mBuilder, mBlock, markerRect,
                                  aLine->GetAscent(), mRight.mMarkerString);
    if (marker) {
      marker = ClipMarker(mBuilder, mBlock, marker,
                          mContentArea + mBuilder->ToReferenceFrame(mBlock),
                          &markerRect);
    }
    mMarkerList->AppendNewToTop(marker);
  }
}

void
TextOverflow::Marker::SetupString(nsIFrame* aFrame)
{
  if (mInitialized) {
    return;
  }
  nsRefPtr<nsRenderingContext> rc =
    aFrame->PresContext()->PresShell()->GetReferenceRenderingContext();
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm));
  rc->SetFont(fm);

  mMarkerString = mStyle->mType == NS_STYLE_TEXT_OVERFLOW_ELLIPSIS ?
                    GetEllipsis(aFrame) : mStyle->mString;
  mWidth = nsLayoutUtils::GetStringWidth(aFrame, rc, mMarkerString.get(),
                                         mMarkerString.Length());
  mInitialized = true;
}

}  // namespace css
}  // namespace mozilla
