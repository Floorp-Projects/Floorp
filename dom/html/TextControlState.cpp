/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextControlState.h"
#include "mozilla/Attributes.h"
#include "mozilla/TextInputListener.h"

#include "nsCOMPtr.h"
#include "nsView.h"
#include "nsCaret.h"
#include "nsLayoutCID.h"
#include "nsITextControlFrame.h"
#include "nsContentCreatorFunctions.h"
#include "nsTextControlFrame.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMEventListener.h"
#include "nsIWidget.h"
#include "nsIDocumentEncoder.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/EventListenerManager.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsTextNode.h"
#include "nsIController.h"
#include "nsIScrollableFrame.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/InputEventOptions.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/Text.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_ui.h"
#include "nsFrameSelection.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ShortcutKeys.h"
#include "mozilla/KeyEventHandler.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/ScrollTypes.h"

namespace mozilla {

using namespace dom;
using ValueSetterOption = TextControlState::ValueSetterOption;
using ValueSetterOptions = TextControlState::ValueSetterOptions;
using SelectionDirection = nsITextControlFrame::SelectionDirection;

/*****************************************************************************
 * TextControlElement
 *****************************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(TextControlElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    TextControlElement, nsGenericHTMLFormControlElementWithState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    TextControlElement, nsGenericHTMLFormControlElementWithState)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    TextControlElement, nsGenericHTMLFormControlElementWithState)

/*static*/
bool TextControlElement::GetWrapPropertyEnum(
    nsIContent* aContent, TextControlElement::nsHTMLTextWrap& aWrapProp) {
  // soft is the default; "physical" defaults to soft as well because all other
  // browsers treat it that way and there is no real reason to maintain physical
  // and virtual as separate entities if no one else does.  Only hard and off
  // do anything different.
  aWrapProp = eHTMLTextWrap_Soft;  // the default

  if (!aContent->IsHTMLElement()) {
    return false;
  }

  static mozilla::dom::Element::AttrValuesArray strings[] = {
      nsGkAtoms::HARD, nsGkAtoms::OFF, nullptr};
  switch (aContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::wrap, strings, eIgnoreCase)) {
    case 0:
      aWrapProp = eHTMLTextWrap_Hard;
      break;
    case 1:
      aWrapProp = eHTMLTextWrap_Off;
      break;
  }

  return true;
}

/*static*/
already_AddRefed<TextControlElement>
TextControlElement::GetTextControlElementFromEditingHost(nsIContent* aHost) {
  if (!aHost) {
    return nullptr;
  }

  RefPtr<TextControlElement> parent =
      TextControlElement::FromNodeOrNull(aHost->GetParent());
  return parent.forget();
}

TextControlElement::FocusTristate TextControlElement::FocusState() {
  // We can't be focused if we aren't in a (composed) document
  Document* doc = GetComposedDoc();
  if (!doc) {
    return FocusTristate::eUnfocusable;
  }

  // first see if we are disabled or not. If disabled then do nothing.
  if (IsDisabled()) {
    return FocusTristate::eUnfocusable;
  }

  return IsInActiveTab(doc) ? FocusTristate::eActiveWindow
                            : FocusTristate::eInactiveWindow;
}

using ValueChangeKind = TextControlElement::ValueChangeKind;

MOZ_CAN_RUN_SCRIPT inline nsresult SetEditorFlagsIfNecessary(
    EditorBase& aEditorBase, uint32_t aFlags) {
  if (aEditorBase.Flags() == aFlags) {
    return NS_OK;
  }
  return aEditorBase.SetFlags(aFlags);
}

/*****************************************************************************
 * mozilla::AutoInputEventSuppresser
 *****************************************************************************/

class MOZ_STACK_CLASS AutoInputEventSuppresser final {
 public:
  explicit AutoInputEventSuppresser(TextEditor* aTextEditor)
      : mTextEditor(aTextEditor),
        // To protect against a reentrant call to SetValue, we check whether
        // another SetValue is already happening for this editor.  If it is,
        // we must wait until we unwind to re-enable oninput events.
        mOuterTransaction(aTextEditor->IsSuppressingDispatchingInputEvent()) {
    MOZ_ASSERT(mTextEditor);
    mTextEditor->SuppressDispatchingInputEvent(true);
  }
  ~AutoInputEventSuppresser() {
    mTextEditor->SuppressDispatchingInputEvent(mOuterTransaction);
  }

 private:
  RefPtr<TextEditor> mTextEditor;
  bool mOuterTransaction;
};

/*****************************************************************************
 * mozilla::RestoreSelectionState
 *****************************************************************************/

class RestoreSelectionState : public Runnable {
 public:
  RestoreSelectionState(TextControlState* aState, nsTextControlFrame* aFrame)
      : Runnable("RestoreSelectionState"),
        mFrame(aFrame),
        mTextControlState(aState) {}

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    if (!mTextControlState) {
      return NS_OK;
    }

    AutoHideSelectionChanges hideSelectionChanges(
        mFrame->GetConstFrameSelection());

    if (mFrame) {
      // SetSelectionRange leads to
      // Selection::AddRangeAndSelectFramesAndNotifyListeners which flushes
      // Layout - need to block script to avoid nested PrepareEditor calls (bug
      // 642800).
      nsAutoScriptBlocker scriptBlocker;
      TextControlState::SelectionProperties& properties =
          mTextControlState->GetSelectionProperties();
      if (properties.IsDirty()) {
        mFrame->SetSelectionRange(properties.GetStart(), properties.GetEnd(),
                                  properties.GetDirection());
      }
    }

    if (mTextControlState) {
      mTextControlState->FinishedRestoringSelection();
    }
    return NS_OK;
  }

  // Let the text editor tell us we're no longer relevant - avoids use of
  // AutoWeakFrame
  void Revoke() {
    mFrame = nullptr;
    mTextControlState = nullptr;
  }

 private:
  nsTextControlFrame* mFrame;
  TextControlState* mTextControlState;
};

/*****************************************************************************
 * mozilla::AutoRestoreEditorState
 *****************************************************************************/

class MOZ_RAII AutoRestoreEditorState final {
 public:
  MOZ_CAN_RUN_SCRIPT explicit AutoRestoreEditorState(TextEditor* aTextEditor)
      : mTextEditor(aTextEditor),
        mSavedFlags(mTextEditor->Flags()),
        mSavedMaxLength(mTextEditor->MaxTextLength()),
        mSavedEchoingPasswordPrevented(
            mTextEditor->EchoingPasswordPrevented()) {
    MOZ_ASSERT(mTextEditor);

    // EditorBase::SetFlags() is a virtual method.  Even though it does nothing
    // if new flags and current flags are same, the calling cost causes
    // appearing the method in profile.  So, this class should check if it's
    // necessary to call.
    uint32_t flags = mSavedFlags;
    flags &= ~nsIEditor::eEditorReadonlyMask;
    if (mSavedFlags != flags) {
      // It's aTextEditor and whose lifetime must be guaranteed by the caller.
      MOZ_KnownLive(mTextEditor)->SetFlags(flags);
    }
    mTextEditor->PreventToEchoPassword();
    mTextEditor->SetMaxTextLength(-1);
  }

  MOZ_CAN_RUN_SCRIPT ~AutoRestoreEditorState() {
    if (!mSavedEchoingPasswordPrevented) {
      mTextEditor->AllowToEchoPassword();
    }
    mTextEditor->SetMaxTextLength(mSavedMaxLength);
    // mTextEditor's lifetime must be guaranteed by owner of the instance
    // since the constructor is marked as `MOZ_CAN_RUN_SCRIPT` and this is
    // a stack only class.
    SetEditorFlagsIfNecessary(MOZ_KnownLive(*mTextEditor), mSavedFlags);
  }

 private:
  TextEditor* mTextEditor;
  uint32_t mSavedFlags;
  int32_t mSavedMaxLength;
  bool mSavedEchoingPasswordPrevented;
};

/*****************************************************************************
 * mozilla::AutoDisableUndo
 *****************************************************************************/

class MOZ_RAII AutoDisableUndo final {
 public:
  explicit AutoDisableUndo(TextEditor* aTextEditor)
      : mTextEditor(aTextEditor), mNumberOfMaximumTransactions(0) {
    MOZ_ASSERT(mTextEditor);

    mNumberOfMaximumTransactions =
        mTextEditor ? mTextEditor->NumberOfMaximumTransactions() : 0;
    DebugOnly<bool> disabledUndoRedo = mTextEditor->DisableUndoRedo();
    NS_WARNING_ASSERTION(disabledUndoRedo,
                         "Failed to disable undo/redo transactions");
  }

  ~AutoDisableUndo() {
    // Don't change enable/disable of undo/redo if it's enabled after
    // it's disabled by the constructor because we shouldn't change
    // the maximum undo/redo count to the old value.
    if (mTextEditor->IsUndoRedoEnabled()) {
      return;
    }
    // If undo/redo was enabled, mNumberOfMaximumTransactions is -1 or lager
    // than 0.  Only when it's 0, it was disabled.
    if (mNumberOfMaximumTransactions) {
      DebugOnly<bool> enabledUndoRedo =
          mTextEditor->EnableUndoRedo(mNumberOfMaximumTransactions);
      NS_WARNING_ASSERTION(enabledUndoRedo,
                           "Failed to enable undo/redo transactions");
    } else {
      DebugOnly<bool> disabledUndoRedo = mTextEditor->DisableUndoRedo();
      NS_WARNING_ASSERTION(disabledUndoRedo,
                           "Failed to disable undo/redo transactions");
    }
  }

 private:
  TextEditor* mTextEditor;
  int32_t mNumberOfMaximumTransactions;
};

static bool SuppressEventHandlers(nsPresContext* aPresContext) {
  bool suppressHandlers = false;

  if (aPresContext) {
    // Right now we only suppress event handlers and controller manipulation
    // when in a print preview or print context!

    // In the current implementation, we only paginate when
    // printing or in print preview.

    suppressHandlers = aPresContext->IsPaginated();
  }

  return suppressHandlers;
}

/*****************************************************************************
 * mozilla::TextInputSelectionController
 *****************************************************************************/

class TextInputSelectionController final : public nsSupportsWeakReference,
                                           public nsISelectionController {
  ~TextInputSelectionController() = default;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TextInputSelectionController,
                                           nsISelectionController)

  TextInputSelectionController(PresShell* aPresShell, nsIContent* aLimiter);

  void SetScrollableFrame(nsIScrollableFrame* aScrollableFrame);
  nsFrameSelection* GetConstFrameSelection() { return mFrameSelection; }
  // Will return null if !mFrameSelection.
  Selection* GetSelection(SelectionType aSelectionType);

  // NSISELECTIONCONTROLLER INTERFACES
  NS_IMETHOD SetDisplaySelection(int16_t toggle) override;
  NS_IMETHOD GetDisplaySelection(int16_t* _retval) override;
  NS_IMETHOD SetSelectionFlags(int16_t aInEnable) override;
  NS_IMETHOD GetSelectionFlags(int16_t* aOutEnable) override;
  NS_IMETHOD GetSelectionFromScript(RawSelectionType aRawSelectionType,
                                    Selection** aSelection) override;
  Selection* GetSelection(RawSelectionType aRawSelectionType) override;
  NS_IMETHOD ScrollSelectionIntoView(RawSelectionType aRawSelectionType,
                                     int16_t aRegion, int16_t aFlags) override;
  NS_IMETHOD RepaintSelection(RawSelectionType aRawSelectionType) override;
  nsresult RepaintSelection(nsPresContext* aPresContext,
                            SelectionType aSelectionType);
  NS_IMETHOD SetCaretEnabled(bool enabled) override;
  NS_IMETHOD SetCaretReadOnly(bool aReadOnly) override;
  NS_IMETHOD GetCaretEnabled(bool* _retval) override;
  NS_IMETHOD GetCaretVisible(bool* _retval) override;
  NS_IMETHOD SetCaretVisibilityDuringSelection(bool aVisibility) override;
  NS_IMETHOD PhysicalMove(int16_t aDirection, int16_t aAmount,
                          bool aExtend) override;
  NS_IMETHOD CharacterMove(bool aForward, bool aExtend) override;
  NS_IMETHOD WordMove(bool aForward, bool aExtend) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD LineMove(bool aForward,
                                                  bool aExtend) override;
  NS_IMETHOD IntraLineMove(bool aForward, bool aExtend) override;
  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD PageMove(bool aForward, bool aExtend) override;
  NS_IMETHOD CompleteScroll(bool aForward) override;
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD CompleteMove(bool aForward,
                                             bool aExtend) override;
  NS_IMETHOD ScrollPage(bool aForward) override;
  NS_IMETHOD ScrollLine(bool aForward) override;
  NS_IMETHOD ScrollCharacter(bool aRight) override;
  NS_IMETHOD CheckVisibility(nsINode* node, int16_t startOffset,
                             int16_t EndOffset, bool* _retval) override;
  virtual nsresult CheckVisibilityContent(nsIContent* aNode,
                                          int16_t aStartOffset,
                                          int16_t aEndOffset,
                                          bool* aRetval) override;
  void SelectionWillTakeFocus() override;
  void SelectionWillLoseFocus() override;

 private:
  RefPtr<nsFrameSelection> mFrameSelection;
  nsIScrollableFrame* mScrollFrame;
  nsWeakPtr mPresShellWeak;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextInputSelectionController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextInputSelectionController)
NS_INTERFACE_TABLE_HEAD(TextInputSelectionController)
  NS_INTERFACE_TABLE(TextInputSelectionController, nsISelectionController,
                     nsISelectionDisplay, nsISupportsWeakReference)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(TextInputSelectionController)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WEAK(TextInputSelectionController, mFrameSelection)

TextInputSelectionController::TextInputSelectionController(
    PresShell* aPresShell, nsIContent* aLimiter)
    : mScrollFrame(nullptr) {
  if (aPresShell) {
    bool accessibleCaretEnabled =
        PresShell::AccessibleCaretEnabled(aLimiter->OwnerDoc()->GetDocShell());
    mFrameSelection =
        new nsFrameSelection(aPresShell, aLimiter, accessibleCaretEnabled);
    mPresShellWeak = do_GetWeakReference(aPresShell);
  }
}

void TextInputSelectionController::SetScrollableFrame(
    nsIScrollableFrame* aScrollableFrame) {
  mScrollFrame = aScrollableFrame;
  if (!mScrollFrame && mFrameSelection) {
    mFrameSelection->DisconnectFromPresShell();
    mFrameSelection = nullptr;
  }
}

Selection* TextInputSelectionController::GetSelection(
    SelectionType aSelectionType) {
  if (!mFrameSelection) {
    return nullptr;
  }

  return mFrameSelection->GetSelection(aSelectionType);
}

NS_IMETHODIMP
TextInputSelectionController::SetDisplaySelection(int16_t aToggle) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  mFrameSelection->SetDisplaySelection(aToggle);
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::GetDisplaySelection(int16_t* aToggle) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  *aToggle = mFrameSelection->GetDisplaySelection();
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::SetSelectionFlags(int16_t aToggle) {
  return NS_OK;  // stub this out. not used in input
}

NS_IMETHODIMP
TextInputSelectionController::GetSelectionFlags(int16_t* aOutEnable) {
  *aOutEnable = nsISelectionDisplay::DISPLAY_TEXT;
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::GetSelectionFromScript(
    RawSelectionType aRawSelectionType, Selection** aSelection) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }

  *aSelection =
      mFrameSelection->GetSelection(ToSelectionType(aRawSelectionType));

  // GetSelection() fails only when aRawSelectionType is invalid value.
  if (!(*aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF(*aSelection);
  return NS_OK;
}

Selection* TextInputSelectionController::GetSelection(
    RawSelectionType aRawSelectionType) {
  return GetSelection(ToSelectionType(aRawSelectionType));
}

NS_IMETHODIMP
TextInputSelectionController::ScrollSelectionIntoView(
    RawSelectionType aRawSelectionType, int16_t aRegion, int16_t aFlags) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->ScrollSelectionIntoView(
      ToSelectionType(aRawSelectionType), aRegion, aFlags);
}

NS_IMETHODIMP
TextInputSelectionController::RepaintSelection(
    RawSelectionType aRawSelectionType) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->RepaintSelection(ToSelectionType(aRawSelectionType));
}

nsresult TextInputSelectionController::RepaintSelection(
    nsPresContext* aPresContext, SelectionType aSelectionType) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->RepaintSelection(aSelectionType);
}

NS_IMETHODIMP
TextInputSelectionController::SetCaretEnabled(bool enabled) {
  if (!mPresShellWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  RefPtr<PresShell> presShell = do_QueryReferent(mPresShellWeak);
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  // tell the pres shell to enable the caret, rather than settings its
  // visibility directly. this way the presShell's idea of caret visibility is
  // maintained.
  presShell->SetCaretEnabled(enabled);

  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::SetCaretReadOnly(bool aReadOnly) {
  if (!mPresShellWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv;
  RefPtr<PresShell> presShell = do_QueryReferent(mPresShellWeak, &rv);
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    return NS_ERROR_FAILURE;
  }

  if (!mFrameSelection) {
    return NS_ERROR_FAILURE;
  }

  Selection* selection = mFrameSelection->GetSelection(SelectionType::eNormal);
  if (selection) {
    caret->SetCaretReadOnly(aReadOnly);
  }
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::GetCaretEnabled(bool* _retval) {
  return GetCaretVisible(_retval);
}

NS_IMETHODIMP
TextInputSelectionController::GetCaretVisible(bool* _retval) {
  if (!mPresShellWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv;
  RefPtr<PresShell> presShell = do_QueryReferent(mPresShellWeak, &rv);
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    return NS_ERROR_FAILURE;
  }
  *_retval = caret->IsVisible();
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::SetCaretVisibilityDuringSelection(
    bool aVisibility) {
  if (!mPresShellWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv;
  RefPtr<PresShell> presShell = do_QueryReferent(mPresShellWeak, &rv);
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    return NS_ERROR_FAILURE;
  }
  Selection* selection = mFrameSelection->GetSelection(SelectionType::eNormal);
  if (selection) {
    caret->SetVisibilityDuringSelection(aVisibility);
  }
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::PhysicalMove(int16_t aDirection, int16_t aAmount,
                                           bool aExtend) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->PhysicalMove(aDirection, aAmount, aExtend);
}

NS_IMETHODIMP
TextInputSelectionController::CharacterMove(bool aForward, bool aExtend) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->CharacterMove(aForward, aExtend);
}

NS_IMETHODIMP
TextInputSelectionController::WordMove(bool aForward, bool aExtend) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->WordMove(aForward, aExtend);
}

NS_IMETHODIMP
TextInputSelectionController::LineMove(bool aForward, bool aExtend) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  nsresult result = frameSelection->LineMove(aForward, aExtend);
  if (NS_FAILED(result)) {
    result = CompleteMove(aForward, aExtend);
  }
  return result;
}

NS_IMETHODIMP
TextInputSelectionController::IntraLineMove(bool aForward, bool aExtend) {
  if (!mFrameSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->IntraLineMove(aForward, aExtend);
}

NS_IMETHODIMP
TextInputSelectionController::PageMove(bool aForward, bool aExtend) {
  // expected behavior for PageMove is to scroll AND move the caret
  // and to remain relative position of the caret in view. see Bug 4302.
  if (mScrollFrame) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    nsIFrame* scrollFrame = do_QueryFrame(mScrollFrame);
    // We won't scroll parent scrollable element of mScrollFrame.  Therefore,
    // this may be handled when mScrollFrame is completely outside of the view.
    // In such case, user may be confused since they might have wanted to
    // scroll a parent scrollable element.  For making clearer which element
    // handles PageDown/PageUp, we should move selection into view even if
    // selection is not changed.
    return frameSelection->PageMove(aForward, aExtend, scrollFrame,
                                    nsFrameSelection::SelectionIntoView::Yes);
  }
  // Similarly, if there is no scrollable frame, we should move the editor
  // frame into the view for making it clearer which element handles
  // PageDown/PageUp.
  return ScrollSelectionIntoView(
      nsISelectionController::SELECTION_NORMAL,
      nsISelectionController::SELECTION_FOCUS_REGION,
      nsISelectionController::SCROLL_SYNCHRONOUS |
          nsISelectionController::SCROLL_FOR_CARET_MOVE);
}

NS_IMETHODIMP
TextInputSelectionController::CompleteScroll(bool aForward) {
  if (!mScrollFrame) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1), ScrollUnit::WHOLE,
                         ScrollMode::Instant);
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::CompleteMove(bool aForward, bool aExtend) {
  if (NS_WARN_IF(!mFrameSelection)) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;

  // grab the parent / root DIV for this text widget
  nsIContent* parentDIV = frameSelection->GetLimiter();
  if (!parentDIV) {
    return NS_ERROR_UNEXPECTED;
  }

  // make the caret be either at the very beginning (0) or the very end
  int32_t offset = 0;
  CaretAssociationHint hint = CARET_ASSOCIATE_BEFORE;
  if (aForward) {
    offset = parentDIV->GetChildCount();

    // Prevent the caret from being placed after the last
    // BR node in the content tree!

    if (offset > 0) {
      nsIContent* child = parentDIV->GetLastChild();

      if (child->IsHTMLElement(nsGkAtoms::br)) {
        --offset;
        hint = CARET_ASSOCIATE_AFTER;  // for Bug 106855
      }
    }
  }

  const RefPtr<nsIContent> pinnedParentDIV{parentDIV};
  const nsFrameSelection::FocusMode focusMode =
      aExtend ? nsFrameSelection::FocusMode::kExtendSelection
              : nsFrameSelection::FocusMode::kCollapseToNewPoint;
  frameSelection->HandleClick(pinnedParentDIV, offset, offset, focusMode, hint);

  // if we got this far, attempt to scroll no matter what the above result is
  return CompleteScroll(aForward);
}

NS_IMETHODIMP
TextInputSelectionController::ScrollPage(bool aForward) {
  if (!mScrollFrame) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1), ScrollUnit::PAGES,
                         ScrollMode::Smooth);
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::ScrollLine(bool aForward) {
  if (!mScrollFrame) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1), ScrollUnit::LINES,
                         ScrollMode::Smooth);
  return NS_OK;
}

NS_IMETHODIMP
TextInputSelectionController::ScrollCharacter(bool aRight) {
  if (!mScrollFrame) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mScrollFrame->ScrollBy(nsIntPoint(aRight ? 1 : -1, 0), ScrollUnit::LINES,
                         ScrollMode::Smooth);
  return NS_OK;
}

void TextInputSelectionController::SelectionWillTakeFocus() {
  if (mFrameSelection) {
    if (PresShell* shell = mFrameSelection->GetPresShell()) {
      shell->FrameSelectionWillTakeFocus(*mFrameSelection);
    }
  }
}

void TextInputSelectionController::SelectionWillLoseFocus() {
  if (mFrameSelection) {
    if (PresShell* shell = mFrameSelection->GetPresShell()) {
      shell->FrameSelectionWillLoseFocus(*mFrameSelection);
    }
  }
}

NS_IMETHODIMP
TextInputSelectionController::CheckVisibility(nsINode* node,
                                              int16_t startOffset,
                                              int16_t EndOffset,
                                              bool* _retval) {
  if (!mPresShellWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv;
  nsCOMPtr<nsISelectionController> presShell =
      do_QueryReferent(mPresShellWeak, &rv);
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }
  return presShell->CheckVisibility(node, startOffset, EndOffset, _retval);
}

nsresult TextInputSelectionController::CheckVisibilityContent(
    nsIContent* aNode, int16_t aStartOffset, int16_t aEndOffset,
    bool* aRetval) {
  if (!mPresShellWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsISelectionController> presShell = do_QueryReferent(mPresShellWeak);
  if (NS_WARN_IF(!presShell)) {
    return NS_ERROR_FAILURE;
  }
  return presShell->CheckVisibilityContent(aNode, aStartOffset, aEndOffset,
                                           aRetval);
}

/*****************************************************************************
 * mozilla::TextInputListener
 *****************************************************************************/

TextInputListener::TextInputListener(TextControlElement* aTxtCtrlElement)
    : mFrame(nullptr),
      mTxtCtrlElement(aTxtCtrlElement),
      mTextControlState(aTxtCtrlElement ? aTxtCtrlElement->GetTextControlState()
                                        : nullptr),
      mSelectionWasCollapsed(true),
      mHadUndoItems(false),
      mHadRedoItems(false),
      mSettingValue(false),
      mSetValueChanged(true),
      mListeningToSelectionChange(false) {}

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextInputListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextInputListener)

NS_INTERFACE_MAP_BEGIN(TextInputListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(TextInputListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(TextInputListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TextInputListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TextInputListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void TextInputListener::OnSelectionChange(Selection& aSelection,
                                          int16_t aReason) {
  if (!mListeningToSelectionChange) {
    return;
  }

  AutoWeakFrame weakFrame = mFrame;

  // Fire the select event
  // The specs don't exactly say when we should fire the select event.
  // IE: Whenever you add/remove a character to/from the selection. Also
  //     each time for select all. Also if you get to the end of the text
  //     field you will get new event for each keypress or a continuous
  //     stream of events if you use the mouse. IE will fire select event
  //     when the selection collapses to nothing if you are holding down
  //     the shift or mouse button.
  // Mozilla: If we have non-empty selection we will fire a new event for each
  //          keypress (or mouseup) if the selection changed. Mozilla will also
  //          create the event each time select all is called, even if
  //          everything was previously selected, because technically select all
  //          will first collapse and then extend. Mozilla will never create an
  //          event if the selection collapses to nothing.
  // FYI: If you want to skip dispatching eFormSelect event and if there are no
  //      event listeners, you can refer
  //      nsPIDOMWindow::HasFormSelectEventListeners(), but be careful about
  //      some C++ event handlers, e.g., HTMLTextAreaElement::PostHandleEvent().
  bool collapsed = aSelection.IsCollapsed();
  if (!collapsed && (aReason & (nsISelectionListener::MOUSEUP_REASON |
                                nsISelectionListener::KEYPRESS_REASON |
                                nsISelectionListener::SELECTALL_REASON))) {
    if (nsCOMPtr<nsIContent> content = mFrame->GetContent()) {
      if (nsCOMPtr<Document> doc = content->GetComposedDoc()) {
        if (RefPtr<PresShell> presShell = doc->GetPresShell()) {
          nsEventStatus status = nsEventStatus_eIgnore;
          WidgetEvent event(true, eFormSelect);

          presShell->HandleEventWithTarget(&event, mFrame, content, &status);
        }
      }
    }
  }

  // if the collapsed state did not change, don't fire notifications
  if (collapsed == mSelectionWasCollapsed) {
    return;
  }

  mSelectionWasCollapsed = collapsed;

  if (!weakFrame.IsAlive() || !mFrame ||
      !nsContentUtils::IsFocusedContent(mFrame->GetContent())) {
    return;
  }

  UpdateTextInputCommands(u"select"_ns, &aSelection, aReason);
}

MOZ_CAN_RUN_SCRIPT
static void DoCommandCallback(Command aCommand, void* aData) {
  nsTextControlFrame* frame = static_cast<nsTextControlFrame*>(aData);
  nsIContent* content = frame->GetContent();

  nsCOMPtr<nsIControllers> controllers;
  HTMLInputElement* input = HTMLInputElement::FromNode(content);
  if (input) {
    input->GetControllers(getter_AddRefs(controllers));
  } else {
    HTMLTextAreaElement* textArea = HTMLTextAreaElement::FromNode(content);

    if (textArea) {
      textArea->GetControllers(getter_AddRefs(controllers));
    }
  }

  if (!controllers) {
    NS_WARNING("Could not get controllers");
    return;
  }

  const char* commandStr = WidgetKeyboardEvent::GetCommandStr(aCommand);

  nsCOMPtr<nsIController> controller;
  controllers->GetControllerForCommand(commandStr, getter_AddRefs(controller));
  if (!controller) {
    return;
  }

  bool commandEnabled;
  if (NS_WARN_IF(NS_FAILED(
          controller->IsCommandEnabled(commandStr, &commandEnabled)))) {
    return;
  }
  if (commandEnabled) {
    controller->DoCommand(commandStr);
  }
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
TextInputListener::HandleEvent(Event* aEvent) {
  if (aEvent->DefaultPrevented()) {
    return NS_OK;
  }

  if (!aEvent->IsTrusted()) {
    return NS_OK;
  }

  RefPtr<KeyboardEvent> keyEvent = aEvent->AsKeyboardEvent();
  if (!keyEvent) {
    return NS_ERROR_UNEXPECTED;
  }

  WidgetKeyboardEvent* widgetKeyEvent =
      aEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (!widgetKeyEvent) {
    return NS_ERROR_UNEXPECTED;
  }

  {
    auto* input = HTMLInputElement::FromNode(mTxtCtrlElement);
    if (input && input->StepsInputValue(*widgetKeyEvent)) {
      // As an special case, don't handle key events that would step the value
      // of our <input type=number>.
      return NS_OK;
    }
  }

  auto ExecuteOurShortcutKeys = [&](TextControlElement& aTextControlElement)
                                    MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION -> bool {
    KeyEventHandler* keyHandlers = ShortcutKeys::GetHandlers(
        aTextControlElement.IsTextArea() ? HandlerType::eTextArea
                                         : HandlerType::eInput);

    RefPtr<nsAtom> eventTypeAtom =
        ShortcutKeys::ConvertEventToDOMEventType(widgetKeyEvent);
    for (KeyEventHandler* handler = keyHandlers; handler;
         handler = handler->GetNextHandler()) {
      if (!handler->EventTypeEquals(eventTypeAtom)) {
        continue;
      }

      if (!handler->KeyEventMatched(keyEvent, 0, IgnoreModifierState())) {
        continue;
      }

      // XXX Do we execute only one handler even if the handler neither stops
      //     propagation nor prevents default of the event?
      nsresult rv = handler->ExecuteHandler(&aTextControlElement, aEvent);
      if (NS_SUCCEEDED(rv)) {
        return true;
      }
    }
    return false;
  };

  auto ExecuteNativeKeyBindings =
      [&](TextControlElement& aTextControlElement)
          MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION -> bool {
    if (widgetKeyEvent->mMessage != eKeyPress) {
      return false;
    }

    NativeKeyBindingsType nativeKeyBindingsType =
        aTextControlElement.IsTextArea()
            ? NativeKeyBindingsType::MultiLineEditor
            : NativeKeyBindingsType::SingleLineEditor;

    nsIWidget* widget = widgetKeyEvent->mWidget;
    // If the event is created by chrome script, the widget is nullptr.
    if (MOZ_UNLIKELY(!widget)) {
      widget = mFrame->GetNearestWidget();
      if (MOZ_UNLIKELY(NS_WARN_IF(!widget))) {
        return false;
      }
    }

    // WidgetKeyboardEvent::ExecuteEditCommands() requires non-nullptr mWidget.
    // If the event is created by chrome script, it is nullptr but we need to
    // execute native key bindings.  Therefore, we need to set widget to
    // WidgetEvent::mWidget temporarily.
    AutoRestore<nsCOMPtr<nsIWidget>> saveWidget(widgetKeyEvent->mWidget);
    widgetKeyEvent->mWidget = widget;
    if (widgetKeyEvent->ExecuteEditCommands(nativeKeyBindingsType,
                                            DoCommandCallback, mFrame)) {
      aEvent->PreventDefault();
      return true;
    }
    return false;
  };

  OwningNonNull<TextControlElement> textControlElement(*mTxtCtrlElement);
  if (StaticPrefs::
          ui_key_textcontrol_prefer_native_key_bindings_over_builtin_shortcut_key_definitions()) {
    if (!ExecuteNativeKeyBindings(textControlElement)) {
      ExecuteOurShortcutKeys(textControlElement);
    }
  } else {
    if (!ExecuteOurShortcutKeys(textControlElement)) {
      ExecuteNativeKeyBindings(textControlElement);
    }
  }
  return NS_OK;
}

nsresult TextInputListener::OnEditActionHandled(TextEditor& aTextEditor) {
  if (mFrame) {
    // XXX Do we still need this or can we just remove the mFrame and
    // frame.IsAlive() conditions below?
    AutoWeakFrame weakFrame = mFrame;

    // Update the undo / redo menus
    //
    size_t numUndoItems = aTextEditor.NumberOfUndoItems();
    size_t numRedoItems = aTextEditor.NumberOfRedoItems();
    if ((numUndoItems && !mHadUndoItems) || (!numUndoItems && mHadUndoItems) ||
        (numRedoItems && !mHadRedoItems) || (!numRedoItems && mHadRedoItems)) {
      // Modify the menu if undo or redo items are different
      UpdateTextInputCommands(u"undo"_ns);

      mHadUndoItems = numUndoItems != 0;
      mHadRedoItems = numRedoItems != 0;
    }

    if (weakFrame.IsAlive()) {
      HandleValueChanged();
    }
  }

  return mTextControlState ? mTextControlState->OnEditActionHandled() : NS_OK;
}

void TextInputListener::HandleValueChanged() {
  // Make sure we know we were changed (do NOT set this to false if there are
  // no undo items; JS could change the value and we'd still need to save it)
  if (mSetValueChanged) {
    mTxtCtrlElement->SetValueChanged(true);
  }

  if (!mSettingValue) {
    // NOTE(emilio): execCommand might get here even though it might not be a
    // "proper" user-interactive change. Might be worth reconsidering which
    // ValueChangeKind are we passing down.
    mTxtCtrlElement->OnValueChanged(ValueChangeKind::UserInteraction);
    if (mTextControlState) {
      mTextControlState->ClearLastInteractiveValue();
    }
  }
}

nsresult TextInputListener::UpdateTextInputCommands(
    const nsAString& aCommandsToUpdate, Selection* aSelection,
    int16_t aReason) {
  nsIContent* content = mFrame->GetContent();
  if (NS_WARN_IF(!content)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Document> doc = content->GetComposedDoc();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }
  nsPIDOMWindowOuter* domWindow = doc->GetWindow();
  if (NS_WARN_IF(!domWindow)) {
    return NS_ERROR_FAILURE;
  }
  domWindow->UpdateCommands(aCommandsToUpdate, aSelection, aReason);
  return NS_OK;
}

/*****************************************************************************
 * mozilla::AutoTextControlHandlingState
 *
 * This class is temporarily created in the stack and can manage nested
 * handling state of TextControlState.  While this instance exists, lifetime of
 * TextControlState which created the instance is guaranteed.  In other words,
 * you can use this class as "kungFuDeathGrip" for TextControlState.
 *****************************************************************************/

enum class TextControlAction {
  CommitComposition,
  Destructor,
  PrepareEditor,
  SetRangeText,
  SetSelectionRange,
  SetValue,
  UnbindFromFrame,
  Unlink,
};

class MOZ_STACK_CLASS AutoTextControlHandlingState {
 public:
  AutoTextControlHandlingState() = delete;
  explicit AutoTextControlHandlingState(const AutoTextControlHandlingState&) =
      delete;
  AutoTextControlHandlingState(AutoTextControlHandlingState&&) = delete;
  void operator=(AutoTextControlHandlingState&) = delete;
  void operator=(const AutoTextControlHandlingState&) = delete;

  /**
   * Generic constructor.  If TextControlAction does not require additional
   * data, must use this constructor.
   */
  MOZ_CAN_RUN_SCRIPT AutoTextControlHandlingState(
      TextControlState& aTextControlState, TextControlAction aTextControlAction)
      : mParent(aTextControlState.mHandlingState),
        mTextControlState(aTextControlState),
        mTextCtrlElement(aTextControlState.mTextCtrlElement),
        mTextInputListener(aTextControlState.mTextListener),
        mTextControlAction(aTextControlAction) {
    MOZ_ASSERT(aTextControlAction != TextControlAction::SetValue,
               "Use specific constructor");
    mTextControlState.mHandlingState = this;
    if (Is(TextControlAction::CommitComposition)) {
      MOZ_ASSERT(mParent);
      MOZ_ASSERT(mParent->Is(TextControlAction::SetValue));
      // If we're trying to commit composition before handling SetValue,
      // the parent old values will be outdated so that we need to clear
      // them.
      mParent->InvalidateOldValue();
    }
  }

  /**
   * TextControlAction::SetValue specific constructor.  Current setting value
   * must be specified and the creator should check whether we succeeded to
   * allocate memory for line breaker conversion.
   */
  MOZ_CAN_RUN_SCRIPT AutoTextControlHandlingState(
      TextControlState& aTextControlState, TextControlAction aTextControlAction,
      const nsAString& aSettingValue, const nsAString* aOldValue,
      const ValueSetterOptions& aOptions, ErrorResult& aRv)
      : mParent(aTextControlState.mHandlingState),
        mTextControlState(aTextControlState),
        mTextCtrlElement(aTextControlState.mTextCtrlElement),
        mTextInputListener(aTextControlState.mTextListener),
        mSettingValue(aSettingValue),
        mOldValue(aOldValue),
        mValueSetterOptions(aOptions),
        mTextControlAction(aTextControlAction) {
    MOZ_ASSERT(aTextControlAction == TextControlAction::SetValue,
               "Use generic constructor");
    mTextControlState.mHandlingState = this;
    if (!nsContentUtils::PlatformToDOMLineBreaks(mSettingValue, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    // Update all setting value's new value because older value shouldn't
    // overwrite newer value.
    if (mParent) {
      // If SetValue is nested, parents cannot trust their old value anymore.
      // So, we need to clear them.
      mParent->UpdateSettingValueAndInvalidateOldValue(mSettingValue);
    }
  }

  MOZ_CAN_RUN_SCRIPT ~AutoTextControlHandlingState() {
    mTextControlState.mHandlingState = mParent;
    if (!mParent && mTextControlStateDestroyed) {
      mTextControlState.DeleteOrCacheForReuse();
    }
    if (!mTextControlStateDestroyed && mPreareEditorLater) {
      MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
      mTextControlState.PrepareEditor();
    }
  }

  void OnDestroyTextControlState() {
    if (IsHandling(TextControlAction::Destructor)) {
      // Do nothing since mTextContrlState.DeleteOrCacheForReuse() has
      // already been called.
      return;
    }
    mTextControlStateDestroyed = true;
    if (mParent) {
      mParent->OnDestroyTextControlState();
    }
  }

  void PrepareEditorLater() {
    MOZ_ASSERT(IsHandling(TextControlAction::SetValue));
    MOZ_ASSERT(!IsHandling(TextControlAction::PrepareEditor));
    // Look for the top most SetValue.
    AutoTextControlHandlingState* settingValue = nullptr;
    for (AutoTextControlHandlingState* handlingSomething = this;
         handlingSomething; handlingSomething = handlingSomething->mParent) {
      if (handlingSomething->Is(TextControlAction::SetValue)) {
        settingValue = handlingSomething;
      }
    }
    settingValue->mPreareEditorLater = true;
  }

  /**
   * WillSetValueWithTextEditor() is called when TextControlState sets
   * value with its mTextEditor.
   */
  void WillSetValueWithTextEditor() {
    MOZ_ASSERT(Is(TextControlAction::SetValue));
    MOZ_ASSERT(mTextControlState.mBoundFrame);
    mTextControlFrame = mTextControlState.mBoundFrame;
    // If we'reemulating user input, we don't need to manage mTextInputListener
    // by ourselves since everything should be handled by TextEditor as normal
    // user input.
    if (mValueSetterOptions.contains(ValueSetterOption::BySetUserInputAPI)) {
      return;
    }
    // Otherwise, if we're setting the value programatically, we need to manage
    // mTextInputListener by ourselves since TextEditor users special path
    // for the performance.
    mTextInputListener->SettingValue(true);
    mTextInputListener->SetValueChanged(
        mValueSetterOptions.contains(ValueSetterOption::SetValueChanged));
    mEditActionHandled = false;
    // Even if falling back to `TextControlState::SetValueWithoutTextEditor()`
    // due to editor destruction, it shouldn't dispatch "beforeinput" event
    // anymore.  Therefore, we should mark that we've already dispatched
    // "beforeinput" event.
    WillDispatchBeforeInputEvent();
  }

  /**
   * WillDispatchBeforeInputEvent() is called immediately before dispatching
   * "beforeinput" event in `TextControlState`.
   */
  void WillDispatchBeforeInputEvent() {
    mBeforeInputEventHasBeenDispatched = true;
  }

  /**
   * OnEditActionHandled() is called when the TextEditor handles something
   * and immediately before dispatching "input" event.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult OnEditActionHandled() {
    MOZ_ASSERT(!mEditActionHandled);
    mEditActionHandled = true;
    if (!Is(TextControlAction::SetValue)) {
      return NS_OK;
    }
    if (!mValueSetterOptions.contains(ValueSetterOption::BySetUserInputAPI)) {
      mTextInputListener->SetValueChanged(true);
      mTextInputListener->SettingValue(
          mParent && mParent->IsHandling(TextControlAction::SetValue));
    }
    if (!IsOriginalTextControlFrameAlive()) {
      return SetValueWithoutTextEditorAgain() ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    // The new value never includes line breaks caused by hard-wrap.
    // So, mCachedValue can always cache the new value.
    nsITextControlFrame* textControlFrame =
        do_QueryFrame(mTextControlFrame.GetFrame());
    return static_cast<nsTextControlFrame*>(textControlFrame)
                   ->CacheValue(mSettingValue, fallible)
               ? NS_OK
               : NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * SetValueWithoutTextEditorAgain() should be called if the frame for
   * mTextControlState was destroyed during setting value.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT bool SetValueWithoutTextEditorAgain() {
    MOZ_ASSERT(!IsOriginalTextControlFrameAlive());
    // If the frame was destroyed because of a flush somewhere inside
    // TextEditor, mBoundFrame here will be nullptr.  But it's also
    // possible for the frame to go away because of another reason (such
    // as deleting the existing selection -- see bug 574558), in which
    // case we don't need to reset the value here.
    if (mTextControlState.mBoundFrame) {
      return true;
    }
    // XXX It's odd to drop flags except
    //     ValueSetterOption::SetValueChanged.
    //     Probably, this intended to drop ValueSetterOption::BySetUserInputAPI
    //     and ValueSetterOption::ByContentAPI, but other flags are added later.
    ErrorResult error;
    AutoTextControlHandlingState handlingSetValueWithoutEditor(
        mTextControlState, TextControlAction::SetValue, mSettingValue,
        mOldValue, mValueSetterOptions & ValueSetterOption::SetValueChanged,
        error);
    if (error.Failed()) {
      MOZ_ASSERT(error.ErrorCodeIs(NS_ERROR_OUT_OF_MEMORY));
      error.SuppressException();
      return false;
    }
    return mTextControlState.SetValueWithoutTextEditor(
        handlingSetValueWithoutEditor);
  }

  bool IsTextControlStateDestroyed() const {
    return mTextControlStateDestroyed;
  }
  bool IsOriginalTextControlFrameAlive() const {
    return const_cast<AutoTextControlHandlingState*>(this)
        ->mTextControlFrame.IsAlive();
  }
  bool HasEditActionHandled() const { return mEditActionHandled; }
  bool HasBeforeInputEventDispatched() const {
    return mBeforeInputEventHasBeenDispatched;
  }
  bool Is(TextControlAction aTextControlAction) const {
    return mTextControlAction == aTextControlAction;
  }
  bool IsHandling(TextControlAction aTextControlAction) const {
    if (mTextControlAction == aTextControlAction) {
      return true;
    }
    return mParent ? mParent->IsHandling(aTextControlAction) : false;
  }
  TextControlElement* GetTextControlElement() const { return mTextCtrlElement; }
  TextInputListener* GetTextInputListener() const { return mTextInputListener; }
  const ValueSetterOptions& ValueSetterOptionsRef() const {
    MOZ_ASSERT(Is(TextControlAction::SetValue));
    return mValueSetterOptions;
  }
  const nsAString* GetOldValue() const {
    MOZ_ASSERT(Is(TextControlAction::SetValue));
    return mOldValue;
  }
  const nsString& GetSettingValue() const {
    MOZ_ASSERT(IsHandling(TextControlAction::SetValue));
    if (mTextControlAction == TextControlAction::SetValue) {
      return mSettingValue;
    }
    return mParent->GetSettingValue();
  }

 private:
  void UpdateSettingValueAndInvalidateOldValue(const nsString& aSettingValue) {
    if (mTextControlAction == TextControlAction::SetValue) {
      mSettingValue = aSettingValue;
    }
    mOldValue = nullptr;
    if (mParent) {
      mParent->UpdateSettingValueAndInvalidateOldValue(aSettingValue);
    }
  }
  void InvalidateOldValue() {
    mOldValue = nullptr;
    if (mParent) {
      mParent->InvalidateOldValue();
    }
  }

  AutoTextControlHandlingState* const mParent;
  TextControlState& mTextControlState;
  // mTextControlFrame should be set immediately before calling methods
  // which may destroy the frame.  Then, you can check whether the frame
  // was destroyed/replaced.
  AutoWeakFrame mTextControlFrame;
  // mTextCtrlElement grabs TextControlState::mTextCtrlElement since
  // if the text control element releases mTextControlState, only this
  // can guarantee the instance of the text control element.
  RefPtr<TextControlElement> const mTextCtrlElement;
  // mTextInputListener grabs TextControlState::mTextListener because if
  // TextControlState is unbind from the frame, it's released.
  RefPtr<TextInputListener> const mTextInputListener;
  nsString mSettingValue;
  const nsAString* mOldValue = nullptr;
  ValueSetterOptions mValueSetterOptions;
  TextControlAction const mTextControlAction;
  bool mTextControlStateDestroyed = false;
  bool mEditActionHandled = false;
  bool mPreareEditorLater = false;
  bool mBeforeInputEventHasBeenDispatched = false;
};

/*****************************************************************************
 * mozilla::TextControlState
 *****************************************************************************/

/**
 * For avoiding allocation cost of the instance, we should reuse instances
 * as far as possible.
 *
 * FYI: `25` is just a magic number considered without enough investigation,
 *      but at least, this value must not make damage for footprint.
 *      Feel free to change it if you find better number.
 */
static constexpr size_t kMaxCountOfCacheToReuse = 25;
static AutoTArray<void*, kMaxCountOfCacheToReuse>* sReleasedInstances = nullptr;
static bool sHasShutDown = false;

TextControlState::TextControlState(TextControlElement* aOwningElement)
    : mTextCtrlElement(aOwningElement),
      mEverInited(false),
      mEditorInitialized(false),
      mValueTransferInProgress(false),
      mSelectionCached(true)
// When adding more member variable initializations here, add the same
// also to ::Construct.
{
  MOZ_COUNT_CTOR(TextControlState);
  static_assert(sizeof(*this) <= 128,
                "Please keep small TextControlState as far as possible");
}

TextControlState* TextControlState::Construct(
    TextControlElement* aOwningElement) {
  void* mem;
  if (sReleasedInstances && !sReleasedInstances->IsEmpty()) {
    mem = sReleasedInstances->PopLastElement();
  } else {
    mem = moz_xmalloc(sizeof(TextControlState));
  }

  return new (mem) TextControlState(aOwningElement);
}

TextControlState::~TextControlState() {
  MOZ_ASSERT(!mHandlingState);
  MOZ_COUNT_DTOR(TextControlState);
  AutoTextControlHandlingState handlingDesctructor(
      *this, TextControlAction::Destructor);
  Clear();
}

void TextControlState::Shutdown() {
  sHasShutDown = true;
  if (sReleasedInstances) {
    for (void* mem : *sReleasedInstances) {
      free(mem);
    }
    delete sReleasedInstances;
  }
}

void TextControlState::Destroy() {
  // If we're handling something, we should be deleted later.
  if (mHandlingState) {
    mHandlingState->OnDestroyTextControlState();
    return;
  }
  DeleteOrCacheForReuse();
  // Note that this instance may have already been deleted here.  Don't touch
  // any members.
}

void TextControlState::DeleteOrCacheForReuse() {
  MOZ_ASSERT(!IsBusy());

  void* mem = this;
  this->~TextControlState();

  // If we can cache this instance, we should do it instead of deleting it.
  if (!sHasShutDown && (!sReleasedInstances || sReleasedInstances->Length() <
                                                   kMaxCountOfCacheToReuse)) {
    // Put this instance to the cache.  Note that now, the array may be full,
    // but it's not problem to cache more instances than kMaxCountOfCacheToReuse
    // because it just requires reallocation cost of the array buffer.
    if (!sReleasedInstances) {
      sReleasedInstances = new AutoTArray<void*, kMaxCountOfCacheToReuse>;
    }
    sReleasedInstances->AppendElement(mem);
  } else {
    free(mem);
  }
}

nsresult TextControlState::OnEditActionHandled() {
  return mHandlingState ? mHandlingState->OnEditActionHandled() : NS_OK;
}

Element* TextControlState::GetRootNode() {
  return mBoundFrame ? mBoundFrame->GetRootNode() : nullptr;
}

Element* TextControlState::GetPreviewNode() {
  return mBoundFrame ? mBoundFrame->GetPreviewNode() : nullptr;
}

void TextControlState::Clear() {
  MOZ_ASSERT(mHandlingState);
  MOZ_ASSERT(mHandlingState->Is(TextControlAction::Destructor) ||
             mHandlingState->Is(TextControlAction::Unlink));
  if (mTextEditor) {
    mTextEditor->SetTextInputListener(nullptr);
  }

  if (mBoundFrame) {
    // Oops, we still have a frame!
    // This should happen when the type of a text input control is being changed
    // to something which is not a text control.  In this case, we should
    // pretend that a frame is being destroyed, and clean up after ourselves
    // properly.
    UnbindFromFrame(mBoundFrame);
    mTextEditor = nullptr;
  } else {
    // If we have a bound frame around, UnbindFromFrame will call DestroyEditor
    // for us.
    DestroyEditor();
  }
  mTextListener = nullptr;
}

void TextControlState::Unlink() {
  AutoTextControlHandlingState handlingUnlink(*this, TextControlAction::Unlink);
  UnlinkInternal();
}

void TextControlState::UnlinkInternal() {
  MOZ_ASSERT(mHandlingState);
  MOZ_ASSERT(mHandlingState->Is(TextControlAction::Unlink));
  TextControlState* tmp = this;
  tmp->Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelCon)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTextEditor)
}

void TextControlState::Traverse(nsCycleCollectionTraversalCallback& cb) {
  TextControlState* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelCon)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTextEditor)
}

nsFrameSelection* TextControlState::GetConstFrameSelection() {
  return mSelCon ? mSelCon->GetConstFrameSelection() : nullptr;
}

TextEditor* TextControlState::GetTextEditor() {
  // Note that if the instance is destroyed in PrepareEditor(), it returns
  // NS_ERROR_NOT_INITIALIZED so that we don't need to create kungFuDeathGrip
  // in this hot path.
  if (!mTextEditor && NS_WARN_IF(NS_FAILED(PrepareEditor()))) {
    return nullptr;
  }
  return mTextEditor;
}

TextEditor* TextControlState::GetTextEditorWithoutCreation() {
  return mTextEditor;
}

nsISelectionController* TextControlState::GetSelectionController() const {
  return mSelCon;
}

// Helper class, used below in BindToFrame().
class PrepareEditorEvent : public Runnable {
 public:
  PrepareEditorEvent(TextControlState& aState, nsIContent* aOwnerContent,
                     const nsAString& aCurrentValue)
      : Runnable("PrepareEditorEvent"),
        mState(&aState),
        mOwnerContent(aOwnerContent),
        mCurrentValue(aCurrentValue) {
    aState.mValueTransferInProgress = true;
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    if (NS_WARN_IF(!mState)) {
      return NS_ERROR_NULL_POINTER;
    }

    // Transfer the saved value to the editor if we have one
    const nsAString* value = nullptr;
    if (!mCurrentValue.IsEmpty()) {
      value = &mCurrentValue;
    }

    nsAutoScriptBlocker scriptBlocker;

    mState->PrepareEditor(value);

    mState->mValueTransferInProgress = false;

    return NS_OK;
  }

 private:
  WeakPtr<TextControlState> mState;
  nsCOMPtr<nsIContent> mOwnerContent;  // strong reference
  nsAutoString mCurrentValue;
};

nsresult TextControlState::BindToFrame(nsTextControlFrame* aFrame) {
  MOZ_ASSERT(
      !nsContentUtils::IsSafeToRunScript(),
      "TextControlState::BindToFrame() has to be called with script blocker");
  NS_ASSERTION(aFrame, "The frame to bind to should be valid");
  if (!aFrame) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ASSERTION(!mBoundFrame, "Cannot bind twice, need to unbind first");
  if (mBoundFrame) {
    return NS_ERROR_FAILURE;
  }

  // If we'll need to transfer our current value to the editor, save it before
  // binding to the frame.
  nsAutoString currentValue;
  if (mTextEditor) {
    GetValue(currentValue, true);
  }

  mBoundFrame = aFrame;

  Element* rootNode = aFrame->GetRootNode();
  MOZ_ASSERT(rootNode);

  PresShell* presShell = aFrame->PresContext()->GetPresShell();
  MOZ_ASSERT(presShell);

  // Create a SelectionController
  mSelCon = new TextInputSelectionController(presShell, rootNode);
  MOZ_ASSERT(!mTextListener, "Should not overwrite the object");
  mTextListener = new TextInputListener(mTextCtrlElement);

  mTextListener->SetFrame(mBoundFrame);

  // Editor will override this as needed from InitializeSelection.
  mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);

  // Get the caret and make it a selection listener.
  // FYI: It's safe to use raw pointer for calling
  //      Selection::AddSelectionListner() because it only appends the listener
  //      to its internal array.
  Selection* selection = mSelCon->GetSelection(SelectionType::eNormal);
  if (selection) {
    RefPtr<nsCaret> caret = presShell->GetCaret();
    if (caret) {
      selection->AddSelectionListener(caret);
    }
    mTextListener->StartToListenToSelectionChange();
  }

  // If an editor exists from before, prepare it for usage
  if (mTextEditor) {
    if (NS_WARN_IF(!mTextCtrlElement)) {
      return NS_ERROR_FAILURE;
    }

    // Set the correct direction on the newly created root node
    if (mTextEditor->IsRightToLeft()) {
      rootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, u"rtl"_ns, false);
    } else if (mTextEditor->IsLeftToRight()) {
      rootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, u"ltr"_ns, false);
    } else {
      // otherwise, inherit the content node's direction
    }

    nsContentUtils::AddScriptRunner(
        new PrepareEditorEvent(*this, mTextCtrlElement, currentValue));
  }

  return NS_OK;
}

struct MOZ_STACK_CLASS PreDestroyer {
  void Init(TextEditor* aTextEditor) { mTextEditor = aTextEditor; }
  ~PreDestroyer() {
    if (mTextEditor) {
      // In this case, we don't need to restore the unmasked range of password
      // editor.
      UniquePtr<PasswordMaskData> passwordMaskData = mTextEditor->PreDestroy();
    }
  }
  void Swap(RefPtr<TextEditor>& aTextEditor) {
    return mTextEditor.swap(aTextEditor);
  }

 private:
  RefPtr<TextEditor> mTextEditor;
};

nsresult TextControlState::PrepareEditor(const nsAString* aValue) {
  if (!mBoundFrame) {
    // Cannot create an editor without a bound frame.
    // Don't return a failure code, because js callers can't handle that.
    return NS_OK;
  }

  if (mEditorInitialized) {
    // Do not initialize the editor multiple times.
    return NS_OK;
  }

  AutoHideSelectionChanges hideSelectionChanges(GetConstFrameSelection());

  if (mHandlingState) {
    // Don't attempt to initialize recursively!
    if (mHandlingState->IsHandling(TextControlAction::PrepareEditor)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    // Reschedule creating editor later if we're setting value.
    if (mHandlingState->IsHandling(TextControlAction::SetValue)) {
      mHandlingState->PrepareEditorLater();
      return NS_ERROR_NOT_INITIALIZED;
    }
  }

  MOZ_ASSERT(mTextCtrlElement);

  AutoTextControlHandlingState preparingEditor(
      *this, TextControlAction::PrepareEditor);

  // Note that we don't check mTextEditor here, because we might already have
  // one around, in which case we don't create a new one, and we'll just tie
  // the required machinery to it.

  nsPresContext* presContext = mBoundFrame->PresContext();
  PresShell* presShell = presContext->GetPresShell();

  // Setup the editor flags
  uint32_t editorFlags = nsIEditor::eEditorPlaintextMask;
  if (IsSingleLineTextControl()) {
    editorFlags |= nsIEditor::eEditorSingleLineMask;
  }
  if (IsPasswordTextControl()) {
    editorFlags |= nsIEditor::eEditorPasswordMask;
  }

  // Spell check is diabled at creation time. It is enabled once
  // the editor comes into focus.
  editorFlags |= nsIEditor::eEditorSkipSpellCheck;

  bool shouldInitializeEditor = false;
  RefPtr<TextEditor> newTextEditor;  // the editor that we might create
  PreDestroyer preDestroyer;
  if (!mTextEditor) {
    shouldInitializeEditor = true;

    // Create an editor
    newTextEditor = new TextEditor();
    preDestroyer.Init(newTextEditor);

    // Make sure we clear out the non-breaking space before we initialize the
    // editor
    nsresult rv = mBoundFrame->UpdateValueDisplay(true, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsTextControlFrame::UpdateValueDisplay() failed");
      return rv;
    }
  } else {
    if (aValue || !mEditorInitialized) {
      // Set the correct value in the root node
      nsresult rv =
          mBoundFrame->UpdateValueDisplay(true, !mEditorInitialized, aValue);
      if (NS_FAILED(rv)) {
        NS_WARNING("nsTextControlFrame::UpdateValueDisplay() failed");
        return rv;
      }
    }

    newTextEditor = mTextEditor;  // just pretend that we have a new editor!

    // Don't lose application flags in the process.
    if (newTextEditor->IsMailEditor()) {
      editorFlags |= nsIEditor::eEditorMailMask;
    }
  }

  // Get the current value of the textfield from the content.
  // Note that if we've created a new editor, mTextEditor is null at this stage,
  // so we will get the real value from the content.
  nsAutoString defaultValue;
  if (aValue) {
    defaultValue = *aValue;
  } else {
    GetValue(defaultValue, true);
  }

  if (!mEditorInitialized) {
    // Now initialize the editor.
    //
    // NOTE: Conversion of '\n' to <BR> happens inside the
    //       editor's Init() call.

    // Get the DOM document
    nsCOMPtr<Document> doc = presShell->GetDocument();
    if (NS_WARN_IF(!doc)) {
      return NS_ERROR_FAILURE;
    }

    // What follows is a bit of a hack.  The editor uses the public DOM APIs
    // for its content manipulations, and it causes it to fail some security
    // checks deep inside when initializing. So we explictly make it clear that
    // we're native code.
    // Note that any script that's directly trying to access our value
    // has to be going through some scriptable object to do that and that
    // already does the relevant security checks.
    AutoNoJSAPI nojsapi;

    RefPtr<Element> anonymousDivElement = GetRootNode();
    if (NS_WARN_IF(!anonymousDivElement) || NS_WARN_IF(!mSelCon)) {
      return NS_ERROR_FAILURE;
    }
    OwningNonNull<TextInputSelectionController> selectionController(*mSelCon);
    UniquePtr<PasswordMaskData> passwordMaskData;
    if (editorFlags & nsIEditor::eEditorPasswordMask) {
      if (mPasswordMaskData) {
        passwordMaskData = std::move(mPasswordMaskData);
      } else {
        passwordMaskData = MakeUnique<PasswordMaskData>();
      }
    } else {
      mPasswordMaskData = nullptr;
    }
    nsresult rv =
        newTextEditor->Init(*doc, *anonymousDivElement, selectionController,
                            editorFlags, std::move(passwordMaskData));
    if (NS_FAILED(rv)) {
      NS_WARNING("TextEditor::Init() failed");
      return rv;
    }
  }

  // Initialize the controller for the editor

  nsresult rv = NS_OK;
  if (!SuppressEventHandlers(presContext)) {
    nsCOMPtr<nsIControllers> controllers;
    if (HTMLInputElement* inputElement =
            HTMLInputElement::FromNodeOrNull(mTextCtrlElement)) {
      nsresult rv = inputElement->GetControllers(getter_AddRefs(controllers));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      HTMLTextAreaElement* textAreaElement =
          HTMLTextAreaElement::FromNodeOrNull(mTextCtrlElement);
      if (!textAreaElement) {
        return NS_ERROR_FAILURE;
      }

      nsresult rv =
          textAreaElement->GetControllers(getter_AddRefs(controllers));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (controllers) {
      // XXX Oddly, nsresult value is overwritten in the following loop, and
      //     only the last result or `found` decides the value.
      uint32_t numControllers;
      bool found = false;
      rv = controllers->GetControllerCount(&numControllers);
      for (uint32_t i = 0; i < numControllers; i++) {
        nsCOMPtr<nsIController> controller;
        rv = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(rv) && controller) {
          nsCOMPtr<nsIControllerContext> editController =
              do_QueryInterface(controller);
          if (editController) {
            editController->SetCommandContext(
                static_cast<nsIEditor*>(newTextEditor));
            found = true;
          }
        }
      }
      if (!found) {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  // Initialize the plaintext editor
  if (shouldInitializeEditor) {
    const int32_t wrapCols = GetWrapCols();
    MOZ_ASSERT(wrapCols >= 0);
    newTextEditor->SetWrapColumn(wrapCols);
  }

  // Set max text field length
  newTextEditor->SetMaxTextLength(mTextCtrlElement->UsedMaxLength());

  editorFlags = newTextEditor->Flags();

  // Check if the readonly attribute is set.
  //
  // TODO: Should probably call IsDisabled(), as it is cheaper.
  if (mTextCtrlElement->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) ||
      mTextCtrlElement->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    editorFlags |= nsIEditor::eEditorReadonlyMask;
  }

  SetEditorFlagsIfNecessary(*newTextEditor, editorFlags);

  if (shouldInitializeEditor) {
    // Hold on to the newly created editor
    preDestroyer.Swap(mTextEditor);
  }

  // If we have a default value, insert it under the div we created
  // above, but be sure to use the editor so that '*' characters get
  // displayed for password fields, etc. SetValue() will call the
  // editor for us.

  if (!defaultValue.IsEmpty()) {
    // XXX rv may store error code which indicates there is no controller.
    //     However, we overwrite it only in this case.
    rv = SetEditorFlagsIfNecessary(*newTextEditor, editorFlags);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Now call SetValue() which will make the necessary editor calls to set
    // the default value.  Make sure to turn off undo before setting the default
    // value, and turn it back on afterwards. This will make sure we can't undo
    // past the default value.
    // So, we use ValueSetterOption::ByInternalAPI only that it will turn off
    // undo.

    if (NS_WARN_IF(!SetValue(defaultValue, ValueSetterOption::ByInternalAPI))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Now restore the original editor flags.
    rv = SetEditorFlagsIfNecessary(*newTextEditor, editorFlags);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (IsPasswordTextControl()) {
    // Disable undo for <input type="password">.  Note that we want to do this
    // at the very end of InitEditor(), so the calls to EnableUndoRedo() when
    // setting the default value don't screw us up.  Since changing the
    // control type does a reframe, we don't have to worry about dynamic type
    // changes here.
    DebugOnly<bool> disabledUndoRedo = newTextEditor->DisableUndoRedo();
    NS_WARNING_ASSERTION(disabledUndoRedo,
                         "Failed to disable undo/redo transaction");
  } else {
    DebugOnly<bool> enabledUndoRedo =
        newTextEditor->EnableUndoRedo(TextControlElement::DEFAULT_UNDO_CAP);
    NS_WARNING_ASSERTION(enabledUndoRedo,
                         "Failed to enable undo/redo transaction");
  }

  if (!mEditorInitialized) {
    newTextEditor->PostCreate();
    mEverInited = true;
    mEditorInitialized = true;
  }

  if (mTextListener) {
    newTextEditor->SetTextInputListener(mTextListener);
  }

  // Restore our selection after being bound to a new frame
  if (mSelectionCached) {
    if (mRestoringSelection) {  // paranoia
      mRestoringSelection->Revoke();
    }
    mRestoringSelection = new RestoreSelectionState(this, mBoundFrame);
    if (mRestoringSelection) {
      nsContentUtils::AddScriptRunner(mRestoringSelection);
    }
  }

  // The selection cache is no longer going to be valid.
  //
  // XXXbz Shouldn't we do this at the point when we're actually about to
  // restore the properties or something?  As things stand, if UnbindFromFrame
  // happens before our RestoreSelectionState runs, it looks like we'll lose our
  // selection info, because we will think we don't have it cached and try to
  // read it from the selection controller, which will not have it yet.
  mSelectionCached = false;

  return preparingEditor.IsTextControlStateDestroyed()
             ? NS_ERROR_NOT_INITIALIZED
             : rv;
}

void TextControlState::FinishedRestoringSelection() {
  mRestoringSelection = nullptr;
}

void TextControlState::SyncUpSelectionPropertiesBeforeDestruction() {
  if (mBoundFrame) {
    UnbindFromFrame(mBoundFrame);
  }
}

void TextControlState::SetSelectionProperties(
    TextControlState::SelectionProperties& aProps) {
  if (mBoundFrame) {
    mBoundFrame->SetSelectionRange(aProps.GetStart(), aProps.GetEnd(),
                                   aProps.GetDirection());
    // The instance may have already been deleted here.
  } else {
    mSelectionProperties = aProps;
  }
}

void TextControlState::GetSelectionRange(uint32_t* aSelectionStart,
                                         uint32_t* aSelectionEnd,
                                         ErrorResult& aRv) {
  MOZ_ASSERT(aSelectionStart);
  MOZ_ASSERT(aSelectionEnd);
  MOZ_ASSERT(IsSelectionCached() || GetSelectionController(),
             "How can we not have a cached selection if we have no selection "
             "controller?");

  // Note that we may have both IsSelectionCached() _and_
  // GetSelectionController() if we haven't initialized our editor yet.
  if (IsSelectionCached()) {
    const SelectionProperties& props = GetSelectionProperties();
    *aSelectionStart = props.GetStart();
    *aSelectionEnd = props.GetEnd();
    return;
  }

  Selection* sel = mSelCon->GetSelection(SelectionType::eNormal);
  if (NS_WARN_IF(!sel)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  Element* root = GetRootNode();
  if (NS_WARN_IF(!root)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  nsContentUtils::GetSelectionInTextControl(sel, root, *aSelectionStart,
                                            *aSelectionEnd);
}

SelectionDirection TextControlState::GetSelectionDirection(ErrorResult& aRv) {
  MOZ_ASSERT(IsSelectionCached() || GetSelectionController(),
             "How can we not have a cached selection if we have no selection "
             "controller?");

  // Note that we may have both IsSelectionCached() _and_
  // GetSelectionController() if we haven't initialized our editor yet.
  if (IsSelectionCached()) {
    return GetSelectionProperties().GetDirection();
  }

  Selection* sel = mSelCon->GetSelection(SelectionType::eNormal);
  if (NS_WARN_IF(!sel)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return SelectionDirection::Forward;
  }

  nsDirection direction = sel->GetDirection();
  if (direction == eDirNext) {
    return SelectionDirection::Forward;
  }

  MOZ_ASSERT(direction == eDirPrevious);
  return SelectionDirection::Backward;
}

void TextControlState::SetSelectionRange(uint32_t aStart, uint32_t aEnd,
                                         SelectionDirection aDirection,
                                         ErrorResult& aRv,
                                         ScrollAfterSelection aScroll) {
  MOZ_ASSERT(IsSelectionCached() || mBoundFrame,
             "How can we have a non-cached selection but no frame?");

  AutoTextControlHandlingState handlingSetSelectionRange(
      *this, TextControlAction::SetSelectionRange);

  if (aStart > aEnd) {
    aStart = aEnd;
  }

  if (!IsSelectionCached()) {
    MOZ_ASSERT(mBoundFrame, "Our frame should still be valid");
    aRv = mBoundFrame->SetSelectionRange(aStart, aEnd, aDirection);
    if (aRv.Failed() ||
        handlingSetSelectionRange.IsTextControlStateDestroyed()) {
      return;
    }
    if (aScroll == ScrollAfterSelection::Yes && mBoundFrame) {
      // mBoundFrame could be gone if selection listeners flushed layout for
      // example.
      mBoundFrame->ScrollSelectionIntoViewAsync();
    }
    return;
  }

  SelectionProperties& props = GetSelectionProperties();
  if (!props.HasMaxLength()) {
    // A clone without a dirty value flag may not have a max length yet
    nsAutoString value;
    GetValue(value, false);
    props.SetMaxLength(value.Length());
  }

  bool changed = props.SetStart(aStart);
  changed |= props.SetEnd(aEnd);
  changed |= props.SetDirection(aDirection);

  if (!changed) {
    return;
  }

  // It sure would be nice if we had an existing Element* or so to work with.
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(mTextCtrlElement, eFormSelect, CanBubble::eYes);
  asyncDispatcher->PostDOMEvent();

  // SelectionChangeEventDispatcher covers this when !IsSelectionCached().
  // XXX(krosylight): Shouldn't it fire before select event?
  // Currently Gecko and Blink both fire selectionchange after select.
  if (IsSelectionCached() &&
      StaticPrefs::dom_select_events_textcontrols_selectionchange_enabled()) {
    asyncDispatcher = new AsyncEventDispatcher(
        mTextCtrlElement, eSelectionChange, CanBubble::eYes);
    asyncDispatcher->PostDOMEvent();
  }
}

void TextControlState::SetSelectionStart(const Nullable<uint32_t>& aStart,
                                         ErrorResult& aRv) {
  uint32_t start = 0;
  if (!aStart.IsNull()) {
    start = aStart.Value();
  }

  uint32_t ignored, end;
  GetSelectionRange(&ignored, &end, aRv);
  if (aRv.Failed()) {
    return;
  }

  SelectionDirection dir = GetSelectionDirection(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (end < start) {
    end = start;
  }

  SetSelectionRange(start, end, dir, aRv);
  // The instance may have already been deleted here.
}

void TextControlState::SetSelectionEnd(const Nullable<uint32_t>& aEnd,
                                       ErrorResult& aRv) {
  uint32_t end = 0;
  if (!aEnd.IsNull()) {
    end = aEnd.Value();
  }

  uint32_t start, ignored;
  GetSelectionRange(&start, &ignored, aRv);
  if (aRv.Failed()) {
    return;
  }

  SelectionDirection dir = GetSelectionDirection(aRv);
  if (aRv.Failed()) {
    return;
  }

  SetSelectionRange(start, end, dir, aRv);
  // The instance may have already been deleted here.
}

static void DirectionToName(SelectionDirection dir, nsAString& aDirection) {
  switch (dir) {
    case SelectionDirection::None:
      // TODO(mbrodesser): this should be supported, see
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1541454.
      NS_WARNING("We don't actually support this... how did we get it?");
      return aDirection.AssignLiteral("none");
    case SelectionDirection::Forward:
      return aDirection.AssignLiteral("forward");
    case SelectionDirection::Backward:
      return aDirection.AssignLiteral("backward");
  }
  MOZ_ASSERT_UNREACHABLE("Invalid SelectionDirection value");
}

void TextControlState::GetSelectionDirectionString(nsAString& aDirection,
                                                   ErrorResult& aRv) {
  SelectionDirection dir = GetSelectionDirection(aRv);
  if (aRv.Failed()) {
    return;
  }
  DirectionToName(dir, aDirection);
}

static SelectionDirection DirectionStringToSelectionDirection(
    const nsAString& aDirection) {
  if (aDirection.EqualsLiteral("backward")) {
    return SelectionDirection::Backward;
  }
  // We don't support directionless selections, see bug 1541454.
  return SelectionDirection::Forward;
}

void TextControlState::SetSelectionDirection(const nsAString& aDirection,
                                             ErrorResult& aRv) {
  SelectionDirection dir = DirectionStringToSelectionDirection(aDirection);

  uint32_t start, end;
  GetSelectionRange(&start, &end, aRv);
  if (aRv.Failed()) {
    return;
  }

  SetSelectionRange(start, end, dir, aRv);
  // The instance may have already been deleted here.
}

static SelectionDirection DirectionStringToSelectionDirection(
    const Optional<nsAString>& aDirection) {
  if (!aDirection.WasPassed()) {
    // We don't support directionless selections.
    return SelectionDirection::Forward;
  }

  return DirectionStringToSelectionDirection(aDirection.Value());
}

void TextControlState::SetSelectionRange(uint32_t aSelectionStart,
                                         uint32_t aSelectionEnd,
                                         const Optional<nsAString>& aDirection,
                                         ErrorResult& aRv,
                                         ScrollAfterSelection aScroll) {
  SelectionDirection dir = DirectionStringToSelectionDirection(aDirection);

  SetSelectionRange(aSelectionStart, aSelectionEnd, dir, aRv, aScroll);
  // The instance may have already been deleted here.
}

void TextControlState::SetRangeText(const nsAString& aReplacement,
                                    ErrorResult& aRv) {
  uint32_t start, end;
  GetSelectionRange(&start, &end, aRv);
  if (aRv.Failed()) {
    return;
  }

  SetRangeText(aReplacement, start, end, SelectionMode::Preserve, aRv,
               Some(start), Some(end));
  // The instance may have already been deleted here.
}

void TextControlState::SetRangeText(const nsAString& aReplacement,
                                    uint32_t aStart, uint32_t aEnd,
                                    SelectionMode aSelectMode, ErrorResult& aRv,
                                    const Maybe<uint32_t>& aSelectionStart,
                                    const Maybe<uint32_t>& aSelectionEnd) {
  if (aStart > aEnd) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  AutoTextControlHandlingState handlingSetRangeText(
      *this, TextControlAction::SetRangeText);

  nsAutoString value;
  mTextCtrlElement->GetValueFromSetRangeText(value);
  uint32_t inputValueLength = value.Length();

  if (aStart > inputValueLength) {
    aStart = inputValueLength;
  }

  if (aEnd > inputValueLength) {
    aEnd = inputValueLength;
  }

  uint32_t selectionStart, selectionEnd;
  if (!aSelectionStart) {
    MOZ_ASSERT(!aSelectionEnd);
    GetSelectionRange(&selectionStart, &selectionEnd, aRv);
    if (aRv.Failed()) {
      return;
    }
  } else {
    MOZ_ASSERT(aSelectionEnd);
    selectionStart = *aSelectionStart;
    selectionEnd = *aSelectionEnd;
  }

  // Batch selectionchanges from SetValueFromSetRangeText and SetSelectionRange
  Selection* selection =
      mSelCon ? mSelCon->GetSelection(SelectionType::eNormal) : nullptr;
  SelectionBatcher selectionBatcher(
      selection, __FUNCTION__,
      nsISelectionListener::JS_REASON);  // no-op if nullptr

  MOZ_ASSERT(aStart <= aEnd);
  value.Replace(aStart, aEnd - aStart, aReplacement);
  nsresult rv =
      MOZ_KnownLive(mTextCtrlElement)->SetValueFromSetRangeText(value);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  uint32_t newEnd = aStart + aReplacement.Length();
  int32_t delta = aReplacement.Length() - (aEnd - aStart);

  switch (aSelectMode) {
    case SelectionMode::Select:
      selectionStart = aStart;
      selectionEnd = newEnd;
      break;
    case SelectionMode::Start:
      selectionStart = selectionEnd = aStart;
      break;
    case SelectionMode::End:
      selectionStart = selectionEnd = newEnd;
      break;
    case SelectionMode::Preserve:
      if (selectionStart > aEnd) {
        selectionStart += delta;
      } else if (selectionStart > aStart) {
        selectionStart = aStart;
      }

      if (selectionEnd > aEnd) {
        selectionEnd += delta;
      } else if (selectionEnd > aStart) {
        selectionEnd = newEnd;
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown mode!");
  }

  SetSelectionRange(selectionStart, selectionEnd, Optional<nsAString>(), aRv);
  if (IsSelectionCached()) {
    // SetValueFromSetRangeText skipped SetMaxLength, set it here properly
    GetSelectionProperties().SetMaxLength(value.Length());
  }
}

void TextControlState::DestroyEditor() {
  // notify the editor that we are going away
  if (mEditorInitialized) {
    // FYI: TextEditor checks whether it's destroyed or not immediately after
    //      changes the DOM tree or selection so that it's safe to call
    //      PreDestroy() here even while we're handling actions with
    //      mTextEditor.
    MOZ_ASSERT(!mPasswordMaskData);
    RefPtr<TextEditor> textEditor = mTextEditor;
    mPasswordMaskData = textEditor->PreDestroy();
    MOZ_ASSERT_IF(mPasswordMaskData, !mPasswordMaskData->mTimer);
    mEditorInitialized = false;
  }
}

void TextControlState::UnbindFromFrame(nsTextControlFrame* aFrame) {
  if (NS_WARN_IF(!mBoundFrame)) {
    return;
  }

  // If it was, however, it should be unbounded from the same frame.
  MOZ_ASSERT(aFrame == mBoundFrame, "Unbinding from the wrong frame");
  if (aFrame && aFrame != mBoundFrame) {
    return;
  }

  AutoTextControlHandlingState handlingUnbindFromFrame(
      *this, TextControlAction::UnbindFromFrame);

  if (mSelCon) {
    mSelCon->SelectionWillLoseFocus();
  }

  // We need to start storing the value outside of the editor if we're not
  // going to use it anymore, so retrieve it for now.
  nsAutoString value;
  GetValue(value, true);

  if (mRestoringSelection) {
    mRestoringSelection->Revoke();
    mRestoringSelection = nullptr;
  }

  // Save our selection state if needed.
  // Note that GetSelectionRange will attempt to work with our selection
  // controller, so we should make sure we do it before we start doing things
  // like destroying our editor (if we have one), tearing down the selection
  // controller, and so forth.
  if (!IsSelectionCached()) {
    // Go ahead and cache it now.
    uint32_t start = 0, end = 0;
    GetSelectionRange(&start, &end, IgnoreErrors());

    nsITextControlFrame::SelectionDirection direction =
        GetSelectionDirection(IgnoreErrors());

    SelectionProperties& props = GetSelectionProperties();
    props.SetMaxLength(value.Length());
    props.SetStart(start);
    props.SetEnd(end);
    props.SetDirection(direction);
    mSelectionCached = true;
  }

  // Destroy our editor
  DestroyEditor();

  // Clean up the controller
  if (!SuppressEventHandlers(mBoundFrame->PresContext())) {
    nsCOMPtr<nsIControllers> controllers;
    if (HTMLInputElement* inputElement =
            HTMLInputElement::FromNodeOrNull(mTextCtrlElement)) {
      inputElement->GetControllers(getter_AddRefs(controllers));
    } else {
      HTMLTextAreaElement* textAreaElement =
          HTMLTextAreaElement::FromNodeOrNull(mTextCtrlElement);
      if (textAreaElement) {
        textAreaElement->GetControllers(getter_AddRefs(controllers));
      }
    }

    if (controllers) {
      uint32_t numControllers;
      nsresult rv = controllers->GetControllerCount(&numControllers);
      NS_ASSERTION((NS_SUCCEEDED(rv)),
                   "bad result in gfx text control destructor");
      for (uint32_t i = 0; i < numControllers; i++) {
        nsCOMPtr<nsIController> controller;
        rv = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(rv) && controller) {
          nsCOMPtr<nsIControllerContext> editController =
              do_QueryInterface(controller);
          if (editController) {
            editController->SetCommandContext(nullptr);
          }
        }
      }
    }
  }

  if (mSelCon) {
    if (mTextListener) {
      mTextListener->EndListeningToSelectionChange();
    }

    mSelCon->SetScrollableFrame(nullptr);
    mSelCon = nullptr;
  }

  if (mTextListener) {
    mTextListener->SetFrame(nullptr);

    EventListenerManager* manager =
        mTextCtrlElement->GetExistingListenerManager();
    if (manager) {
      manager->RemoveEventListenerByType(mTextListener, u"keydown"_ns,
                                         TrustedEventsAtSystemGroupBubble());
      manager->RemoveEventListenerByType(mTextListener, u"keypress"_ns,
                                         TrustedEventsAtSystemGroupBubble());
      manager->RemoveEventListenerByType(mTextListener, u"keyup"_ns,
                                         TrustedEventsAtSystemGroupBubble());
    }

    mTextListener = nullptr;
  }

  mBoundFrame = nullptr;

  // Now that we don't have a frame any more, store the value in the text
  // buffer. The only case where we don't do this is if a value transfer is in
  // progress.
  if (!mValueTransferInProgress) {
    DebugOnly<bool> ok = SetValue(value, ValueSetterOption::ByInternalAPI);
    // TODO Find something better to do if this fails...
    NS_WARNING_ASSERTION(ok, "SetValue() couldn't allocate memory");
  }
}

void TextControlState::GetValue(nsAString& aValue, bool aIgnoreWrap) const {
  // While SetValue() is being called and requesting to commit composition to
  // IME, GetValue() may be called for appending text or something.  Then, we
  // need to return the latest aValue of SetValue() since the value hasn't
  // been set to the editor yet.
  // XXX After implementing "beforeinput" event, this becomes wrong.  The
  //     value should be modified immediately after "beforeinput" event for
  //     "insertReplacementText".
  if (mHandlingState &&
      mHandlingState->IsHandling(TextControlAction::CommitComposition)) {
    aValue = mHandlingState->GetSettingValue();
    MOZ_ASSERT(aValue.FindChar(u'\r') == -1);
    return;
  }

  if (mTextEditor && mBoundFrame &&
      (mEditorInitialized || !IsSingleLineTextControl())) {
    if (aIgnoreWrap && !mBoundFrame->CachedValue().IsVoid()) {
      aValue = mBoundFrame->CachedValue();
      MOZ_ASSERT(aValue.FindChar(u'\r') == -1);
      return;
    }

    aValue.Truncate();  // initialize out param

    uint32_t flags = (nsIDocumentEncoder::OutputLFLineBreak |
                      nsIDocumentEncoder::OutputPreformatted |
                      nsIDocumentEncoder::OutputPersistNBSP |
                      nsIDocumentEncoder::OutputBodyOnly);
    if (!aIgnoreWrap) {
      TextControlElement::nsHTMLTextWrap wrapProp;
      if (mTextCtrlElement &&
          TextControlElement::GetWrapPropertyEnum(mTextCtrlElement, wrapProp) &&
          wrapProp == TextControlElement::eHTMLTextWrap_Hard) {
        flags |= nsIDocumentEncoder::OutputWrap;
      }
    }

    // What follows is a bit of a hack.  The problem is that we could be in
    // this method because we're being destroyed for whatever reason while
    // script is executing.  If that happens, editor will run with the
    // privileges of the executing script, which means it may not be able to
    // access its own DOM nodes!  Let's try to deal with that by pushing a null
    // JSContext on the JSContext stack to make it clear that we're native
    // code.  Note that any script that's directly trying to access our value
    // has to be going through some scriptable object to do that and that
    // already does the relevant security checks.
    // XXXbz if we could just get the textContent of our anonymous content (eg
    // if plaintext editor didn't create <br> nodes all over), we wouldn't need
    // this.
    { /* Scope for AutoNoJSAPI. */
      AutoNoJSAPI nojsapi;

      DebugOnly<nsresult> rv = mTextEditor->ComputeTextValue(flags, aValue);
      MOZ_ASSERT(aValue.FindChar(u'\r') == -1);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to get value");
    }
    // Only when the result doesn't include line breaks caused by hard-wrap,
    // mCacheValue should cache the value.
    if (!(flags & nsIDocumentEncoder::OutputWrap)) {
      mBoundFrame->CacheValue(aValue);
    } else {
      mBoundFrame->ClearCachedValue();
    }
  } else {
    if (!mTextCtrlElement->ValueChanged() || mValue.IsVoid()) {
      // Use nsString to avoid copying string buffer at setting aValue.
      nsString value;
      mTextCtrlElement->GetDefaultValueFromContent(value);
      // TODO: We should make default value not include \r.
      nsContentUtils::PlatformToDOMLineBreaks(value);
      aValue = value;
    } else {
      aValue = mValue;
      MOZ_ASSERT(aValue.FindChar(u'\r') == -1);
    }
  }
}

bool TextControlState::ValueEquals(const nsAString& aValue) const {
  // We can avoid copying string buffer in many cases.  Therefore, we should
  // use nsString rather than nsAutoString here.
  nsString value;
  GetValue(value, true);
  return aValue.Equals(value);
}

#ifdef DEBUG
// @param aOptions TextControlState::ValueSetterOptions
bool AreFlagsNotDemandingContradictingMovements(
    const ValueSetterOptions& aOptions) {
  return !aOptions.contains(
      {ValueSetterOption::MoveCursorToBeginSetSelectionDirectionForward,
       ValueSetterOption::MoveCursorToEndIfValueChanged});
}
#endif  // DEBUG

bool TextControlState::SetValue(const nsAString& aValue,
                                const nsAString* aOldValue,
                                const ValueSetterOptions& aOptions) {
  if (mHandlingState &&
      mHandlingState->IsHandling(TextControlAction::CommitComposition)) {
    // GetValue doesn't return current text frame's content during committing.
    // So we cannot trust this old value
    aOldValue = nullptr;
  }

  if (mPasswordMaskData) {
    if (mHandlingState &&
        mHandlingState->Is(TextControlAction::UnbindFromFrame)) {
      // If we're called by UnbindFromFrame, we shouldn't reset unmasked range.
    } else {
      // Otherwise, we should mask the new password, even if it's same value
      // since the same value may be one for different web app's.
      mPasswordMaskData->Reset();
    }
  }

  const bool wasHandlingSetValue =
      mHandlingState && mHandlingState->IsHandling(TextControlAction::SetValue);

  ErrorResult error;
  AutoTextControlHandlingState handlingSetValue(
      *this, TextControlAction::SetValue, aValue, aOldValue, aOptions, error);
  if (error.Failed()) {
    MOZ_ASSERT(error.ErrorCodeIs(NS_ERROR_OUT_OF_MEMORY));
    error.SuppressException();
    return false;
  }

  const auto changeKind = [&] {
    if (aOptions.contains(ValueSetterOption::ByInternalAPI)) {
      return ValueChangeKind::Internal;
    }
    if (aOptions.contains(ValueSetterOption::BySetUserInputAPI)) {
      return ValueChangeKind::UserInteraction;
    }
    return ValueChangeKind::Script;
  }();

  if (changeKind == ValueChangeKind::Script) {
    // This value change will not be interactive. If we're an input that was
    // interactively edited, save the last interactive value now before it goes
    // away.
    if (auto* input = HTMLInputElement::FromNode(mTextCtrlElement)) {
      if (input->LastValueChangeWasInteractive()) {
        GetValue(mLastInteractiveValue, /* aIgnoreWrap = */ true);
      }
    }
  }

  // Note that if this may be called during reframe of the editor.  In such
  // case, we shouldn't commit composition.  Therefore, when this is called
  // for internal processing, we shouldn't commit the composition.
  // TODO: In strictly speaking, we should move committing composition into
  //       editor because if "beforeinput" for this setting value is canceled,
  //       we shouldn't commit composition.  However, in Firefox, we never
  //       call this via `setUserInput` during composition.  Therefore, the
  //       bug must not be reproducible actually.
  if (aOptions.contains(ValueSetterOption::BySetUserInputAPI) ||
      aOptions.contains(ValueSetterOption::ByContentAPI)) {
    if (EditorHasComposition()) {
      // When this is called recursively, there shouldn't be composition.
      if (handlingSetValue.IsHandling(TextControlAction::CommitComposition)) {
        // Don't request to commit composition again.  But if it occurs,
        // we should skip to set the new value to the editor here.  It should
        // be set later with the newest value.
        return true;
      }
      if (NS_WARN_IF(!mBoundFrame)) {
        // We're not sure if this case is possible.
      } else {
        // If setting value won't change current value, we shouldn't commit
        // composition for compatibility with the other browsers.
        MOZ_ASSERT(!aOldValue || mBoundFrame->TextEquals(*aOldValue));
        bool isSameAsCurrentValue =
            aOldValue
                ? aOldValue->Equals(handlingSetValue.GetSettingValue())
                : mBoundFrame->TextEquals(handlingSetValue.GetSettingValue());
        if (isSameAsCurrentValue) {
          // Note that in this case, we shouldn't fire any events with setting
          // value because event handlers may try to set value recursively but
          // we cannot commit composition at that time due to unsafe to run
          // script (see below).
          return true;
        }
      }
      // If there is composition, need to commit composition first because
      // other browsers do that.
      // NOTE: We don't need to block nested calls of this because input nor
      //       other events won't be fired by setting values and script blocker
      //       is used during setting the value to the editor.  IE also allows
      //       to set the editor value on the input event which is caused by
      //       forcibly committing composition.
      AutoTextControlHandlingState handlingCommitComposition(
          *this, TextControlAction::CommitComposition);
      if (nsContentUtils::IsSafeToRunScript()) {
        // WARNING: During this call, compositionupdate, compositionend, input
        // events will be fired.  Therefore, everything can occur.  E.g., the
        // document may be unloaded.
        RefPtr<TextEditor> textEditor = mTextEditor;
        nsresult rv = textEditor->CommitComposition();
        if (handlingCommitComposition.IsTextControlStateDestroyed()) {
          return true;
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("TextControlState failed to commit composition");
          return true;
        }
        // Note that if a composition event listener sets editor value again,
        // we should use the new value here.  The new value is stored in
        // handlingSetValue right now.
      } else {
        NS_WARNING(
            "SetValue() is called when there is composition but "
            "it's not safe to request to commit the composition");
      }
    }
  }

  if (mTextEditor && mBoundFrame) {
    if (!SetValueWithTextEditor(handlingSetValue)) {
      return false;
    }
  } else if (!SetValueWithoutTextEditor(handlingSetValue)) {
    return false;
  }

  // If we were handling SetValue() before, don't update the DOM state twice,
  // just let the outer call do so.
  if (!wasHandlingSetValue) {
    handlingSetValue.GetTextControlElement()->OnValueChanged(changeKind);
  }
  return true;
}

bool TextControlState::SetValueWithTextEditor(
    AutoTextControlHandlingState& aHandlingSetValue) {
  MOZ_ASSERT(aHandlingSetValue.Is(TextControlAction::SetValue));
  MOZ_ASSERT(mTextEditor);
  MOZ_ASSERT(mBoundFrame);
  NS_WARNING_ASSERTION(!EditorHasComposition(),
                       "Failed to commit composition before setting value.  "
                       "Investigate the cause!");

#ifdef DEBUG
  if (IsSingleLineTextControl()) {
    NS_ASSERTION(mEditorInitialized || aHandlingSetValue.IsHandling(
                                           TextControlAction::PrepareEditor),
                 "We should never try to use the editor if we're not "
                 "initialized unless we're being initialized");
  }
#endif

  MOZ_ASSERT(!aHandlingSetValue.GetOldValue() ||
             mBoundFrame->TextEquals(*aHandlingSetValue.GetOldValue()));
  bool isSameAsCurrentValue =
      aHandlingSetValue.GetOldValue()
          ? aHandlingSetValue.GetOldValue()->Equals(
                aHandlingSetValue.GetSettingValue())
          : mBoundFrame->TextEquals(aHandlingSetValue.GetSettingValue());

  // this is necessary to avoid infinite recursion
  if (isSameAsCurrentValue) {
    return true;
  }

  RefPtr<TextEditor> textEditor = mTextEditor;

  nsCOMPtr<Document> document = textEditor->GetDocument();
  if (NS_WARN_IF(!document)) {
    return true;
  }

  // Time to mess with our security context... See comments in GetValue()
  // for why this is needed.  Note that we have to do this up here, because
  // otherwise SelectAll() will fail.
  AutoNoJSAPI nojsapi;

  // FYI: It's safe to use raw pointer for selection here because
  //      SelectionBatcher will grab it with RefPtr.
  Selection* selection = mSelCon->GetSelection(SelectionType::eNormal);
  SelectionBatcher selectionBatcher(selection, __FUNCTION__);

  // get the flags, remove readonly, disabled and max-length,
  // set the value, restore flags
  AutoRestoreEditorState restoreState(textEditor);

  aHandlingSetValue.WillSetValueWithTextEditor();

  if (aHandlingSetValue.ValueSetterOptionsRef().contains(
          ValueSetterOption::BySetUserInputAPI)) {
    // If the caller inserts text as part of user input, for example,
    // autocomplete, we need to replace the text as "insert string"
    // because undo should cancel only this operation (i.e., previous
    // transactions typed by user shouldn't be merged with this).
    // In this case, we need to dispatch "input" event because
    // web apps may need to know the user's operation.
    // In this case, we need to dispatch "beforeinput" events since
    // we're emulating the user's input.  Passing nullptr as
    // nsIPrincipal means that that may be user's input.  So, let's
    // do it.
    nsresult rv = textEditor->ReplaceTextAsAction(
        aHandlingSetValue.GetSettingValue(), nullptr,
        StaticPrefs::dom_input_event_allow_to_cancel_set_user_input()
            ? TextEditor::AllowBeforeInputEventCancelable::Yes
            : TextEditor::AllowBeforeInputEventCancelable::No,
        nullptr);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::ReplaceTextAsAction() failed");
    return rv != NS_ERROR_OUT_OF_MEMORY;
  }

  // Don't dispatch "beforeinput" event nor "input" event for setting value
  // by script.
  AutoInputEventSuppresser suppressInputEventDispatching(textEditor);

  // On <input> or <textarea>, we shouldn't preserve existing undo
  // transactions because other browsers do not preserve them too
  // and not preserving transactions makes setting value faster.
  //
  // (Except if chrome opts into this behavior).
  Maybe<AutoDisableUndo> disableUndo;
  if (!aHandlingSetValue.ValueSetterOptionsRef().contains(
          ValueSetterOption::PreserveUndoHistory)) {
    disableUndo.emplace(textEditor);
  }

  if (selection) {
    // Since we don't use undo transaction, we don't need to store
    // selection state.  SetText will set selection to tail.
    IgnoredErrorResult ignoredError;
    MOZ_KnownLive(selection)->RemoveAllRanges(ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Selection::RemoveAllRanges() failed, but ignored");
  }

  // In this case, we makes the editor stop dispatching "input"
  // event so that passing nullptr as nsIPrincipal is safe for now.
  nsresult rv = textEditor->SetTextAsAction(
      aHandlingSetValue.GetSettingValue(),
      aHandlingSetValue.ValueSetterOptionsRef().contains(
          ValueSetterOption::BySetUserInputAPI) &&
              !StaticPrefs::dom_input_event_allow_to_cancel_set_user_input()
          ? TextEditor::AllowBeforeInputEventCancelable::No
          : TextEditor::AllowBeforeInputEventCancelable::Yes,
      nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::SetTextAsAction() failed");

  // Call the listener's OnEditActionHandled() callback manually if
  // OnEditActionHandled() hasn't been called yet since TextEditor don't use
  // the transaction manager in this path and it could be that the editor
  // would bypass calling the listener for that reason.
  if (!aHandlingSetValue.HasEditActionHandled()) {
    nsresult rvOnEditActionHandled =
        MOZ_KnownLive(aHandlingSetValue.GetTextInputListener())
            ->OnEditActionHandled(*textEditor);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvOnEditActionHandled),
                         "TextInputListener::OnEditActionHandled() failed");
    if (rv != NS_ERROR_OUT_OF_MEMORY) {
      rv = rvOnEditActionHandled;
    }
  }

  return rv != NS_ERROR_OUT_OF_MEMORY;
}

bool TextControlState::SetValueWithoutTextEditor(
    AutoTextControlHandlingState& aHandlingSetValue) {
  MOZ_ASSERT(aHandlingSetValue.Is(TextControlAction::SetValue));
  MOZ_ASSERT(!mTextEditor || !mBoundFrame);
  NS_WARNING_ASSERTION(!EditorHasComposition(),
                       "Failed to commit composition before setting value.  "
                       "Investigate the cause!");

  if (mValue.IsVoid()) {
    mValue.SetIsVoid(false);
  }

  // We can't just early-return here, because OnValueChanged below still need to
  // be called.
  if (!mValue.Equals(aHandlingSetValue.GetSettingValue()) ||
      !StaticPrefs::dom_input_skip_cursor_move_for_same_value_set()) {
    bool handleSettingValue = true;
    // If `SetValue()` call is nested, `GetSettingValue()` result will be
    // modified.  So, we need to store input event data value before
    // dispatching beforeinput event.
    nsString inputEventData(aHandlingSetValue.GetSettingValue());
    if (aHandlingSetValue.ValueSetterOptionsRef().contains(
            ValueSetterOption::BySetUserInputAPI) &&
        StaticPrefs::dom_input_events_beforeinput_enabled() &&
        !aHandlingSetValue.HasBeforeInputEventDispatched()) {
      // This probably occurs when session restorer sets the old value with
      // `setUserInput`.  If so, we need to dispatch "beforeinput" event of
      // "insertReplacementText" for conforming to the spec.  However, the
      // spec does NOT treat the session restoring case.  Therefore, if this
      // breaks session restorere in a lot of web apps, we should probably
      // stop dispatching it or make it non-cancelable.
      MOZ_ASSERT(aHandlingSetValue.GetTextControlElement());
      MOZ_ASSERT(!aHandlingSetValue.GetSettingValue().IsVoid());
      aHandlingSetValue.WillDispatchBeforeInputEvent();
      nsEventStatus status = nsEventStatus_eIgnore;
      DebugOnly<nsresult> rvIgnored = nsContentUtils::DispatchInputEvent(
          MOZ_KnownLive(aHandlingSetValue.GetTextControlElement()),
          eEditorBeforeInput, EditorInputType::eInsertReplacementText, nullptr,
          InputEventOptions(
              inputEventData,
              StaticPrefs::dom_input_event_allow_to_cancel_set_user_input()
                  ? InputEventOptions::NeverCancelable::No
                  : InputEventOptions::NeverCancelable::Yes),
          &status);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "Failed to dispatch beforeinput event");
      if (status == nsEventStatus_eConsumeNoDefault) {
        return true;  // "beforeinput" event was canceled.
      }
      // If we were destroyed by "beforeinput" event listeners, probably, we
      // don't need to keep handling it.
      if (aHandlingSetValue.IsTextControlStateDestroyed()) {
        return true;
      }
      // Even if "beforeinput" event was not canceled, its listeners may do
      // something.  If it causes creating `TextEditor` and bind this to a
      // frame, we need to use the path, but `TextEditor` shouldn't fire
      // "beforeinput" event again.  Therefore, we need to prevent editor
      // to dispatch it.
      if (mTextEditor && mBoundFrame) {
        AutoInputEventSuppresser suppressInputEvent(mTextEditor);
        if (!SetValueWithTextEditor(aHandlingSetValue)) {
          return false;
        }
        // If we were destroyed by "beforeinput" event listeners, probably, we
        // don't need to keep handling it.
        if (aHandlingSetValue.IsTextControlStateDestroyed()) {
          return true;
        }
        handleSettingValue = false;
      }
    }

    if (handleSettingValue) {
      if (!mValue.Assign(aHandlingSetValue.GetSettingValue(), fallible)) {
        return false;
      }

      // Since we have no editor we presumably have cached selection state.
      if (IsSelectionCached()) {
        MOZ_ASSERT(AreFlagsNotDemandingContradictingMovements(
            aHandlingSetValue.ValueSetterOptionsRef()));

        SelectionProperties& props = GetSelectionProperties();
        // Setting a max length and thus capping selection range early prevents
        // selection change detection in setRangeText. Temporarily disable
        // capping here with UINT32_MAX, and set it later in ::SetRangeText().
        props.SetMaxLength(aHandlingSetValue.ValueSetterOptionsRef().contains(
                               ValueSetterOption::BySetRangeTextAPI)
                               ? UINT32_MAX
                               : aHandlingSetValue.GetSettingValue().Length());
        if (aHandlingSetValue.ValueSetterOptionsRef().contains(
                ValueSetterOption::MoveCursorToEndIfValueChanged)) {
          props.SetStart(aHandlingSetValue.GetSettingValue().Length());
          props.SetEnd(aHandlingSetValue.GetSettingValue().Length());
          props.SetDirection(SelectionDirection::Forward);
        } else if (aHandlingSetValue.ValueSetterOptionsRef().contains(
                       ValueSetterOption::
                           MoveCursorToBeginSetSelectionDirectionForward)) {
          props.SetStart(0);
          props.SetEnd(0);
          props.SetDirection(SelectionDirection::Forward);
        }
      }

      // Update the frame display if needed
      if (mBoundFrame) {
        mBoundFrame->UpdateValueDisplay(true);
      }
    }

    // If this is called as part of user input, we need to dispatch "input"
    // event with "insertReplacementText" since web apps may want to know
    // the user operation which changes editor value with a built-in function
    // like autocomplete, password manager, session restore, etc.
    // XXX Should we stop dispatching `input` event if the text control
    //     element has already removed from the DOM tree by a `beforeinput`
    //     event listener?
    if (aHandlingSetValue.ValueSetterOptionsRef().contains(
            ValueSetterOption::BySetUserInputAPI)) {
      MOZ_ASSERT(aHandlingSetValue.GetTextControlElement());

      // Update validity state before dispatching "input" event for its
      // listeners like `EditorBase::NotifyEditorObservers()`.
      aHandlingSetValue.GetTextControlElement()->OnValueChanged(
          ValueChangeKind::UserInteraction);

      ClearLastInteractiveValue();

      MOZ_ASSERT(!aHandlingSetValue.GetSettingValue().IsVoid());
      DebugOnly<nsresult> rvIgnored = nsContentUtils::DispatchInputEvent(
          MOZ_KnownLive(aHandlingSetValue.GetTextControlElement()),
          eEditorInput, EditorInputType::eInsertReplacementText, nullptr,
          InputEventOptions(inputEventData,
                            InputEventOptions::NeverCancelable::No));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "Failed to dispatch input event");
    }
  } else {
    // Even if our value is not actually changing, apparently we need to mark
    // our SelectionProperties dirty to make accessibility tests happy.
    // Probably because they depend on the SetSelectionRange() call we make on
    // our frame in RestoreSelectionState, but I have no idea why they do.
    if (IsSelectionCached()) {
      SelectionProperties& props = GetSelectionProperties();
      props.SetIsDirty();
    }
  }

  return true;
}

bool TextControlState::HasNonEmptyValue() {
  // If the frame for editor is alive, we can compute it with mTextEditor.
  // Otherwise, we need to check cached value via GetValue().
  if (mTextEditor && mBoundFrame && mEditorInitialized &&
      !(mHandlingState &&
        mHandlingState->IsHandling(TextControlAction::CommitComposition))) {
    return !mTextEditor->IsEmpty();
  }

  nsAutoString value;
  GetValue(value, true);
  return !value.IsEmpty();
}

void TextControlState::InitializeKeyboardEventListeners() {
  // register key listeners
  EventListenerManager* manager =
      mTextCtrlElement->GetOrCreateListenerManager();
  if (manager) {
    manager->AddEventListenerByType(mTextListener, u"keydown"_ns,
                                    TrustedEventsAtSystemGroupBubble());
    manager->AddEventListenerByType(mTextListener, u"keypress"_ns,
                                    TrustedEventsAtSystemGroupBubble());
    manager->AddEventListenerByType(mTextListener, u"keyup"_ns,
                                    TrustedEventsAtSystemGroupBubble());
  }

  mSelCon->SetScrollableFrame(mBoundFrame->GetScrollTargetFrame());
}

void TextControlState::SetPreviewText(const nsAString& aValue, bool aNotify) {
  // If we don't have a preview div, there's nothing to do.
  Element* previewDiv = GetPreviewNode();
  if (!previewDiv) {
    return;
  }

  nsAutoString previewValue(aValue);

  nsContentUtils::RemoveNewlines(previewValue);
  MOZ_ASSERT(previewDiv->GetFirstChild(), "preview div has no child");
  previewDiv->GetFirstChild()->AsText()->SetText(previewValue, aNotify);
}

void TextControlState::GetPreviewText(nsAString& aValue) {
  // If we don't have a preview div, there's nothing to do.
  Element* previewDiv = GetPreviewNode();
  if (!previewDiv) {
    return;
  }

  MOZ_ASSERT(previewDiv->GetFirstChild(), "preview div has no child");
  const nsTextFragment* text = previewDiv->GetFirstChild()->GetText();

  aValue.Truncate();
  text->AppendTo(aValue);
}

bool TextControlState::EditorHasComposition() {
  return mTextEditor && mTextEditor->IsIMEComposing();
}

}  // namespace mozilla
