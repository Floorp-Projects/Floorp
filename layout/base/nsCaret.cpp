/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#include "nsCaret.h"

#include <algorithm>

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsITimer.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
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

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

// The bidi indicator hangs off the caret to one side, to show which
// direction the typing is in. It needs to be at least 2x2 to avoid looking like
// an insignificant dot
static const int32_t kMinBidiIndicatorPixels = 2;

// The default caret blinking rate (in ms of blinking interval)
static const uint32_t kDefaultCaretBlinkRate = 500;

/**
 * Find the first frame in an in-order traversal of the frame subtree rooted
 * at aFrame which is either a text frame logically at the end of a line,
 * or which is aStopAtFrame. Return null if no such frame is found. We don't
 * descend into the children of non-eLineParticipant frames.
 */
static nsIFrame* CheckForTrailingTextFrameRecursive(nsIFrame* aFrame,
                                                    nsIFrame* aStopAtFrame) {
  if (aFrame == aStopAtFrame ||
      ((aFrame->IsTextFrame() &&
        (static_cast<nsTextFrame*>(aFrame))->IsAtEndOfLine())))
    return aFrame;
  if (!aFrame->IsFrameOfType(nsIFrame::eLineParticipant)) return nullptr;

  for (nsIFrame* f : aFrame->PrincipalChildList()) {
    nsIFrame* r = CheckForTrailingTextFrameRecursive(f, aStopAtFrame);
    if (r) return r;
  }
  return nullptr;
}

static nsLineBox* FindContainingLine(nsIFrame* aFrame) {
  while (aFrame && aFrame->IsFrameOfType(nsIFrame::eLineParticipant)) {
    nsIFrame* parent = aFrame->GetParent();
    nsBlockFrame* blockParent = do_QueryFrame(parent);
    if (blockParent) {
      bool isValid;
      nsBlockInFlowLineIterator iter(blockParent, aFrame, &isValid);
      return isValid ? iter.GetLine().get() : nullptr;
    }
    aFrame = parent;
  }
  return nullptr;
}

static void AdjustCaretFrameForLineEnd(nsIFrame** aFrame, int32_t* aOffset) {
  nsLineBox* line = FindContainingLine(*aFrame);
  if (!line) {
    return;
  }
  int32_t count = line->GetChildCount();
  for (nsIFrame* f = line->mFirstChild; count > 0;
       --count, f = f->GetNextSibling()) {
    nsIFrame* r = CheckForTrailingTextFrameRecursive(f, *aFrame);
    if (r == *aFrame) {
      return;
    }
    if (r) {
      // We found our frame, but we may not be able to properly paint the caret
      // if -moz-user-modify differs from our actual frame.
      MOZ_ASSERT(r->IsTextFrame(), "Expected text frame");
      *aFrame = r;
      *aOffset = (static_cast<nsTextFrame*>(r))->GetContentEnd();
      return;
    }
  }
}

static bool IsBidiUI() { return Preferences::GetBool("bidi.browser.ui"); }

nsCaret::nsCaret()
    : mOverrideOffset(0),
      mBlinkCount(-1),
      mBlinkRate(0),
      mHideCount(0),
      mIsBlinkOn(false),
      mVisible(false),
      mReadOnly(false),
      mShowDuringSelection(false),
      mIgnoreUserModify(true) {}

nsCaret::~nsCaret() { StopBlinking(); }

nsresult nsCaret::Init(PresShell* aPresShell) {
  NS_ENSURE_ARG(aPresShell);

  mPresShell =
      do_GetWeakReference(aPresShell);  // the presshell owns us, so no addref
  NS_ASSERTION(mPresShell, "Hey, pres shell should support weak refs");

  mShowDuringSelection =
      LookAndFeel::GetInt(LookAndFeel::IntID::ShowCaretDuringSelection,
                          mShowDuringSelection ? 1 : 0) != 0;

  RefPtr<Selection> selection =
      aPresShell->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  selection->AddSelectionListener(this);
  mDomSelectionWeak = selection;

  return NS_OK;
}

static bool DrawCJKCaret(nsIFrame* aFrame, int32_t aOffset) {
  nsIContent* content = aFrame->GetContent();
  const nsTextFragment* frag = content->GetText();
  if (!frag) return false;
  if (aOffset < 0 || uint32_t(aOffset) >= frag->GetLength()) return false;
  char16_t ch = frag->CharAt(aOffset);
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

  mOverrideContent = nullptr;
}

NS_IMPL_ISUPPORTS(nsCaret, nsISelectionListener)

Selection* nsCaret::GetSelection() { return mDomSelectionWeak; }

void nsCaret::SetSelection(Selection* aDOMSel) {
  MOZ_ASSERT(aDOMSel);
  mDomSelectionWeak = aDOMSel;
  ResetBlinking();
  SchedulePaint(aDOMSel);
}

void nsCaret::SetVisible(bool inMakeVisible) {
  mVisible = inMakeVisible;
  mIgnoreUserModify = mVisible;
  ResetBlinking();
  SchedulePaint();
}

void nsCaret::AddForceHide() {
  MOZ_ASSERT(mHideCount < UINT32_MAX);
  if (++mHideCount > 1) {
    return;
  }
  ResetBlinking();
  SchedulePaint();
}

void nsCaret::RemoveForceHide() {
  if (!mHideCount || --mHideCount) {
    return;
  }
  ResetBlinking();
  SchedulePaint();
}

void nsCaret::SetCaretReadOnly(bool inMakeReadonly) {
  mReadOnly = inMakeReadonly;
  ResetBlinking();
  SchedulePaint();
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
  nscoord baseline = frame->GetCaretBaseline();
  nscoord ascent = 0, descent = 0;
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(aFrame);
  NS_ASSERTION(fm, "We should be able to get the font metrics");
  if (fm) {
    ascent = fm->MaxAscent();
    descent = fm->MaxDescent();
  }
  nscoord height = ascent + descent;
  WritingMode wm = aFrame->GetWritingMode();
  bool vertical = wm.IsVertical();
  if (vertical) {
    if (wm.IsLineInverted()) {
      framePos.x = baseline - descent;
    } else {
      framePos.x = baseline - ascent;
    }
  } else {
    framePos.y = baseline - ascent;
  }
  Metrics caretMetrics = ComputeMetrics(aFrame, aFrameOffset, height);

  nsTextFrame* textFrame = do_QueryFrame(aFrame);
  if (textFrame) {
    gfxTextRun* textRun =
        textFrame->GetTextRun(nsTextFrame::TextRunType::eInflated);
    if (textRun) {
      // For "upstream" text where the textrun direction is reversed from the
      // frame's inline-dir we want the caret to be painted before rather than
      // after its nominal inline position, so we offset by its width.
      bool textRunDirIsReverseOfFrame =
          wm.IsInlineReversed() != textRun->IsInlineReversed();
      // However, in sideways-lr mode we invert this behavior because this is
      // the one writing mode where bidi-LTR corresponds to inline-reversed
      // already, which reverses the desired caret placement behavior.
      // Note that the following condition is equivalent to:
      //   if ( (!textRun->IsSidewaysLeft() && textRunDirIsReverseOfFrame) ||
      //        (textRun->IsSidewaysLeft()  && !textRunDirIsReverseOfFrame) )
      if (textRunDirIsReverseOfFrame != textRun->IsSidewaysLeft()) {
        int dir = wm.IsBidiLTR() ? -1 : 1;
        if (vertical) {
          framePos.y += dir * caretMetrics.mCaretWidth;
        } else {
          framePos.x += dir * caretMetrics.mCaretWidth;
        }
      }
    }
  }

  rect = nsRect(framePos, vertical ? nsSize(height, caretMetrics.mCaretWidth)
                                   : nsSize(caretMetrics.mCaretWidth, height));

  // Clamp the inline-position to be within our scroll frame. If we don't, then
  // it clips us, and we don't appear at all. See bug 335560.

  // Find the ancestor scroll frame and determine whether we have any transforms
  // up the ancestor chain.
  bool hasTransform = false;
  nsIFrame* scrollFrame = nullptr;
  for (nsIFrame* f = aFrame; f; f = f->GetParent()) {
    if (f->IsScrollFrame()) {
      scrollFrame = f;
      break;
    }
    if (f->IsTransformed()) {
      hasTransform = true;
    }
  }

  // FIXME(heycam): Skip clamping if we find any transform up the ancestor
  // chain, since the GetOffsetTo call below doesn't take transforms into
  // account. We could change this clamping to take transforms into account, but
  // the clamping seems to be broken anyway; see bug 1539720.
  if (scrollFrame && !hasTransform) {
    // First, use the scrollFrame to get at the scrollable view that we're in.
    nsIScrollableFrame* sf = do_QueryFrame(scrollFrame);
    nsIFrame* scrolled = sf->GetScrolledFrame();
    nsRect caretInScroll = rect + aFrame->GetOffsetTo(scrolled);

    // Now see if the caret extends beyond the view's bounds. If it does,
    // then snap it back, put it as close to the edge as it can.
    if (vertical) {
      nscoord overflow = caretInScroll.YMost() -
                         scrolled->InkOverflowRectRelativeToSelf().height;
      if (overflow > 0) {
        rect.y -= overflow;
      }
    } else {
      nscoord overflow = caretInScroll.XMost() -
                         scrolled->InkOverflowRectRelativeToSelf().width;
      if (overflow > 0) {
        rect.x -= overflow;
      }
    }
  }

  if (aBidiIndicatorSize) {
    *aBidiIndicatorSize = caretMetrics.mBidiIndicatorSize;
  }
  return rect;
}

nsIFrame* nsCaret::GetFrameAndOffset(const Selection* aSelection,
                                     nsINode* aOverrideNode,
                                     int32_t aOverrideOffset,
                                     int32_t* aFrameOffset,
                                     nsIFrame** aUnadjustedFrame) {
  if (aUnadjustedFrame) {
    *aUnadjustedFrame = nullptr;
  }

  nsINode* focusNode;
  int32_t focusOffset;

  if (aOverrideNode) {
    focusNode = aOverrideNode;
    focusOffset = aOverrideOffset;
  } else if (aSelection) {
    focusNode = aSelection->GetFocusNode();
    focusOffset = aSelection->FocusOffset();
  } else {
    return nullptr;
  }

  if (!focusNode || !focusNode->IsContent() || !aSelection) {
    return nullptr;
  }

  nsIContent* contentNode = focusNode->AsContent();
  nsFrameSelection* frameSelection = aSelection->GetFrameSelection();
  nsBidiLevel bidiLevel = frameSelection->GetCaretBidiLevel();

  return nsCaret::GetCaretFrameForNodeOffset(
      frameSelection, contentNode, focusOffset, frameSelection->GetHint(),
      bidiLevel, aUnadjustedFrame, aFrameOffset);
}

/* static */
nsIFrame* nsCaret::GetGeometry(const Selection* aSelection, nsRect* aRect) {
  int32_t frameOffset;
  nsIFrame* frame = GetFrameAndOffset(aSelection, nullptr, 0, &frameOffset);
  if (frame) {
    *aRect = GetGeometryForFrame(frame, frameOffset, nullptr);
  }
  return frame;
}

void nsCaret::SchedulePaint(Selection* aSelection) {
  Selection* selection;
  if (aSelection) {
    selection = aSelection;
  } else {
    selection = GetSelection();
  }

  int32_t frameOffset;
  nsIFrame* frame = GetFrameAndOffset(selection, mOverrideContent,
                                      mOverrideOffset, &frameOffset);

  if (frame) {
    frame->SchedulePaint();
  }
}

void nsCaret::SetVisibilityDuringSelection(bool aVisibility) {
  mShowDuringSelection = aVisibility;
  SchedulePaint();
}

void nsCaret::SetCaretPosition(nsINode* aNode, int32_t aOffset) {
  mOverrideContent = aNode;
  mOverrideOffset = aOffset;

  ResetBlinking();
  SchedulePaint();
}

void nsCaret::CheckSelectionLanguageChange() {
  if (!IsBidiUI()) {
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
  if (aFrame->IsBlockOutside() || aFrame->IsBlockFrameOrSubclass()) {
    return aFrame;
  }
  nsIFrame* containingBlock = aFrame->GetContainingBlock();
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

  int32_t frameOffset;
  nsIFrame* unadjustedFrame = nullptr;
  nsIFrame* frame =
      GetFrameAndOffset(GetSelection(), mOverrideContent, mOverrideOffset,
                        &frameOffset, &unadjustedFrame);
  MOZ_ASSERT(!!frame == !!unadjustedFrame);
  if (!frame) {
    return nullptr;
  }

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
  if ((!mIgnoreUserModify && ui->mUserModify == StyleUserModify::ReadOnly) ||
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
nsCaret::NotifySelectionChanged(Document*, Selection* aDomSel,
                                int16_t aReason) {
  // Note that aDomSel, per the comment below may not be the same as our
  // selection, but that's OK since if that is the case, it wouldn't have
  // mattered what IsVisible() returns here, so we just opt for checking
  // the selection later down below.
  if ((aReason & nsISelectionListener::MOUSEUP_REASON) ||
      !IsVisible(aDomSel))  // this wont do
    return NS_OK;

  // The same caret is shared amongst the document and any text widgets it
  // may contain. This means that the caret could get notifications from
  // multiple selections.
  //
  // If this notification is for a selection that is not the one the
  // the caret is currently interested in (mDomSelectionWeak), then there
  // is nothing to do!

  if (mDomSelectionWeak != aDomSel) return NS_OK;

  ResetBlinking();
  SchedulePaint(aDomSel);

  return NS_OK;
}

void nsCaret::ResetBlinking() {
  mIsBlinkOn = true;

  if (mReadOnly || !mVisible || mHideCount) {
    StopBlinking();
    return;
  }

  uint32_t blinkRate = static_cast<uint32_t>(LookAndFeel::GetInt(
      LookAndFeel::IntID::CaretBlinkTime, kDefaultCaretBlinkRate));
  if (mBlinkRate == blinkRate) {
    // If the rate hasn't changed, then there is nothing to do.
    return;
  }
  mBlinkRate = blinkRate;

  if (mBlinkTimer) {
    mBlinkTimer->Cancel();
  } else {
    nsIEventTarget* target = nullptr;
    if (RefPtr<PresShell> presShell = do_QueryReferent(mPresShell)) {
      if (nsCOMPtr<Document> doc = presShell->GetDocument()) {
        target = doc->EventTargetFor(TaskCategory::Other);
      }
    }

    mBlinkTimer = NS_NewTimer(target);
    if (!mBlinkTimer) {
      return;
    }
  }

  if (blinkRate > 0) {
    mBlinkCount = LookAndFeel::GetInt(LookAndFeel::IntID::CaretBlinkCount, -1);
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

nsIFrame* nsCaret::GetCaretFrameForNodeOffset(
    nsFrameSelection* aFrameSelection, nsIContent* aContentNode,
    int32_t aOffset, CaretAssociationHint aFrameHint, nsBidiLevel aBidiLevel,
    nsIFrame** aReturnUnadjustedFrame, int32_t* aReturnOffset) {
  if (!aFrameSelection) {
    return nullptr;
  }

  PresShell* presShell = aFrameSelection->GetPresShell();
  if (!presShell) {
    return nullptr;
  }

  if (!aContentNode || !aContentNode->IsInComposedDoc() ||
      presShell->GetDocument() != aContentNode->GetComposedDoc()) {
    return nullptr;
  }

  nsIFrame* theFrame = nullptr;
  int32_t theFrameOffset = 0;

  theFrame = nsFrameSelection::GetFrameForNodeOffset(
      aContentNode, aOffset, aFrameHint, &theFrameOffset);
  if (!theFrame) {
    return nullptr;
  }

  if (aReturnUnadjustedFrame) {
    *aReturnUnadjustedFrame = theFrame;
  }

  if (nsFrameSelection::AdjustFrameForLineStart(theFrame, theFrameOffset)) {
    aFrameSelection->SetHint(CARET_ASSOCIATE_AFTER);
  } else {
    // if theFrame is after a text frame that's logically at the end of the line
    // (e.g. if theFrame is a <br> frame), then put the caret at the end of
    // that text frame instead. This way, the caret will be positioned as if
    // trailing whitespace was not trimmed.
    AdjustCaretFrameForLineEnd(&theFrame, &theFrameOffset);
  }

  // Mamdouh : modification of the caret to work at rtl and ltr with Bidi
  //
  // Direction Style from visibility->mDirection
  // ------------------
  if (theFrame->PresContext()->BidiEnabled()) {
    // If there has been a reflow, take the caret Bidi level to be the level of
    // the current frame
    if (aBidiLevel & BIDI_LEVEL_UNDEFINED) {
      aBidiLevel = theFrame->GetEmbeddingLevel();
    }

    nsIFrame* frameBefore;
    nsIFrame* frameAfter;
    nsBidiLevel levelBefore;  // Bidi level of the character before the caret
    nsBidiLevel levelAfter;   // Bidi level of the character after the caret

    auto [start, end] = theFrame->GetOffsets();
    if (start == 0 || end == 0 || start == theFrameOffset ||
        end == theFrameOffset) {
      nsPrevNextBidiLevels levels =
          aFrameSelection->GetPrevNextBidiLevels(aContentNode, aOffset, false);

      /* Boundary condition, we need to know the Bidi levels of the characters
       * before and after the caret */
      if (levels.mFrameBefore || levels.mFrameAfter) {
        frameBefore = levels.mFrameBefore;
        frameAfter = levels.mFrameAfter;
        levelBefore = levels.mLevelBefore;
        levelAfter = levels.mLevelAfter;

        if ((levelBefore != levelAfter) || (aBidiLevel != levelBefore)) {
          aBidiLevel = std::max(aBidiLevel,
                                std::min(levelBefore, levelAfter));  // rule c3
          aBidiLevel = std::min(aBidiLevel,
                                std::max(levelBefore, levelAfter));  // rule c4
          if (aBidiLevel == levelBefore ||                           // rule c1
              (aBidiLevel > levelBefore && aBidiLevel < levelAfter &&
               IS_SAME_DIRECTION(aBidiLevel, levelBefore)) ||  // rule c5
              (aBidiLevel < levelBefore && aBidiLevel > levelAfter &&
               IS_SAME_DIRECTION(aBidiLevel, levelBefore)))  // rule c9
          {
            if (theFrame != frameBefore) {
              if (frameBefore) {  // if there is a frameBefore, move into it
                theFrame = frameBefore;
                std::tie(start, end) = theFrame->GetOffsets();
                theFrameOffset = end;
              } else {
                // if there is no frameBefore, we must be at the beginning of
                // the line so we stay with the current frame. Exception: when
                // the first frame on the line has a different Bidi level from
                // the paragraph level, there is no real frame for the caret to
                // be in. We have to find the visually first frame on the line.
                nsBidiLevel baseLevel = frameAfter->GetBaseLevel();
                if (baseLevel != levelAfter) {
                  nsPeekOffsetStruct pos(eSelectBeginLine, eDirPrevious, 0,
                                         nsPoint(0, 0), false, true, false,
                                         true, false);
                  if (NS_SUCCEEDED(frameAfter->PeekOffset(&pos))) {
                    theFrame = pos.mResultFrame;
                    theFrameOffset = pos.mContentOffset;
                  }
                }
              }
            }
          } else if (aBidiLevel == levelAfter ||  // rule c2
                     (aBidiLevel > levelBefore && aBidiLevel < levelAfter &&
                      IS_SAME_DIRECTION(aBidiLevel, levelAfter)) ||  // rule c6
                     (aBidiLevel < levelBefore && aBidiLevel > levelAfter &&
                      IS_SAME_DIRECTION(aBidiLevel, levelAfter)))  // rule c10
          {
            if (theFrame != frameAfter) {
              if (frameAfter) {
                // if there is a frameAfter, move into it
                theFrame = frameAfter;
                std::tie(start, end) = theFrame->GetOffsets();
                theFrameOffset = start;
              } else {
                // if there is no frameAfter, we must be at the end of the line
                // so we stay with the current frame.
                // Exception: when the last frame on the line has a different
                // Bidi level from the paragraph level, there is no real frame
                // for the caret to be in. We have to find the visually last
                // frame on the line.
                nsBidiLevel baseLevel = frameBefore->GetBaseLevel();
                if (baseLevel != levelBefore) {
                  nsPeekOffsetStruct pos(eSelectEndLine, eDirNext, 0,
                                         nsPoint(0, 0), false, true, false,
                                         true, false);
                  if (NS_SUCCEEDED(frameBefore->PeekOffset(&pos))) {
                    theFrame = pos.mResultFrame;
                    theFrameOffset = pos.mContentOffset;
                  }
                }
              }
            }
          } else if (aBidiLevel > levelBefore &&
                     aBidiLevel < levelAfter &&  // rule c7/8
                     // before and after have the same parity
                     IS_SAME_DIRECTION(levelBefore, levelAfter) &&
                     // caret has different parity
                     !IS_SAME_DIRECTION(aBidiLevel, levelAfter)) {
            if (NS_SUCCEEDED(aFrameSelection->GetFrameFromLevel(
                    frameAfter, eDirNext, aBidiLevel, &theFrame))) {
              std::tie(start, end) = theFrame->GetOffsets();
              levelAfter = theFrame->GetEmbeddingLevel();
              if (IS_LEVEL_RTL(aBidiLevel))  // c8: caret to the right of the
                                             // rightmost character
                theFrameOffset = IS_LEVEL_RTL(levelAfter) ? start : end;
              else  // c7: caret to the left of the leftmost character
                theFrameOffset = IS_LEVEL_RTL(levelAfter) ? end : start;
            }
          } else if (aBidiLevel < levelBefore &&
                     aBidiLevel > levelAfter &&  // rule c11/12
                     // before and after have the same parity
                     IS_SAME_DIRECTION(levelBefore, levelAfter) &&
                     // caret has different parity
                     !IS_SAME_DIRECTION(aBidiLevel, levelAfter)) {
            if (NS_SUCCEEDED(aFrameSelection->GetFrameFromLevel(
                    frameBefore, eDirPrevious, aBidiLevel, &theFrame))) {
              std::tie(start, end) = theFrame->GetOffsets();
              levelBefore = theFrame->GetEmbeddingLevel();
              if (IS_LEVEL_RTL(aBidiLevel))  // c12: caret to the left of the
                                             // leftmost character
                theFrameOffset = IS_LEVEL_RTL(levelBefore) ? end : start;
              else  // c11: caret to the right of the rightmost character
                theFrameOffset = IS_LEVEL_RTL(levelBefore) ? start : end;
            }
          }
        }
      }
    }
  }

  *aReturnOffset = theFrameOffset;
  return theFrame;
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

bool nsCaret::IsMenuPopupHidingCaret() {
#ifdef MOZ_XUL
  // Check if there are open popups.
  nsXULPopupManager* popMgr = nsXULPopupManager::GetInstance();
  nsTArray<nsIFrame*> popups;
  popMgr->GetVisiblePopups(popups);

  if (popups.Length() == 0)
    return false;  // No popups, so caret can't be hidden by them.

  // Get the selection focus content, that's where the caret would
  // go if it was drawn.
  if (!mDomSelectionWeak) {
    return true;  // No selection/caret to draw.
  }
  nsCOMPtr<nsIContent> caretContent =
      nsIContent::FromNodeOrNull(mDomSelectionWeak->GetFocusNode());
  if (!caretContent) return true;  // No selection/caret to draw.

  // If there's a menu popup open before the popup with
  // the caret, don't show the caret.
  for (uint32_t i = 0; i < popups.Length(); i++) {
    nsMenuPopupFrame* popupFrame = static_cast<nsMenuPopupFrame*>(popups[i]);
    nsIContent* popupContent = popupFrame->GetContent();

    if (caretContent->IsInclusiveDescendantOf(popupContent)) {
      // The caret is in this popup. There were no menu popups before this
      // popup, so don't hide the caret.
      return false;
    }

    if (popupFrame->PopupType() == ePopupTypeMenu &&
        !popupFrame->IsContextMenu()) {
      // This is an open menu popup. It does not contain the caret (else we'd
      // have returned above). Even if the caret is in a subsequent popup,
      // or another document/frame, it should be hidden.
      return true;
    }
  }
#endif

  // There are no open menu popups, no need to hide the caret.
  return false;
}

void nsCaret::ComputeCaretRects(nsIFrame* aFrame, int32_t aFrameOffset,
                                nsRect* aCaretRect, nsRect* aHookRect) {
  NS_ASSERTION(aFrame, "Should have a frame here");

  WritingMode wm = aFrame->GetWritingMode();
  bool isVertical = wm.IsVertical();

  nscoord bidiIndicatorSize;
  *aCaretRect = GetGeometryForFrame(aFrame, aFrameOffset, &bidiIndicatorSize);

  // on RTL frames the right edge of mCaretRect must be equal to framePos
  const nsStyleVisibility* vis = aFrame->StyleVisibility();
  if (StyleDirection::Rtl == vis->mDirection) {
    if (isVertical) {
      aCaretRect->y -= aCaretRect->height;
    } else {
      aCaretRect->x -= aCaretRect->width;
    }
  }

  // Simon -- make a hook to draw to the left or right of the caret to show
  // keyboard language direction
  aHookRect->SetEmpty();
  if (!IsBidiUI()) {
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
