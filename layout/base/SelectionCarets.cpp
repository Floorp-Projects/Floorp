/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "SelectionCarets.h"

#include "gfxPrefs.h"
#include "nsBidiPresUtils.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsDocShell.h"
#include "nsDOMTokenList.h"
#include "nsFocusManager.h"
#include "nsFrame.h"
#include "nsGenericHTMLElement.h"
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
#include "Layers.h"
#include "TouchCaret.h"
#include "nsFrameSelection.h"

using namespace mozilla;
using namespace mozilla::dom;

static PRLogModuleInfo* gSelectionCaretsLog;
static const char* kSelectionCaretsLogModuleName = "SelectionCarets";

// To enable all the SELECTIONCARETS_LOG print statements, set the environment
// variable NSPR_LOG_MODULES=SelectionCarets:5
#define SELECTIONCARETS_LOG(message, ...)                                      \
  MOZ_LOG(gSelectionCaretsLog, LogLevel::Debug,                                    \
         ("SelectionCarets (%p): %s:%d : " message "\n", this, __FUNCTION__,   \
          __LINE__, ##__VA_ARGS__));

#define SELECTIONCARETS_LOG_STATIC(message, ...)                               \
  MOZ_LOG(gSelectionCaretsLog, LogLevel::Debug,                                    \
         ("SelectionCarets: %s:%d : " message "\n", __FUNCTION__, __LINE__,    \
          ##__VA_ARGS__));

// We treat mouse/touch move as "REAL" move event once its move distance
// exceed this value, in CSS pixel.
static const int32_t kMoveStartTolerancePx = 5;

NS_IMPL_ISUPPORTS(SelectionCarets,
                  nsIReflowObserver,
                  nsISelectionListener,
                  nsIScrollObserver,
                  nsISupportsWeakReference)

/*static*/ int32_t SelectionCarets::sSelectionCaretsInflateSize = 0;
/*static*/ bool SelectionCarets::sSelectionCaretDetectsLongTap = true;
/*static*/ bool SelectionCarets::sCaretManagesAndroidActionbar = false;
/*static*/ bool SelectionCarets::sSelectionCaretObservesCompositions = false;

SelectionCarets::SelectionCarets(nsIPresShell* aPresShell)
  : mPresShell(aPresShell)
  , mActiveTouchId(-1)
  , mCaretCenterToDownPointOffsetY(0)
  , mDragMode(NONE)
  , mUseAsyncPanZoom(false)
  , mInAsyncPanZoomGesture(false)
  , mEndCaretVisible(false)
  , mStartCaretVisible(false)
  , mSelectionVisibleInScrollFrames(true)
  , mVisible(false)
  , mActionBarViewID(0)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gSelectionCaretsLog) {
    gSelectionCaretsLog = PR_NewLogModule(kSelectionCaretsLogModuleName);
  }

  SELECTIONCARETS_LOG("Constructor, PresShell=%p", mPresShell);

  static bool addedPref = false;
  if (!addedPref) {
    Preferences::AddIntVarCache(&sSelectionCaretsInflateSize,
                                "selectioncaret.inflatesize.threshold");
    Preferences::AddBoolVarCache(&sSelectionCaretDetectsLongTap,
                                 "selectioncaret.detects.longtap", true);
    Preferences::AddBoolVarCache(&sCaretManagesAndroidActionbar,
                                 "caret.manages-android-actionbar");
    Preferences::AddBoolVarCache(&sSelectionCaretObservesCompositions,
                                 "selectioncaret.observes.compositions");
    addedPref = true;
  }
}

void
SelectionCarets::Init()
{
  nsPresContext* presContext = mPresShell->GetPresContext();
  MOZ_ASSERT(presContext, "PresContext should be given in PresShell::Init()");

  nsIDocShell* docShell = presContext->GetDocShell();
  if (!docShell) {
    return;
  }

#if defined(MOZ_WIDGET_GONK)
  mUseAsyncPanZoom = mPresShell->AsyncPanZoomEnabled();
#endif

  docShell->AddWeakReflowObserver(this);
  docShell->AddWeakScrollObserver(this);

  mDocShell = static_cast<nsDocShell*>(docShell);
}

SelectionCarets::~SelectionCarets()
{
  SELECTIONCARETS_LOG("Destructor");
  MOZ_ASSERT(NS_IsMainThread());

  mPresShell = nullptr;
}

void
SelectionCarets::Terminate()
{
  nsRefPtr<nsDocShell> docShell(mDocShell.get());
  if (docShell) {
    docShell->RemoveWeakReflowObserver(this);
    docShell->RemoveWeakScrollObserver(this);
  }

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
  LayoutDeviceIntPoint movePoint;
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
    movePoint = mouseEvent->AsGUIEvent()->refPoint;
  }

  // XUL has no SelectionCarets elements.
  if (!mPresShell->GetSelectionCaretsStartElement() ||
      !mPresShell->GetSelectionCaretsEndElement()) {
    return nsEventStatus_eIgnore;
  }

  // Get event coordinate relative to root frame
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return nsEventStatus_eIgnore;
  }
  nsPoint ptInRoot =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, movePoint, rootFrame);

  if (aEvent->mMessage == eTouchStart ||
      (aEvent->mMessage == eMouseDown &&
       mouseEvent->button == WidgetMouseEvent::eLeftButton)) {
    // If having a active touch, ignore other touch down event
    if (aEvent->mMessage == eTouchStart && mActiveTouchId >= 0) {
      return nsEventStatus_eConsumeNoDefault;
    }

    mActiveTouchId = nowTouchId;
    mDownPoint = ptInRoot;
    if (IsOnStartFrameInner(ptInRoot)) {
      mDragMode = START_FRAME;
      mCaretCenterToDownPointOffsetY = GetCaretYCenterPosition() - ptInRoot.y;
      SetSelectionDirection(eDirPrevious);
      SetSelectionDragState(true);
      return nsEventStatus_eConsumeNoDefault;
    } else if (IsOnEndFrameInner(ptInRoot)) {
      mDragMode = END_FRAME;
      mCaretCenterToDownPointOffsetY = GetCaretYCenterPosition() - ptInRoot.y;
      SetSelectionDirection(eDirNext);
      SetSelectionDragState(true);
      return nsEventStatus_eConsumeNoDefault;
    } else {
      mDragMode = NONE;
      mActiveTouchId = -1;
      LaunchLongTapDetector();
    }
  } else if (aEvent->mMessage == eTouchEnd ||
             aEvent->mMessage == NS_TOUCH_CANCEL ||
             aEvent->mMessage == eMouseUp) {
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
  } else if (aEvent->mMessage == eTouchMove || aEvent->mMessage == eMouseMove) {
    if (mDragMode == START_FRAME || mDragMode == END_FRAME) {
      if (mActiveTouchId == nowTouchId) {
        ptInRoot.y += mCaretCenterToDownPointOffsetY;

        if (mDragMode == START_FRAME) {
          if (ptInRoot.y > mDragDownYBoundary) {
            ptInRoot.y = mDragDownYBoundary;
          }
        } else if (mDragMode == END_FRAME) {
          if (ptInRoot.y < mDragUpYBoundary) {
            ptInRoot.y = mDragUpYBoundary;
          }
        }
        return DragSelection(ptInRoot);
      }

      return nsEventStatus_eConsumeNoDefault;
    }

    nsPoint delta = mDownPoint - ptInRoot;
    if (NS_hypot(delta.x, delta.y) >
          nsPresContext::AppUnitsPerCSSPixel() * kMoveStartTolerancePx) {
      CancelLongTapDetector();
    }

  } else if (aEvent->mMessage == eMouseLongTap) {
    if (!mVisible || !sSelectionCaretDetectsLongTap) {
      SELECTIONCARETS_LOG("SelectWord from eMouseLongTap");

      mDownPoint = ptInRoot;
      nsresult wordSelected = SelectWord();

      if (NS_FAILED(wordSelected)) {
        SELECTIONCARETS_LOG("SelectWord from eMouseLongTap failed!");
        return nsEventStatus_eIgnore;
      }

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
    SELECTIONCARETS_LOG("Set visibility %s, same as the old one",
                        (aVisible ? "shown" : "hidden"));
    return;
  }

  if (!aVisible) {
    mSelectionVisibleInScrollFrames = false;
  }

  mVisible = aVisible;
  SELECTIONCARETS_LOG("Set visibility %s", (mVisible ? "shown" : "hidden"));

  dom::Element* startElement = mPresShell->GetSelectionCaretsStartElement();
  SetElementVisibility(startElement, mVisible && mStartCaretVisible);

  dom::Element* endElement = mPresShell->GetSelectionCaretsEndElement();
  SetElementVisibility(endElement, mVisible && mEndCaretVisible);

  // Update the Android Actionbar visibility if in use.
  if (sCaretManagesAndroidActionbar) {
    TouchCaret::UpdateAndroidActionBarVisibility(mVisible, mActionBarViewID);
  }
}

void
SelectionCarets::SetStartFrameVisibility(bool aVisible)
{
  mStartCaretVisible = aVisible;
  SELECTIONCARETS_LOG("Set start frame visibility %s",
                      (mStartCaretVisible ? "shown" : "hidden"));

  dom::Element* element = mPresShell->GetSelectionCaretsStartElement();
  SetElementVisibility(element, mVisible && mStartCaretVisible);
}

void
SelectionCarets::SetEndFrameVisibility(bool aVisible)
{
  mEndCaretVisible = aVisible;
  SELECTIONCARETS_LOG("Set end frame visibility %s",
                      (mEndCaretVisible ? "shown" : "hidden"));

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

  SELECTIONCARETS_LOG("Set tilted selection carets %s",
                      (aIsTilt ? "enabled" : "disabled"));

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
    SELECTIONCARETS_LOG("Cannot get selection!");
    SetVisibility(false);
    return;
  }

  if (selection->IsCollapsed()) {
    SELECTIONCARETS_LOG("Selection is collapsed!");
    SetVisibility(false);
    return;
  }

  int32_t rangeCount = selection->RangeCount();
  nsRefPtr<nsRange> firstRange = selection->GetRangeAt(0);
  nsRefPtr<nsRange> lastRange = selection->GetRangeAt(rangeCount - 1);

  mPresShell->FlushPendingNotifications(Flush_Layout);

  nsIFrame* rootFrame = mPresShell->GetRootFrame();

  if (!rootFrame) {
    SetVisibility(false);
    return;
  }

  // Check start and end frame is rtl or ltr text
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    SetVisibility(false);
    return;
  }

  int32_t startOffset;
  nsIFrame* startFrame = FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                                firstRange, fs, false, startOffset);

  int32_t endOffset;
  nsIFrame* endFrame = FindFirstNodeWithFrame(mPresShell->GetDocument(),
                                              lastRange, fs, true, endOffset);

  if (!startFrame || !endFrame) {
    SetVisibility(false);
    return;
  }

  // Check if startFrame is after endFrame.
  if (nsLayoutUtils::CompareTreePosition(startFrame, endFrame) > 0) {
    SetVisibility(false);
    return;
  }

  // If the selection is not visible, we should dispatch a event.
  nsIFrame* commonAncestorFrame =
    nsLayoutUtils::FindNearestCommonAncestorFrame(startFrame, endFrame);

  nsRect selectionRectInRootFrame = nsContentUtils::GetSelectionBoundingRect(selection);
  nsRect selectionRectInCommonAncestorFrame = selectionRectInRootFrame;
  nsLayoutUtils::TransformRect(rootFrame, commonAncestorFrame,
                               selectionRectInCommonAncestorFrame);

  mSelectionVisibleInScrollFrames =
    nsLayoutUtils::IsRectVisibleInScrollFrames(commonAncestorFrame,
                                               selectionRectInCommonAncestorFrame);
  SELECTIONCARETS_LOG("Selection visibility %s",
                      (mSelectionVisibleInScrollFrames ? "shown" : "hidden"));


  nsRect firstRectInStartFrame =
    nsCaret::GetGeometryForFrame(startFrame, startOffset, nullptr);
  nsRect lastRectInEndFrame =
    nsCaret::GetGeometryForFrame(endFrame, endOffset, nullptr);

  bool startFrameVisible =
    nsLayoutUtils::IsRectVisibleInScrollFrames(startFrame, firstRectInStartFrame);
  bool endFrameVisible =
    nsLayoutUtils::IsRectVisibleInScrollFrames(endFrame, lastRectInEndFrame);

  nsRect firstRectInRootFrame = firstRectInStartFrame;
  nsRect lastRectInRootFrame = lastRectInEndFrame;
  nsLayoutUtils::TransformRect(startFrame, rootFrame, firstRectInRootFrame);
  nsLayoutUtils::TransformRect(endFrame, rootFrame, lastRectInRootFrame);

  SetStartFrameVisibility(startFrameVisible);
  SetEndFrameVisibility(endFrameVisible);

  SetStartFramePos(firstRectInRootFrame);
  SetEndFramePos(lastRectInRootFrame);
  SetVisibility(true);

  // Use half of the first(last) rect as the dragup(dragdown) boundary
  mDragUpYBoundary =
    (firstRectInRootFrame.BottomLeft().y + firstRectInRootFrame.TopLeft().y) / 2;
  mDragDownYBoundary =
    (lastRectInRootFrame.BottomRight().y + lastRectInRootFrame.TopRight().y) / 2;

  nsRect rectStart = GetStartFrameRect();
  nsRect rectEnd = GetEndFrameRect();
  bool isTilt = rectStart.Intersects(rectEnd);
  if (isTilt) {
    SetCaretDirection(mPresShell->GetSelectionCaretsStartElement(), rectStart.x > rectEnd.x);
    SetCaretDirection(mPresShell->GetSelectionCaretsEndElement(), rectStart.x <= rectEnd.x);
  }
  SetTilted(isTilt);
}

nsresult
SelectionCarets::SelectWord()
{
  if (!mPresShell) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Find content offsets for mouse down point
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, mDownPoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_ERROR_FAILURE;
  }

  bool selectable;
  ptFrame->IsSelectable(&selectable, nullptr);
  if (!selectable) {
    SELECTIONCARETS_LOG(" frame %p is not selectable", ptFrame);
    return NS_ERROR_FAILURE;
  }

  nsPoint ptInFrame = mDownPoint;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);

  nsIFrame* currFrame = ptFrame;
  nsIContent* newFocusContent = nullptr;
  while (currFrame) {
    int32_t tabIndexUnused = 0;
    if (currFrame->IsFocusable(&tabIndexUnused, true)) {
      newFocusContent = currFrame->GetContent();
      nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocusContent));
      if (domElement)
        break;
    }
    currFrame = currFrame->GetParent();
  }


  // If target frame is focusable, we should move focus to it. If target frame
  // isn't focusable, and our previous focused content is editable, we should
  // clear focus.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  nsIContent* editingHost = ptFrame->GetContent()->GetEditingHost();
  if (newFocusContent && currFrame) {
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocusContent));
    fm->SetFocus(domElement,0);

    if (editingHost && !nsContentUtils::HasNonEmptyTextContent(
          editingHost, nsContentUtils::eRecurseIntoChildren)) {
      SELECTIONCARETS_LOG("Select a editable content %p with empty text",
                          editingHost);
      // Long tap on the content with empty text, no action for
      // selectioncarets but need to dispatch the taponcaret event
      // to support the short cut mode
      DispatchSelectionStateChangedEvent(GetSelection(),
                                         SelectionState::Taponcaret);
      return NS_OK;
    }
  } else {
    nsIContent* focusedContent = GetFocusedContent();
    if (focusedContent) {
      // Clear focus if content was editable element, or contentEditable.
      nsGenericHTMLElement* focusedGeneric =
        nsGenericHTMLElement::FromContent(focusedContent);
      if (focusedContent->GetTextEditorRootContent() ||
          (focusedGeneric && focusedGeneric->IsContentEditable())) {
        nsIDOMWindow* win = mPresShell->GetDocument()->GetWindow();
        if (win) {
          fm->ClearFocus(win);
        }
      }
    }
  }

  SetSelectionDragState(true);
  nsFrame* frame = static_cast<nsFrame*>(ptFrame);
  nsresult rs = frame->SelectByTypeAtPoint(mPresShell->GetPresContext(), ptInFrame,
                                           eSelectWord, eSelectWord, 0);

#ifdef DEBUG_FRAME_DUMP
  nsCString frameTag;
  frame->ListTag(frameTag);
  SELECTIONCARETS_LOG("Frame=%s, ptInFrame=(%d, %d)", frameTag.get(),
                      ptInFrame.x, ptInFrame.y);
#endif

  SetSelectionDragState(false);

  // Clear maintain selection otherwise we cannot select less than a word
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->MaintainSelection();
  }
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
                         nsPoint(0, 0),
                         true,
                         true,  //limit on scrolled views
                         false,
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
  if (!rootFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Find out which content we point to
  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, movePoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsresult result;
  nsIFrame *newFrame = nullptr;
  nsPoint newPoint;
  nsPoint ptInFrame = movePoint;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);
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
  if (!selection) {
    return nsEventStatus_eConsumeNoDefault;
  }

  int32_t rangeCount = selection->RangeCount();
  if (rangeCount <= 0) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Limit the drag behavior not to cross the end of last selection range
  // when drag the start frame and vice versa
  nsRefPtr<nsRange> range = mDragMode == START_FRAME ?
    selection->GetRangeAt(rangeCount - 1) : selection->GetRangeAt(0);
  if (!CompareRangeWithContentOffset(range, fs, offsets, mDragMode)) {
    return nsEventStatus_eConsumeNoDefault;
  }

  nsIFrame* anchorFrame;
  selection->GetPrimaryFrameForAnchorNode(&anchorFrame);
  if (!anchorFrame) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Clear maintain selection so that we can drag caret freely.
  fs->MaintainSelection(eSelectNoAmount);

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
  nsLayoutUtils::TransformPoint(rootFrame, capturingFrame, ptInScrolled);
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, TouchCaret::sAutoScrollTimerDelay);
  UpdateSelectionCarets();
  return nsEventStatus_eConsumeNoDefault;
}

nscoord
SelectionCarets::GetCaretYCenterPosition()
{
  nsIFrame* rootFrame = mPresShell->GetRootFrame();

  if (!rootFrame) {
    return 0;
  }

  nsRefPtr<dom::Selection> selection = GetSelection();
  if (!selection) {
    return 0;
  }

  int32_t rangeCount = selection->RangeCount();
  if (rangeCount <= 0) {
    return 0;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return 0;
  }

  MOZ_ASSERT(mDragMode != NONE);
  nsCOMPtr<nsIContent> node;
  uint32_t nodeOffset;
  if (mDragMode == START_FRAME) {
    nsRefPtr<nsRange> range = selection->GetRangeAt(0);
    node = do_QueryInterface(range->GetStartParent());
    nodeOffset = range->StartOffset();
  } else {
    nsRefPtr<nsRange> range = selection->GetRangeAt(rangeCount - 1);
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
  nsLayoutUtils::TransformRect(theFrame, rootFrame, frameRect);
  return frameRect.Center().y;
}

void
SelectionCarets::SetSelectionDragState(bool aState)
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->SetDragState(aState);
  }
}

void
SelectionCarets::SetSelectionDirection(nsDirection aDir)
{
  nsRefPtr<dom::Selection> selection = GetSelection();
  if (selection) {
    selection->AdjustAnchorFocusForMultiRange(aDir);
  }
}

static void
SetFramePos(dom::Element* aElement, const nsRect& aCaretRect)
{
  if (!aElement) {
    return;
  }

  nsAutoString styleStr;
  styleStr.AppendLiteral("left: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aCaretRect.Center().x));
  styleStr.AppendLiteral("px; top: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aCaretRect.y));
  styleStr.AppendLiteral("px; padding-top: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aCaretRect.height));
  styleStr.AppendLiteral("px;");

  SELECTIONCARETS_LOG_STATIC("Set style: %s",
                             NS_ConvertUTF16toUTF8(styleStr).get());

  aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr, true);
}

void
SelectionCarets::SetStartFramePos(const nsRect& aCaretRect)
{
  SELECTIONCARETS_LOG("x=%d, y=%d, w=%d, h=%d",
    aCaretRect.x, aCaretRect.y, aCaretRect.width, aCaretRect.height);
  SetFramePos(mPresShell->GetSelectionCaretsStartElement(), aCaretRect);
}

void
SelectionCarets::SetEndFramePos(const nsRect& aCaretRect)
{
  SELECTIONCARETS_LOG("x=%d, y=%d, w=%d, h=%d",
    aCaretRect.x, aCaretRect.y, aCaretRect.width, aCaretRect.height);
  SetFramePos(mPresShell->GetSelectionCaretsEndElement(), aCaretRect);
}

bool
SelectionCarets::IsOnStartFrameInner(const nsPoint& aPosition)
{
  return mVisible &&
    nsLayoutUtils::ContainsPoint(GetStartFrameRectInner(), aPosition,
                                 SelectionCaretsInflateSize());
}

bool
SelectionCarets::IsOnEndFrameInner(const nsPoint& aPosition)
{
  return mVisible &&
    nsLayoutUtils::ContainsPoint(GetEndFrameRectInner(), aPosition,
                                 SelectionCaretsInflateSize());
}

nsRect
SelectionCarets::GetStartFrameRect()
{
  dom::Element* element = mPresShell->GetSelectionCaretsStartElement();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(element, rootFrame);
}

nsRect
SelectionCarets::GetEndFrameRect()
{
  dom::Element* element = mPresShell->GetSelectionCaretsEndElement();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(element, rootFrame);
}

nsRect
SelectionCarets::GetStartFrameRectInner()
{
  dom::Element* element = mPresShell->GetSelectionCaretsStartElement();
  dom::Element* childElement = element->GetFirstElementChild();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(childElement, rootFrame);
}

nsRect
SelectionCarets::GetEndFrameRectInner()
{
  dom::Element* element = mPresShell->GetSelectionCaretsEndElement();
  dom::Element* childElement = element->GetFirstElementChild();
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(childElement, rootFrame);
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

    // Prevent us from touching the nsFrameSelection associated to other
    // PresShell.
    nsRefPtr<nsFrameSelection> fs = focusFrame->GetFrameSelection();
    if (!fs || fs->GetShell() != mPresShell) {
      return nullptr;
    }

    return fs.forget();
  } else {
    return mPresShell->FrameSelection();
  }
}

static dom::Sequence<SelectionState>
GetSelectionStates(int16_t aReason)
{
  dom::Sequence<SelectionState> states;
  if (aReason & nsISelectionListener::DRAG_REASON) {
    states.AppendElement(SelectionState::Drag, fallible);
  }
  if (aReason & nsISelectionListener::MOUSEDOWN_REASON) {
    states.AppendElement(SelectionState::Mousedown, fallible);
  }
  if (aReason & nsISelectionListener::MOUSEUP_REASON) {
    states.AppendElement(SelectionState::Mouseup, fallible);
  }
  if (aReason & nsISelectionListener::KEYPRESS_REASON) {
    states.AppendElement(SelectionState::Keypress, fallible);
  }
  if (aReason & nsISelectionListener::SELECTALL_REASON) {
    states.AppendElement(SelectionState::Selectall, fallible);
  }
  if (aReason & nsISelectionListener::COLLAPSETOSTART_REASON) {
    states.AppendElement(SelectionState::Collapsetostart, fallible);
  }
  if (aReason & nsISelectionListener::COLLAPSETOEND_REASON) {
    states.AppendElement(SelectionState::Collapsetoend, fallible);
  }
  return states;
}

void
SelectionCarets::DispatchCustomEvent(const nsAString& aEvent)
{
  SELECTIONCARETS_LOG("dispatch %s event", NS_ConvertUTF16toUTF8(aEvent).get());
  bool defaultActionEnabled = true;
  nsIDocument* doc = mPresShell->GetDocument();
  MOZ_ASSERT(doc);
  nsContentUtils::DispatchTrustedEvent(doc,
                                       ToSupports(doc),
                                       aEvent,
                                       true,
                                       false,
                                       &defaultActionEnabled);
}

void
SelectionCarets::DispatchSelectionStateChangedEvent(Selection* aSelection,
                                                    SelectionState aState)
{
  dom::Sequence<SelectionState> state;
  state.AppendElement(aState, fallible);
  DispatchSelectionStateChangedEvent(aSelection, state);
}

void
SelectionCarets::DispatchSelectionStateChangedEvent(Selection* aSelection,
                                                    const Sequence<SelectionState>& aStates)
{
  nsIDocument* doc = mPresShell->GetDocument();

  MOZ_ASSERT(doc);

  SelectionStateChangedEventInit init;
  init.mBubbles = true;

  if (aSelection) {
    // XXX: Do we need to flush layout?
    mPresShell->FlushPendingNotifications(Flush_Layout);
    nsRect rect = nsContentUtils::GetSelectionBoundingRect(aSelection);
    nsRefPtr<DOMRect>domRect = new DOMRect(ToSupports(doc));

    domRect->SetLayoutRect(rect);
    init.mBoundingClientRect = domRect;
    init.mVisible = mSelectionVisibleInScrollFrames;

    aSelection->Stringify(init.mSelectedText);
  }
  init.mStates = aStates;

  nsRefPtr<SelectionStateChangedEvent> event =
    SelectionStateChangedEvent::Constructor(doc, NS_LITERAL_STRING("mozselectionstatechanged"), init);

  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
  bool ret;
  doc->DispatchEvent(event, &ret);
}

void
SelectionCarets::NotifyBlur(bool aIsLeavingDocument)
{
  SELECTIONCARETS_LOG("Send out the blur event");
  SetVisibility(false);
  if (aIsLeavingDocument) {
    CancelLongTapDetector();
  }
  CancelScrollEndDetector();
  mInAsyncPanZoomGesture = false;
  DispatchSelectionStateChangedEvent(nullptr, SelectionState::Blur);
}

nsresult
SelectionCarets::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                        nsISelection* aSel,
                                        int16_t aReason)
{
  SELECTIONCARETS_LOG("aSel (%p), Reason=%d", aSel, aReason);

  if (aSel != GetSelection()) {
    SELECTIONCARETS_LOG("Return for selection mismatch!");
    return NS_OK;
  }

  // Update SelectionCaret visibility.
  if (sSelectionCaretObservesCompositions) {
    // When observing selection change notifications generated for example
    // by Android soft-keyboard compositions, we can only obtain visibility
    // after mouse-up by long-tap, or final caret-drag.
    if (!mVisible) {
      if (aReason & nsISelectionListener::MOUSEUP_REASON) {
        UpdateSelectionCarets();
      }
    } else {
      // If already visible, we hide immediately for some known
      // event-reasons: drag, keypress, or mouse down.
      if (aReason & (nsISelectionListener::DRAG_REASON |
                     nsISelectionListener::KEYPRESS_REASON |
                     nsISelectionListener::MOUSEDOWN_REASON)) {
        SetVisibility(false);
      } else {
        // Else we look further at the selection status, as currently
        // style-composition changes don't provide reason codes.
        UpdateSelectionCarets();
      }
    }
  } else {
    // Default logic, mainly employed by b2g, isn't aware of soft-keyboard
    // selection change compositions.
    if (!aReason || (aReason & (nsISelectionListener::DRAG_REASON |
                                nsISelectionListener::KEYPRESS_REASON |
                                nsISelectionListener::MOUSEDOWN_REASON))) {
      SetVisibility(false);
    } else {
      UpdateSelectionCarets();
    }
  }

  // Maybe trigger Android ActionBar updates.
  if (mVisible && sCaretManagesAndroidActionbar) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "ActionBar:UpdateState", nullptr);
    }
  }

  DispatchSelectionStateChangedEvent(static_cast<Selection*>(aSel),
                                     GetSelectionStates(aReason));
  return NS_OK;
}

static void
DispatchScrollViewChangeEvent(nsIPresShell *aPresShell, const dom::ScrollState aState)
{
  nsCOMPtr<nsIDocument> doc = aPresShell->GetDocument();
  if (doc) {
    bool ret;
    ScrollViewChangeEventInit detail;
    detail.mBubbles = true;
    detail.mCancelable = false;
    detail.mState = aState;
    nsRefPtr<ScrollViewChangeEvent> event =
      ScrollViewChangeEvent::Constructor(doc, NS_LITERAL_STRING("scrollviewchange"), detail);

    event->SetTrusted(true);
    event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
    doc->DispatchEvent(event, &ret);
  }
}

void
SelectionCarets::AsyncPanZoomStarted()
{
  if (mVisible) {
    mInAsyncPanZoomGesture = true;
    // Hide selection carets if not using ActionBar.
    if (!sCaretManagesAndroidActionbar) {
      SetVisibility(false);
    }

    SELECTIONCARETS_LOG("Dispatch scroll started");
    DispatchScrollViewChangeEvent(mPresShell, dom::ScrollState::Started);
  } else {
    nsRefPtr<dom::Selection> selection = GetSelection();
    if (selection && selection->RangeCount() && selection->IsCollapsed()) {
      mInAsyncPanZoomGesture = true;
      DispatchScrollViewChangeEvent(mPresShell, dom::ScrollState::Started);
    }
  }
}

void
SelectionCarets::AsyncPanZoomStopped()
{
  if (mInAsyncPanZoomGesture) {
    mInAsyncPanZoomGesture = false;
    SELECTIONCARETS_LOG("Update selection carets after APZ is stopped!");
    UpdateSelectionCarets();

    // SelectionStateChangedEvent should be dispatched before ScrollViewChangeEvent.
    DispatchSelectionStateChangedEvent(GetSelection(),
                                       SelectionState::Updateposition);

    SELECTIONCARETS_LOG("Dispatch scroll stopped");

    DispatchScrollViewChangeEvent(mPresShell, dom::ScrollState::Stopped);
  }
}

void
SelectionCarets::ScrollPositionChanged()
{
  if (mVisible) {
    if (!mUseAsyncPanZoom) {
      // Hide selection carets if not using ActionBar.
      if (!sCaretManagesAndroidActionbar) {
        SetVisibility(false);
      }

      //TODO: handling scrolling for selection bubble when APZ is off
      // Dispatch event to notify gaia to hide selection bubble.
      // Positions will be updated when scroll is end, so no need to calculate
      // and keep scroll positions here. An arbitrary (0, 0) is sent instead.
      DispatchScrollViewChangeEvent(mPresShell, dom::ScrollState::Started);

      SELECTIONCARETS_LOG("Launch scroll end detector");
      LaunchScrollEndDetector();
    } else {
      if (!mInAsyncPanZoomGesture) {
        UpdateSelectionCarets();
        DispatchSelectionStateChangedEvent(GetSelection(),
                                           SelectionState::Updateposition);
      }
    }
  } else {
    nsRefPtr<dom::Selection> selection = GetSelection();
    if (selection && selection->RangeCount() && selection->IsCollapsed()) {
      DispatchSelectionStateChangedEvent(selection,
                                         SelectionState::Updateposition);
    }
  }
}

void
SelectionCarets::LaunchLongTapDetector()
{
  if (!sSelectionCaretDetectsLongTap || mUseAsyncPanZoom) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    mLongTapDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  MOZ_ASSERT(mLongTapDetectorTimer);
  CancelLongTapDetector();
  int32_t longTapDelay = gfxPrefs::UiClickHoldContextMenusDelay();

  SELECTIONCARETS_LOG("Will fire long tap after %d ms", longTapDelay);
  mLongTapDetectorTimer->InitWithFuncCallback(FireLongTap,
                                              this,
                                              longTapDelay,
                                              nsITimer::TYPE_ONE_SHOT);
}

void
SelectionCarets::CancelLongTapDetector()
{
  if (mUseAsyncPanZoom) {
    return;
  }

  if (!mLongTapDetectorTimer) {
    return;
  }

  SELECTIONCARETS_LOG("Cancel long tap detector!");
  mLongTapDetectorTimer->Cancel();
}

/* static */void
SelectionCarets::FireLongTap(nsITimer* aTimer, void* aSelectionCarets)
{
  nsRefPtr<SelectionCarets> self = static_cast<SelectionCarets*>(aSelectionCarets);
  NS_PRECONDITION(aTimer == self->mLongTapDetectorTimer,
                  "Unexpected timer");

  SELECTIONCARETS_LOG_STATIC("SelectWord from non-APZ");
  nsresult wordSelected = self->SelectWord();

  if (NS_FAILED(wordSelected)) {
    SELECTIONCARETS_LOG_STATIC("SelectWord from non-APZ failed!");
  }
}

void
SelectionCarets::LaunchScrollEndDetector()
{
  if (!mScrollEndDetectorTimer) {
    mScrollEndDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  MOZ_ASSERT(mScrollEndDetectorTimer);

  SELECTIONCARETS_LOG("Will fire scroll end after %d ms",
    TouchCaret::sScrollEndTimerDelay);
  mScrollEndDetectorTimer->InitWithFuncCallback(FireScrollEnd,
                                                this,
                                                TouchCaret::sScrollEndTimerDelay,
                                                nsITimer::TYPE_ONE_SHOT);
}

void
SelectionCarets::CancelScrollEndDetector()
{
  if (!mScrollEndDetectorTimer) {
    return;
  }

  SELECTIONCARETS_LOG("Cancel scroll end detector!");
  mScrollEndDetectorTimer->Cancel();
}


/* static */void
SelectionCarets::FireScrollEnd(nsITimer* aTimer, void* aSelectionCarets)
{
  nsRefPtr<SelectionCarets> self = static_cast<SelectionCarets*>(aSelectionCarets);
  NS_PRECONDITION(aTimer == self->mScrollEndDetectorTimer,
                  "Unexpected timer");

  SELECTIONCARETS_LOG_STATIC("Update selection carets!");
  self->UpdateSelectionCarets();
  self->DispatchSelectionStateChangedEvent(self->GetSelection(),
                                           SelectionState::Updateposition);
}

NS_IMETHODIMP
SelectionCarets::Reflow(DOMHighResTimeStamp aStart, DOMHighResTimeStamp aEnd)
{
  if (mVisible) {
    SELECTIONCARETS_LOG("Update selection carets after reflow!");
    UpdateSelectionCarets();

    // We don't care selection state when we're at drag mode. We always hide
    // bubble in drag mode. So, don't dispatch event here.
    if (mDragMode == NONE) {
      DispatchSelectionStateChangedEvent(GetSelection(),
                                         SelectionState::Updateposition);
    }
  } else {
    nsRefPtr<dom::Selection> selection = GetSelection();
    if (selection && selection->RangeCount() && selection->IsCollapsed()) {
      DispatchSelectionStateChangedEvent(selection,
                                         SelectionState::Updateposition);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
SelectionCarets::ReflowInterruptible(DOMHighResTimeStamp aStart,
                                     DOMHighResTimeStamp aEnd)
{
  return Reflow(aStart, aEnd);
}
