/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#include "nsCaret.h"

#include <algorithm>

#include "gfxUtils.h"
#include "mozilla/CaretAssociationHint.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/intl/BidiEmbeddingLevel.h"
#include "mozilla/ScrollContainerFrame.h"
#include "mozilla/StaticPrefs_bidi.h"
#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsITimer.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsBlockFrame.h"
#include "nsISelectionController.h"
#include "nsTextFrame.h"
#include "nsXULPopupManager.h"
#include "nsMenuPopupFrame.h"
#include "nsTextFragment.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Selection.h"
#include "nsIBidiKeyboard.h"
#include "nsContentUtils.h"
#include "SelectionMovementUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

using BidiEmbeddingLevel = mozilla::intl::BidiEmbeddingLevel;

// The bidi indicator hangs off the caret to one side, to show which
// direction the typing is in. It needs to be at least 2x2 to avoid looking
// like an insignificant dot
static const int32_t kMinBidiIndicatorPixels = 2;

nsCaret::nsCaret() = default;

nsCaret::~nsCaret() { StopBlinking(); }

nsresult nsCaret::Init(PresShell* aPresShell) {
  NS_ENSURE_ARG(aPresShell);

  mPresShell =
      do_GetWeakReference(aPresShell);  // the presshell owns us, so no addref
  NS_ASSERTION(mPresShell, "Hey, pres shell should support weak refs");

  RefPtr<Selection> selection =
      aPresShell->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  selection->AddSelectionListener(this);
  mDomSelectionWeak = selection;
  UpdateCaretPositionFromSelectionIfNeeded();

  return NS_OK;
}

static bool DrawCJKCaret(nsIFrame* aFrame, int32_t aOffset) {
  nsIContent* content = aFrame->GetContent();
  const nsTextFragment* frag = content->GetText();
  if (!frag) {
    return false;
  }
  if (aOffset < 0 || static_cast<uint32_t>(aOffset) >= frag->GetLength()) {
    return false;
  }
  const char16_t ch = frag->CharAt(AssertedCast<uint32_t>(aOffset));
  return 0x2e80 <= ch && ch <= 0xd7ff;
}

nsCaret::Metrics nsCaret::ComputeMetrics(nsIFrame* aFrame, int32_t aOffset,
                                         nscoord aCaretHeight) {
  // Compute nominal sizes in appunits
  nscoord caretWidth =
      (aCaretHeight *
       LookAndFeel::GetFloat(LookAndFeel::FloatID::CaretAspectRatio, 0.0f)) +
      nsPresContext::CSSPixelsToAppUnits(
          LookAndFeel::GetInt(LookAndFeel::IntID::CaretWidth, 1));

  if (DrawCJKCaret(aFrame, aOffset)) {
    caretWidth += nsPresContext::CSSPixelsToAppUnits(1);
  }
  nscoord bidiIndicatorSize =
      nsPresContext::CSSPixelsToAppUnits(kMinBidiIndicatorPixels);
  bidiIndicatorSize = std::max(caretWidth, bidiIndicatorSize);

  // Round them to device pixels. Always round down, except that anything
  // between 0 and 1 goes up to 1 so we don't let the caret disappear.
  int32_t tpp = aFrame->PresContext()->AppUnitsPerDevPixel();
  Metrics result;
  result.mCaretWidth = NS_ROUND_BORDER_TO_PIXELS(caretWidth, tpp);
  result.mBidiIndicatorSize = NS_ROUND_BORDER_TO_PIXELS(bidiIndicatorSize, tpp);
  return result;
}

void nsCaret::Terminate() {
  // this doesn't erase the caret if it's drawn. Should it? We might not have
  // a good drawing environment during teardown.

  StopBlinking();
  mBlinkTimer = nullptr;

  // unregiser ourselves as a selection listener
  if (mDomSelectionWeak) {
    mDomSelectionWeak->RemoveSelectionListener(this);
  }
  mDomSelectionWeak = nullptr;
  mPresShell = nullptr;
  mCaretPosition = {};
}

NS_IMPL_ISUPPORTS(nsCaret, nsISelectionListener)

Selection* nsCaret::GetSelection() { return mDomSelectionWeak; }

void nsCaret::SetSelection(Selection* aDOMSel) {
  MOZ_ASSERT(aDOMSel);
  mDomSelectionWeak = aDOMSel;
  UpdateCaretPositionFromSelectionIfNeeded();
  ResetBlinking();
  SchedulePaint();
}

void nsCaret::SetVisible(bool aVisible) {
  const bool wasVisible = mVisible;
  mVisible = aVisible;
  mIgnoreUserModify = aVisible;
  if (mVisible != wasVisible) {
    CaretVisibilityMaybeChanged();
  }
}

bool nsCaret::IsVisible() const { return mVisible && !mHideCount; }

void nsCaret::CaretVisibilityMaybeChanged() {
  ResetBlinking();
  SchedulePaint();
  if (IsVisible()) {
    // We ignore caret position updates when the caret is not visible, so we
    // update the caret position here if needed.
    UpdateCaretPositionFromSelectionIfNeeded();
  }
}

void nsCaret::AddForceHide() {
  MOZ_ASSERT(mHideCount < UINT32_MAX);
  if (++mHideCount > 1) {
    return;
  }
  CaretVisibilityMaybeChanged();
}

void nsCaret::RemoveForceHide() {
  if (!mHideCount || --mHideCount) {
    return;
  }
  CaretVisibilityMaybeChanged();
}

void nsCaret::SetCaretReadOnly(bool aReadOnly) {
  mReadOnly = aReadOnly;
  ResetBlinking();
  SchedulePaint();
}

// Clamp the inline-position to be within our closest scroll frame and any
// ancestor clips if any. If we don't, then it clips us, and we don't appear at
// all. See bug 335560 and bug 1539720.
static nsPoint AdjustRectForClipping(const nsRect& aRect, nsIFrame* aFrame,
                                     bool aVertical) {
  nsRect rectRelativeToClip = aRect;
  ScrollContainerFrame* sf = nullptr;
  nsIFrame* scrollFrame = nullptr;
  for (nsIFrame* current = aFrame; current; current = current->GetParent()) {
    if ((sf = do_QueryFrame(current))) {
      scrollFrame = current;
      break;
    }
    if (current->IsTransformed()) {
      // We don't account for transforms in rectRelativeToCurrent, so stop
      // adjusting here.
      break;
    }
    rectRelativeToClip += current->GetPosition();
  }

  if (!sf) {
    return {};
  }

  nsRect clipRect = sf->GetScrollPortRect();
  {
    const auto& disp = *scrollFrame->StyleDisplay();
    if (disp.mOverflowClipBoxBlock == StyleOverflowClipBox::ContentBox ||
        disp.mOverflowClipBoxInline == StyleOverflowClipBox::ContentBox) {
      const WritingMode wm = scrollFrame->GetWritingMode();
      const bool cbH = (wm.IsVertical() ? disp.mOverflowClipBoxBlock
                                        : disp.mOverflowClipBoxInline) ==
                       StyleOverflowClipBox::ContentBox;
      const bool cbV = (wm.IsVertical() ? disp.mOverflowClipBoxInline
                                        : disp.mOverflowClipBoxBlock) ==
                       StyleOverflowClipBox::ContentBox;
      nsMargin padding = scrollFrame->GetUsedPadding();
      if (!cbH) {
        padding.left = padding.right = 0;
      }
      if (!cbV) {
        padding.top = padding.bottom = 0;
      }
      clipRect.Deflate(padding);
    }
  }
  nsPoint offset;
  // Now see if the caret extends beyond the view's bounds. If it does, then
  // snap it back, put it as close to the edge as it can.
  if (aVertical) {
    nscoord overflow = rectRelativeToClip.YMost() - clipRect.YMost();
    if (overflow > 0) {
      offset.y -= overflow;
    } else {
      overflow = rectRelativeToClip.y - clipRect.y;
      if (overflow < 0) {
        offset.y -= overflow;
      }
    }
  } else {
    nscoord overflow = rectRelativeToClip.XMost() - clipRect.XMost();
    if (overflow > 0) {
      offset.x -= overflow;
    } else {
      overflow = rectRelativeToClip.x - clipRect.x;
      if (overflow < 0) {
        offset.x -= overflow;
      }
    }
  }
  return offset;
}

/* static */
nsRect nsCaret::GetGeometryForFrame(nsIFrame* aFrame, int32_t aFrameOffset,
                                    nscoord* aBidiIndicatorSize) {
  nsPoint framePos(0, 0);
  nsRect rect;
  nsresult rv = aFrame->GetPointFromOffset(aFrameOffset, &framePos);
  if (NS_FAILED(rv)) {
    if (aBidiIndicatorSize) {
      *aBidiIndicatorSize = 0;
    }
    return rect;
  }

  nsIFrame* frame = aFrame->GetContentInsertionFrame();
  if (!frame) {
    frame = aFrame;
  }
  NS_ASSERTION(!frame->HasAnyStateBits(NS_FRAME_IN_REFLOW),
               "We should not be in the middle of reflow");
  WritingMode wm = aFrame->GetWritingMode();
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(aFrame);
  const auto caretBlockAxisMetrics = frame->GetCaretBlockAxisMetrics(wm, *fm);
  const bool vertical = wm.IsVertical();
  Metrics caretMetrics =
      ComputeMetrics(aFrame, aFrameOffset, caretBlockAxisMetrics.mExtent);

  nscoord inlineOffset = 0;
  if (nsTextFrame* textFrame = do_QueryFrame(aFrame)) {
    if (gfxTextRun* textRun = textFrame->GetTextRun(nsTextFrame::eInflated)) {
      // For "upstream" text where the textrun direction is reversed from the
      // frame's inline-dir we want the caret to be painted before rather than
      // after its nominal inline position, so we offset by its width.
      const bool textRunDirIsReverseOfFrame =
          wm.IsInlineReversed() != textRun->IsInlineReversed();
      // However, in sideways-lr mode we invert this behavior because this is
      // the one writing mode where bidi-LTR corresponds to inline-reversed
      // already, which reverses the desired caret placement behavior.
      // Note that the following condition is equivalent to:
      //   if ( (!textRun->IsSidewaysLeft() && textRunDirIsReverseOfFrame) ||
      //        (textRun->IsSidewaysLeft()  && !textRunDirIsReverseOfFrame) )
      if (textRunDirIsReverseOfFrame != textRun->IsSidewaysLeft()) {
        inlineOffset = wm.IsBidiLTR() ? -caretMetrics.mCaretWidth
                                      : caretMetrics.mCaretWidth;
      }
    }
  }

  // on RTL frames the right edge of mCaretRect must be equal to framePos
  if (aFrame->StyleVisibility()->mDirection == StyleDirection::Rtl) {
    if (vertical) {
      inlineOffset -= caretMetrics.mCaretWidth;
    } else {
      inlineOffset -= caretMetrics.mCaretWidth;
    }
  }

  if (vertical) {
    framePos.x = caretBlockAxisMetrics.mOffset;
    framePos.y += inlineOffset;
  } else {
    framePos.x += inlineOffset;
    framePos.y = caretBlockAxisMetrics.mOffset;
  }

  rect = nsRect(framePos, vertical ? nsSize(caretBlockAxisMetrics.mExtent,
                                            caretMetrics.mCaretWidth)
                                   : nsSize(caretMetrics.mCaretWidth,
                                            caretBlockAxisMetrics.mExtent));

  rect.MoveBy(AdjustRectForClipping(rect, aFrame, vertical));
  if (aBidiIndicatorSize) {
    *aBidiIndicatorSize = caretMetrics.mBidiIndicatorSize;
  }
  return rect;
}

auto nsCaret::CaretPositionFor(const Selection* aSelection) -> CaretPosition {
  if (!aSelection) {
    return {};
  }
  const nsFrameSelection* frameSelection = aSelection->GetFrameSelection();
  if (!frameSelection) {
    return {};
  }
  nsINode* node = aSelection->GetFocusNode();
  if (!node) {
    return {};
  }
  return {
      node,
      int32_t(aSelection->FocusOffset()),
      frameSelection->GetHint(),
      frameSelection->GetCaretBidiLevel(),
  };
}

CaretFrameData nsCaret::GetFrameAndOffset(const CaretPosition& aPosition) {
  nsINode* focusNode = aPosition.mContent;
  int32_t focusOffset = aPosition.mOffset;

  if (!focusNode || !focusNode->IsContent()) {
    return {};
  }

  nsIContent* contentNode = focusNode->AsContent();
  return SelectionMovementUtils::GetCaretFrameForNodeOffset(
      nullptr, contentNode, focusOffset, aPosition.mHint, aPosition.mBidiLevel,
      ForceEditableRegion::No);
}

/* static */
nsIFrame* nsCaret::GetGeometry(const Selection* aSelection, nsRect* aRect) {
  auto data = GetFrameAndOffset(CaretPositionFor(aSelection));
  if (data.mFrame) {
    *aRect =
        GetGeometryForFrame(data.mFrame, data.mOffsetInFrameContent, nullptr);
  }
  return data.mFrame;
}

[[nodiscard]] static nsIFrame* GetContainingBlockIfNeeded(nsIFrame* aFrame) {
  if (aFrame->IsBlockOutside() || aFrame->IsBlockFrameOrSubclass()) {
    return nullptr;
  }
  return aFrame->GetContainingBlock();
}

void nsCaret::SchedulePaint() {
  if (mLastPaintedFrame) {
    mLastPaintedFrame->SchedulePaint();
    mLastPaintedFrame = nullptr;
  }
  auto data = GetFrameAndOffset(mCaretPosition);
  if (!data.mFrame) {
    return;
  }
  nsIFrame* frame = data.mFrame;
  if (nsIFrame* cb = GetContainingBlockIfNeeded(frame)) {
    frame = cb;
  }
  frame->SchedulePaint();
}

void nsCaret::SetVisibilityDuringSelection(bool aVisibility) {
  if (mShowDuringSelection == aVisibility) {
    return;
  }
  mShowDuringSelection = aVisibility;
  if (mHiddenDuringSelection && aVisibility) {
    RemoveForceHide();
    mHiddenDuringSelection = false;
  }
  SchedulePaint();
}

void nsCaret::UpdateCaretPositionFromSelectionIfNeeded() {
  if (mFixedCaretPosition) {
    return;
  }
  CaretPosition newPos = CaretPositionFor(GetSelection());
  if (newPos == mCaretPosition) {
    return;
  }
  mCaretPosition = newPos;
  SchedulePaint();
}

void nsCaret::SetCaretPosition(nsINode* aNode, int32_t aOffset) {
  // Schedule a paint with the old position to invalidate.
  mFixedCaretPosition = !!aNode;
  if (mFixedCaretPosition) {
    mCaretPosition = {aNode, aOffset};
    SchedulePaint();
  } else {
    UpdateCaretPositionFromSelectionIfNeeded();
  }
  ResetBlinking();
}

void nsCaret::CheckSelectionLanguageChange() {
  if (!StaticPrefs::bidi_browser_ui()) {
    return;
  }

  bool isKeyboardRTL = false;
  nsIBidiKeyboard* bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  if (bidiKeyboard) {
    bidiKeyboard->IsLangRTL(&isKeyboardRTL);
  }
  // Call SelectionLanguageChange on every paint. Mostly it will be a noop
  // but it should be fast anyway. This guarantees we never paint the caret
  // at the wrong place.
  Selection* selection = GetSelection();
  if (selection) {
    selection->SelectionLanguageChange(isKeyboardRTL);
  }
}

// This ensures that the caret is not affected by clips on inlines and so forth.
[[nodiscard]] static nsIFrame* MapToContainingBlock(nsIFrame* aFrame,
                                                    nsRect* aCaretRect,
                                                    nsRect* aHookRect) {
  nsIFrame* containingBlock = GetContainingBlockIfNeeded(aFrame);
  if (!containingBlock) {
    return aFrame;
  }

  *aCaretRect = nsLayoutUtils::TransformFrameRectToAncestor(aFrame, *aCaretRect,
                                                            containingBlock);
  *aHookRect = nsLayoutUtils::TransformFrameRectToAncestor(aFrame, *aHookRect,
                                                           containingBlock);
  return containingBlock;
}

nsIFrame* nsCaret::GetPaintGeometry(nsRect* aCaretRect, nsRect* aHookRect,
                                    nscolor* aCaretColor) {
  // Return null if we should not be visible.
  if (!IsVisible() || !mIsBlinkOn) {
    return nullptr;
  }

  // Update selection language direction now so the new direction will be
  // taken into account when computing the caret position below.
  CheckSelectionLanguageChange();

  auto data = GetFrameAndOffset(mCaretPosition);
  MOZ_ASSERT(!!data.mFrame == !!data.mUnadjustedFrame);
  if (!data.mFrame) {
    return nullptr;
  }

  nsIFrame* frame = data.mFrame;
  nsIFrame* unadjustedFrame = data.mUnadjustedFrame;
  int32_t frameOffset(data.mOffsetInFrameContent);
  // Now we have a frame, check whether it's appropriate to show the caret here.
  // Note we need to check the unadjusted frame, otherwise consider the
  // following case:
  //
  //   <div contenteditable><span contenteditable=false>Text   </span><br>
  //
  // Where the selection is targeting the <br>. We want to display the caret,
  // since the <br> we're focused at is editable, but we do want to paint it at
  // the adjusted frame offset, so that we can see the collapsed whitespace.
  const nsStyleUI* ui = unadjustedFrame->StyleUI();
  if ((!mIgnoreUserModify && ui->UserModify() == StyleUserModify::ReadOnly) ||
      unadjustedFrame->IsContentDisabled()) {
    return nullptr;
  }

  // If the offset falls outside of the frame, then don't paint the caret.
  if (frame->IsTextFrame()) {
    auto [startOffset, endOffset] = frame->GetOffsets();
    if (startOffset > frameOffset || endOffset < frameOffset) {
      return nullptr;
    }
  }

  if (aCaretColor) {
    *aCaretColor = frame->GetCaretColorAt(frameOffset);
  }

  ComputeCaretRects(frame, frameOffset, aCaretRect, aHookRect);
  return MapToContainingBlock(frame, aCaretRect, aHookRect);
}

nsIFrame* nsCaret::GetPaintGeometry(nsRect* aRect) {
  nsRect caretRect;
  nsRect hookRect;
  nsIFrame* frame = GetPaintGeometry(&caretRect, &hookRect);
  aRect->UnionRect(caretRect, hookRect);
  return frame;
}

void nsCaret::PaintCaret(DrawTarget& aDrawTarget, nsIFrame* aForFrame,
                         const nsPoint& aOffset) {
  nsRect caretRect;
  nsRect hookRect;
  nscolor color;
  nsIFrame* frame = GetPaintGeometry(&caretRect, &hookRect, &color);
  MOZ_ASSERT(frame == aForFrame, "We're referring different frame");

  if (!frame) {
    return;
  }

  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
  Rect devPxCaretRect = NSRectToSnappedRect(caretRect + aOffset,
                                            appUnitsPerDevPixel, aDrawTarget);
  Rect devPxHookRect =
      NSRectToSnappedRect(hookRect + aOffset, appUnitsPerDevPixel, aDrawTarget);

  ColorPattern pattern(ToDeviceColor(color));
  aDrawTarget.FillRect(devPxCaretRect, pattern);
  if (!hookRect.IsEmpty()) {
    aDrawTarget.FillRect(devPxHookRect, pattern);
  }
}

NS_IMETHODIMP
nsCaret::NotifySelectionChanged(Document*, Selection* aDomSel, int16_t aReason,
                                int32_t aAmount) {
  // The same caret is shared amongst the document and any text widgets it
  // may contain. This means that the caret could get notifications from
  // multiple selections.
  //
  // If this notification is for a selection that is not the one the
  // the caret is currently interested in (mDomSelectionWeak), or the caret
  // position is fixed, then there is nothing to do!
  if (mDomSelectionWeak != aDomSel) {
    return NS_OK;
  }

  // Check if we need to hide / un-hide the caret due to the selection being
  // collapsed.
  if (!mShowDuringSelection &&
      !aDomSel->IsCollapsed() != mHiddenDuringSelection) {
    if (mHiddenDuringSelection) {
      RemoveForceHide();
    } else {
      AddForceHide();
    }
    mHiddenDuringSelection = !mHiddenDuringSelection;
  }

  // We don't bother computing the caret position when invisible. We'll do it if
  // we become visible in CaretVisibilityMaybeChanged().
  if (IsVisible()) {
    UpdateCaretPositionFromSelectionIfNeeded();
    ResetBlinking();
  }

  return NS_OK;
}

void nsCaret::ResetBlinking() {
  using IntID = LookAndFeel::IntID;

  // The default caret blinking rate (in ms of blinking interval)
  constexpr uint32_t kDefaultBlinkRate = 500;
  // The default caret blinking count (-1 for "never stop blinking")
  constexpr int32_t kDefaultBlinkCount = -1;

  mIsBlinkOn = true;

  if (mReadOnly || !IsVisible()) {
    StopBlinking();
    return;
  }

  auto blinkRate =
      LookAndFeel::GetInt(IntID::CaretBlinkTime, kDefaultBlinkRate);

  if (blinkRate > 0) {
    // Make sure to reset the remaining blink count even if the blink rate
    // hasn't changed.
    mBlinkCount =
        LookAndFeel::GetInt(IntID::CaretBlinkCount, kDefaultBlinkCount);
  }

  if (mBlinkRate == blinkRate) {
    // If the rate hasn't changed, then there is nothing else to do.
    return;
  }

  mBlinkRate = blinkRate;

  if (mBlinkTimer) {
    mBlinkTimer->Cancel();
  } else {
    mBlinkTimer = NS_NewTimer(GetMainThreadSerialEventTarget());
    if (!mBlinkTimer) {
      return;
    }
  }

  if (blinkRate > 0) {
    mBlinkTimer->InitWithNamedFuncCallback(CaretBlinkCallback, this, blinkRate,
                                           nsITimer::TYPE_REPEATING_SLACK,
                                           "nsCaret::CaretBlinkCallback_timer");
  }
}

void nsCaret::StopBlinking() {
  if (mBlinkTimer) {
    mBlinkTimer->Cancel();
    mBlinkRate = 0;
  }
}

size_t nsCaret::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t total = aMallocSizeOf(this);
  if (mPresShell) {
    // We only want the size of the nsWeakReference object, not the PresShell
    // (since we don't own the PresShell).
    total += mPresShell->SizeOfOnlyThis(aMallocSizeOf);
  }
  if (mBlinkTimer) {
    total += mBlinkTimer->SizeOfIncludingThis(aMallocSizeOf);
  }
  return total;
}

void nsCaret::ComputeCaretRects(nsIFrame* aFrame, int32_t aFrameOffset,
                                nsRect* aCaretRect, nsRect* aHookRect) {
  NS_ASSERTION(aFrame, "Should have a frame here");

  WritingMode wm = aFrame->GetWritingMode();
  bool isVertical = wm.IsVertical();

  nscoord bidiIndicatorSize;
  *aCaretRect = GetGeometryForFrame(aFrame, aFrameOffset, &bidiIndicatorSize);

  // Simon -- make a hook to draw to the left or right of the caret to show
  // keyboard language direction
  aHookRect->SetEmpty();
  if (!StaticPrefs::bidi_browser_ui()) {
    return;
  }

  bool isCaretRTL;
  nsIBidiKeyboard* bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  // if bidiKeyboard->IsLangRTL() fails, there is no way to tell the
  // keyboard direction, or the user has no right-to-left keyboard
  // installed, so we never draw the hook.
  if (bidiKeyboard && NS_SUCCEEDED(bidiKeyboard->IsLangRTL(&isCaretRTL))) {
    // If keyboard language is RTL, draw the hook on the left; if LTR, to the
    // right The height of the hook rectangle is the same as the width of the
    // caret rectangle.
    if (isVertical) {
      if (wm.IsSidewaysLR()) {
        aHookRect->SetRect(aCaretRect->x + bidiIndicatorSize,
                           aCaretRect->y + (!isCaretRTL ? bidiIndicatorSize * -1
                                                        : aCaretRect->height),
                           aCaretRect->height, bidiIndicatorSize);
      } else {
        aHookRect->SetRect(aCaretRect->XMost() - bidiIndicatorSize,
                           aCaretRect->y + (isCaretRTL ? bidiIndicatorSize * -1
                                                       : aCaretRect->height),
                           aCaretRect->height, bidiIndicatorSize);
      }
    } else {
      aHookRect->SetRect(aCaretRect->x + (isCaretRTL ? bidiIndicatorSize * -1
                                                     : aCaretRect->width),
                         aCaretRect->y + bidiIndicatorSize, bidiIndicatorSize,
                         aCaretRect->width);
    }
  }
}

/* static */
void nsCaret::CaretBlinkCallback(nsITimer* aTimer, void* aClosure) {
  nsCaret* theCaret = reinterpret_cast<nsCaret*>(aClosure);
  if (!theCaret) {
    return;
  }
  theCaret->mIsBlinkOn = !theCaret->mIsBlinkOn;
  theCaret->SchedulePaint();

  // mBlinkCount of -1 means blink count is not enabled.
  if (theCaret->mBlinkCount == -1) {
    return;
  }

  // Track the blink count, but only at end of a blink cycle.
  if (theCaret->mIsBlinkOn) {
    // If we exceeded the blink count, stop the timer.
    if (--theCaret->mBlinkCount <= 0) {
      theCaret->StopBlinking();
    }
  }
}

void nsCaret::SetIgnoreUserModify(bool aIgnoreUserModify) {
  mIgnoreUserModify = aIgnoreUserModify;
  SchedulePaint();
}
