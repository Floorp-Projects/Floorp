/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaretManager.h"

#include "AccessibleCaret.h"
#include "AccessibleCaretEventHub.h"
#include "AccessibleCaretLogger.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/TreeWalker.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsFrame.h"
#include "nsFrameSelection.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {

#undef AC_LOG
#define AC_LOG(message, ...)                                                   \
  AC_LOG_BASE("AccessibleCaretManager (%p): " message, this, ##__VA_ARGS__);

#undef AC_LOGV
#define AC_LOGV(message, ...)                                                  \
  AC_LOGV_BASE("AccessibleCaretManager (%p): " message, this, ##__VA_ARGS__);

using namespace dom;
using Appearance = AccessibleCaret::Appearance;
using PositionChangedResult = AccessibleCaret::PositionChangedResult;

#define AC_PROCESS_ENUM_TO_STREAM(e) case(e): aStream << #e; break;
std::ostream&
operator<<(std::ostream& aStream,
           const AccessibleCaretManager::CaretMode& aCaretMode)
{
  using CaretMode = AccessibleCaretManager::CaretMode;
  switch (aCaretMode) {
    AC_PROCESS_ENUM_TO_STREAM(CaretMode::None);
    AC_PROCESS_ENUM_TO_STREAM(CaretMode::Cursor);
    AC_PROCESS_ENUM_TO_STREAM(CaretMode::Selection);
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaretManager::UpdateCaretsHint& aHint)
{
  using UpdateCaretsHint = AccessibleCaretManager::UpdateCaretsHint;
  switch (aHint) {
    AC_PROCESS_ENUM_TO_STREAM(UpdateCaretsHint::Default);
    AC_PROCESS_ENUM_TO_STREAM(UpdateCaretsHint::RespectOldAppearance);
  }
  return aStream;
}
#undef AC_PROCESS_ENUM_TO_STREAM

AccessibleCaretManager::AccessibleCaretManager(nsIPresShell* aPresShell)
  : mPresShell(aPresShell)
{
  if (mPresShell) {
    mFirstCaret = MakeUnique<AccessibleCaret>(mPresShell);
    mSecondCaret = MakeUnique<AccessibleCaret>(mPresShell);

    mCaretTimeoutTimer = do_CreateInstance("@mozilla.org/timer;1");
  }
}

AccessibleCaretManager::~AccessibleCaretManager()
{
  CancelCaretTimeoutTimer();
}

nsresult
AccessibleCaretManager::OnSelectionChanged(nsIDOMDocument* aDoc,
                                           nsISelection* aSel, int16_t aReason)
{
  AC_LOG("%s: aSel: %p, GetSelection(): %p, aReason: %d", __FUNCTION__,
         aSel, GetSelection(), aReason);

  if (aSel != GetSelection()) {
    return NS_OK;
  }

  // Move the cursor by Javascript.
  if (aReason == nsISelectionListener::NO_REASON) {
    HideCarets();
    return NS_OK;
  }

  // Move cursor by keyboard.
  if (aReason & nsISelectionListener::KEYPRESS_REASON) {
    HideCarets();
    return NS_OK;
  }

  // OnBlur() might be called between mouse down and mouse up, so we hide carets
  // upon mouse down anyway, and update carets upon mouse up.
  if (aReason & nsISelectionListener::MOUSEDOWN_REASON) {
    HideCarets();
    return NS_OK;
  }

  // Range will collapse after cutting or copying text.
  if (aReason & (nsISelectionListener::COLLAPSETOSTART_REASON |
                 nsISelectionListener::COLLAPSETOEND_REASON)) {
    HideCarets();
    return NS_OK;
  }

  UpdateCarets();
  return NS_OK;
}

void
AccessibleCaretManager::HideCarets()
{
  if (mFirstCaret->IsLogicallyVisible() || mSecondCaret->IsLogicallyVisible()) {
    AC_LOG("%s", __FUNCTION__);
    mFirstCaret->SetAppearance(Appearance::None);
    mSecondCaret->SetAppearance(Appearance::None);
    DispatchCaretStateChangedEvent(CaretChangedReason::Visibilitychange);
    CancelCaretTimeoutTimer();
  }
}

void
AccessibleCaretManager::UpdateCarets(UpdateCaretsHint aHint)
{
  mLastUpdateCaretMode = GetCaretMode();

  switch (mLastUpdateCaretMode) {
  case CaretMode::None:
    HideCarets();
    break;
  case CaretMode::Cursor:
    UpdateCaretsForCursorMode(aHint);
    break;
  case CaretMode::Selection:
    UpdateCaretsForSelectionMode(aHint);
    break;
  }
}

bool
AccessibleCaretManager::IsCaretDisplayableInCursorMode(nsIFrame** aOutFrame,
                                                       int32_t* aOutOffset) const
{
  nsRefPtr<nsCaret> caret = mPresShell->GetCaret();
  if (!caret || !caret->IsVisible()) {
    return false;
  }

  int32_t offset = 0;
  nsIFrame* frame = nsCaret::GetFrameAndOffset(GetSelection(), nullptr, 0, &offset);

  if (!frame) {
    return false;
  }

  if (!GetEditingHostForFrame(frame)) {
    return false;
  }

  if (aOutFrame) {
    *aOutFrame = frame;
  }

  if (aOutOffset) {
    *aOutOffset = offset;
  }

  return true;
}

bool
AccessibleCaretManager::HasNonEmptyTextContent(nsINode* aNode) const
{
  return nsContentUtils::HasNonEmptyTextContent(
           aNode, nsContentUtils::eRecurseIntoChildren);
}


void
AccessibleCaretManager::UpdateCaretsForCursorMode(UpdateCaretsHint aHint)
{
  AC_LOG("%s, selection: %p", __FUNCTION__, GetSelection());

  int32_t offset = 0;
  nsIFrame* frame = nullptr;
  if (!IsCaretDisplayableInCursorMode(&frame, &offset)) {
    HideCarets();
    return;
  }

  bool oldSecondCaretVisible = mSecondCaret->IsLogicallyVisible();
  PositionChangedResult result = mFirstCaret->SetPosition(frame, offset);

  switch (result) {
    case PositionChangedResult::NotChanged:
      // Do nothing
      break;

    case PositionChangedResult::Changed:
      switch (aHint) {
        case UpdateCaretsHint::Default:
          if (HasNonEmptyTextContent(GetEditingHostForFrame(frame))) {
            mFirstCaret->SetAppearance(Appearance::Normal);
          } else {
            mFirstCaret->SetAppearance(Appearance::NormalNotShown);
          }
          break;

        case UpdateCaretsHint::RespectOldAppearance:
          // Do nothing to prevent the appearance of the caret being
          // changed from NormalNotShown to Normal.
          break;
      }
      break;

    case PositionChangedResult::Invisible:
      mFirstCaret->SetAppearance(Appearance::NormalNotShown);
      break;
  }

  mFirstCaret->SetSelectionBarEnabled(false);
  mSecondCaret->SetAppearance(Appearance::None);

  LaunchCaretTimeoutTimer();

  if ((result != PositionChangedResult::NotChanged || oldSecondCaretVisible) &&
      !mActiveCaret) {
    DispatchCaretStateChangedEvent(CaretChangedReason::Updateposition);
  }
}

void
AccessibleCaretManager::UpdateCaretsForSelectionMode(UpdateCaretsHint aHint)
{
  AC_LOG("%s: selection: %p", __FUNCTION__, GetSelection());

  int32_t startOffset = 0;
  nsIFrame* startFrame = FindFirstNodeWithFrame(false, &startOffset);

  int32_t endOffset = 0;
  nsIFrame* endFrame = FindFirstNodeWithFrame(true, &endOffset);

  if (!CompareTreePosition(startFrame, endFrame)) {
    // XXX: Do we really have to hide carets if this condition isn't satisfied?
    HideCarets();
    return;
  }

  auto updateSingleCaret = [](AccessibleCaret* aCaret, nsIFrame* aFrame,
                              int32_t aOffset) -> PositionChangedResult
  {
    PositionChangedResult result = aCaret->SetPosition(aFrame, aOffset);
    aCaret->SetSelectionBarEnabled(true);

    switch (result) {
      case PositionChangedResult::NotChanged:
        // Do nothing
        break;

      case PositionChangedResult::Changed:
        aCaret->SetAppearance(Appearance::Normal);
        break;

      case PositionChangedResult::Invisible:
        aCaret->SetAppearance(Appearance::NormalNotShown);
        break;
    }
    return result;
  };

  PositionChangedResult firstCaretResult =
    updateSingleCaret(mFirstCaret.get(), startFrame, startOffset);
  PositionChangedResult secondCaretResult =
    updateSingleCaret(mSecondCaret.get(), endFrame, endOffset);

  if (firstCaretResult == PositionChangedResult::Changed ||
      secondCaretResult == PositionChangedResult::Changed) {
    // Flush layout to make the carets intersection correct.
    FlushLayout();
  }

  UpdateCaretsForTilt();

  if ((firstCaretResult == PositionChangedResult::Changed ||
       secondCaretResult == PositionChangedResult::Changed ||
       firstCaretResult == PositionChangedResult::Invisible ||
       secondCaretResult == PositionChangedResult::Invisible) &&
      !mActiveCaret) {
    DispatchCaretStateChangedEvent(CaretChangedReason::Updateposition);
  }
}

void
AccessibleCaretManager::UpdateCaretsForTilt()
{
  if (mFirstCaret->IsVisuallyVisible() && mSecondCaret->IsVisuallyVisible()) {
    if (mFirstCaret->Intersects(*mSecondCaret)) {
      if (mFirstCaret->LogicalPosition().x <=
          mSecondCaret->LogicalPosition().x) {
        mFirstCaret->SetAppearance(Appearance::Left);
        mSecondCaret->SetAppearance(Appearance::Right);
      } else {
        mFirstCaret->SetAppearance(Appearance::Right);
        mSecondCaret->SetAppearance(Appearance::Left);
      }
    } else {
      mFirstCaret->SetAppearance(Appearance::Normal);
      mSecondCaret->SetAppearance(Appearance::Normal);
    }
  }
}

nsresult
AccessibleCaretManager::PressCaret(const nsPoint& aPoint)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mFirstCaret->Contains(aPoint)) {
    mActiveCaret = mFirstCaret.get();
    SetSelectionDirection(eDirPrevious);
  } else if (mSecondCaret->Contains(aPoint)) {
    mActiveCaret = mSecondCaret.get();
    SetSelectionDirection(eDirNext);
  }

  if (mActiveCaret) {
    mOffsetYToCaretLogicalPosition =
      mActiveCaret->LogicalPosition().y - aPoint.y;
    SetSelectionDragState(true);
    DispatchCaretStateChangedEvent(CaretChangedReason::Presscaret);
    CancelCaretTimeoutTimer();
    rv = NS_OK;
  }

  return rv;
}

nsresult
AccessibleCaretManager::DragCaret(const nsPoint& aPoint)
{
  MOZ_ASSERT(mActiveCaret);
  MOZ_ASSERT(GetCaretMode() != CaretMode::None);

  nsPoint point(aPoint.x, aPoint.y + mOffsetYToCaretLogicalPosition);
  DragCaretInternal(point);
  UpdateCarets();
  return NS_OK;
}

nsresult
AccessibleCaretManager::ReleaseCaret()
{
  MOZ_ASSERT(mActiveCaret);

  mActiveCaret = nullptr;
  SetSelectionDragState(false);
  DispatchCaretStateChangedEvent(CaretChangedReason::Releasecaret);
  LaunchCaretTimeoutTimer();
  return NS_OK;
}

nsresult
AccessibleCaretManager::TapCaret(const nsPoint& aPoint)
{
  MOZ_ASSERT(GetCaretMode() != CaretMode::None);

  nsresult rv = NS_ERROR_FAILURE;

  if (GetCaretMode() == CaretMode::Cursor) {
    DispatchCaretStateChangedEvent(CaretChangedReason::Taponcaret);
    rv = NS_OK;
  }

  return rv;
}

nsresult
AccessibleCaretManager::SelectWordOrShortcut(const nsPoint& aPoint)
{
  if (!mPresShell) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Find the frame under point.
  nsIFrame* ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, aPoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* focusableFrame = GetFocusableFrame(ptFrame);

#ifdef DEBUG_FRAME_DUMP
  AC_LOG("%s: Found %s under (%d, %d)", __FUNCTION__, ptFrame->ListTag().get(),
         aPoint.x, aPoint.y);
  AC_LOG("%s: Found %s focusable", __FUNCTION__,
         focusableFrame ? focusableFrame->ListTag().get() : "no frame");
#endif

  // Firstly check long press on an empty editable content.
  Element* newFocusEditingHost = GetEditingHostForFrame(ptFrame);
  if (focusableFrame && newFocusEditingHost &&
      !HasNonEmptyTextContent(newFocusEditingHost)) {
    ChangeFocusToOrClearOldFocus(focusableFrame);
    // We need to update carets to get correct information before dispatching
    // CaretStateChangedEvent.
    UpdateCarets();
    DispatchCaretStateChangedEvent(CaretChangedReason::Longpressonemptycontent);
    return NS_OK;
  }

  bool selectable = false;
  ptFrame->IsSelectable(&selectable, nullptr);

#ifdef DEBUG_FRAME_DUMP
  AC_LOG("%s: %s %s selectable.", __FUNCTION__, ptFrame->ListTag().get(),
         selectable ? "is" : "is NOT");
#endif

  if (!selectable) {
    return NS_ERROR_FAILURE;
  }

  // ptFrame is selectable. Now change the focus.
  ChangeFocusToOrClearOldFocus(focusableFrame);

  // Then try select a word under point.
  nsPoint ptInFrame = aPoint;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);

  nsresult rv = SelectWord(ptFrame, ptInFrame);
  UpdateCarets();
  return rv;
}

void
AccessibleCaretManager::OnScrollStart()
{
  AC_LOG("%s", __FUNCTION__);

  if (GetCaretMode() == CaretMode::Cursor) {
    mFirstCaretAppearanceOnScrollStart = mFirstCaret->GetAppearance();
  }

  HideCarets();
}

void
AccessibleCaretManager::OnScrollEnd()
{
  if (mLastUpdateCaretMode != GetCaretMode()) {
    return;
  }

  if (GetCaretMode() == CaretMode::Cursor) {
    mFirstCaret->SetAppearance(mFirstCaretAppearanceOnScrollStart);
    if (!mFirstCaret->IsLogicallyVisible()) {
      // If the caret is hide (Appearance::None) due to timeout or blur, no need
      // to update it.
      return;
    }
  }

  AC_LOG("%s: UpdateCarets()", __FUNCTION__);
  UpdateCarets();
}

void
AccessibleCaretManager::OnScrollPositionChanged()
{
  if (mLastUpdateCaretMode != GetCaretMode()) {
    return;
  }

  if (mFirstCaret->IsLogicallyVisible() || mSecondCaret->IsLogicallyVisible()) {
    AC_LOG("%s: UpdateCarets(RespectOldAppearance)", __FUNCTION__);
    UpdateCarets(UpdateCaretsHint::RespectOldAppearance);
  }
}

void
AccessibleCaretManager::OnReflow()
{
  if (mLastUpdateCaretMode != GetCaretMode()) {
    return;
  }

  if (mFirstCaret->IsLogicallyVisible() || mSecondCaret->IsLogicallyVisible()) {
    AC_LOG("%s: UpdateCarets(RespectOldAppearance)", __FUNCTION__);
    UpdateCarets(UpdateCaretsHint::RespectOldAppearance);
  }
}

void
AccessibleCaretManager::OnBlur()
{
  AC_LOG("%s: HideCarets()", __FUNCTION__);
  HideCarets();
}

void
AccessibleCaretManager::OnKeyboardEvent()
{
  if (GetCaretMode() == CaretMode::Cursor) {
    AC_LOG("%s: HideCarets()", __FUNCTION__);
    HideCarets();
  }
}

Selection*
AccessibleCaretManager::GetSelection() const
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return nullptr;
  }
  return fs->GetSelection(nsISelectionController::SELECTION_NORMAL);
}

already_AddRefed<nsFrameSelection>
AccessibleCaretManager::GetFrameSelection() const
{
  if (!mPresShell) {
    return nullptr;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  MOZ_ASSERT(fm);

  nsIContent* focusedContent = fm->GetFocusedContent();
  if (focusedContent) {
    nsIFrame* focusFrame = focusedContent->GetPrimaryFrame();
    if (!focusFrame) {
      return nullptr;
    }

    // Prevent us from touching the nsFrameSelection associated with other
    // PresShell.
    nsRefPtr<nsFrameSelection> fs = focusFrame->GetFrameSelection();
    if (!fs || fs->GetShell() != mPresShell) {
      return nullptr;
    }

    return fs.forget();
  } else {
    // For non-editable content
    return mPresShell->FrameSelection();
  }
}

Element*
AccessibleCaretManager::GetEditingHostForFrame(nsIFrame* aFrame) const
{
  if (!aFrame) {
    return nullptr;
  }

  auto content = aFrame->GetContent();
  if (!content) {
    return nullptr;
  }

  return content->GetEditingHost();
}


AccessibleCaretManager::CaretMode
AccessibleCaretManager::GetCaretMode() const
{
  Selection* selection = GetSelection();
  if (!selection) {
    return CaretMode::None;
  }

  uint32_t rangeCount = selection->RangeCount();
  if (rangeCount <= 0) {
    return CaretMode::None;
  }

  if (selection->IsCollapsed()) {
    return CaretMode::Cursor;
  }

  return CaretMode::Selection;
}

nsIFrame*
AccessibleCaretManager::GetFocusableFrame(nsIFrame* aFrame) const
{
  // This implementation is similar to EventStateManager::PostHandleEvent().
  // Look for the nearest enclosing focusable frame.
  nsIFrame* focusableFrame = aFrame;
  while (focusableFrame) {
    if (focusableFrame->IsFocusable(nullptr, true)) {
      break;
    }
    focusableFrame = focusableFrame->GetParent();
  }
  return focusableFrame;
}

void
AccessibleCaretManager::ChangeFocusToOrClearOldFocus(nsIFrame* aFrame) const
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  MOZ_ASSERT(fm);

  if (aFrame) {
    nsIContent* focusableContent = aFrame->GetContent();
    MOZ_ASSERT(focusableContent, "Focusable frame must have content!");
    nsCOMPtr<nsIDOMElement> focusableElement = do_QueryInterface(focusableContent);
    fm->SetFocus(focusableElement, nsIFocusManager::FLAG_BYMOUSE);
  } else {
    nsIDOMWindow* win = mPresShell->GetDocument()->GetWindow();
    if (win) {
      fm->ClearFocus(win);
      fm->SetFocusedWindow(win);
    }
  }
}

nsresult
AccessibleCaretManager::SelectWord(nsIFrame* aFrame, const nsPoint& aPoint) const
{
  SetSelectionDragState(true);
  nsFrame* frame = static_cast<nsFrame*>(aFrame);
  nsresult rs = frame->SelectByTypeAtPoint(mPresShell->GetPresContext(), aPoint,
                                           eSelectWord, eSelectWord, 0);

  SetSelectionDragState(false);
  ClearMaintainedSelection();

  return rs;
}

void
AccessibleCaretManager::SetSelectionDragState(bool aState) const
{
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->SetDragState(aState);
  }
}

void
AccessibleCaretManager::SetSelectionDirection(nsDirection aDir) const
{
  Selection* selection = GetSelection();
  if (selection) {
    selection->AdjustAnchorFocusForMultiRange(aDir);
  }
}

void
AccessibleCaretManager::ClearMaintainedSelection() const
{
  // Selection made by double-clicking for example will maintain the original
  // word selection. We should clear it so that we can drag caret freely.
  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->MaintainSelection(eSelectNoAmount);
  }
}

void
AccessibleCaretManager::FlushLayout() const
{
  if (mPresShell) {
    mPresShell->FlushPendingNotifications(Flush_Layout);
  }
}

nsIFrame*
AccessibleCaretManager::FindFirstNodeWithFrame(bool aBackward,
                                               int32_t* aOutOffset) const
{
  if (!mPresShell) {
    return nullptr;
  }

  nsRefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return nullptr;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return nullptr;
  }

  uint32_t rangeCount = selection->RangeCount();
  if (rangeCount <= 0) {
    return nullptr;
  }

  nsRange* range = selection->GetRangeAt(aBackward ? rangeCount - 1 : 0);
  nsRefPtr<nsINode> startNode =
    aBackward ? range->GetEndParent() : range->GetStartParent();
  nsRefPtr<nsINode> endNode =
    aBackward ? range->GetStartParent() : range->GetEndParent();
  int32_t offset = aBackward ? range->EndOffset() : range->StartOffset();
  nsCOMPtr<nsIContent> startContent = do_QueryInterface(startNode);
  CaretAssociationHint hintStart =
    aBackward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
  nsIFrame* startFrame =
    fs->GetFrameForNodeOffset(startContent, offset, hintStart, aOutOffset);

  if (startFrame) {
    return startFrame;
  }

  ErrorResult err;
  nsRefPtr<TreeWalker> walker = mPresShell->GetDocument()->CreateTreeWalker(
    *startNode, nsIDOMNodeFilter::SHOW_ALL, nullptr, err);

  if (!walker) {
    return nullptr;
  }

  startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  while (!startFrame && startNode != endNode) {
    startNode = aBackward ? walker->PreviousNode(err) : walker->NextNode(err);

    if (!startNode) {
      break;
    }

    startContent = startNode->AsContent();
    startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
  }
  return startFrame;
}

bool
AccessibleCaretManager::CompareRangeWithContentOffset(nsIFrame::ContentOffsets& aOffsets)
{
  Selection* selection = GetSelection();
  if (!selection) {
    return false;
  }

  uint32_t rangeCount = selection->RangeCount();
  MOZ_ASSERT(rangeCount > 0);

  int32_t rangeIndex = (mActiveCaret == mFirstCaret.get() ? rangeCount - 1 : 0);
  nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIndex);

  nsINode* node = nullptr;
  int32_t nodeOffset = 0;
  CaretAssociationHint hint;
  nsDirection dir;

  if (mActiveCaret == mFirstCaret.get()) {
    // Check previous character of end node offset
    node = range->GetEndParent();
    nodeOffset = range->EndOffset();
    hint = CARET_ASSOCIATE_BEFORE;
    dir = eDirPrevious;
  } else {
    // Check next character of start node offset
    node = range->GetStartParent();
    nodeOffset = range->StartOffset();
    hint = CARET_ASSOCIATE_AFTER;
    dir = eDirNext;
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(node);

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return false;
  }

  int32_t offset = 0;
  nsIFrame* theFrame =
    fs->GetFrameForNodeOffset(content, nodeOffset, hint, &offset);

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
  if ((mActiveCaret == mFirstCaret.get() && result == 1) ||
      (mActiveCaret == mSecondCaret.get() && result == -1)) {
    aOffsets.content = pos.mResultContent;
    aOffsets.offset = pos.mContentOffset;
    aOffsets.secondaryOffset = pos.mContentOffset;
  }

  return true;
}

bool
AccessibleCaretManager::CompareTreePosition(nsIFrame* aStartFrame,
                                            nsIFrame* aEndFrame) const
{
  return (aStartFrame && aEndFrame &&
          nsLayoutUtils::CompareTreePosition(aStartFrame, aEndFrame) <= 0);
}

nsresult
AccessibleCaretManager::DragCaretInternal(const nsPoint& aPoint)
{
  if (!mPresShell) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  nsPoint point = AdjustDragBoundary(aPoint);

  // Find out which content we point to
  nsIFrame* ptFrame = nsLayoutUtils::GetFrameForPoint(
    rootFrame, point,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult result;
  nsIFrame* newFrame = nullptr;
  nsPoint newPoint;
  nsPoint ptInFrame = point;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);
  result = fs->ConstrainFrameAndPointToAnchorSubtree(ptFrame, ptInFrame,
                                                     &newFrame, newPoint);
  if (NS_FAILED(result) || !newFrame) {
    return NS_ERROR_FAILURE;
  }

  bool selectable;
  newFrame->IsSelectable(&selectable, nullptr);
  if (!selectable) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame::ContentOffsets offsets =
    newFrame->GetContentOffsetsFromPoint(newPoint);
  if (!offsets.content) {
    return NS_ERROR_FAILURE;
  }

  Selection* selection = GetSelection();
  if (!selection) {
    return NS_ERROR_NULL_POINTER;
  }

  if (GetCaretMode() == CaretMode::Selection &&
      !CompareRangeWithContentOffset(offsets)) {
    return NS_ERROR_FAILURE;
  }

  ClearMaintainedSelection();

  nsIFrame* anchorFrame = nullptr;
  selection->GetPrimaryFrameForAnchorNode(&anchorFrame);

  nsIFrame* scrollable =
    nsLayoutUtils::GetClosestFrameOfType(anchorFrame, nsGkAtoms::scrollFrame);
  nsWeakFrame weakScrollable = scrollable;
  fs->HandleClick(offsets.content, offsets.StartOffset(), offsets.EndOffset(),
                  GetCaretMode() == CaretMode::Selection, false,
                  offsets.associate);
  if (!weakScrollable.IsAlive()) {
    return NS_OK;
  }

  // Scroll scrolled frame.
  nsIScrollableFrame* saf = do_QueryFrame(scrollable);
  nsIFrame* capturingFrame = saf->GetScrolledFrame();
  nsPoint ptInScrolled = point;
  nsLayoutUtils::TransformPoint(rootFrame, capturingFrame, ptInScrolled);
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, kAutoScrollTimerDelay);
  return NS_OK;
}

nsRect
AccessibleCaretManager::GetContentBoundaryForFrame(nsIFrame* aFrame) const
{
  nsRect resultRect;
  nsIFrame* rootFrame = mPresShell->GetRootFrame();

  for (; aFrame; aFrame = aFrame->GetNextContinuation()) {
    nsRect rect = aFrame->GetContentRectRelativeToSelf();
    nsLayoutUtils::TransformRect(aFrame, rootFrame, rect);
    resultRect = resultRect.Union(rect);

    nsIFrame::ChildListIterator lists(aFrame);
    for (; !lists.IsDone(); lists.Next()) {
      // Loop over all children to take the overflow rect into consideration.
      for (nsIFrame* child : lists.CurrentList()) {
        nsRect overflowRect = child->GetScrollableOverflowRect();
        nsLayoutUtils::TransformRect(child, rootFrame, overflowRect);
        resultRect = resultRect.Union(overflowRect);
      }
    }
  }

  // Shrink rect to make sure we never hit the boundary.
  resultRect.Deflate(kBoundaryAppUnits);
  return resultRect;
}

nsPoint
AccessibleCaretManager::AdjustDragBoundary(const nsPoint& aPoint) const
{
  nsPoint adjustedPoint = aPoint;

  int32_t focusOffset = 0;
  nsIFrame* focusFrame =
    nsCaret::GetFrameAndOffset(GetSelection(), nullptr, 0, &focusOffset);
  Element* editingHost = GetEditingHostForFrame(focusFrame);

  if (editingHost) {
    nsRect boundary =
      GetContentBoundaryForFrame(editingHost->GetPrimaryFrame());
    adjustedPoint = boundary.ClampPoint(adjustedPoint);
  }

  if (GetCaretMode() == CaretMode::Selection) {
    // Bug 1068474: Adjust the Y-coordinate so that the carets won't be in tilt
    // mode when a caret is being dragged surpass the other caret.
    //
    // For example, when dragging the second caret, the horizontal boundary (lower
    // bound) of its Y-coordinate is the logical position of the first caret.
    // Likewise, when dragging the first caret, the horizontal boundary (upper
    // bound) of its Y-coordinate is the logical position of the second caret.
    if (mActiveCaret == mFirstCaret.get()) {
      nscoord dragDownBoundaryY = mSecondCaret->LogicalPosition().y;
      if (dragDownBoundaryY > 0 && adjustedPoint.y > dragDownBoundaryY) {
        adjustedPoint.y = dragDownBoundaryY;
      }
    } else {
      nscoord dragUpBoundaryY = mFirstCaret->LogicalPosition().y;
      if (adjustedPoint.y < dragUpBoundaryY) {
        adjustedPoint.y = dragUpBoundaryY;
      }
    }
  }

  return adjustedPoint;
}

uint32_t
AccessibleCaretManager::CaretTimeoutMs() const
{
  static bool added = false;
  static uint32_t caretTimeoutMs = 0;

  if (!added) {
    Preferences::AddUintVarCache(&caretTimeoutMs,
                                 "layout.accessiblecaret.timeout_ms");
    added = true;
  }

  return caretTimeoutMs;
}

void
AccessibleCaretManager::LaunchCaretTimeoutTimer()
{
  if (!mCaretTimeoutTimer || CaretTimeoutMs() == 0 ||
      GetCaretMode() != CaretMode::Cursor || mActiveCaret) {
    return;
  }

  nsTimerCallbackFunc callback = [](nsITimer* aTimer, void* aClosure) {
    auto self = static_cast<AccessibleCaretManager*>(aClosure);
    if (self->GetCaretMode() == CaretMode::Cursor) {
      self->HideCarets();
    }
  };

  mCaretTimeoutTimer->InitWithFuncCallback(callback, this, CaretTimeoutMs(),
                                           nsITimer::TYPE_ONE_SHOT);
}

void
AccessibleCaretManager::CancelCaretTimeoutTimer()
{
  if (mCaretTimeoutTimer) {
    mCaretTimeoutTimer->Cancel();
  }
}

void
AccessibleCaretManager::DispatchCaretStateChangedEvent(CaretChangedReason aReason) const
{
  // Holding PresShell to prevent AccessibleCaretManager to be destroyed.
  nsCOMPtr<nsIPresShell> presShell = mPresShell;

  FlushLayout();
  if (presShell->IsDestroying()) {
    return;
  }

  Selection* sel = GetSelection();
  if (!sel) {
    return;
  }

  nsIDocument* doc = mPresShell->GetDocument();
  MOZ_ASSERT(doc);

  CaretStateChangedEventInit init;
  init.mBubbles = true;

  const nsRange* range = sel->GetAnchorFocusRange();
  nsINode* commonAncestorNode = nullptr;
  if (range) {
    commonAncestorNode = range->GetCommonAncestor();
  }

  if (!commonAncestorNode) {
    commonAncestorNode = sel->GetFrameSelection()->GetAncestorLimiter();
  }

  nsRefPtr<DOMRect> domRect = new DOMRect(ToSupports(doc));
  nsRect rect = nsContentUtils::GetSelectionBoundingRect(sel);

  nsIFrame* commonAncestorFrame = nullptr;
  nsIFrame* rootFrame = mPresShell->GetRootFrame();

  if (commonAncestorNode && commonAncestorNode->IsContent()) {
    commonAncestorFrame = commonAncestorNode->AsContent()->GetPrimaryFrame();
  }

  if (commonAncestorFrame && rootFrame) {
    nsLayoutUtils::TransformRect(rootFrame, commonAncestorFrame, rect);
    nsRect clampedRect = nsLayoutUtils::ClampRectToScrollFrames(commonAncestorFrame,
                                                                rect);
    nsLayoutUtils::TransformRect(commonAncestorFrame, rootFrame, clampedRect);
    domRect->SetLayoutRect(clampedRect);
    init.mSelectionVisible = !clampedRect.IsEmpty();
  } else {
    domRect->SetLayoutRect(rect);
    init.mSelectionVisible = true;
  }

  // Send isEditable info w/ event detail. This info can help determine
  // whether to show cut command on selection dialog or not.
  init.mSelectionEditable = commonAncestorFrame &&
    GetEditingHostForFrame(commonAncestorFrame);

  init.mBoundingClientRect = domRect;
  init.mReason = aReason;
  init.mCollapsed = sel->IsCollapsed();
  init.mCaretVisible = mFirstCaret->IsLogicallyVisible() ||
                       mSecondCaret->IsLogicallyVisible();
  sel->Stringify(init.mSelectedTextContent);

  nsRefPtr<CaretStateChangedEvent> event =
    CaretStateChangedEvent::Constructor(doc, NS_LITERAL_STRING("mozcaretstatechanged"), init);

  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;

  AC_LOG("%s: reason %d, collapsed %d, caretVisible %d", __FUNCTION__,
         init.mReason, init.mCollapsed, init.mCaretVisible);

  (new AsyncEventDispatcher(doc, event))->RunDOMEventWhenSafe();
}

} // namespace mozilla
