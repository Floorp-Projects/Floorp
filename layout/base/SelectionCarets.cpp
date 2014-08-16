/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SelectionCarets.h"

#include "gfxPrefs.h"
#include "nsBidiPresUtils.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsDOMTokenList.h"
#include "nsFrame.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeFilter.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsView.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/TreeWalker.h"
#include "mozilla/Preferences.h"
#include "mozilla/TouchEvents.h"
#include "TouchCaret.h"
#include "nsFrameSelection.h"

using namespace mozilla;

// We treat mouse/touch move as "REAL" move event once its move distance
// exceed this value, in CSS pixel.
static const int32_t kMoveStartTolerancePx = 5;
// Time for trigger scroll end event, in miliseconds.
static const int32_t kScrollEndTimerDelay = 300;

NS_IMPL_ISUPPORTS(SelectionCarets,
                  nsISelectionListener,
                  nsIScrollObserver,
                  nsISupportsWeakReference)

/*static*/ int32_t SelectionCarets::sSelectionCaretsInflateSize = 0;

SelectionCarets::SelectionCarets(nsIPresShell *aPresShell)
  : mActiveTouchId(-1)
  , mCaretCenterToDownPointOffsetY(0)
  , mDragMode(NONE)
  , mVisible(false)
  , mStartCaretVisible(false)
  , mEndCaretVisible(false)
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool addedPref = false;
  if (!addedPref) {
    Preferences::AddIntVarCache(&sSelectionCaretsInflateSize,
                                "selectioncaret.inflatesize.threshold");
    addedPref = true;
  }

  mPresShell = aPresShell;
}

SelectionCarets::~SelectionCarets()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mLongTapDetectorTimer) {
    mLongTapDetectorTimer->Cancel();
    mLongTapDetectorTimer = nullptr;
  }

  if (mScrollEndDetectorTimer) {
    mScrollEndDetectorTimer->Cancel();
    mScrollEndDetectorTimer = nullptr;
  }

  mPresShell = nullptr;
}

static bool
IsOnRect(const nsRect& aRect,
         const nsPoint& aPoint,
         int32_t aInflateSize)
{
  // Check if the click was in the bounding box of the selection caret
  nsRect rect = aRect;
  rect.Inflate(aInflateSize);
  return rect.Contains(aPoint);
}

nsEventStatus
SelectionCarets::HandleEvent(WidgetEvent* aEvent)
{
  WidgetMouseEvent *mouseEvent = aEvent->AsMouseEvent();
  if (mouseEvent && mouseEvent->reason == WidgetMouseEvent::eSynthesized) {
    return nsEventStatus_eIgnore;
  }

  WidgetTouchEvent *touchEvent = aEvent->AsTouchEvent();
  nsIntPoint movePoint;
  int32_t nowTouchId = -1;
  if (touchEvent && !touchEvent->touches.IsEmpty()) {
    // If touch happened, just grab event with same identifier
    if (mActiveTouchId >= 0) {
      for (uint32_t i = 0; i < touchEvent->touches.Length(); ++i) {
        if (touchEvent->touches[i]->Identifier() == mActiveTouchId) {
          movePoint = touchEvent->touches[i]->mRefPoint;
          nowTouchId = touchEvent->touches[i]->Identifier();
          break;
        }
      }

      // not found, consume it
      if (nowTouchId == -1) {
        return nsEventStatus_eConsumeNoDefault;
      }
    } else {
      movePoint = touchEvent->touches[0]->mRefPoint;
      nowTouchId = touchEvent->touches[0]->Identifier();
    }
  } else if (mouseEvent) {
    movePoint = LayoutDeviceIntPoint::ToUntyped(mouseEvent->AsGUIEvent()->refPoint);
  }

  // Get event coordinate relative to canvas frame
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  if (!canvasFrame) {
    return nsEventStatus_eIgnore;
  }
  nsPoint ptInCanvas =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, movePoint, canvasFrame);

  if (aEvent->message == NS_TOUCH_START ||
      (aEvent->message == NS_MOUSE_BUTTON_DOWN &&
       mouseEvent->button == WidgetMouseEvent::eLeftButton)) {
    // If having a active touch, ignore other touch down event
    if (aEvent->message == NS_TOUCH_START && mActiveTouchId >= 0) {
      return nsEventStatus_eConsumeNoDefault;
    }

    mActiveTouchId = nowTouchId;
    mDownPoint = ptInCanvas;
    int32_t inflateSize = SelectionCaretsInflateSize();
    if (mVisible && IsOnRect(GetStartFrameRect(), ptInCanvas, inflateSize)) {
      mDragMode = START_FRAME;
      mCaretCenterToDownPointOffsetY = GetCaretYCenterPosition() - ptInCanvas.y;
      SetSelectionDirection(false);
      SetSelectionDragState(true);
      return nsEventStatus_eConsumeNoDefault;
    } else if (mVisible && IsOnRect(GetEndFrameRect(), ptInCanvas, inflateSize)) {
      mDragMode = END_FRAME;
      mCaretCenterToDownPointOffsetY = GetCaretYCenterPosition() - ptInCanvas.y;
      SetSelectionDirection(true);
      SetSelectionDragState(true);
      return nsEventStatus_eConsumeNoDefault;
    } else {
      mDragMode = NONE;
      mActiveTouchId = -1;
      SetVisibility(false);
      LaunchLongTapDetector();
    }
  } else if (aEvent->message == NS_TOUCH_END ||
             aEvent->message == NS_TOUCH_CANCEL ||
             aEvent->message == NS_MOUSE_BUTTON_UP) {
    CancelLongTapDetector();
    if (mDragMode != NONE) {
      // Only care about same id
      if (mActiveTouchId == nowTouchId) {
        SetSelectionDragState(false);
        mDragMode = NONE;
        mActiveTouchId = -1;
      }
      return nsEventStatus_eConsumeNoDefault;
    }
  } else if (aEvent->message == NS_TOUCH_MOVE ||
             aEvent->message == NS_MOUSE_MOVE) {
    if (mDragMode == START_FRAME || mDragMode == END_FRAME) {
      if (mActiveTouchId == nowTouchId) {
        ptInCanvas.y += mCaretCenterToDownPointOffsetY;
        return DragSelection(ptInCanvas);
      }

      return nsEventStatus_eConsumeNoDefault;
    }

    nsPoint delta = mDownPoint - ptInCanvas;
    if (NS_hypot(delta.x, delta.y) >
          nsPresContext::AppUnitsPerCSSPixel() * kMoveStartTolerancePx) {
      CancelLongTapDetector();
    }
  } else if (aEvent->message == NS_MOUSE_MOZLONGTAP) {
    if (!mVisible) {
      SelectWord();
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

static void
SetElementVisibility(dom::Element* aElement, bool aVisible)
{
  NS_ENSURE_TRUE_VOID(aElement);
  ErrorResult err;
  aElement->ClassList()->Toggle(NS_LITERAL_STRING("hidden"),
                                   dom::Optional<bool>(!aVisible), err);
}

void
SelectionCarets::SetVisibility(bool aVisible)
{
  if (!mPresShell) {
    return;
  }

  if (mVisible == aVisible) {
    return;
  }
  mVisible = aVisible;

  dom::Element* startElement = mPresShell->GetSelectionCaretsStartElement();
  SetElementVisibility(startElement, mVisible && mStartCaretVisible);

  dom::Element* endElement = mPresShell->GetSelectionCaretsEndElement();
  SetElementVisibility(endElement, mVisible && mEndCaretVisible);

  // We must call SetHasTouchCaret() in order to get APZC to wait until the
  // event has been round-tripped and check whether it has been handled,
  // otherwise B2G will end up panning the document when the user tries to drag
  // selection caret.
  mPresShell->SetMayHaveTouchCaret(mVisible);
}

void
SelectionCarets::SetStartFrameVisibility(bool aVisible)
{
  mStartCaretVisible = aVisible;
  dom::Element* element = mPresShell->GetSelectionCaretsStartElement();
  SetElementVisibility(element, mVisible && mStartCaretVisible);
}

void
SelectionCarets::SetEndFrameVisibility(bool aVisible)
{
  mEndCaretVisible = aVisible;
  dom::Element* element = mPresShell->GetSelectionCaretsEndElement();
  SetElementVisibility(element, mVisible && mEndCaretVisible);
}

void
SelectionCarets::SetTilted(bool aIsTilt)
{
  dom::Element* startElement = mPresShell->GetSelectionCaretsStartElement();
  dom::Element* endElement = mPresShell->GetSelectionCaretsEndElement();
  NS_ENSURE_TRUE_VOID(startElement && endElement);

  ErrorResult err;
  startElement->ClassList()->Toggle(NS_LITERAL_STRING("tilt"),
                                       dom::Optional<bool>(aIsTilt), err);

  endElement->ClassList()->Toggle(NS_LITERAL_STRING("tilt"),
                                     dom::Optional<bool>(aIsTilt), err);
}

static void
SetCaretDirection(dom::Element* aElement, bool aIsRight)
{
  MOZ_ASSERT(aElement);

  ErrorResult err;
  if (aIsRight) {
    aElement->ClassList()->Add(NS_LITERAL_STRING("moz-selectioncaret-right"), err);
    aElement->ClassList()->Remove(NS_LITERAL_STRING("moz-selectioncaret-left"), err);
  } else {
    aElement->ClassList()->Add(NS_LITERAL_STRING("moz-selectioncaret-left"), err);
    aElement->ClassList()->Remove(NS_LITERAL_STRING("moz-selectioncaret-right"), err);
  }
}

static bool
IsRightToLeft(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame);

  return aFrame->IsFrameOfType(nsIFrame::eLineParticipant) ?
    (nsBidiPresUtils::GetFrameEmbeddingLevel(aFrame) & 1) :
    aFrame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
}

/*
 * Reduce rect to 1 app unit width along either left or right edge base on
 * aToRightEdge parameter.
 */
static void
ReduceRectToVerticalEdge(nsRect& aRect, bool aToRightEdge)
{
  if (aToRightEdge) {
    aRect.x = aRect.XMost() - 1;
  }
  aRect.width = 1;
}

static nsIFrame*
FindFirstNodeWithFrame(nsIDocument* aDocument,
                       nsRange* aRange,
                       nsFrameSelection* aFrameSelection,
                       bool aBackward,
                       int& aOutOffset)
{
  NS_ENSURE_TRUE(aDocument && aRange && aFrameSelection, nullptr);

  nsCOMPtr<nsINode> startNode =
    do_QueryInterface(aBackward ? aRange->GetEndParent() : aRange->GetStartParent());
  nsCOMPtr<nsINode> endNode =
    do_QueryInterface(aBackward ? aRange->GetStartParent() : aRange->GetEndParent());
  int32_t offset = aBackward ? aRange->EndOffset() : aRange->StartOffset();

  nsCOMPtr<nsIContent> startContent = do_QueryInterface(startNode);
  nsCOMPtr<nsIContent> endContent = do_QueryInterface(endNode);
  CaretAssociationHint hintStart =
    nsFrameSelection::GetHintForPosition(startContent, offset);
  nsIFrame* startFrame = aFrameSelection->GetFrameForNodeOffset(startContent,
                                                                offset,
                                                                hintStart,
                                                                &aOutOffset);

  if (startFrame) {
    return startFrame;
  }

  ErrorResult err;
  nsRefPtr<dom::TreeWalker> walker =
    aDocument->CreateTreeWalker(*startNode,
                                nsIDOMNodeFilter::SHOW_ALL,
                                nullptr,
                                err);

  NS_ENSURE_TRUE(walker, nullptr);
  startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  while (!startFrame && startNode != endNode) {
    if (aBackward) {
      startNode = walker->PreviousNode(err);
    } else {
      startNode = walker->NextNode(err);
    }
    startContent = do_QueryInterface(startNode);
    startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  }
  return startFrame;
}

void
SelectionCarets::UpdateSelectionCarets()
{
  if (!mPresShell) {
    return;
  }

  nsISelection* caretSelection = GetSelection();
  if (!caretSelection) {
    SetVisibility(false);
    return;
  }

  nsRefPtr<dom::Selection> selection = static_cast<dom::Selection*>(caretSelection);
  if (selection->GetRangeCount() <= 0) {
    SetVisibility(false);
    return;
  }

  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  if (range->Collapsed()) {
    SetVisibility(false);
    return;
  }

  nsLayoutUtils::FirstAndLastRectCollector collector;
  nsRange::CollectClientRects(&collector, range,
                              range->GetStartParent(), range->StartOffset(),
                              range->GetEndParent(), range->EndOffset(), true, true);

  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();

  if (!canvasFrame || !rootFrame) {
    SetVisibility(false);
    return;
  }

  // Check if caret inside the scroll frame's boundary
  nsIFrame* caretFocusFrame = GetCaretFocusFrame();
  if (!caretFocusFrame) {
    SetVisibility(false);
    return;
  }
  nsIContent *editableAncestor = caretFocusFrame->GetContent()->GetEditingHost();

  if (!editableAncestor) {
    SetVisibility(false);
    return;
  }

  nsRect resultRect;
  for (nsIFrame* frame = editableAncestor->GetPrimaryFrame();
      frame != nullptr;
      frame = frame->GetNextContinuation()) {
    nsRect rect = frame->GetRectRelativeToSelf();
    nsLayoutUtils::TransformRect(frame, rootFrame, rect);
    resultRect = resultRect.Union(rect);
  }

  // Check start and end frame is rtl or ltr text
  nsRefPtr<nsFrameSelection> fs = caretFocusFrame->GetFrameSelection();
  int32_t startOffset;
  nsIFrame* startFrame = FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                range, fs, false, startOffset);

  int32_t endOffset;
  nsIFrame* endFrame = FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                              range, fs, true, endOffset);

  if (!startFrame || !endFrame) {
    SetVisibility(false);
    return;
  }

  // Check if startFrame is after endFrame.
  if (nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
    SetVisibility(false);
    return;
  }

  bool startFrameIsRTL = IsRightToLeft(startFrame);
  bool endFrameIsRTL = IsRightToLeft(endFrame);

  // If start frame is LTR, then place start caret in first rect's leftmost
  // otherwise put it to first rect's rightmost.
  ReduceRectToVerticalEdge(collector.mFirstRect, startFrameIsRTL);

  // Contrary to start frame, if end frame is LTR, put end caret to last
  // rect's rightmost position, otherwise, put it to last rect's leftmost.
  ReduceRectToVerticalEdge(collector.mLastRect, !endFrameIsRTL);

  SetStartFrameVisibility(resultRect.Intersects(collector.mFirstRect));
  SetEndFrameVisibility(resultRect.Intersects(collector.mLastRect));

  nsLayoutUtils::TransformRect(rootFrame, canvasFrame, collector.mFirstRect);
  nsLayoutUtils::TransformRect(rootFrame, canvasFrame, collector.mLastRect);

  SetStartFramePos(collector.mFirstRect.BottomLeft());
  SetEndFramePos(collector.mLastRect.BottomRight());
  SetVisibility(true);

  // If range select only one character, append tilt class name to it.
  bool isTilt = false;
  if (startFrame && endFrame) {
    // In this case <textarea>abc</textarea> and we select 'c' character,
    // EndContent would be HTMLDivElement and mResultContent which get by
    // calling startFrame->PeekOffset() with selecting next cluster would be
    // TextNode. Although the position is same, nsContentUtils::ComparePoints
    // still shows HTMLDivElement is after TextNode. So that we cannot use
    // EndContent or StartContent to compare with result of PeekOffset().
    // So we compare between next charater of startFrame and previous character
    // of endFrame.
    nsPeekOffsetStruct posNext(eSelectCluster,
                               eDirNext,
                               startOffset,
                               0,
                               false,
                               true,  //limit on scrolled views
                               false,
                               false);

    nsPeekOffsetStruct posPrev(eSelectCluster,
                               eDirPrevious,
                               endOffset,
                               0,
                               false,
                               true,  //limit on scrolled views
                               false,
                               false);
    startFrame->PeekOffset(&posNext);
    endFrame->PeekOffset(&posPrev);

    if (posNext.mResultContent && posPrev.mResultContent &&
        nsContentUtils::ComparePoints(posNext.mResultContent, posNext.mContentOffset,
                                      posPrev.mResultContent, posPrev.mContentOffset) > 0) {
      isTilt = true;
    }
  }

  SetCaretDirection(mPresShell->GetSelectionCaretsStartElement(), startFrameIsRTL);
  SetCaretDirection(mPresShell->GetSelectionCaretsEndElement(), !endFrameIsRTL);
  SetTilted(isTilt);
}

nsresult
SelectionCarets::SelectWord()
{
  // If caret isn't visible, the word is not selectable
  if (!GetCaretVisible()) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(mPresShell, NS_OK);

  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  NS_ENSURE_TRUE(canvasFrame, NS_OK);

  // Find content offsets for mouse down point
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(canvasFrame, mDownPoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  NS_ENSURE_TRUE(ptFrame, NS_OK);
  nsPoint ptInFrame = mDownPoint;
  nsLayoutUtils::TransformPoint(canvasFrame, ptFrame, ptInFrame);

  nsIFrame* caretFocusFrame = GetCaretFocusFrame();
  SetSelectionDragState(true);
  nsFrame* frame = static_cast<nsFrame*>(ptFrame);
  nsresult rs = frame->SelectByTypeAtPoint(mPresShell->GetPresContext(), ptInFrame,
                                           eSelectWord, eSelectWord, 0);
  SetSelectionDragState(false);

  // Clear maintain selection otherwise we cannot select less than a word
  nsRefPtr<nsFrameSelection> fs = caretFocusFrame->GetFrameSelection();
  fs->MaintainSelection();
  return rs;
}

/*
 * If we're dragging start caret, we do not want to drag over previous
 * character of end caret. Same as end caret. So we check if content offset
 * exceed previous/next character of end/start caret base on aDragMode.
 */
static bool
CompareRangeWithContentOffset(nsRange* aRange,
                              nsFrameSelection* aSelection,
                              nsIFrame::ContentOffsets& aOffsets,
                              SelectionCarets::DragMode aDragMode)
{
  MOZ_ASSERT(aDragMode != SelectionCarets::NONE);
  nsINode* node = nullptr;
  int32_t nodeOffset = 0;
  CaretAssociationHint hint;
  nsDirection dir;

  if (aDragMode == SelectionCarets::START_FRAME) {
    // Check previous character of end node offset
    node = aRange->GetEndParent();
    nodeOffset = aRange->EndOffset();
    hint = CARET_ASSOCIATE_BEFORE;
    dir = eDirPrevious;
  } else {
    // Check next character of start node offset
    node = aRange->GetStartParent();
    nodeOffset = aRange->StartOffset();
    hint =  CARET_ASSOCIATE_AFTER;
    dir = eDirNext;
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(node);

  int32_t offset = 0;
  nsIFrame* theFrame =
    aSelection->GetFrameForNodeOffset(content, nodeOffset, hint, &offset);

  NS_ENSURE_TRUE(theFrame, false);

  // Move one character forward/backward from point and get offset
  nsPeekOffsetStruct pos(eSelectCluster,
                         dir,
                         offset,
                         0,
                         true,
                         true,  //limit on scrolled views
                         false,
                         false);
  nsresult rv = theFrame->PeekOffset(&pos);
  if (NS_FAILED(rv)) {
    pos.mResultContent = content;
    pos.mContentOffset = nodeOffset;
  }

  // Compare with current point
  int32_t result = nsContentUtils::ComparePoints(aOffsets.content,
                                                 aOffsets.StartOffset(),
                                                 pos.mResultContent,
                                                 pos.mContentOffset);
  if ((aDragMode == SelectionCarets::START_FRAME && result == 1) ||
      (aDragMode == SelectionCarets::END_FRAME && result == -1)) {
    aOffsets.content = pos.mResultContent;
    aOffsets.offset = pos.mContentOffset;
    aOffsets.secondaryOffset = pos.mContentOffset;
  }

  return true;
}

nsEventStatus
SelectionCarets::DragSelection(const nsPoint &movePoint)
{
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  NS_ENSURE_TRUE(canvasFrame, nsEventStatus_eConsumeNoDefault);

  // Find out which content we point to
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(canvasFrame, movePoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsIFrame* caretFocusFrame = GetCaretFocusFrame();
  nsRefPtr<nsFrameSelection> fs = caretFocusFrame->GetFrameSelection();

  nsresult result;
  nsIFrame *newFrame = nullptr;
  nsPoint newPoint;
  nsPoint ptInFrame = movePoint;
  nsLayoutUtils::TransformPoint(canvasFrame, ptFrame, ptInFrame);
  result = fs->ConstrainFrameAndPointToAnchorSubtree(ptFrame, ptInFrame, &newFrame, newPoint);
  if (NS_FAILED(result) || !newFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsFrame::ContentOffsets offsets =
    newFrame->GetContentOffsetsFromPoint(newPoint);
  NS_ENSURE_TRUE(offsets.content, nsEventStatus_eConsumeNoDefault);

  nsISelection* caretSelection = GetSelection();
  nsRefPtr<dom::Selection> selection = static_cast<dom::Selection*>(caretSelection);
  if (selection->GetRangeCount() <= 0) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  if (!CompareRangeWithContentOffset(range, fs, offsets, mDragMode)) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Move caret postion.
  nsIFrame *scrollable =
    nsLayoutUtils::GetClosestFrameOfType(caretFocusFrame, nsGkAtoms::scrollFrame);
  nsWeakFrame weakScrollable = scrollable;
  fs->HandleClick(offsets.content, offsets.StartOffset(),
                  offsets.EndOffset(),
                  true,
                  false,
                  offsets.associate);
  if (!weakScrollable.IsAlive()) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Scroll scrolled frame.
  nsIScrollableFrame *saf = do_QueryFrame(scrollable);
  nsIFrame *capturingFrame = saf->GetScrolledFrame();
  nsPoint ptInScrolled = movePoint;
  nsLayoutUtils::TransformPoint(canvasFrame, capturingFrame, ptInScrolled);
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, TouchCaret::sAutoScrollTimerDelay);
  UpdateSelectionCarets();
  return nsEventStatus_eConsumeNoDefault;
}

nscoord
SelectionCarets::GetCaretYCenterPosition()
{
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  nsIFrame* caretFocusFrame = GetCaretFocusFrame();

  if (!canvasFrame || !caretFocusFrame) {
    return 0;
  }
  nsISelection* caretSelection = GetSelection();
  nsRefPtr<dom::Selection> selection = static_cast<dom::Selection*>(caretSelection);
  if (selection->GetRangeCount() <= 0) {
    return 0;
  }
  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  nsRefPtr<nsFrameSelection> fs = caretFocusFrame->GetFrameSelection();

  MOZ_ASSERT(mDragMode != NONE);
  nsCOMPtr<nsIContent> node;
  uint32_t nodeOffset;
  if (mDragMode == START_FRAME) {
    node = do_QueryInterface(range->GetStartParent());
    nodeOffset = range->StartOffset();
  } else {
    node = do_QueryInterface(range->GetEndParent());
    nodeOffset = range->EndOffset();
  }

  int32_t offset;
  CaretAssociationHint hint =
    nsFrameSelection::GetHintForPosition(node, nodeOffset);
  nsIFrame* theFrame =
    fs->GetFrameForNodeOffset(node, nodeOffset, hint, &offset);

  if (!theFrame) {
    return 0;
  }
  nsRect frameRect = theFrame->GetRectRelativeToSelf();
  nsLayoutUtils::TransformRect(theFrame, canvasFrame, frameRect);
  return frameRect.Center().y;
}

void
SelectionCarets::SetSelectionDragState(bool aState)
{
  nsIFrame* caretFocusFrame = GetCaretFocusFrame();
  nsRefPtr<nsFrameSelection> fs = caretFocusFrame->GetFrameSelection();
  if (fs->GetDragState() == aState) {
    return;
  }
  fs->SetDragState(aState);

  if (aState) {
    fs->StartBatchChanges();
  } else {
    fs->EndBatchChanges();
  }
}

void
SelectionCarets::SetSelectionDirection(bool aForward)
{
  nsISelection* caretSelection = GetSelection();
  nsRefPtr<dom::Selection> selection = static_cast<dom::Selection*>(caretSelection);
  selection->SetDirection(aForward ? eDirNext : eDirPrevious);
}

static void
SetFramePos(dom::Element* aElement, const nsPoint& aPosition)
{
  NS_ENSURE_TRUE_VOID(aElement);

  nsAutoString styleStr;
  styleStr.AppendLiteral("left:");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aPosition.x));
  styleStr.AppendLiteral("px;top:");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aPosition.y));
  styleStr.AppendLiteral("px;");

  aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr, true);
}

void
SelectionCarets::SetStartFramePos(const nsPoint& aPosition)
{
  SetFramePos(mPresShell->GetSelectionCaretsStartElement(), aPosition);
}

void
SelectionCarets::SetEndFramePos(const nsPoint& aPosition)
{
  SetFramePos(mPresShell->GetSelectionCaretsEndElement(), aPosition);
}

nsRect
SelectionCarets::GetStartFrameRect()
{
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  dom::Element* element = mPresShell->GetSelectionCaretsStartElement();
  NS_ENSURE_TRUE(element, nsRect());

  nsIFrame* frame = element->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, nsRect());

  nsRect frameRect = frame->GetRectRelativeToSelf();
  nsLayoutUtils::TransformRect(frame, canvasFrame, frameRect);
  return frameRect;
}

nsRect
SelectionCarets::GetEndFrameRect()
{
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  dom::Element* element = mPresShell->GetSelectionCaretsEndElement();
  NS_ENSURE_TRUE(element, nsRect());

  nsIFrame* frame = element->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, nsRect());

  nsRect frameRect = frame->GetRectRelativeToSelf();
  nsLayoutUtils::TransformRect(frame, canvasFrame, frameRect);
  return frameRect;
}

nsIFrame*
SelectionCarets::GetCaretFocusFrame()
{
  nsRefPtr<nsCaret> caret = mPresShell->GetCaret();
  NS_ENSURE_TRUE(caret, nullptr);

  nsRect focusRect;
  return caret->GetGeometry(&focusRect);
}

bool
SelectionCarets::GetCaretVisible()
{
  NS_ENSURE_TRUE(mPresShell, false);
  nsRefPtr<nsCaret> caret = mPresShell->GetCaret();
  NS_ENSURE_TRUE(caret, false);

  return caret->IsVisible();
}

nsISelection*
SelectionCarets::GetSelection()
{
  nsRefPtr<nsCaret> caret = mPresShell->GetCaret();
  return caret->GetSelection();
}

nsresult
SelectionCarets::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                       nsISelection* aSel,
                                       int16_t aReason)
{
  bool isCollapsed;
  aSel->GetIsCollapsed(&isCollapsed);
  if (isCollapsed) {
    SetVisibility(false);
    return NS_OK;
  }
  if (aReason & nsISelectionListener::KEYPRESS_REASON) {
    SetVisibility(false);
  } else {
    UpdateSelectionCarets();
  }
  return NS_OK;
}

void
SelectionCarets::ScrollPositionChanged()
{
  SetVisibility(false);
  LaunchScrollEndDetector();
}

void
SelectionCarets::LaunchLongTapDetector()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    mLongTapDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  MOZ_ASSERT(mLongTapDetectorTimer);
  CancelLongTapDetector();
  int32_t longTapDelay = gfxPrefs::UiClickHoldContextMenusDelay();
  mLongTapDetectorTimer->InitWithFuncCallback(FireLongTap,
                                              this,
                                              longTapDelay,
                                              nsITimer::TYPE_ONE_SHOT);
}

void
SelectionCarets::CancelLongTapDetector()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    return;
  }

  mLongTapDetectorTimer->Cancel();
}

/* static */void
SelectionCarets::FireLongTap(nsITimer* aTimer, void* aSelectionCarets)
{
  nsRefPtr<SelectionCarets> self = static_cast<SelectionCarets*>(aSelectionCarets);
  NS_PRECONDITION(aTimer == self->mLongTapDetectorTimer,
                  "Unexpected timer");

  self->SelectWord();
}

void
SelectionCarets::LaunchScrollEndDetector()
{
  if (!mScrollEndDetectorTimer) {
    mScrollEndDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  MOZ_ASSERT(mScrollEndDetectorTimer);
  mScrollEndDetectorTimer->InitWithFuncCallback(FireScrollEnd,
                                                this,
                                                kScrollEndTimerDelay,
                                                nsITimer::TYPE_ONE_SHOT);
}

/* static */void
SelectionCarets::FireScrollEnd(nsITimer* aTimer, void* aSelectionCarets)
{
  nsRefPtr<SelectionCarets> self = static_cast<SelectionCarets*>(aSelectionCarets);
  NS_PRECONDITION(aTimer == self->mScrollEndDetectorTimer,
                  "Unexpected timer");
  self->SetVisibility(true);
  self->UpdateSelectionCarets();
}
