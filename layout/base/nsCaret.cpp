/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#include "nsCaret.h"

#include <algorithm>

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMNode.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsRenderingContext.h"
#include "nsPresContext.h"
#include "nsBlockFrame.h"
#include "nsISelectionController.h"
#include "nsTextFrame.h"
#include "nsXULPopupManager.h"
#include "nsMenuPopupFrame.h"
#include "nsTextFragment.h"
#include "mozilla/Preferences.h"
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

/**
 * Find the first frame in an in-order traversal of the frame subtree rooted
 * at aFrame which is either a text frame logically at the end of a line,
 * or which is aStopAtFrame. Return null if no such frame is found. We don't
 * descend into the children of non-eLineParticipant frames.
 */
static nsIFrame*
CheckForTrailingTextFrameRecursive(nsIFrame* aFrame, nsIFrame* aStopAtFrame)
{
  if (aFrame == aStopAtFrame ||
      ((aFrame->GetType() == nsGkAtoms::textFrame &&
       (static_cast<nsTextFrame*>(aFrame))->IsAtEndOfLine())))
    return aFrame;
  if (!aFrame->IsFrameOfType(nsIFrame::eLineParticipant))
    return nullptr;

  for (nsIFrame* f = aFrame->GetFirstPrincipalChild(); f; f = f->GetNextSibling())
  {
    nsIFrame* r = CheckForTrailingTextFrameRecursive(f, aStopAtFrame);
    if (r)
      return r;
  }
  return nullptr;
}

static nsLineBox*
FindContainingLine(nsIFrame* aFrame)
{
  while (aFrame && aFrame->IsFrameOfType(nsIFrame::eLineParticipant))
  {
    nsIFrame* parent = aFrame->GetParent();
    nsBlockFrame* blockParent = nsLayoutUtils::GetAsBlock(parent);
    if (blockParent)
    {
      bool isValid;
      nsBlockInFlowLineIterator iter(blockParent, aFrame, &isValid);
      return isValid ? iter.GetLine().get() : nullptr;
    }
    aFrame = parent;
  }
  return nullptr;
}

static void
AdjustCaretFrameForLineEnd(nsIFrame** aFrame, int32_t* aOffset)
{
  nsLineBox* line = FindContainingLine(*aFrame);
  if (!line)
    return;
  int32_t count = line->GetChildCount();
  for (nsIFrame* f = line->mFirstChild; count > 0; --count, f = f->GetNextSibling())
  {
    nsIFrame* r = CheckForTrailingTextFrameRecursive(f, *aFrame);
    if (r == *aFrame)
      return;
    if (r)
    {
      *aFrame = r;
      NS_ASSERTION(r->GetType() == nsGkAtoms::textFrame, "Expected text frame");
      *aOffset = (static_cast<nsTextFrame*>(r))->GetContentEnd();
      return;
    }
  }
}

static bool
IsBidiUI()
{
  return Preferences::GetBool("bidi.browser.ui");
}

nsCaret::nsCaret()
: mOverrideOffset(0)
, mIsBlinkOn(false)
, mVisible(false)
, mReadOnly(false)
, mShowDuringSelection(false)
, mIgnoreUserModify(true)
{
}

nsCaret::~nsCaret()
{
  StopBlinking();
}

nsresult nsCaret::Init(nsIPresShell *inPresShell)
{
  NS_ENSURE_ARG(inPresShell);

  mPresShell = do_GetWeakReference(inPresShell);    // the presshell owns us, so no addref
  NS_ASSERTION(mPresShell, "Hey, pres shell should support weak refs");

  mShowDuringSelection =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ShowCaretDuringSelection,
                        mShowDuringSelection ? 1 : 0) != 0;

  // get the selection from the pres shell, and set ourselves up as a selection
  // listener

  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShell);
  if (!selCon)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelection> domSelection;
  nsresult rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                     getter_AddRefs(domSelection));
  if (NS_FAILED(rv))
    return rv;
  if (!domSelection)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelectionPrivate> privateSelection = do_QueryInterface(domSelection);
  if (privateSelection)
    privateSelection->AddSelectionListener(this);
  mDomSelectionWeak = do_GetWeakReference(domSelection);
  
  return NS_OK;
}

static bool
DrawCJKCaret(nsIFrame* aFrame, int32_t aOffset)
{
  nsIContent* content = aFrame->GetContent();
  const nsTextFragment* frag = content->GetText();
  if (!frag)
    return false;
  if (aOffset < 0 || uint32_t(aOffset) >= frag->GetLength())
    return false;
  char16_t ch = frag->CharAt(aOffset);
  return 0x2e80 <= ch && ch <= 0xd7ff;
}

nsCaret::Metrics
nsCaret::ComputeMetrics(nsIFrame* aFrame, int32_t aOffset, nscoord aCaretHeight)
{
  // Compute nominal sizes in appunits
  nscoord caretWidth =
    (aCaretHeight * LookAndFeel::GetFloat(LookAndFeel::eFloatID_CaretAspectRatio, 0.0f)) +
    nsPresContext::CSSPixelsToAppUnits(
        LookAndFeel::GetInt(LookAndFeel::eIntID_CaretWidth, 1));

  if (DrawCJKCaret(aFrame, aOffset)) {
    caretWidth += nsPresContext::CSSPixelsToAppUnits(1);
  }
  nscoord bidiIndicatorSize = nsPresContext::CSSPixelsToAppUnits(kMinBidiIndicatorPixels);
  bidiIndicatorSize = std::max(caretWidth, bidiIndicatorSize);

  // Round them to device pixels. Always round down, except that anything
  // between 0 and 1 goes up to 1 so we don't let the caret disappear.
  int32_t tpp = aFrame->PresContext()->AppUnitsPerDevPixel();
  Metrics result;
  result.mCaretWidth = NS_ROUND_BORDER_TO_PIXELS(caretWidth, tpp);
  result.mBidiIndicatorSize = NS_ROUND_BORDER_TO_PIXELS(bidiIndicatorSize, tpp);
  return result;
}

void nsCaret::Terminate()
{
  // this doesn't erase the caret if it's drawn. Should it? We might not have
  // a good drawing environment during teardown.
  
  StopBlinking();
  mBlinkTimer = nullptr;

  // unregiser ourselves as a selection listener
  nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSelection));
  if (privateSelection)
    privateSelection->RemoveSelectionListener(this);
  mDomSelectionWeak = nullptr;
  mPresShell = nullptr;

  mOverrideContent = nullptr;
}

NS_IMPL_ISUPPORTS(nsCaret, nsISelectionListener)

nsISelection* nsCaret::GetSelection()
{
  nsCOMPtr<nsISelection> sel(do_QueryReferent(mDomSelectionWeak));
  return sel;
}

void nsCaret::SetSelection(nsISelection *aDOMSel)
{
  MOZ_ASSERT(aDOMSel);
  mDomSelectionWeak = do_GetWeakReference(aDOMSel);   // weak reference to pres shell
  ResetBlinking();
  SchedulePaint();
}

void nsCaret::SetVisible(bool inMakeVisible)
{
  mVisible = inMakeVisible;
  mIgnoreUserModify = mVisible;
  ResetBlinking();
  SchedulePaint();
}

bool nsCaret::IsVisible()
{
  if (!mVisible) {
    return false;
  }

  if (!mShowDuringSelection) {
    Selection* selection = GetSelectionInternal();
    if (!selection) {
      return false;
    }
    bool isCollapsed;
    if (NS_FAILED(selection->GetIsCollapsed(&isCollapsed)) || !isCollapsed) {
      return false;
    }
  }

  if (IsMenuPopupHidingCaret()) {
    return false;
  }

  return true;
}

void nsCaret::SetCaretReadOnly(bool inMakeReadonly)
{
  mReadOnly = inMakeReadonly;
  ResetBlinking();
  SchedulePaint();
}

/* static */ nsRect
nsCaret::GetGeometryForFrame(nsIFrame* aFrame,
                             int32_t   aFrameOffset,
                             nscoord*  aBidiIndicatorSize)
{
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
  NS_ASSERTION(!(frame->GetStateBits() & NS_FRAME_IN_REFLOW),
               "We should not be in the middle of reflow");
  nscoord baseline = frame->GetCaretBaseline();
  nscoord ascent = 0, descent = 0;
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(aFrame));
  NS_ASSERTION(fm, "We should be able to get the font metrics");
  if (fm) {
    ascent = fm->MaxAscent();
    descent = fm->MaxDescent();
  }
  nscoord height = ascent + descent;
  bool vertical = aFrame->GetWritingMode().IsVertical();
  if (vertical) {
    framePos.x = baseline - ascent;
  } else {
    framePos.y = baseline - ascent;
  }
  Metrics caretMetrics = ComputeMetrics(aFrame, aFrameOffset, height);
  rect = nsRect(framePos, vertical ? nsSize(height, caretMetrics.mCaretWidth) :
                                     nsSize(caretMetrics.mCaretWidth, height));

  // Clamp the inline-position to be within our scroll frame. If we don't, then
  // it clips us, and we don't appear at all. See bug 335560.
  nsIFrame *scrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(aFrame, nsGkAtoms::scrollFrame);
  if (scrollFrame) {
    // First, use the scrollFrame to get at the scrollable view that we're in.
    nsIScrollableFrame *sf = do_QueryFrame(scrollFrame);
    nsIFrame *scrolled = sf->GetScrolledFrame();
    nsRect caretInScroll = rect + aFrame->GetOffsetTo(scrolled);

    // Now see if the caret extends beyond the view's bounds. If it does,
    // then snap it back, put it as close to the edge as it can.
    if (vertical) {
      nscoord overflow = caretInScroll.YMost() -
        scrolled->GetVisualOverflowRectRelativeToSelf().height;
      if (overflow > 0) {
        rect.y -= overflow;
      }
    } else {
      nscoord overflow = caretInScroll.XMost() -
        scrolled->GetVisualOverflowRectRelativeToSelf().width;
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

static nsIFrame*
GetFrameAndOffset(Selection* aSelection,
                  nsINode* aOverrideNode, int32_t aOverrideOffset,
                  int32_t* aFrameOffset)
{
  nsINode* focusNode;
  int32_t focusOffset;

  if (aOverrideNode) {
    focusNode = aOverrideNode;
    focusOffset = aOverrideOffset;
  } else if (aSelection) {
    focusNode = aSelection->GetFocusNode();
    aSelection->GetFocusOffset(&focusOffset);
  } else {
    return nullptr;
  }

  if (!focusNode || !focusNode->IsContent()) {
    return nullptr;
  }

  nsIContent* contentNode = focusNode->AsContent();
  nsFrameSelection* frameSelection = aSelection->GetFrameSelection();
  uint8_t bidiLevel = frameSelection->GetCaretBidiLevel();
  nsIFrame* frame;
  nsresult rv = nsCaret::GetCaretFrameForNodeOffset(
      frameSelection, contentNode, focusOffset,
      frameSelection->GetHint(), bidiLevel, &frame, aFrameOffset);
  if (NS_FAILED(rv) || !frame) {
    return nullptr;
  }

  return frame;
}

/* static */ nsIFrame*
nsCaret::GetGeometry(nsISelection* aSelection, nsRect* aRect)
{
  int32_t frameOffset;
  nsIFrame* frame = GetFrameAndOffset(
      static_cast<Selection*>(aSelection), nullptr, 0, &frameOffset);
  if (frame) {
    *aRect = GetGeometryForFrame(frame, frameOffset, nullptr);
  }
  return frame;
}

Selection*
nsCaret::GetSelectionInternal()
{
  return static_cast<Selection*>(GetSelection());
}

void nsCaret::SchedulePaint()
{
  Selection* selection = GetSelectionInternal();
  nsINode* focusNode;
  if (mOverrideContent) {
    focusNode = mOverrideContent;
  } else if (selection) {
    focusNode = selection->GetFocusNode();
  } else {
    return;
  }
  if (!focusNode || !focusNode->IsContent()) {
    return;
  }
  nsIFrame* f = focusNode->AsContent()->GetPrimaryFrame();
  if (!f) {
    return;
  }
  // This may not be the correct continuation frame, but that's OK since we're
  // just scheduling a paint of the window (or popup).
  f->SchedulePaint();
}

void nsCaret::SetVisibilityDuringSelection(bool aVisibility) 
{
  mShowDuringSelection = aVisibility;
  SchedulePaint();
}

void
nsCaret::SetCaretPosition(nsIDOMNode* aNode, int32_t aOffset)
{
  mOverrideContent = do_QueryInterface(aNode);
  mOverrideOffset = aOffset;

  ResetBlinking();
  SchedulePaint();
}

void
nsCaret::CheckSelectionLanguageChange()
{
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
  Selection* selection = GetSelectionInternal();
  if (selection) {
    selection->SelectionLanguageChange(isKeyboardRTL);
  }
}

nsIFrame*
nsCaret::GetPaintGeometry(nsRect* aRect)
{
  // Return null if we should not be visible.
  if (!IsVisible() || !mIsBlinkOn) {
    return nullptr;
  }

  // Update selection language direction now so the new direction will be
  // taken into account when computing the caret position below.
  CheckSelectionLanguageChange();

  int32_t frameOffset;
  nsIFrame *frame = GetFrameAndOffset(GetSelectionInternal(),
      mOverrideContent, mOverrideOffset, &frameOffset);
  if (!frame) {
    return nullptr;
  }

  // now we have a frame, check whether it's appropriate to show the caret here
  const nsStyleUserInterface* userinterface = frame->StyleUserInterface();
  if ((!mIgnoreUserModify &&
       userinterface->mUserModify == NS_STYLE_USER_MODIFY_READ_ONLY) ||
      userinterface->mUserInput == NS_STYLE_USER_INPUT_NONE ||
      userinterface->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
    return nullptr;
  }

  // If the offset falls outside of the frame, then don't paint the caret.
  int32_t startOffset, endOffset;
  if (frame->GetType() == nsGkAtoms::textFrame &&
      (NS_FAILED(frame->GetOffsets(startOffset, endOffset)) ||
      startOffset > frameOffset ||
      endOffset < frameOffset)) {
    return nullptr;
  }

  nsRect caretRect;
  nsRect hookRect;
  ComputeCaretRects(frame, frameOffset, &caretRect, &hookRect);

  aRect->UnionRect(caretRect, hookRect);
  return frame;
}

void nsCaret::PaintCaret(nsDisplayListBuilder *aBuilder,
                         nsRenderingContext *aCtx,
                         nsIFrame* aForFrame,
                         const nsPoint &aOffset)
{
  int32_t contentOffset;
  nsIFrame* frame = GetFrameAndOffset(GetSelectionInternal(),
    mOverrideContent, mOverrideOffset, &contentOffset);
  if (!frame) {
    return;
  }
  NS_ASSERTION(frame == aForFrame, "We're referring different frame");

  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();

  nsRect caretRect;
  nsRect hookRect;
  ComputeCaretRects(frame, contentOffset, &caretRect, &hookRect);

  Rect devPxCaretRect =
    NSRectToRect(caretRect + aOffset, appUnitsPerDevPixel, *drawTarget);
  Rect devPxHookRect =
    NSRectToRect(hookRect + aOffset, appUnitsPerDevPixel, *drawTarget);
  ColorPattern color(ToDeviceColor(frame->GetCaretColorAt(contentOffset)));

  drawTarget->FillRect(devPxCaretRect, color);
  if (!hookRect.IsEmpty()) {
    drawTarget->FillRect(devPxHookRect, color);
  }
}

NS_IMETHODIMP
nsCaret::NotifySelectionChanged(nsIDOMDocument *, nsISelection *aDomSel,
                                int16_t aReason)
{
  if (aReason & nsISelectionListener::MOUSEUP_REASON)//this wont do
    return NS_OK;

  nsCOMPtr<nsISelection> domSel(do_QueryReferent(mDomSelectionWeak));

  // The same caret is shared amongst the document and any text widgets it
  // may contain. This means that the caret could get notifications from
  // multiple selections.
  //
  // If this notification is for a selection that is not the one the
  // the caret is currently interested in (mDomSelectionWeak), then there
  // is nothing to do!

  if (domSel != aDomSel)
    return NS_OK;

  ResetBlinking();
  SchedulePaint();

  return NS_OK;
}

void nsCaret::ResetBlinking()
{
  mIsBlinkOn = true;

  if (mReadOnly || !mVisible) {
    StopBlinking();
    return;
  }

  if (mBlinkTimer) {
    mBlinkTimer->Cancel();
  } else {
    nsresult  err;
    mBlinkTimer = do_CreateInstance("@mozilla.org/timer;1", &err);
    if (NS_FAILED(err))
      return;
  }

  uint32_t blinkRate = static_cast<uint32_t>(
    LookAndFeel::GetInt(LookAndFeel::eIntID_CaretBlinkTime, 500));
  if (blinkRate > 0) {
    mBlinkTimer->InitWithFuncCallback(CaretBlinkCallback, this, blinkRate,
                                      nsITimer::TYPE_REPEATING_SLACK);
  }
}

void nsCaret::StopBlinking()
{
  if (mBlinkTimer)
  {
    mBlinkTimer->Cancel();
  }
}

nsresult 
nsCaret::GetCaretFrameForNodeOffset(nsFrameSelection*    aFrameSelection,
                                    nsIContent*          aContentNode,
                                    int32_t              aOffset,
                                    CaretAssociationHint aFrameHint,
                                    uint8_t              aBidiLevel,
                                    nsIFrame**           aReturnFrame,
                                    int32_t*             aReturnOffset)
{
  if (!aFrameSelection)
    return NS_ERROR_FAILURE;
  nsIPresShell* presShell = aFrameSelection->GetShell();
  if (!presShell)
    return NS_ERROR_FAILURE;

  if (!aContentNode || !aContentNode->IsInComposedDoc() ||
      presShell->GetDocument() != aContentNode->GetComposedDoc())
    return NS_ERROR_FAILURE;

  nsIFrame* theFrame = nullptr;
  int32_t   theFrameOffset = 0;

  theFrame = aFrameSelection->GetFrameForNodeOffset(
      aContentNode, aOffset, aFrameHint, &theFrameOffset);
  if (!theFrame)
    return NS_ERROR_FAILURE;

  // if theFrame is after a text frame that's logically at the end of the line
  // (e.g. if theFrame is a <br> frame), then put the caret at the end of
  // that text frame instead. This way, the caret will be positioned as if
  // trailing whitespace was not trimmed.
  AdjustCaretFrameForLineEnd(&theFrame, &theFrameOffset);
  
  // Mamdouh : modification of the caret to work at rtl and ltr with Bidi
  //
  // Direction Style from visibility->mDirection
  // ------------------
  // NS_STYLE_DIRECTION_LTR : LTR or Default
  // NS_STYLE_DIRECTION_RTL
  // NS_STYLE_DIRECTION_INHERIT
  if (IsBidiUI())
  {
    // If there has been a reflow, take the caret Bidi level to be the level of the current frame
    if (aBidiLevel & BIDI_LEVEL_UNDEFINED)
      aBidiLevel = NS_GET_EMBEDDING_LEVEL(theFrame);

    int32_t start;
    int32_t end;
    nsIFrame* frameBefore;
    nsIFrame* frameAfter;
    uint8_t levelBefore;     // Bidi level of the character before the caret
    uint8_t levelAfter;      // Bidi level of the character after the caret

    theFrame->GetOffsets(start, end);
    if (start == 0 || end == 0 || start == theFrameOffset || end == theFrameOffset)
    {
      nsPrevNextBidiLevels levels = aFrameSelection->
        GetPrevNextBidiLevels(aContentNode, aOffset, false);
    
      /* Boundary condition, we need to know the Bidi levels of the characters before and after the caret */
      if (levels.mFrameBefore || levels.mFrameAfter)
      {
        frameBefore = levels.mFrameBefore;
        frameAfter = levels.mFrameAfter;
        levelBefore = levels.mLevelBefore;
        levelAfter = levels.mLevelAfter;

        if ((levelBefore != levelAfter) || (aBidiLevel != levelBefore))
        {
          aBidiLevel = std::max(aBidiLevel, std::min(levelBefore, levelAfter));                                  // rule c3
          aBidiLevel = std::min(aBidiLevel, std::max(levelBefore, levelAfter));                                  // rule c4
          if (aBidiLevel == levelBefore                                                                      // rule c1
              || (aBidiLevel > levelBefore && aBidiLevel < levelAfter && !((aBidiLevel ^ levelBefore) & 1))    // rule c5
              || (aBidiLevel < levelBefore && aBidiLevel > levelAfter && !((aBidiLevel ^ levelBefore) & 1)))  // rule c9
          {
            if (theFrame != frameBefore)
            {
              if (frameBefore) // if there is a frameBefore, move into it
              {
                theFrame = frameBefore;
                theFrame->GetOffsets(start, end);
                theFrameOffset = end;
              }
              else 
              {
                // if there is no frameBefore, we must be at the beginning of the line
                // so we stay with the current frame.
                // Exception: when the first frame on the line has a different Bidi level from the paragraph level, there is no
                // real frame for the caret to be in. We have to find the visually first frame on the line.
                uint8_t baseLevel = NS_GET_BASE_LEVEL(frameAfter);
                if (baseLevel != levelAfter)
                {
                  nsPeekOffsetStruct pos(eSelectBeginLine, eDirPrevious, 0, 0, false, true, false, true);
                  if (NS_SUCCEEDED(frameAfter->PeekOffset(&pos))) {
                    theFrame = pos.mResultFrame;
                    theFrameOffset = pos.mContentOffset;
                  }
                }
              }
            }
          }
          else if (aBidiLevel == levelAfter                                                                     // rule c2
                   || (aBidiLevel > levelBefore && aBidiLevel < levelAfter && !((aBidiLevel ^ levelAfter) & 1))   // rule c6
                   || (aBidiLevel < levelBefore && aBidiLevel > levelAfter && !((aBidiLevel ^ levelAfter) & 1)))  // rule c10
          {
            if (theFrame != frameAfter)
            {
              if (frameAfter)
              {
                // if there is a frameAfter, move into it
                theFrame = frameAfter;
                theFrame->GetOffsets(start, end);
                theFrameOffset = start;
              }
              else 
              {
                // if there is no frameAfter, we must be at the end of the line
                // so we stay with the current frame.
                // Exception: when the last frame on the line has a different Bidi level from the paragraph level, there is no
                // real frame for the caret to be in. We have to find the visually last frame on the line.
                uint8_t baseLevel = NS_GET_BASE_LEVEL(frameBefore);
                if (baseLevel != levelBefore)
                {
                  nsPeekOffsetStruct pos(eSelectEndLine, eDirNext, 0, 0, false, true, false, true);
                  if (NS_SUCCEEDED(frameBefore->PeekOffset(&pos))) {
                    theFrame = pos.mResultFrame;
                    theFrameOffset = pos.mContentOffset;
                  }
                }
              }
            }
          }
          else if (aBidiLevel > levelBefore && aBidiLevel < levelAfter  // rule c7/8
                   && !((levelBefore ^ levelAfter) & 1)                 // before and after have the same parity
                   && ((aBidiLevel ^ levelAfter) & 1))                  // caret has different parity
          {
            if (NS_SUCCEEDED(aFrameSelection->GetFrameFromLevel(frameAfter, eDirNext, aBidiLevel, &theFrame)))
            {
              theFrame->GetOffsets(start, end);
              levelAfter = NS_GET_EMBEDDING_LEVEL(theFrame);
              if (aBidiLevel & 1) // c8: caret to the right of the rightmost character
                theFrameOffset = (levelAfter & 1) ? start : end;
              else               // c7: caret to the left of the leftmost character
                theFrameOffset = (levelAfter & 1) ? end : start;
            }
          }
          else if (aBidiLevel < levelBefore && aBidiLevel > levelAfter  // rule c11/12
                   && !((levelBefore ^ levelAfter) & 1)                 // before and after have the same parity
                   && ((aBidiLevel ^ levelAfter) & 1))                  // caret has different parity
          {
            if (NS_SUCCEEDED(aFrameSelection->GetFrameFromLevel(frameBefore, eDirPrevious, aBidiLevel, &theFrame)))
            {
              theFrame->GetOffsets(start, end);
              levelBefore = NS_GET_EMBEDDING_LEVEL(theFrame);
              if (aBidiLevel & 1) // c12: caret to the left of the leftmost character
                theFrameOffset = (levelBefore & 1) ? end : start;
              else               // c11: caret to the right of the rightmost character
                theFrameOffset = (levelBefore & 1) ? start : end;
            }
          }   
        }
      }
    }
  }

  *aReturnFrame = theFrame;
  *aReturnOffset = theFrameOffset;
  return NS_OK;
}

size_t nsCaret::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t total = aMallocSizeOf(this);
  if (mPresShell) {
    // We only want the size of the nsWeakReference object, not the PresShell
    // (since we don't own the PresShell).
    total += mPresShell->SizeOfOnlyThis(aMallocSizeOf);
  }
  if (mDomSelectionWeak) {
    // We only want size of the nsWeakReference object, not the selection
    // (again, we don't own the selection).
    total += mDomSelectionWeak->SizeOfOnlyThis(aMallocSizeOf);
  }
  if (mBlinkTimer) {
    total += mBlinkTimer->SizeOfIncludingThis(aMallocSizeOf);
  }
  return total;
}

bool nsCaret::IsMenuPopupHidingCaret()
{
#ifdef MOZ_XUL
  // Check if there are open popups.
  nsXULPopupManager *popMgr = nsXULPopupManager::GetInstance();
  nsTArray<nsIFrame*> popups;
  popMgr->GetVisiblePopups(popups);

  if (popups.Length() == 0)
    return false; // No popups, so caret can't be hidden by them.

  // Get the selection focus content, that's where the caret would 
  // go if it was drawn.
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  if (!domSelection)
    return true; // No selection/caret to draw.
  domSelection->GetFocusNode(getter_AddRefs(node));
  if (!node)
    return true; // No selection/caret to draw.
  nsCOMPtr<nsIContent> caretContent = do_QueryInterface(node);
  if (!caretContent)
    return true; // No selection/caret to draw.

  // If there's a menu popup open before the popup with
  // the caret, don't show the caret.
  for (uint32_t i=0; i<popups.Length(); i++) {
    nsMenuPopupFrame* popupFrame = static_cast<nsMenuPopupFrame*>(popups[i]);
    nsIContent* popupContent = popupFrame->GetContent();

    if (nsContentUtils::ContentIsDescendantOf(caretContent, popupContent)) {
      // The caret is in this popup. There were no menu popups before this
      // popup, so don't hide the caret.
      return false;
    }

    if (popupFrame->PopupType() == ePopupTypeMenu && !popupFrame->IsContextMenu()) {
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

void
nsCaret::ComputeCaretRects(nsIFrame* aFrame, int32_t aFrameOffset,
                           nsRect* aCaretRect, nsRect* aHookRect)
{
  NS_ASSERTION(aFrame, "Should have a frame here");

  bool isVertical = aFrame->GetWritingMode().IsVertical();

  nscoord bidiIndicatorSize;
  *aCaretRect = GetGeometryForFrame(aFrame, aFrameOffset, &bidiIndicatorSize);

  // on RTL frames the right edge of mCaretRect must be equal to framePos
  const nsStyleVisibility* vis = aFrame->StyleVisibility();
  if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
    if (isVertical) {
      aCaretRect->y -= aCaretRect->height;
    } else {
      aCaretRect->x -= aCaretRect->width;
    }
  }

  // Simon -- make a hook to draw to the left or right of the caret to show keyboard language direction
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
    // If keyboard language is RTL, draw the hook on the left; if LTR, to the right
    // The height of the hook rectangle is the same as the width of the caret
    // rectangle.
    if (isVertical) {
      aHookRect->SetRect(aCaretRect->XMost() - bidiIndicatorSize,
                         aCaretRect->y + (isCaretRTL ? bidiIndicatorSize * -1 :
                                                       aCaretRect->height),
                         aCaretRect->height,
                         bidiIndicatorSize);
    } else {
      aHookRect->SetRect(aCaretRect->x + (isCaretRTL ? bidiIndicatorSize * -1 :
                                                       aCaretRect->width),
                         aCaretRect->y + bidiIndicatorSize,
                         bidiIndicatorSize,
                         aCaretRect->width);
    }
  }
}

/* static */
void nsCaret::CaretBlinkCallback(nsITimer* aTimer, void* aClosure)
{
  nsCaret* theCaret = reinterpret_cast<nsCaret*>(aClosure);
  if (!theCaret) {
    return;
  }
  theCaret->mIsBlinkOn = !theCaret->mIsBlinkOn;
  theCaret->SchedulePaint();
}

void
nsCaret::SetIgnoreUserModify(bool aIgnoreUserModify)
{
  mIgnoreUserModify = aIgnoreUserModify;
  SchedulePaint();
}
