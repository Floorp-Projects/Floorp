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
#include "nsFocusManager.h"
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
#include "mozilla/dom/ScrollViewChangeEvent.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/TreeWalker.h"
#include "mozilla/Preferences.h"
#include "mozilla/TouchEvents.h"
#include "TouchCaret.h"
#include "nsFrameSelection.h"

using namespace mozilla;
using namespace mozilla::dom;

// We treat mouse/touch move as "REAL" move event once its move distance
// exceed this value, in CSS pixel.
static const int32_t kMoveStartTolerancePx = 5;
// Time for trigger scroll end event, in miliseconds.
static const int32_t kScrollEndTimerDelay = 300;
// Read from preference "selectioncaret.noneditable". Indicate whether support
// non-editable fields selection or not. We have stable state for editable
// fields selection now. And we don't want to break this stable state when
// enabling non-editable support. So I add a pref to control to support or
// not. Once non-editable fields support is stable. We should remove this
// pref.
static bool kSupportNonEditableFields = false;

NS_IMPL_ISUPPORTS(SelectionCarets,
                  nsISelectionListener,
                  nsIScrollObserver,
                  nsISupportsWeakReference)

/*static*/ int32_t SelectionCarets::sSelectionCaretsInflateSize = 0;

SelectionCarets::SelectionCarets(nsIPresShell *aPresShell)
  : mActiveTouchId(-1)
  , mCaretCenterToDownPointOffsetY(0)
  , mDragMode(NONE)
  , mAPZenabled(false)
  , mEndCaretVisible(false)
  , mStartCaretVisible(false)
  , mVisible(false)
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool addedPref = false;
  if (!addedPref) {
    Preferences::AddIntVarCache(&sSelectionCaretsInflateSize,
                                "selectioncaret.inflatesize.threshold");
    Preferences::AddBoolVarCache(&kSupportNonEditableFields,
                                 "selectioncaret.noneditable");
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
    if (IsOnStartFrame(ptInCanvas)) {
      mDragMode = START_FRAME;
      mCaretCenterToDownPointOffsetY = GetCaretYCenterPosition() - ptInCanvas.y;
      SetSelectionDirection(false);
      SetSelectionDragState(true);
      return nsEventStatus_eConsumeNoDefault;
    } else if (IsOnEndFrame(ptInCanvas)) {
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
  if (!aElement) {
    return;
  }

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

  if (!startElement || !endElement) {
    return;
  }

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
 * Reduce rect to 1 css pixel width along either left or right edge base on
 * aToRightEdge parameter.
 */
static void
ReduceRectToVerticalEdge(nsRect& aRect, bool aToRightEdge)
{
  if (aToRightEdge) {
    aRect.x = aRect.XMost() - AppUnitsPerCSSPixel();
  }
  aRect.width = AppUnitsPerCSSPixel();
}

static nsIFrame*
FindFirstNodeWithFrame(nsIDocument* aDocument,
                       nsRange* aRange,
                       nsFrameSelection* aFrameSelection,
                       bool aBackward,
                       int& aOutOffset)
{
  if (!aDocument || !aRange || !aFrameSelection) {
    return nullptr;
  }

  nsCOMPtr<nsINode> startNode =
    do_QueryInterface(aBackward ? aRange->GetEndParent() : aRange->GetStartParent());
  nsCOMPtr<nsINode> endNode =
    do_QueryInterface(aBackward ? aRange->GetStartParent() : aRange->GetEndParent());
  int32_t offset = aBackward ? aRange->EndOffset() : aRange->StartOffset();

  nsCOMPtr<nsIContent> startContent = do_QueryInterface(startNode);
  CaretAssociationHint hintStart =
    aBackward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
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

  if (!walker) {
    return nullptr;
  }

  startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  while (!startFrame && startNode != endNode) {
    if (aBackward) {
      startNode = walker->PreviousNode(err);
    } else {
      startNode = walker->NextNode(err);
    }

    if (!startNode) {
      break;
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

  nsRefPtr<dom::Selection> selection = GetSelection();
  if (!selection) {
    SetVisibility(false);
    return;
  }

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

  // Check start and end frame is rtl or ltr text
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
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

  // If frame isn't editable and we don't support non-editable fields, bail
  // out.
  if (!kSupportNonEditableFields &&
      (!startFrame->GetContent()->IsEditable() ||
       !endFrame->GetContent()->IsEditable())) {
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

  nsAutoTArray<nsIFrame*, 16> hitFramesInFirstRect;
  nsLayoutUtils::GetFramesForArea(rootFrame,
    collector.mFirstRect,
    hitFramesInFirstRect,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION |
      nsLayoutUtils::IGNORE_CROSS_DOC |
      nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME);

  nsAutoTArray<nsIFrame*, 16> hitFramesInLastRect;
  nsLayoutUtils::GetFramesForArea(rootFrame,
    collector.mLastRect,
    hitFramesInLastRect,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION |
      nsLayoutUtils::IGNORE_CROSS_DOC |
      nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME);

  SetStartFrameVisibility(hitFramesInFirstRect.Contains(startFrame));
  SetEndFrameVisibility(hitFramesInLastRect.Contains(endFrame));

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
  if (!mPresShell) {
    return NS_OK;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  if (!rootFrame || !canvasFrame) {
    return NS_OK;
  }

  // Find content offsets for mouse down point
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, mDownPoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_OK;
  }

  // If frame isn't editable and we don't support non-editable fields, bail
  // out.
  if (!kSupportNonEditableFields && !ptFrame->GetContent()->IsEditable()) {
    return NS_OK;
  }

  nsPoint ptInFrame = mDownPoint;
  nsLayoutUtils::TransformPoint(canvasFrame, ptFrame, ptInFrame);

  // If target frame is editable, we should move focus to targe frame. If
  // target frame isn't editable and our focus content is editable, we should
  // clear focus.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  nsIContent* editingHost = ptFrame->GetContent()->GetEditingHost();
  if (editingHost) {
    nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(editingHost->GetParent());
    if (elt) {
      fm->SetFocus(elt, 0);
    }
  } else {
    nsIContent* focusedContent = GetFocusedContent();
    if (focusedContent && focusedContent->GetTextEditorRootContent()) {
      nsIDOMWindow* win = mPresShell->GetDocument()->GetWindow();
      if (win) {
        fm->ClearFocus(win);
      }
    }
  }

  SetSelectionDragState(true);
  nsFrame* frame = static_cast<nsFrame*>(ptFrame);
  nsresult rs = frame->SelectByTypeAtPoint(mPresShell->GetPresContext(), ptInFrame,
                                           eSelectWord, eSelectWord, 0);
  SetSelectionDragState(false);

  // Clear maintain selection otherwise we cannot select less than a word
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
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

  if (!theFrame) {
    return false;
  }

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
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  if (!rootFrame || !canvasFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Find out which content we point to
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, movePoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();

  nsresult result;
  nsIFrame *newFrame = nullptr;
  nsPoint newPoint;
  nsPoint ptInFrame = movePoint;
  nsLayoutUtils::TransformPoint(canvasFrame, ptFrame, ptInFrame);
  result = fs->ConstrainFrameAndPointToAnchorSubtree(ptFrame, ptInFrame, &newFrame, newPoint);
  if (NS_FAILED(result) || !newFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  bool selectable;
  newFrame->IsSelectable(&selectable, nullptr);
  if (!selectable) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsFrame::ContentOffsets offsets =
    newFrame->GetContentOffsetsFromPoint(newPoint);
  if (!offsets.content) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<dom::Selection> selection = GetSelection();
  if (selection->GetRangeCount() <= 0) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  if (!CompareRangeWithContentOffset(range, fs, offsets, mDragMode)) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsIFrame* anchorFrame;
  selection->GetPrimaryFrameForAnchorNode(&anchorFrame);
  if (!anchorFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Move caret postion.
  nsIFrame *scrollable =
    nsLayoutUtils::GetClosestFrameOfType(anchorFrame, nsGkAtoms::scrollFrame);
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

  if (!canvasFrame) {
    return 0;
  }

  nsRefPtr<dom::Selection> selection = GetSelection();
  if (selection->GetRangeCount() <= 0) {
    return 0;
  }

  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();

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
    mDragMode == START_FRAME ? CARET_ASSOCIATE_AFTER : CARET_ASSOCIATE_BEFORE;
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
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  fs->SetDragState(aState);
}

void
SelectionCarets::SetSelectionDirection(bool aForward)
{
  nsRefPtr<dom::Selection> selection = GetSelection();
  selection->SetDirection(aForward ? eDirNext : eDirPrevious);
}

static void
SetFramePos(dom::Element* aElement, const nsPoint& aPosition)
{
  if (!aElement) {
    return;
  }

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

bool
SelectionCarets::IsOnStartFrame(const nsPoint& aPosition)
{
  return mVisible &&
    nsLayoutUtils::ContainsPoint(GetStartFrameRect(), aPosition,
                                 SelectionCaretsInflateSize());
}

bool
SelectionCarets::IsOnEndFrame(const nsPoint& aPosition)
{
  return mVisible &&
    nsLayoutUtils::ContainsPoint(GetEndFrameRect(), aPosition,
                                 SelectionCaretsInflateSize());
}

nsRect
SelectionCarets::GetStartFrameRect()
{
  dom::Element* element = mPresShell->GetSelectionCaretsStartElement();
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(element, canvasFrame);
}

nsRect
SelectionCarets::GetEndFrameRect()
{
  dom::Element* element = mPresShell->GetSelectionCaretsEndElement();
  nsIFrame* canvasFrame = mPresShell->GetCanvasFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(element, canvasFrame);
}

nsIContent*
SelectionCarets::GetFocusedContent()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    return fm->GetFocusedContent();
  }

  return nullptr;
}

Selection*
SelectionCarets::GetSelection()
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    return fs->GetSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return nullptr;
}

already_AddRefed<nsFrameSelection>
SelectionCarets::GetFrameSelection()
{
  nsIContent* focusNode = GetFocusedContent();
  if (focusNode) {
    nsIFrame* focusFrame = focusNode->GetPrimaryFrame();
    if (!focusFrame) {
      return nullptr;
    }
    return focusFrame->GetFrameSelection();
  } else {
    return mPresShell->FrameSelection();
  }
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
  if (!aReason || (aReason & (nsISelectionListener::DRAG_REASON |
                               nsISelectionListener::KEYPRESS_REASON |
                               nsISelectionListener::MOUSEDOWN_REASON))) {
    SetVisibility(false);
  } else {
    UpdateSelectionCarets();
  }
  return NS_OK;
}

static void
DispatchScrollViewChangeEvent(nsIPresShell *aPresShell, const dom::ScrollState aState, const mozilla::CSSIntPoint aScrollPos)
{
  nsCOMPtr<nsIDocument> doc = aPresShell->GetDocument();
  if (doc) {
    bool ret;
    ScrollViewChangeEventInit detail;
    detail.mBubbles = true;
    detail.mCancelable = false;
    detail.mState = aState;
    detail.mScrollX = aScrollPos.x;
    detail.mScrollY = aScrollPos.y;
    nsRefPtr<ScrollViewChangeEvent> event =
      ScrollViewChangeEvent::Constructor(doc, NS_LITERAL_STRING("scrollviewchange"), detail);

    event->SetTrusted(true);
    event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
    doc->DispatchEvent(event, &ret);
  }
}

void
SelectionCarets::AsyncPanZoomStarted(const mozilla::CSSIntPoint aScrollPos)
{
  // Receives the notifications from AsyncPanZoom, sets mAPZenabled as true here
  // to bypass the notifications from ScrollPositionChanged callbacks
  mAPZenabled = true;
  SetVisibility(false);
  DispatchScrollViewChangeEvent(mPresShell, dom::ScrollState::Started, aScrollPos);
}

void
SelectionCarets::AsyncPanZoomStopped(const mozilla::CSSIntPoint aScrollPos)
{
  UpdateSelectionCarets();
  DispatchScrollViewChangeEvent(mPresShell, dom::ScrollState::Stopped, aScrollPos);
}

void
SelectionCarets::ScrollPositionChanged()
{
  if (!mAPZenabled && mVisible) {
    SetVisibility(false);
    //TODO: handling scrolling for selection bubble when APZ is off
    LaunchScrollEndDetector();
  }
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
