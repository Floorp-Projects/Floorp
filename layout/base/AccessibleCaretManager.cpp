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
#include "mozilla/AutoRestore.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/NodeFilterBinding.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/TreeWalker.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "nsCaret.h"
#include "nsContainerFrame.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsFrame.h"
#include "nsFrameSelection.h"
#include "nsGenericHTMLElement.h"
#include "nsIHapticFeedback.h"
#ifdef MOZ_WIDGET_ANDROID
#include "nsWindow.h"
#endif

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
    AC_PROCESS_ENUM_TO_STREAM(UpdateCaretsHint::DispatchNoEvent);
  }
  return aStream;
}
#undef AC_PROCESS_ENUM_TO_STREAM

/* static */ bool
AccessibleCaretManager::sSelectionBarEnabled = false;
/* static */ bool
AccessibleCaretManager::sCaretShownWhenLongTappingOnEmptyContent = false;
/* static */ bool
AccessibleCaretManager::sCaretsAlwaysTilt = false;
/* static */ bool
AccessibleCaretManager::sCaretsScriptUpdates = false;
/* static */ bool
AccessibleCaretManager::sCaretsAllowDraggingAcrossOtherCaret = true;
/* static */ bool
AccessibleCaretManager::sHapticFeedback = false;
/* static */ bool
AccessibleCaretManager::sExtendSelectionForPhoneNumber = false;
/* static */ bool
AccessibleCaretManager::sHideCaretsForMouseInput = true;

AccessibleCaretManager::AccessibleCaretManager(nsIPresShell* aPresShell)
  : mPresShell(aPresShell)
{
  if (!mPresShell) {
    return;
  }

  mFirstCaret = MakeUnique<AccessibleCaret>(mPresShell);
  mSecondCaret = MakeUnique<AccessibleCaret>(mPresShell);

  static bool addedPrefs = false;
  if (!addedPrefs) {
    Preferences::AddBoolVarCache(&sSelectionBarEnabled,
                                 "layout.accessiblecaret.bar.enabled");
    Preferences::AddBoolVarCache(&sCaretShownWhenLongTappingOnEmptyContent,
      "layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content");
    Preferences::AddBoolVarCache(&sCaretsAlwaysTilt,
                                 "layout.accessiblecaret.always_tilt");
    Preferences::AddBoolVarCache(&sCaretsScriptUpdates,
      "layout.accessiblecaret.allow_script_change_updates");
    Preferences::AddBoolVarCache(&sCaretsAllowDraggingAcrossOtherCaret,
      "layout.accessiblecaret.allow_dragging_across_other_caret", true);
    Preferences::AddBoolVarCache(&sHapticFeedback,
                                 "layout.accessiblecaret.hapticfeedback");
    Preferences::AddBoolVarCache(&sExtendSelectionForPhoneNumber,
      "layout.accessiblecaret.extend_selection_for_phone_number");
    Preferences::AddBoolVarCache(&sHideCaretsForMouseInput,
      "layout.accessiblecaret.hide_carets_for_mouse_input");
    addedPrefs = true;
  }
}

AccessibleCaretManager::~AccessibleCaretManager()
{
  MOZ_RELEASE_ASSERT(!mFlushingLayout, "Going away in FlushLayout? Bad!");
}

void
AccessibleCaretManager::Terminate()
{
  mFirstCaret = nullptr;
  mSecondCaret = nullptr;
  mActiveCaret = nullptr;
  mPresShell = nullptr;
}

nsresult
AccessibleCaretManager::OnSelectionChanged(nsIDocument* aDoc,
                                           Selection* aSel, int16_t aReason)
{
  Selection* selection = GetSelection();
  AC_LOG("%s: aSel: %p, GetSelection(): %p, aReason: %d", __FUNCTION__,
         aSel, selection, aReason);
  if (aSel != selection) {
    return NS_OK;
  }

  // eSetSelection events from the Fennec widget IME can be generated
  // by autoSuggest / autoCorrect composition changes, or by TYPE_REPLACE_TEXT
  // actions, either positioning cursor for text insert, or selecting
  // text-to-be-replaced. None should affect AccessibleCaret visibility.
  if (aReason & nsISelectionListener::IME_REASON) {
    return NS_OK;
  }

  // Move the cursor by Javascript / or unknown internal.
  if (aReason == nsISelectionListener::NO_REASON) {
    // Update visible carets, if javascript changes are allowed.
    if (sCaretsScriptUpdates &&
        (mFirstCaret->IsLogicallyVisible() || mSecondCaret->IsLogicallyVisible())) {
        UpdateCarets();
        return NS_OK;
    }
    // Default for NO_REASON is to make hidden.
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

  // For mouse input we don't want to show the carets.
  if (sHideCaretsForMouseInput &&
      mLastInputSource == MouseEventBinding::MOZ_SOURCE_MOUSE) {
    HideCarets();
    return NS_OK;
  }

  // When we want to hide the carets for mouse input, hide them for select
  // all action fired by keyboard as well.
  if (sHideCaretsForMouseInput &&
      mLastInputSource == MouseEventBinding::MOZ_SOURCE_KEYBOARD &&
      (aReason & nsISelectionListener::SELECTALL_REASON)) {
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
  }
}

void
AccessibleCaretManager::UpdateCarets(const UpdateCaretsHintSet& aHint)
{
  if (!FlushLayout()) {
    return;
  }

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
  RefPtr<nsCaret> caret = mPresShell->GetCaret();
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
AccessibleCaretManager::UpdateCaretsForCursorMode(const UpdateCaretsHintSet& aHints)
{
  AC_LOG("%s, selection: %p", __FUNCTION__, GetSelection());

  int32_t offset = 0;
  nsIFrame* frame = nullptr;
  if (!IsCaretDisplayableInCursorMode(&frame, &offset)) {
    HideCarets();
    return;
  }

  PositionChangedResult result = mFirstCaret->SetPosition(frame, offset);

  switch (result) {
    case PositionChangedResult::NotChanged:
    case PositionChangedResult::Changed:
      if (aHints == UpdateCaretsHint::Default) {
        if (HasNonEmptyTextContent(GetEditingHostForFrame(frame))) {
          mFirstCaret->SetAppearance(Appearance::Normal);
        } else if (sCaretShownWhenLongTappingOnEmptyContent) {
          if (mFirstCaret->IsLogicallyVisible()) {
            // Possible cases are: 1) SelectWordOrShortcut() sets the
            // appearance to Normal. 2) When the caret is out of viewport and
            // now scrolling into viewport, it has appearance NormalNotShown.
            mFirstCaret->SetAppearance(Appearance::Normal);
          } else {
            // Possible cases are: a) Single tap on current empty content;
            // OnSelectionChanged() sets the appearance to None due to
            // MOUSEDOWN_REASON. b) Single tap on other empty content;
            // OnBlur() sets the appearance to None.
            //
            // Do nothing to make the appearance remains None so that it can
            // be distinguished from case 2). Also do not set the appearance
            // to NormalNotShown here like the default update behavior.
          }
        } else {
          mFirstCaret->SetAppearance(Appearance::NormalNotShown);
        }
      } else if (aHints.contains(UpdateCaretsHint::RespectOldAppearance)) {
        // Do nothing to preserve the appearance of the caret set by the
        // caller.
      }
      break;

    case PositionChangedResult::Invisible:
      mFirstCaret->SetAppearance(Appearance::NormalNotShown);
      break;
  }

  mFirstCaret->SetSelectionBarEnabled(false);
  mSecondCaret->SetAppearance(Appearance::None);

  if (!aHints.contains(UpdateCaretsHint::DispatchNoEvent) &&
      !mActiveCaret) {
    DispatchCaretStateChangedEvent(CaretChangedReason::Updateposition);
  }
}

void
AccessibleCaretManager::UpdateCaretsForSelectionMode(const UpdateCaretsHintSet& aHints)
{
  AC_LOG("%s: selection: %p", __FUNCTION__, GetSelection());

  int32_t startOffset = 0;
  nsIFrame* startFrame =
    GetFrameForFirstRangeStartOrLastRangeEnd(eDirNext, &startOffset);

  int32_t endOffset = 0;
  nsIFrame* endFrame =
    GetFrameForFirstRangeStartOrLastRangeEnd(eDirPrevious, &endOffset);

  if (!CompareTreePosition(startFrame, endFrame)) {
    // XXX: Do we really have to hide carets if this condition isn't satisfied?
    HideCarets();
    return;
  }

  auto updateSingleCaret = [aHints](AccessibleCaret* aCaret, nsIFrame* aFrame,
                                    int32_t aOffset) -> PositionChangedResult
  {
    PositionChangedResult result = aCaret->SetPosition(aFrame, aOffset);
    aCaret->SetSelectionBarEnabled(sSelectionBarEnabled);

    switch (result) {
      case PositionChangedResult::NotChanged:
      case PositionChangedResult::Changed:
        if (aHints == UpdateCaretsHint::Default) {
          aCaret->SetAppearance(Appearance::Normal);
        } else if (aHints.contains(UpdateCaretsHint::RespectOldAppearance)) {
          // Do nothing to preserve the appearance of the caret set by the
          // caller.
        }
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
    if (!FlushLayout()) {
      return;
    }
  }

  if (aHints == UpdateCaretsHint::Default) {
    // Only check for tilt carets with default update hint. Otherwise we might
    // override the appearance set by the caller.
    if (sCaretsAlwaysTilt) {
      UpdateCaretsForAlwaysTilt(startFrame, endFrame);
    } else {
      UpdateCaretsForOverlappingTilt();
    }
  }

  if (!aHints.contains(UpdateCaretsHint::DispatchNoEvent) &&
      !mActiveCaret) {
    DispatchCaretStateChangedEvent(CaretChangedReason::Updateposition);
  }
}

bool
AccessibleCaretManager::UpdateCaretsForOverlappingTilt()
{
  if (!mFirstCaret->IsVisuallyVisible() || !mSecondCaret->IsVisuallyVisible()) {
    return false;
  }

  if (!mFirstCaret->Intersects(*mSecondCaret)) {
    mFirstCaret->SetAppearance(Appearance::Normal);
    mSecondCaret->SetAppearance(Appearance::Normal);
    return false;
  }

  if (mFirstCaret->LogicalPosition().x <=
      mSecondCaret->LogicalPosition().x) {
    mFirstCaret->SetAppearance(Appearance::Left);
    mSecondCaret->SetAppearance(Appearance::Right);
  } else {
    mFirstCaret->SetAppearance(Appearance::Right);
    mSecondCaret->SetAppearance(Appearance::Left);
  }

  return true;
}

void
AccessibleCaretManager::UpdateCaretsForAlwaysTilt(nsIFrame* aStartFrame,
                                                  nsIFrame* aEndFrame)
{
  // When a short LTR word in RTL environment is selected, the two carets
  // tilted inward might be overlapped. Make them tilt outward.
  if (UpdateCaretsForOverlappingTilt()) {
    return;
  }

  if (mFirstCaret->IsVisuallyVisible()) {
    auto startFrameWritingMode = aStartFrame->GetWritingMode();
    mFirstCaret->SetAppearance(startFrameWritingMode.IsBidiLTR() ?
                               Appearance::Left : Appearance::Right);
  }
  if (mSecondCaret->IsVisuallyVisible()) {
    auto endFrameWritingMode = aEndFrame->GetWritingMode();
    mSecondCaret->SetAppearance(endFrameWritingMode.IsBidiLTR() ?
                                Appearance::Right : Appearance::Left);
  }
}

void
AccessibleCaretManager::ProvideHapticFeedback()
{
  if (sHapticFeedback) {
    nsCOMPtr<nsIHapticFeedback> haptic =
      do_GetService("@mozilla.org/widget/hapticfeedback;1");
    haptic->PerformSimpleAction(haptic->LongPress);
  }
}

nsresult
AccessibleCaretManager::PressCaret(const nsPoint& aPoint,
                                   EventClassID aEventClass)
{
  nsresult rv = NS_ERROR_FAILURE;

  MOZ_ASSERT(aEventClass == eMouseEventClass || aEventClass == eTouchEventClass,
             "Unexpected event class!");

  using TouchArea = AccessibleCaret::TouchArea;
  TouchArea touchArea =
    aEventClass == eMouseEventClass ? TouchArea::CaretImage : TouchArea::Full;

  if (mFirstCaret->Contains(aPoint, touchArea)) {
    mActiveCaret = mFirstCaret.get();
    SetSelectionDirection(eDirPrevious);
  } else if (mSecondCaret->Contains(aPoint, touchArea)) {
    mActiveCaret = mSecondCaret.get();
    SetSelectionDirection(eDirNext);
  }

  if (mActiveCaret) {
    mOffsetYToCaretLogicalPosition =
      mActiveCaret->LogicalPosition().y - aPoint.y;
    SetSelectionDragState(true);
    DispatchCaretStateChangedEvent(CaretChangedReason::Presscaret);
    rv = NS_OK;
  }

  return rv;
}

nsresult
AccessibleCaretManager::DragCaret(const nsPoint& aPoint)
{
  MOZ_ASSERT(mActiveCaret);
  MOZ_ASSERT(GetCaretMode() != CaretMode::None);

  if (!mPresShell || !mPresShell->GetRootFrame() || !GetSelection()) {
    return NS_ERROR_NULL_POINTER;
  }

  StopSelectionAutoScrollTimer();
  DragCaretInternal(aPoint);

  // We want to scroll the page even if we failed to drag the caret.
  StartSelectionAutoScrollTimer(aPoint);
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
  // If the long-tap is landing on a pre-existing selection, don't replace
  // it with a new one. Instead just return and let the context menu pop up
  // on the pre-existing selection.
  if (GetCaretMode() == CaretMode::Selection &&
      GetSelection()->ContainsPoint(aPoint)) {
    AC_LOG("%s: UpdateCarets() for current selection", __FUNCTION__);
    UpdateCarets();
    ProvideHapticFeedback();
    return NS_OK;
  }

  if (!mPresShell) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Find the frame under point.
  AutoWeakFrame ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, aPoint,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame.IsAlive()) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* focusableFrame = GetFocusableFrame(ptFrame);

#ifdef DEBUG_FRAME_DUMP
  AC_LOG("%s: Found %s under (%d, %d)", __FUNCTION__, ptFrame->ListTag().get(),
         aPoint.x, aPoint.y);
  AC_LOG("%s: Found %s focusable", __FUNCTION__,
         focusableFrame ? focusableFrame->ListTag().get() : "no frame");
#endif

  // Get ptInFrame here so that we don't need to check whether rootFrame is
  // alive later. Note that if ptFrame is being moved by
  // IMEStateManager::NotifyIME() or ChangeFocusToOrClearOldFocus() below,
  // something under the original point will be selected, which may not be the
  // original text the user wants to select.
  nsPoint ptInFrame = aPoint;
  nsLayoutUtils::TransformPoint(rootFrame, ptFrame, ptInFrame);

  // Firstly check long press on an empty editable content.
  Element* newFocusEditingHost = GetEditingHostForFrame(ptFrame);
  if (focusableFrame && newFocusEditingHost &&
      !HasNonEmptyTextContent(newFocusEditingHost)) {
    ChangeFocusToOrClearOldFocus(focusableFrame);

    if (sCaretShownWhenLongTappingOnEmptyContent) {
      mFirstCaret->SetAppearance(Appearance::Normal);
    }
    // We need to update carets to get correct information before dispatching
    // CaretStateChangedEvent.
    UpdateCarets();
    ProvideHapticFeedback();
    DispatchCaretStateChangedEvent(CaretChangedReason::Longpressonemptycontent);
    return NS_OK;
  }

  bool selectable = ptFrame->IsSelectable(nullptr);

#ifdef DEBUG_FRAME_DUMP
  AC_LOG("%s: %s %s selectable.", __FUNCTION__, ptFrame->ListTag().get(),
         selectable ? "is" : "is NOT");
#endif

  if (!selectable) {
    return NS_ERROR_FAILURE;
  }

  // Commit the composition string of the old editable focus element (if there
  // is any) before changing the focus.
  IMEStateManager::NotifyIME(widget::REQUEST_TO_COMMIT_COMPOSITION,
                             mPresShell->GetPresContext());
  if (!ptFrame.IsAlive()) {
    // Cannot continue because ptFrame died.
    return NS_ERROR_FAILURE;
  }

  // ptFrame is selectable. Now change the focus.
  ChangeFocusToOrClearOldFocus(focusableFrame);
  if (!ptFrame.IsAlive()) {
    // Cannot continue because ptFrame died.
    return NS_ERROR_FAILURE;
  }

  // Then try select a word under point.
  nsresult rv = SelectWord(ptFrame, ptInFrame);
  UpdateCarets();
  ProvideHapticFeedback();

  return rv;
}

void
AccessibleCaretManager::OnScrollStart()
{
  AC_LOG("%s", __FUNCTION__);

  mIsScrollStarted = true;

  if (mFirstCaret->IsLogicallyVisible() || mSecondCaret->IsLogicallyVisible()) {
    // Dispatch the event only if one of the carets is logically visible like in
    // HideCarets().
    DispatchCaretStateChangedEvent(CaretChangedReason::Scroll);
  }
}

void
AccessibleCaretManager::OnScrollEnd()
{
  if (mLastUpdateCaretMode != GetCaretMode()) {
    return;
  }

  mIsScrollStarted = false;

  if (GetCaretMode() == CaretMode::Cursor) {
    if (!mFirstCaret->IsLogicallyVisible()) {
      // If the caret is hidden (Appearance::None) due to blur, no
      // need to update it.
      return;
    }
  }

  // For mouse input we don't want to show the carets.
  if (sHideCaretsForMouseInput &&
      mLastInputSource == MouseEventBinding::MOZ_SOURCE_MOUSE) {
    AC_LOG("%s: HideCarets()", __FUNCTION__);
    HideCarets();
    return;
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
    if (mIsScrollStarted) {
      // We don't want extra CaretStateChangedEvents dispatched when user is
      // scrolling the page.
      AC_LOG("%s: UpdateCarets(RespectOldAppearance | DispatchNoEvent)",
             __FUNCTION__);
      UpdateCarets({ UpdateCaretsHint::RespectOldAppearance,
                     UpdateCaretsHint::DispatchNoEvent });
    } else {
      AC_LOG("%s: UpdateCarets(RespectOldAppearance)", __FUNCTION__);
      UpdateCarets(UpdateCaretsHint::RespectOldAppearance);
    }
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

void
AccessibleCaretManager::OnFrameReconstruction()
{
  mFirstCaret->EnsureApzAware();
  mSecondCaret->EnsureApzAware();
}

void
AccessibleCaretManager::SetLastInputSource(uint16_t aInputSource)
{
  mLastInputSource = aInputSource;
}

Selection*
AccessibleCaretManager::GetSelection() const
{
  RefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (!fs) {
    return nullptr;
  }
  return fs->GetSelection(SelectionType::eNormal);
}

already_AddRefed<nsFrameSelection>
AccessibleCaretManager::GetFrameSelection() const
{
  if (!mPresShell) {
    return nullptr;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  MOZ_ASSERT(fm);

  nsIContent* focusedContent = fm->GetFocusedElement();
  if (!focusedContent) {
    // For non-editable content
    return mPresShell->FrameSelection();
  }

  nsIFrame* focusFrame = focusedContent->GetPrimaryFrame();
  if (!focusFrame) {
    return nullptr;
  }

  // Prevent us from touching the nsFrameSelection associated with other
  // PresShell.
  RefPtr<nsFrameSelection> fs = focusFrame->GetFrameSelection();
  if (!fs || fs->GetShell() != mPresShell) {
    return nullptr;
  }

  return fs.forget();
}

nsAutoString
AccessibleCaretManager::StringifiedSelection() const
{
  nsAutoString str;
  Selection* selection = GetSelection();
  if (selection) {
    selection->Stringify(str);
  }
  return str;
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
    RefPtr<Element> focusableElement =
      focusableContent->IsElement() ? focusableContent->AsElement() : nullptr;
    fm->SetFocus(focusableElement, nsIFocusManager::FLAG_BYMOUSE);
  } else {
    nsPIDOMWindowOuter* win = mPresShell->GetDocument()->GetWindow();
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

  // Smart-select phone numbers if possible.
  if (sExtendSelectionForPhoneNumber) {
    SelectMoreIfPhoneNumber();
  }

  return rs;
}

void
AccessibleCaretManager::SetSelectionDragState(bool aState) const
{
  RefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->SetDragState(aState);
  }

  // Pin Fennecs DynamicToolbarAnimator in place before/after dragging,
  // to avoid co-incident screen scrolling.
  #ifdef MOZ_WIDGET_ANDROID
    nsIDocument* doc = mPresShell->GetDocument();
    MOZ_ASSERT(doc);
    nsIWidget* widget = nsContentUtils::WidgetForDocument(doc);
    static_cast<nsWindow*>(widget)->SetSelectionDragState(aState);
  #endif
}

bool
AccessibleCaretManager::IsPhoneNumber(nsAString& aCandidate) const
{
  RefPtr<nsIDocument> doc = mPresShell->GetDocument();
  nsAutoString phoneNumberRegex(
    NS_LITERAL_STRING("(^\\+)?[0-9 ,\\-.()*#pw]{1,30}$"));
  return nsContentUtils::IsPatternMatching(aCandidate, phoneNumberRegex, doc);
}

void
AccessibleCaretManager::SelectMoreIfPhoneNumber() const
{
  nsAutoString selectedText = StringifiedSelection();

  if (IsPhoneNumber(selectedText)) {
    SetSelectionDirection(eDirNext);
    ExtendPhoneNumberSelection(NS_LITERAL_STRING("forward"));

    SetSelectionDirection(eDirPrevious);
    ExtendPhoneNumberSelection(NS_LITERAL_STRING("backward"));

    SetSelectionDirection(eDirNext);
  }
}

void
AccessibleCaretManager::ExtendPhoneNumberSelection(const nsAString& aDirection) const
{
  if (!mPresShell) {
    return;
  }

  // Extend the phone number selection until we find a boundary.
  RefPtr<Selection> selection = GetSelection();

  while (selection) {
    const nsRange* anchorFocusRange = selection->GetAnchorFocusRange();
    if (!anchorFocusRange) {
      return;
    }

    // Backup the anchor focus range since both anchor node and focus node might
    // be changed after calling Selection::Modify().
    RefPtr<nsRange> oldAnchorFocusRange = anchorFocusRange->CloneRange();

    // Save current focus node, focus offset and the selected text so that
    // we can compare them with the modified ones later.
    nsINode* oldFocusNode = selection->GetFocusNode();
    uint32_t oldFocusOffset = selection->FocusOffset();
    nsAutoString oldSelectedText = StringifiedSelection();

    // Extend the selection by one char.
    selection->Modify(NS_LITERAL_STRING("extend"),
                      aDirection,
                      NS_LITERAL_STRING("character"),
                      IgnoreErrors());
    if (IsTerminated()) {
      return;
    }

    // If the selection didn't change, (can't extend further), we're done.
    if (selection->GetFocusNode() == oldFocusNode &&
        selection->FocusOffset() == oldFocusOffset) {
      return;
    }

    // If the changed selection isn't a valid phone number, we're done.
    // Also, if the selection was extended to a new block node, the string
    // returned by stringify() won't have a new line at the beginning or the
    // end of the string. Therefore, if either focus node or offset is
    // changed, but selected text is not changed, we're done, too.
    nsAutoString selectedText = StringifiedSelection();

    if (!IsPhoneNumber(selectedText) || oldSelectedText == selectedText) {
      // Backout the undesired selection extend, restore the old anchor focus
      // range before exit.
      selection->SetAnchorFocusToRange(oldAnchorFocusRange);
      return;
    }
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
  RefPtr<nsFrameSelection> fs = GetFrameSelection();
  if (fs) {
    fs->MaintainSelection(eSelectNoAmount);
  }
}

bool
AccessibleCaretManager::FlushLayout()
{
  if (mPresShell) {
    AutoRestore<bool> flushing(mFlushingLayout);
    mFlushingLayout = true;

    if (nsIDocument* doc = mPresShell->GetDocument()) {
      doc->FlushPendingNotifications(FlushType::Layout);
    }
  }

  return !IsTerminated();
}

nsIFrame*
AccessibleCaretManager::GetFrameForFirstRangeStartOrLastRangeEnd(
  nsDirection aDirection,
  int32_t* aOutOffset,
  nsIContent** aOutContent,
  int32_t* aOutContentOffset) const
{
  if (!mPresShell) {
    return nullptr;
  }

  MOZ_ASSERT(GetCaretMode() == CaretMode::Selection);
  MOZ_ASSERT(aOutOffset, "aOutOffset shouldn't be nullptr!");

  nsRange* range = nullptr;
  RefPtr<nsINode> startNode;
  RefPtr<nsINode> endNode;
  int32_t nodeOffset = 0;
  CaretAssociationHint hint;

  RefPtr<Selection> selection = GetSelection();
  bool findInFirstRangeStart = aDirection == eDirNext;

  if (findInFirstRangeStart) {
    range = selection->GetRangeAt(0);
    startNode = range->GetStartContainer();
    endNode = range->GetEndContainer();
    nodeOffset = range->StartOffset();
    hint = CARET_ASSOCIATE_AFTER;
  } else {
    range = selection->GetRangeAt(selection->RangeCount() - 1);
    startNode = range->GetEndContainer();
    endNode = range->GetStartContainer();
    nodeOffset = range->EndOffset();
    hint = CARET_ASSOCIATE_BEFORE;
  }

  nsCOMPtr<nsIContent> startContent = do_QueryInterface(startNode);
  RefPtr<nsFrameSelection> fs = GetFrameSelection();
  nsIFrame* startFrame =
    fs->GetFrameForNodeOffset(startContent, nodeOffset, hint, aOutOffset);

  if (!startFrame) {
    ErrorResult err;
    RefPtr<TreeWalker> walker = mPresShell->GetDocument()->CreateTreeWalker(
      *startNode, dom::NodeFilterBinding::SHOW_ALL, nullptr, err);

    if (!walker) {
      return nullptr;
    }

    startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
    while (!startFrame && startNode != endNode) {
      startNode = findInFirstRangeStart ? walker->NextNode(err)
                                        : walker->PreviousNode(err);

      if (!startNode) {
        break;
      }

      startContent = startNode->AsContent();
      startFrame = startContent ? startContent->GetPrimaryFrame() : nullptr;
    }

    // We are walking among the nodes in the content tree, so the node offset
    // relative to startNode should be set to 0.
    nodeOffset = 0;
    *aOutOffset = 0;
  }

  if (startFrame) {
    if (aOutContent) {
      startContent.forget(aOutContent);
    }
    if (aOutContentOffset) {
      *aOutContentOffset = nodeOffset;
    }
  }

  return startFrame;
}

bool
AccessibleCaretManager::RestrictCaretDraggingOffsets(
  nsIFrame::ContentOffsets& aOffsets)
{
  if (!mPresShell) {
    return false;
  }

  MOZ_ASSERT(GetCaretMode() == CaretMode::Selection);

  nsDirection dir = mActiveCaret == mFirstCaret.get() ? eDirPrevious : eDirNext;
  int32_t offset = 0;
  nsCOMPtr<nsIContent> content;
  int32_t contentOffset = 0;
  nsIFrame* frame =
    GetFrameForFirstRangeStartOrLastRangeEnd(dir, &offset,
                                             getter_AddRefs(content),
                                             &contentOffset);

  if (!frame) {
    return false;
  }


  // Compare the active caret's new position (aOffsets) to the inactive caret's
  // position.
  int32_t cmpToInactiveCaretPos =
    nsContentUtils::ComparePoints(aOffsets.content, aOffsets.StartOffset(),
                                  content, contentOffset);

  // Move one character (in the direction of dir) from the inactive caret's
  // position. This is the limit for the active caret's new position.
  nsPeekOffsetStruct limit(eSelectCluster, dir, offset, nsPoint(0, 0), true, true,
                           false, false, false);
  nsresult rv = frame->PeekOffset(&limit);
  if (NS_FAILED(rv)) {
    limit.mResultContent = content;
    limit.mContentOffset = contentOffset;
  }

  // Compare the active caret's new position (aOffsets) to the limit.
  int32_t cmpToLimit =
    nsContentUtils::ComparePoints(aOffsets.content, aOffsets.StartOffset(),
                                  limit.mResultContent, limit.mContentOffset);

  auto SetOffsetsToLimit = [&aOffsets, &limit] () {
    aOffsets.content = limit.mResultContent;
    aOffsets.offset = limit.mContentOffset;
    aOffsets.secondaryOffset = limit.mContentOffset;
  };

  if (!sCaretsAllowDraggingAcrossOtherCaret) {
    if ((mActiveCaret == mFirstCaret.get() && cmpToLimit == 1) ||
        (mActiveCaret == mSecondCaret.get() && cmpToLimit == -1)) {
      // The active caret's position is past the limit, which we don't allow
      // here. So set it to the limit, resulting in one character being
      // selected.
      SetOffsetsToLimit();
    }
  } else {
    switch (cmpToInactiveCaretPos) {
      case 0:
        // The active caret's position is the same as the position of the
        // inactive caret. So set it to the limit to prevent the selection from
        // being collapsed, resulting in one character being selected.
        SetOffsetsToLimit();
        break;
      case 1:
        if (mActiveCaret == mFirstCaret.get()) {
          // First caret was moved across the second caret. After making change
          // to the selection, the user will drag the second caret.
          mActiveCaret = mSecondCaret.get();
        }
        break;
      case -1:
        if (mActiveCaret == mSecondCaret.get()) {
          // Second caret was moved across the first caret. After making change
          // to the selection, the user will drag the first caret.
          mActiveCaret = mFirstCaret.get();
        }
        break;
    }
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
  MOZ_ASSERT(mPresShell);

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  MOZ_ASSERT(rootFrame, "We need root frame to compute caret dragging!");

  nsPoint point = AdjustDragBoundary(
    nsPoint(aPoint.x, aPoint.y + mOffsetYToCaretLogicalPosition));

  // Find out which content we point to
  nsIFrame* ptFrame = nsLayoutUtils::GetFrameForPoint(
    rootFrame, point,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsFrameSelection> fs = GetFrameSelection();
  MOZ_ASSERT(fs);

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

  if (!newFrame->IsSelectable(nullptr)) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame::ContentOffsets offsets =
    newFrame->GetContentOffsetsFromPoint(newPoint);
  if (offsets.IsNull()) {
    return NS_ERROR_FAILURE;
  }

  if (GetCaretMode() == CaretMode::Selection &&
      !RestrictCaretDraggingOffsets(offsets)) {
    return NS_ERROR_FAILURE;
  }

  ClearMaintainedSelection();

  fs->HandleClick(offsets.content, offsets.StartOffset(), offsets.EndOffset(),
                  GetCaretMode() == CaretMode::Selection, false,
                  offsets.associate);
  return NS_OK;
}

nsRect
AccessibleCaretManager::GetAllChildFrameRectsUnion(nsIFrame* aFrame) const
{
  nsRect unionRect;

  // Drill through scroll frames, we don't want to include scrollbar child
  // frames below.
  for (nsIFrame* frame = aFrame->GetContentInsertionFrame();
       frame;
       frame = frame->GetNextContinuation()) {
    nsRect frameRect;

    for (nsIFrame::ChildListIterator lists(frame); !lists.IsDone(); lists.Next()) {
      // Loop all children to union their scrollable overflow rect.
      for (nsIFrame* child : lists.CurrentList()) {
        nsRect childRect = child->GetScrollableOverflowRectRelativeToSelf();
        nsLayoutUtils::TransformRect(child, frame, childRect);

        // A TextFrame containing only '\n' has positive height and width 0, or
        // positive width and height 0 if it's vertical. Need to use UnionEdges
        // to add its rect. BRFrame rect should be non-empty.
        if (childRect.IsEmpty()) {
          frameRect = frameRect.UnionEdges(childRect);
        } else {
          frameRect = frameRect.Union(childRect);
        }
      }
    }

    MOZ_ASSERT(!frameRect.IsEmpty(),
               "Editable frames should have at least one BRFrame child to make "
               "frameRect non-empty!");
    if (frame != aFrame) {
      nsLayoutUtils::TransformRect(frame, aFrame, frameRect);
    }
    unionRect = unionRect.Union(frameRect);
  }

  return unionRect;
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
    nsIFrame* editingHostFrame = editingHost->GetPrimaryFrame();
    if (editingHostFrame) {
      nsRect boundary = GetAllChildFrameRectsUnion(editingHostFrame);
      nsLayoutUtils::TransformRect(editingHostFrame, mPresShell->GetRootFrame(),
                                   boundary);

      // Shrink the rect to make sure we never hit the boundary.
      boundary.Deflate(kBoundaryAppUnits);

      adjustedPoint = boundary.ClampPoint(adjustedPoint);
    }
  }

  if (GetCaretMode() == CaretMode::Selection &&
      !sCaretsAllowDraggingAcrossOtherCaret) {
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

void
AccessibleCaretManager::StartSelectionAutoScrollTimer(
  const nsPoint& aPoint) const
{
  Selection* selection = GetSelection();
  MOZ_ASSERT(selection);

  nsIFrame* anchorFrame = nullptr;
  selection->GetPrimaryFrameForAnchorNode(&anchorFrame);
  if (!anchorFrame) {
    return;
  }

  nsIScrollableFrame* scrollFrame =
    nsLayoutUtils::GetNearestScrollableFrame(
      anchorFrame,
      nsLayoutUtils::SCROLLABLE_SAME_DOC |
      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
  if (!scrollFrame) {
    return;
  }

  nsIFrame* capturingFrame = scrollFrame->GetScrolledFrame();
  if (!capturingFrame) {
    return;
  }

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  MOZ_ASSERT(rootFrame);
  nsPoint ptInScrolled = aPoint;
  nsLayoutUtils::TransformPoint(rootFrame, capturingFrame, ptInScrolled);

  RefPtr<nsFrameSelection> fs = GetFrameSelection();
  MOZ_ASSERT(fs);
  fs->StartAutoScrollTimer(capturingFrame, ptInScrolled, kAutoScrollTimerDelay);
}

void
AccessibleCaretManager::StopSelectionAutoScrollTimer() const
{
  RefPtr<nsFrameSelection> fs = GetFrameSelection();
  MOZ_ASSERT(fs);
  fs->StopAutoScrollTimer();
}

void
AccessibleCaretManager::DispatchCaretStateChangedEvent(CaretChangedReason aReason)
{
  if (!FlushLayout()) {
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

  RefPtr<DOMRect> domRect = new DOMRect(ToSupports(doc));
  nsRect rect = nsLayoutUtils::GetSelectionBoundingRect(sel);

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
  init.mCaretVisuallyVisible = mFirstCaret->IsVisuallyVisible() ||
                                mSecondCaret->IsVisuallyVisible();
  sel->Stringify(init.mSelectedTextContent);

  RefPtr<CaretStateChangedEvent> event =
    CaretStateChangedEvent::Constructor(doc, NS_LITERAL_STRING("mozcaretstatechanged"), init);

  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  AC_LOG("%s: reason %" PRIu32 ", collapsed %d, caretVisible %" PRIu32, __FUNCTION__,
         static_cast<uint32_t>(init.mReason), init.mCollapsed,
         static_cast<uint32_t>(init.mCaretVisible));

  (new AsyncEventDispatcher(doc, event))->RunDOMEventWhenSafe();
}

} // namespace mozilla
