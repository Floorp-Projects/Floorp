/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextEditorState.h"
#include "mozilla/TextInputListener.h"

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsView.h"
#include "nsCaret.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsITextControlFrame.h"
#include "nsContentCreatorFunctions.h"
#include "nsTextControlFrame.h"
#include "nsIControllers.h"
#include "nsITransactionManager.h"
#include "nsIControllerContext.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMEventListener.h"
#include "nsIEditorObserver.h"
#include "nsIWidget.h"
#include "nsIDocumentEncoder.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/EventListenerManager.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsTextNode.h"
#include "nsIController.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/Text.h"
#include "nsNumberControlFrame.h"
#include "nsFrameSelection.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::dom;

class MOZ_STACK_CLASS ValueSetter
{
public:
  explicit ValueSetter(TextEditor* aTextEditor)
    : mTextEditor(aTextEditor)
    // To protect against a reentrant call to SetValue, we check whether
    // another SetValue is already happening for this editor.  If it is,
    // we must wait until we unwind to re-enable oninput events.
    , mOuterTransaction(aTextEditor->IsSuppressingDispatchingInputEvent())
  {
    MOZ_ASSERT(aTextEditor);
  }
  ~ValueSetter()
  {
    mTextEditor->SuppressDispatchingInputEvent(mOuterTransaction);
  }
  void Init()
  {
    mTextEditor->SuppressDispatchingInputEvent(true);
  }

private:
  RefPtr<TextEditor> mTextEditor;
  bool mOuterTransaction;
};

class RestoreSelectionState : public Runnable {
public:
  RestoreSelectionState(nsTextEditorState* aState, nsTextControlFrame* aFrame)
    : mozilla::Runnable("RestoreSelectionState")
    , mFrame(aFrame)
    , mTextEditorState(aState)
  {
  }

  NS_IMETHOD Run() override {
    if (!mTextEditorState) {
      return NS_OK;
    }

    AutoHideSelectionChanges hideSelectionChanges
      (mFrame->GetConstFrameSelection());

    if (mFrame) {
      // SetSelectionRange leads to Selection::AddRange which flushes Layout -
      // need to block script to avoid nested PrepareEditor calls (bug 642800).
      nsAutoScriptBlocker scriptBlocker;
      nsTextEditorState::SelectionProperties& properties =
        mTextEditorState->GetSelectionProperties();
      if (properties.IsDirty()) {
        mFrame->SetSelectionRange(properties.GetStart(),
                                  properties.GetEnd(),
                                  properties.GetDirection());
      }
      if (!mTextEditorState->mSelectionRestoreEagerInit) {
        mTextEditorState->HideSelectionIfBlurred();
      }
      mTextEditorState->mSelectionRestoreEagerInit = false;
    }

    if (mTextEditorState) {
      mTextEditorState->FinishedRestoringSelection();
    }
    return NS_OK;
  }

  // Let the text editor tell us we're no longer relevant - avoids use of AutoWeakFrame
  void Revoke() {
    mFrame = nullptr;
    mTextEditorState = nullptr;
  }

private:
  nsTextControlFrame* mFrame;
  nsTextEditorState* mTextEditorState;
};

class MOZ_RAII AutoRestoreEditorState final
{
public:
  explicit AutoRestoreEditorState(TextEditor* aTextEditor
                                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mTextEditor(aTextEditor)
    , mSavedFlags(mTextEditor->Flags())
    , mSavedMaxLength(mTextEditor->MaxTextLength())
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mTextEditor);

    uint32_t flags = mSavedFlags;
    flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
    flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
    flags |= nsIPlaintextEditor::eEditorDontEchoPassword;
    mTextEditor->SetFlags(flags);

    mTextEditor->SetMaxTextLength(-1);
  }

  ~AutoRestoreEditorState()
  {
     mTextEditor->SetMaxTextLength(mSavedMaxLength);
     mTextEditor->SetFlags(mSavedFlags);
  }

private:
  TextEditor* mTextEditor;
  uint32_t mSavedFlags;
  int32_t mSavedMaxLength;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoDisableUndo final
{
public:
  explicit AutoDisableUndo(TextEditor* aTextEditor
                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mTextEditor(aTextEditor)
    , mPreviousEnabled(true)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mTextEditor);

    mPreviousEnabled = mTextEditor->IsUndoRedoEnabled();
    DebugOnly<bool> disabledUndoRedo = mTextEditor->DisableUndoRedo();
    NS_WARNING_ASSERTION(disabledUndoRedo,
      "Failed to disable undo/redo transactions");
  }

  ~AutoDisableUndo()
  {
    if (mPreviousEnabled) {
      DebugOnly<bool> enabledUndoRedo = mTextEditor->EnableUndoRedo();
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
  bool mPreviousEnabled;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*static*/
bool
nsITextControlElement::GetWrapPropertyEnum(nsIContent* aContent,
  nsITextControlElement::nsHTMLTextWrap& aWrapProp)
{
  // soft is the default; "physical" defaults to soft as well because all other
  // browsers treat it that way and there is no real reason to maintain physical
  // and virtual as separate entities if no one else does.  Only hard and off
  // do anything different.
  aWrapProp = eHTMLTextWrap_Soft; // the default

  nsAutoString wrap;
  if (aContent->IsHTMLElement()) {
    static Element::AttrValuesArray strings[] =
      {&nsGkAtoms::HARD, &nsGkAtoms::OFF, nullptr};

    switch (aContent->AsElement()->FindAttrValueIn(kNameSpaceID_None,
                                                   nsGkAtoms::wrap, strings,
                                                   eIgnoreCase)) {
      case 0: aWrapProp = eHTMLTextWrap_Hard; break;
      case 1: aWrapProp = eHTMLTextWrap_Off; break;
    }

    return true;
  }

  return false;
}

/*static*/
already_AddRefed<nsITextControlElement>
nsITextControlElement::GetTextControlElementFromEditingHost(nsIContent* aHost)
{
  if (!aHost) {
    return nullptr;
  }

  nsCOMPtr<nsITextControlElement> parent =
    do_QueryInterface(aHost->GetParent());

  return parent.forget();
}

static bool
SuppressEventHandlers(nsPresContext* aPresContext)
{
  bool suppressHandlers = false;

  if (aPresContext)
  {
    // Right now we only suppress event handlers and controller manipulation
    // when in a print preview or print context!

    // In the current implementation, we only paginate when
    // printing or in print preview.

    suppressHandlers = aPresContext->IsPaginated();
  }

  return suppressHandlers;
}

class nsAnonDivObserver final : public nsStubMutationObserver
{
public:
  explicit nsAnonDivObserver(nsTextEditorState* aTextEditorState)
  : mTextEditorState(aTextEditorState) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

private:
  ~nsAnonDivObserver() {}
  nsTextEditorState* mTextEditorState;
};

class nsTextInputSelectionImpl final : public nsSupportsWeakReference
                                     , public nsISelectionController
{
  ~nsTextInputSelectionImpl(){}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTextInputSelectionImpl, nsISelectionController)

  nsTextInputSelectionImpl(nsFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter);

  void SetScrollableFrame(nsIScrollableFrame *aScrollableFrame);
  nsFrameSelection* GetConstFrameSelection()
    { return mFrameSelection; }
  // Will return null if !mFrameSelection.
  Selection* GetSelection(SelectionType aSelectionType);

  //NSISELECTIONCONTROLLER INTERFACES
  NS_IMETHOD SetDisplaySelection(int16_t toggle) override;
  NS_IMETHOD GetDisplaySelection(int16_t* _retval) override;
  NS_IMETHOD SetSelectionFlags(int16_t aInEnable) override;
  NS_IMETHOD GetSelectionFlags(int16_t *aOutEnable) override;
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
  NS_IMETHOD PhysicalMove(int16_t aDirection, int16_t aAmount, bool aExtend) override;
  NS_IMETHOD CharacterMove(bool aForward, bool aExtend) override;
  NS_IMETHOD CharacterExtendForDelete() override;
  NS_IMETHOD CharacterExtendForBackspace() override;
  NS_IMETHOD WordMove(bool aForward, bool aExtend) override;
  NS_IMETHOD WordExtendForDelete(bool aForward) override;
  NS_IMETHOD LineMove(bool aForward, bool aExtend) override;
  NS_IMETHOD IntraLineMove(bool aForward, bool aExtend) override;
  NS_IMETHOD PageMove(bool aForward, bool aExtend) override;
  NS_IMETHOD CompleteScroll(bool aForward) override;
  NS_IMETHOD CompleteMove(bool aForward, bool aExtend) override;
  NS_IMETHOD ScrollPage(bool aForward) override;
  NS_IMETHOD ScrollLine(bool aForward) override;
  NS_IMETHOD ScrollCharacter(bool aRight) override;
  NS_IMETHOD SelectAll(void) override;
  NS_IMETHOD CheckVisibility(nsIDOMNode *node, int16_t startOffset, int16_t EndOffset, bool* _retval) override;
  virtual nsresult CheckVisibilityContent(nsIContent* aNode, int16_t aStartOffset, int16_t aEndOffset, bool* aRetval) override;

private:
  RefPtr<nsFrameSelection> mFrameSelection;
  nsCOMPtr<nsIContent>       mLimiter;
  nsIScrollableFrame        *mScrollFrame;
  nsWeakPtr mPresShellWeak;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTextInputSelectionImpl)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTextInputSelectionImpl)
NS_INTERFACE_TABLE_HEAD(nsTextInputSelectionImpl)
  NS_INTERFACE_TABLE(nsTextInputSelectionImpl,
                     nsISelectionController,
                     nsISelectionDisplay,
                     nsISupportsWeakReference)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsTextInputSelectionImpl)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsTextInputSelectionImpl, mFrameSelection, mLimiter)


// BEGIN nsTextInputSelectionImpl

nsTextInputSelectionImpl::nsTextInputSelectionImpl(nsFrameSelection *aSel,
                                                   nsIPresShell *aShell,
                                                   nsIContent *aLimiter)
  : mScrollFrame(nullptr)
{
  if (aSel && aShell)
  {
    mFrameSelection = aSel;//we are the owner now!
    mLimiter = aLimiter;
    bool accessibleCaretEnabled =
      PresShell::AccessibleCaretEnabled(aLimiter->OwnerDoc()->GetDocShell());
    mFrameSelection->Init(aShell, mLimiter, accessibleCaretEnabled);
    mPresShellWeak = do_GetWeakReference(aShell);
  }
}

void
nsTextInputSelectionImpl::SetScrollableFrame(nsIScrollableFrame *aScrollableFrame)
{
  mScrollFrame = aScrollableFrame;
  if (!mScrollFrame && mFrameSelection) {
    mFrameSelection->DisconnectFromPresShell();
    mFrameSelection = nullptr;
  }
}

Selection*
nsTextInputSelectionImpl::GetSelection(SelectionType aSelectionType)
{
  if (!mFrameSelection) {
    return nullptr;
  }

  return mFrameSelection->GetSelection(aSelectionType);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetDisplaySelection(int16_t aToggle)
{
  if (!mFrameSelection)
    return NS_ERROR_NULL_POINTER;

  mFrameSelection->SetDisplaySelection(aToggle);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetDisplaySelection(int16_t *aToggle)
{
  if (!mFrameSelection)
    return NS_ERROR_NULL_POINTER;

  *aToggle = mFrameSelection->GetDisplaySelection();
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetSelectionFlags(int16_t aToggle)
{
  return NS_OK;//stub this out. not used in input
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetSelectionFlags(int16_t *aOutEnable)
{
  *aOutEnable = nsISelectionDisplay::DISPLAY_TEXT;
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetSelectionFromScript(RawSelectionType aRawSelectionType,
                                                 Selection** aSelection)
{
  if (!mFrameSelection)
    return NS_ERROR_NULL_POINTER;

  *aSelection =
    mFrameSelection->GetSelection(ToSelectionType(aRawSelectionType));

  // GetSelection() fails only when aRawSelectionType is invalid value.
  if (!(*aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF(*aSelection);
  return NS_OK;
}

Selection*
nsTextInputSelectionImpl::GetSelection(RawSelectionType aRawSelectionType)
{
  return GetSelection(ToSelectionType(aRawSelectionType));
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollSelectionIntoView(
                            RawSelectionType aRawSelectionType,
                            int16_t aRegion,
                            int16_t aFlags)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->ScrollSelectionIntoView(
                           ToSelectionType(aRawSelectionType),
                           aRegion, aFlags);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(RawSelectionType aRawSelectionType)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->RepaintSelection(ToSelectionType(aRawSelectionType));
}

nsresult
nsTextInputSelectionImpl::RepaintSelection(nsPresContext* aPresContext,
                                           SelectionType aSelectionType)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  return frameSelection->RepaintSelection(aSelectionType);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretEnabled(bool enabled)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak);
  if (!shell) return NS_ERROR_FAILURE;

  // tell the pres shell to enable the caret, rather than settings its visibility directly.
  // this way the presShell's idea of caret visibility is maintained.
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(shell);
  if (!selCon) return NS_ERROR_NO_INTERFACE;
  selCon->SetCaretEnabled(enabled);

  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretReadOnly(bool aReadOnly)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    RefPtr<nsCaret> caret = shell->GetCaret();
    if (caret) {
      Selection* selection =
        mFrameSelection->GetSelection(SelectionType::eNormal);
      if (selection) {
        caret->SetCaretReadOnly(aReadOnly);
      }
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetCaretEnabled(bool *_retval)
{
  return GetCaretVisible(_retval);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetCaretVisible(bool *_retval)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    RefPtr<nsCaret> caret = shell->GetCaret();
    if (caret) {
      *_retval = caret->IsVisible();
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretVisibilityDuringSelection(bool aVisibility)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    RefPtr<nsCaret> caret = shell->GetCaret();
    if (caret) {
      Selection* selection =
        mFrameSelection->GetSelection(SelectionType::eNormal);
      if (selection) {
        caret->SetVisibilityDuringSelection(aVisibility);
      }
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::PhysicalMove(int16_t aDirection, int16_t aAmount,
                                       bool aExtend)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->PhysicalMove(aDirection, aAmount, aExtend);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterMove(bool aForward, bool aExtend)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->CharacterMove(aForward, aExtend);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterExtendForDelete()
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->CharacterExtendForDelete();
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterExtendForBackspace()
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->CharacterExtendForBackspace();
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::WordMove(bool aForward, bool aExtend)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->WordMove(aForward, aExtend);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::WordExtendForDelete(bool aForward)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->WordExtendForDelete(aForward);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::LineMove(bool aForward, bool aExtend)
{
  if (mFrameSelection)
  {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    nsresult result = frameSelection->LineMove(aForward, aExtend);
    if (NS_FAILED(result))
      result = CompleteMove(aForward,aExtend);
    return result;
  }
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::IntraLineMove(bool aForward, bool aExtend)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->IntraLineMove(aForward, aExtend);
  }
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::PageMove(bool aForward, bool aExtend)
{
  // expected behavior for PageMove is to scroll AND move the caret
  // and to remain relative position of the caret in view. see Bug 4302.
  if (mScrollFrame)
  {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->CommonPageMove(aForward, aExtend, mScrollFrame);
  }
  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                 nsISelectionController::SELECTION_FOCUS_REGION,
                                 nsISelectionController::SCROLL_SYNCHRONOUS |
                                 nsISelectionController::SCROLL_FOR_CARET_MOVE);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteScroll(bool aForward)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                         nsIScrollableFrame::WHOLE,
                         nsIScrollableFrame::INSTANT);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteMove(bool aForward, bool aExtend)
{
  NS_ENSURE_STATE(mFrameSelection);
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;

  // grab the parent / root DIV for this text widget
  nsIContent* parentDIV = frameSelection->GetLimiter();
  if (!parentDIV)
    return NS_ERROR_UNEXPECTED;

  // make the caret be either at the very beginning (0) or the very end
  int32_t offset = 0;
  CaretAssociationHint hint = CARET_ASSOCIATE_BEFORE;
  if (aForward)
  {
    offset = parentDIV->GetChildCount();

    // Prevent the caret from being placed after the last
    // BR node in the content tree!

    if (offset > 0)
    {
      nsIContent *child = parentDIV->GetLastChild();

      if (child->IsHTMLElement(nsGkAtoms::br))
      {
        --offset;
        hint = CARET_ASSOCIATE_AFTER; // for Bug 106855
      }
    }
  }

  frameSelection->HandleClick(parentDIV, offset, offset, aExtend,
                               false, hint);

  // if we got this far, attempt to scroll no matter what the above result is
  return CompleteScroll(aForward);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollPage(bool aForward)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                         nsIScrollableFrame::PAGES,
                         nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollLine(bool aForward)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                         nsIScrollableFrame::LINES,
                         nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollCharacter(bool aRight)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(aRight ? 1 : -1, 0),
                         nsIScrollableFrame::LINES,
                         nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SelectAll()
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    return frameSelection->SelectAll();
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CheckVisibility(nsIDOMNode *node, int16_t startOffset, int16_t EndOffset, bool *_retval)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsISelectionController> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    return shell->CheckVisibility(node,startOffset,EndOffset, _retval);
  }
  return NS_ERROR_FAILURE;

}

nsresult
nsTextInputSelectionImpl::CheckVisibilityContent(nsIContent* aNode,
                                                 int16_t aStartOffset,
                                                 int16_t aEndOffset,
                                                 bool* aRetval)
{
  if (!mPresShellWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsISelectionController> shell = do_QueryReferent(mPresShellWeak);
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  return shell->CheckVisibilityContent(aNode, aStartOffset, aEndOffset, aRetval);
}

/*
 * mozilla::TextInputListener implementation
 */

TextInputListener::TextInputListener(nsITextControlElement* aTxtCtrlElement)
  : mFrame(nullptr)
  , mTxtCtrlElement(aTxtCtrlElement)
  , mSelectionWasCollapsed(true)
  , mHadUndoItems(false)
  , mHadRedoItems(false)
  , mSettingValue(false)
  , mSetValueChanged(true)
  , mListeningToSelectionChange(false)
{
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextInputListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextInputListener)

NS_INTERFACE_MAP_BEGIN(TextInputListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(TextInputListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_0(TextInputListener)

void
TextInputListener::OnSelectionChange(Selection& aSelection,
                                     int16_t aReason)
{
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
  //          create the event each time select all is called, even if everything
  //          was previously selected, becase technically select all will first collapse
  //          and then extend. Mozilla will never create an event if the selection
  //          collapses to nothing.
  bool collapsed = aSelection.IsCollapsed();
  if (!collapsed && (aReason & (nsISelectionListener::MOUSEUP_REASON |
                                nsISelectionListener::KEYPRESS_REASON |
                                nsISelectionListener::SELECTALL_REASON))) {
    nsIContent* content = mFrame->GetContent();
    if (content) {
      nsCOMPtr<nsIDocument> doc = content->GetComposedDoc();
      if (doc) {
        nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
        if (presShell) {
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

  UpdateTextInputCommands(NS_LITERAL_STRING("select"),
                          &aSelection, aReason);
}

static void
DoCommandCallback(Command aCommand, void* aData)
{
  nsTextControlFrame *frame = static_cast<nsTextControlFrame*>(aData);
  nsIContent *content = frame->GetContent();

  nsCOMPtr<nsIControllers> controllers;
  HTMLInputElement* input = HTMLInputElement::FromNode(content);
  if (input) {
    input->GetControllers(getter_AddRefs(controllers));
  } else {
    HTMLTextAreaElement* textArea =
      HTMLTextAreaElement::FromNode(content);

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
  nsresult rv = controller->IsCommandEnabled(commandStr, &commandEnabled);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (commandEnabled) {
    controller->DoCommand(commandStr);
  }
}

NS_IMETHODIMP
TextInputListener::HandleEvent(Event* aEvent)
{
  if (aEvent->DefaultPrevented()) {
    return NS_OK;
  }

  if (!aEvent->IsTrusted()) {
    return NS_OK;
  }

  WidgetKeyboardEvent* keyEvent =
    aEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (!keyEvent) {
    return NS_ERROR_UNEXPECTED;
  }

  if (keyEvent->mMessage != eKeyPress) {
    return NS_OK;
  }

  nsIWidget::NativeKeyBindingsType nativeKeyBindingsType =
    mTxtCtrlElement->IsTextArea() ?
      nsIWidget::NativeKeyBindingsForMultiLineEditor :
      nsIWidget::NativeKeyBindingsForSingleLineEditor;

  nsIWidget* widget = keyEvent->mWidget;
  // If the event is created by chrome script, the widget is nullptr.
  if (!widget) {
    widget = mFrame->GetNearestWidget();
    NS_ENSURE_TRUE(widget, NS_OK);
  }

  // WidgetKeyboardEvent::ExecuteEditCommands() requires non-nullptr mWidget.
  // If the event is created by chrome script, it is nullptr but we need to
  // execute native key bindings.  Therefore, we need to set widget to
  // WidgetEvent::mWidget temporarily.
  AutoRestore<nsCOMPtr<nsIWidget>> saveWidget(keyEvent->mWidget);
  keyEvent->mWidget = widget;
  if (keyEvent->ExecuteEditCommands(nativeKeyBindingsType,
                                    DoCommandCallback, mFrame)) {
    aEvent->PreventDefault();
  }
  return NS_OK;
}

void
TextInputListener::OnEditActionHandled()
{
  if (!mFrame) {
    // We've been disconnected from the nsTextEditorState object, nothing to do
    // here.
    return;
  }

  AutoWeakFrame weakFrame = mFrame;

  nsITextControlFrame* frameBase = do_QueryFrame(mFrame);
  nsTextControlFrame* frame = static_cast<nsTextControlFrame*> (frameBase);
  NS_ASSERTION(frame, "Where is our frame?");
  //
  // Update the undo / redo menus
  //
  RefPtr<TextEditor> textEditor = frame->GetTextEditor();

  // Get the number of undo / redo items
  size_t numUndoItems = textEditor->NumberOfUndoItems();
  size_t numRedoItems = textEditor->NumberOfRedoItems();
  if ((numUndoItems && !mHadUndoItems) || (!numUndoItems && mHadUndoItems) ||
      (numRedoItems && !mHadRedoItems) || (!numRedoItems && mHadRedoItems)) {
    // Modify the menu if undo or redo items are different
    UpdateTextInputCommands(NS_LITERAL_STRING("undo"));

    mHadUndoItems = numUndoItems != 0;
    mHadRedoItems = numRedoItems != 0;
  }

  if (!weakFrame.IsAlive()) {
    return;
  }

  HandleValueChanged(frame);
}

void
TextInputListener::HandleValueChanged(nsTextControlFrame* aFrame)
{
  // Make sure we know we were changed (do NOT set this to false if there are
  // no undo items; JS could change the value and we'd still need to save it)
  if (mSetValueChanged) {
    if (!aFrame) {
      nsITextControlFrame* frameBase = do_QueryFrame(mFrame);
      aFrame = static_cast<nsTextControlFrame*> (frameBase);
      NS_ASSERTION(aFrame, "Where is our frame?");
    }
    aFrame->SetValueChanged(true);
  }

  if (!mSettingValue) {
    mTxtCtrlElement->OnValueChanged(/* aNotify = */ true,
                                    /* aWasInteractiveUserChange = */ true);
  }
}

nsresult
TextInputListener::UpdateTextInputCommands(const nsAString& aCommandsToUpdate,
                                           Selection* aSelection,
                                           int16_t aReason)
{
  nsIContent* content = mFrame->GetContent();
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = content->GetComposedDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsPIDOMWindowOuter* domWindow = doc->GetWindow();
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  domWindow->UpdateCommands(aCommandsToUpdate, aSelection, aReason);
  return NS_OK;
}

// END mozilla::TextInputListener

// nsTextEditorState

nsTextEditorState::nsTextEditorState(nsITextControlElement* aOwningElement)
  : mTextCtrlElement(aOwningElement)
  , mBoundFrame(nullptr)
  , mEverInited(false)
  , mEditorInitialized(false)
  , mInitializing(false)
  , mValueTransferInProgress(false)
  , mSelectionCached(true)
  , mSelectionRestoreEagerInit(false)
  , mPlaceholderVisibility(false)
  , mPreviewVisibility(false)
  , mIsCommittingComposition(false)
  // When adding more member variable initializations here, add the same
  // also to ::Construct.
{
  MOZ_COUNT_CTOR(nsTextEditorState);
}

nsTextEditorState*
nsTextEditorState::Construct(nsITextControlElement* aOwningElement,
                             nsTextEditorState** aReusedState)
{
  if (*aReusedState) {
    nsTextEditorState* state = *aReusedState;
    *aReusedState = nullptr;
    state->mTextCtrlElement = aOwningElement;
    state->mBoundFrame = nullptr;
    state->mSelectionProperties = SelectionProperties();
    state->mEverInited = false;
    state->mEditorInitialized = false;
    state->mInitializing = false;
    state->mValueTransferInProgress = false;
    state->mSelectionCached = true;
    state->mSelectionRestoreEagerInit = false;
    state->mPlaceholderVisibility = false;
    state->mPreviewVisibility = false;
    state->mIsCommittingComposition = false;
    // When adding more member variable initializations here, add the same
    // also to the constructor.
    return state;
  }

  return new nsTextEditorState(aOwningElement);
}

nsTextEditorState::~nsTextEditorState()
{
  MOZ_COUNT_DTOR(nsTextEditorState);
  Clear();
}

Element*
nsTextEditorState::GetRootNode()
{
  return mBoundFrame ? mBoundFrame->GetRootNode() : nullptr;
}

Element*
nsTextEditorState::GetPreviewNode()
{
  return mBoundFrame ? mBoundFrame->GetPreviewNode() : nullptr;
}

void
nsTextEditorState::Clear()
{
  if (mTextEditor) {
    mTextEditor->SetTextInputListener(nullptr);
  }

  if (mBoundFrame) {
    // Oops, we still have a frame!
    // This should happen when the type of a text input control is being changed
    // to something which is not a text control.  In this case, we should pretend
    // that a frame is being destroyed, and clean up after ourselves properly.
    UnbindFromFrame(mBoundFrame);
    mTextEditor = nullptr;
  } else {
    // If we have a bound frame around, UnbindFromFrame will call DestroyEditor
    // for us.
    DestroyEditor();
  }
  mTextListener = nullptr;
}

void nsTextEditorState::Unlink()
{
  nsTextEditorState* tmp = this;
  tmp->Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelCon)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTextEditor)
}

void nsTextEditorState::Traverse(nsCycleCollectionTraversalCallback& cb)
{
  nsTextEditorState* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelCon)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTextEditor)
}

nsFrameSelection*
nsTextEditorState::GetConstFrameSelection() {
  if (mSelCon)
    return mSelCon->GetConstFrameSelection();
  return nullptr;
}

TextEditor*
nsTextEditorState::GetTextEditor()
{
  if (!mTextEditor) {
    nsresult rv = PrepareEditor();
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  return mTextEditor;
}

nsISelectionController*
nsTextEditorState::GetSelectionController() const
{
  return mSelCon;
}

// Helper class, used below in BindToFrame().
class PrepareEditorEvent : public Runnable {
public:
  PrepareEditorEvent(nsTextEditorState& aState,
                     nsIContent* aOwnerContent,
                     const nsAString& aCurrentValue)
    : mozilla::Runnable("PrepareEditorEvent")
    , mState(&aState)
    , mOwnerContent(aOwnerContent)
    , mCurrentValue(aCurrentValue)
  {
    aState.mValueTransferInProgress = true;
  }

  NS_IMETHOD Run() override {
    NS_ENSURE_TRUE(mState, NS_ERROR_NULL_POINTER);

    // Transfer the saved value to the editor if we have one
    const nsAString *value = nullptr;
    if (!mCurrentValue.IsEmpty()) {
      value = &mCurrentValue;
    }

    nsAutoScriptBlocker scriptBlocker;

    mState->PrepareEditor(value);

    mState->mValueTransferInProgress = false;

    return NS_OK;
  }

private:
  WeakPtr<nsTextEditorState> mState;
  nsCOMPtr<nsIContent> mOwnerContent; // strong reference
  nsAutoString mCurrentValue;
};

nsresult
nsTextEditorState::BindToFrame(nsTextControlFrame* aFrame)
{
  NS_ASSERTION(aFrame, "The frame to bind to should be valid");
  NS_ENSURE_ARG_POINTER(aFrame);

  NS_ASSERTION(!mBoundFrame, "Cannot bind twice, need to unbind first");
  NS_ENSURE_TRUE(!mBoundFrame, NS_ERROR_FAILURE);

  // If we'll need to transfer our current value to the editor, save it before
  // binding to the frame.
  nsAutoString currentValue;
  if (mTextEditor) {
    GetValue(currentValue, true);
  }

  mBoundFrame = aFrame;

  Element* rootNode = aFrame->GetRootNode();
  MOZ_ASSERT(rootNode);

  nsIPresShell* shell = aFrame->PresContext()->GetPresShell();
  MOZ_ASSERT(shell);

  // Create selection
  RefPtr<nsFrameSelection> frameSel = new nsFrameSelection();

  // Create a SelectionController
  mSelCon = new nsTextInputSelectionImpl(frameSel, shell, rootNode);
  MOZ_ASSERT(!mTextListener, "Should not overwrite the object");
  mTextListener = new TextInputListener(mTextCtrlElement);

  mTextListener->SetFrame(mBoundFrame);
  mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  // Get the caret and make it a selection listener.
  // FYI: It's safe to use raw pointer for calling
  //      Selection::AddSelectionListner() because it only appends the listener
  //      to its internal array.
  Selection* selection = mSelCon->GetSelection(SelectionType::eNormal);
  if (selection) {
    RefPtr<nsCaret> caret = shell->GetCaret();
    if (caret) {
      selection->AddSelectionListener(caret);
    }
    mTextListener->StartToListenToSelectionChange();
  }

  // If an editor exists from before, prepare it for usage
  if (mTextEditor) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

    // Set the correct direction on the newly created root node
    if (mTextEditor->IsRightToLeft()) {
      rootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), false);
    } else if (mTextEditor->IsLeftToRight()) {
      rootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), false);
    } else {
      // otherwise, inherit the content node's direction
    }

    nsContentUtils::AddScriptRunner(
      new PrepareEditorEvent(*this, content, currentValue));
  }

  return NS_OK;
}

struct PreDestroyer
{
  void Init(TextEditor* aTextEditor) { mTextEditor = aTextEditor; }
  ~PreDestroyer()
  {
    if (mTextEditor) {
      mTextEditor->PreDestroy(true);
    }
  }
  void Swap(RefPtr<TextEditor>& aTextEditor)
  {
    return mTextEditor.swap(aTextEditor);
  }
private:
  RefPtr<TextEditor> mTextEditor;
};

nsresult
nsTextEditorState::PrepareEditor(const nsAString *aValue)
{
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

  // Don't attempt to initialize recursively!
  InitializationGuard guard(*this);
  if (guard.IsInitializingRecursively()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Note that we don't check mTextEditor here, because we might already have
  // one around, in which case we don't create a new one, and we'll just tie
  // the required machinery to it.

  nsPresContext *presContext = mBoundFrame->PresContext();
  nsIPresShell *shell = presContext->GetPresShell();

  // Setup the editor flags
  uint32_t editorFlags = nsIPlaintextEditor::eEditorPlaintextMask;
  if (IsSingleLineTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorSingleLineMask;
  if (IsPasswordTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorPasswordMask;

  // All nsTextControlFrames are widgets
  editorFlags |= nsIPlaintextEditor::eEditorWidgetMask;

  // Spell check is diabled at creation time. It is enabled once
  // the editor comes into focus.
  editorFlags |= nsIPlaintextEditor::eEditorSkipSpellCheck;

  bool shouldInitializeEditor = false;
  RefPtr<TextEditor> newTextEditor; // the editor that we might create
  nsresult rv = NS_OK;
  PreDestroyer preDestroyer;
  if (!mTextEditor) {
    shouldInitializeEditor = true;

    // Create an editor
    newTextEditor = new TextEditor();
    preDestroyer.Init(newTextEditor);

    // Make sure we clear out the non-breaking space before we initialize the editor
    rv = mBoundFrame->UpdateValueDisplay(true, true);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    if (aValue || !mEditorInitialized) {
      // Set the correct value in the root node
      rv = mBoundFrame->UpdateValueDisplay(true, !mEditorInitialized, aValue);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    newTextEditor = mTextEditor; // just pretend that we have a new editor!

    // Don't lose application flags in the process.
    if (newTextEditor->IsMailEditor()) {
      editorFlags |= nsIPlaintextEditor::eEditorMailMask;
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
    nsCOMPtr<nsIDocument> doc = shell->GetDocument();
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

    rv = newTextEditor->Init(*doc, GetRootNode(), mSelCon, editorFlags,
                             defaultValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Initialize the controller for the editor

  if (!SuppressEventHandlers(presContext)) {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    HTMLInputElement* inputElement =
      HTMLInputElement::FromNodeOrNull(content);
    if (inputElement) {
      rv = inputElement->GetControllers(getter_AddRefs(controllers));
    } else {
      HTMLTextAreaElement* textAreaElement =
        HTMLTextAreaElement::FromNodeOrNull(content);

      if (!textAreaElement)
        return NS_ERROR_FAILURE;

      rv = textAreaElement->GetControllers(getter_AddRefs(controllers));
    }

    NS_ENSURE_SUCCESS(rv, rv);

    if (controllers) {
      uint32_t numControllers;
      bool found = false;
      rv = controllers->GetControllerCount(&numControllers);
      for (uint32_t i = 0; i < numControllers; i ++) {
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
      if (!found)
        rv = NS_ERROR_FAILURE;
    }
  }

  // Initialize the plaintext editor
  if (shouldInitializeEditor) {
    // Set up wrapping
    newTextEditor->SetWrapColumn(GetWrapCols());
  }

  // Set max text field length
  newTextEditor->SetMaxTextLength(GetMaxLength());

  if (nsCOMPtr<Element> element = do_QueryInterface(mTextCtrlElement)) {
    editorFlags = newTextEditor->Flags();

    // Check if the readonly attribute is set.
    if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly))
      editorFlags |= nsIPlaintextEditor::eEditorReadonlyMask;

    // Check if the disabled attribute is set.
    // TODO: call IsDisabled() here!
    if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled))
      editorFlags |= nsIPlaintextEditor::eEditorDisabledMask;

    // Disable the selection if necessary.
    if (newTextEditor->IsDisabled()) {
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
    }

    newTextEditor->SetFlags(editorFlags);
  }

  if (shouldInitializeEditor) {
    // Hold on to the newly created editor
    preDestroyer.Swap(mTextEditor);
  }

  // If we have a default value, insert it under the div we created
  // above, but be sure to use the editor so that '*' characters get
  // displayed for password fields, etc. SetValue() will call the
  // editor for us.

  if (!defaultValue.IsEmpty()) {
    rv = newTextEditor->SetFlags(editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now call SetValue() which will make the necessary editor calls to set
    // the default value.  Make sure to turn off undo before setting the default
    // value, and turn it back on afterwards. This will make sure we can't undo
    // past the default value.
    // So, we use eSetValue_Internal flag only that it will turn off undo.

    bool success = SetValue(defaultValue, eSetValue_Internal);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

    // Now restore the original editor flags.
    rv = newTextEditor->SetFlags(editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);
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
      newTextEditor->EnableUndoRedo(nsITextControlElement::DEFAULT_UNDO_CAP);
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
  HTMLInputElement* number = GetParentNumberControl(mBoundFrame);
  if (number ? number->IsSelectionCached() : mSelectionCached) {
    if (mRestoringSelection) // paranoia
      mRestoringSelection->Revoke();
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
  if (number) {
    number->ClearSelectionCached();
  } else {
    mSelectionCached = false;
  }

  return rv;
}

void
nsTextEditorState::FinishedRestoringSelection()
{
  mRestoringSelection = nullptr;
}

bool
nsTextEditorState::IsSelectionCached() const
{
  if (mBoundFrame) {
    HTMLInputElement* number = GetParentNumberControl(mBoundFrame);
    if (number) {
      return number->IsSelectionCached();
    }
  }
  return mSelectionCached;
}

nsTextEditorState::SelectionProperties&
nsTextEditorState::GetSelectionProperties()
{
  if (mBoundFrame) {
    HTMLInputElement* number = GetParentNumberControl(mBoundFrame);
    if (number) {
      return number->GetSelectionProperties();
    }
  }
  return mSelectionProperties;
}

void
nsTextEditorState::SyncUpSelectionPropertiesBeforeDestruction()
{
  if (mBoundFrame) {
    UnbindFromFrame(mBoundFrame);
  }
}

void
nsTextEditorState::SetSelectionProperties(nsTextEditorState::SelectionProperties& aProps)
{
  if (mBoundFrame) {
    mBoundFrame->SetSelectionRange(aProps.GetStart(),
                                   aProps.GetEnd(),
                                   aProps.GetDirection());
  } else {
    mSelectionProperties = aProps;
  }
}

void
nsTextEditorState::GetSelectionRange(uint32_t* aSelectionStart,
                                     uint32_t* aSelectionEnd,
                                     ErrorResult& aRv)
{
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

  mozilla::dom::Element* root = GetRootNode();
  if (NS_WARN_IF(!root)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  nsContentUtils::GetSelectionInTextControl(sel, root,
                                            *aSelectionStart, *aSelectionEnd);
}

nsITextControlFrame::SelectionDirection
nsTextEditorState::GetSelectionDirection(ErrorResult& aRv)
{
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
    return nsITextControlFrame::eForward; // Doesn't really matter
  }

  nsDirection direction = sel->GetDirection();
  if (direction == eDirNext) {
    return nsITextControlFrame::eForward;
  }

  MOZ_ASSERT(direction == eDirPrevious);
  return nsITextControlFrame::eBackward;
}

void
nsTextEditorState::SetSelectionRange(uint32_t aStart, uint32_t aEnd,
                                     nsITextControlFrame::SelectionDirection aDirection,
                                     ErrorResult& aRv)
{
  MOZ_ASSERT(IsSelectionCached() || mBoundFrame,
             "How can we have a non-cached selection but no frame?");

  if (aStart > aEnd) {
    aStart = aEnd;
  }

  bool changed = false;
  nsresult rv = NS_OK; // For the ScrollSelectionIntoView() return value.
  if (IsSelectionCached()) {
    nsAutoString value;
    // XXXbz is "false" the right thing to pass here?  Hard to tell, given the
    // various mismatches between our impl and the spec.
    GetValue(value, false);
    uint32_t length = value.Length();
    if (aStart > length) {
      aStart = length;
    }
    if (aEnd > length) {
      aEnd = length;
    }
    SelectionProperties& props = GetSelectionProperties();
    changed = props.GetStart() != aStart ||
              props.GetEnd() != aEnd ||
              props.GetDirection() != aDirection;
    props.SetStart(aStart);
    props.SetEnd(aEnd);
    props.SetDirection(aDirection);
  } else {
    WeakPtr<nsTextEditorState> self(this);
    aRv = mBoundFrame->SetSelectionRange(aStart, aEnd, aDirection);
    if (aRv.Failed() || !self.get()) {
      return;
    }
    rv = mBoundFrame->ScrollSelectionIntoView();
    // Press on to firing the event even if that failed, like our old code did.
    // But is that really what we want?  Firing the event _and_ throwing from
    // here is weird.  Maybe we should just ignore ScrollSelectionIntoView
    // failures?

    // XXXbz This is preserving our current behavior of firing a "select" event
    // on all mutations when we have an editor, but we should really consider
    // fixing that...
    changed = true;
  }

  if (changed) {
    // It sure would be nice if we had an existing Element* or so to work with.
    nsCOMPtr<nsINode> node = do_QueryInterface(mTextCtrlElement);
    RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(node, NS_LITERAL_STRING("select"), true, false);
    asyncDispatcher->PostDOMEvent();
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
nsTextEditorState::SetSelectionStart(const Nullable<uint32_t>& aStart,
                                     ErrorResult& aRv)
{
  uint32_t start = 0;
  if (!aStart.IsNull()) {
    start = aStart.Value();
  }

  uint32_t ignored, end;
  GetSelectionRange(&ignored, &end, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsITextControlFrame::SelectionDirection dir = GetSelectionDirection(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (end < start) {
    end = start;
  }

  SetSelectionRange(start, end, dir, aRv);
}

void
nsTextEditorState::SetSelectionEnd(const Nullable<uint32_t>& aEnd,
                                   ErrorResult& aRv)
{
  uint32_t end = 0;
  if (!aEnd.IsNull()) {
    end = aEnd.Value();
  }

  uint32_t start, ignored;
  GetSelectionRange(&start, &ignored, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsITextControlFrame::SelectionDirection dir = GetSelectionDirection(aRv);
  if (aRv.Failed()) {
    return;
  }

  SetSelectionRange(start, end, dir, aRv);
}

static void
DirectionToName(nsITextControlFrame::SelectionDirection dir, nsAString& aDirection)
{
  if (dir == nsITextControlFrame::eNone) {
    NS_WARNING("We don't actually support this... how did we get it?");
    aDirection.AssignLiteral("none");
  } else if (dir == nsITextControlFrame::eForward) {
    aDirection.AssignLiteral("forward");
  } else if (dir == nsITextControlFrame::eBackward) {
    aDirection.AssignLiteral("backward");
  } else {
    NS_NOTREACHED("Invalid SelectionDirection value");
  }
}

void
nsTextEditorState::GetSelectionDirectionString(nsAString& aDirection,
                                               ErrorResult& aRv)
{
  nsITextControlFrame::SelectionDirection dir = GetSelectionDirection(aRv);
  if (aRv.Failed()) {
    return;
  }
  DirectionToName(dir, aDirection);
}

static nsITextControlFrame::SelectionDirection
DirectionStringToSelectionDirection(const nsAString& aDirection)
{
  if (aDirection.EqualsLiteral("backward")) {
    return nsITextControlFrame::eBackward;
  }

  // We don't support directionless selections.
  return nsITextControlFrame::eForward;
}

void
nsTextEditorState::SetSelectionDirection(const nsAString& aDirection,
                                         ErrorResult& aRv)
{
  nsITextControlFrame::SelectionDirection dir =
    DirectionStringToSelectionDirection(aDirection);

  if (IsSelectionCached()) {
    GetSelectionProperties().SetDirection(dir);
    return;
  }

  uint32_t start, end;
  GetSelectionRange(&start, &end, aRv);
  if (aRv.Failed()) {
    return;
  }

  SetSelectionRange(start, end, dir, aRv);
}

static nsITextControlFrame::SelectionDirection
DirectionStringToSelectionDirection(const Optional<nsAString>& aDirection)
{
  if (!aDirection.WasPassed()) {
    // We don't support directionless selections.
    return nsITextControlFrame::eForward;
  }

  return DirectionStringToSelectionDirection(aDirection.Value());
}

void
nsTextEditorState::SetSelectionRange(uint32_t aSelectionStart,
                                     uint32_t aSelectionEnd,
                                     const Optional<nsAString>& aDirection,
                                     ErrorResult& aRv)
{
  nsITextControlFrame::SelectionDirection dir =
    DirectionStringToSelectionDirection(aDirection);

  SetSelectionRange(aSelectionStart, aSelectionEnd, dir, aRv);
}

void
nsTextEditorState::SetRangeText(const nsAString& aReplacement,
                                ErrorResult& aRv)
{
  uint32_t start, end;
  GetSelectionRange(&start, &end, aRv);
  if (aRv.Failed()) {
    return;
  }

  SetRangeText(aReplacement, start, end, SelectionMode::Preserve,
               aRv, Some(start), Some(end));
}

void
nsTextEditorState::SetRangeText(const nsAString& aReplacement, uint32_t aStart,
                                uint32_t aEnd, SelectionMode aSelectMode,
                                ErrorResult& aRv,
                                const Maybe<uint32_t>& aSelectionStart,
                                const Maybe<uint32_t>& aSelectionEnd)
{
  if (aStart > aEnd) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

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

  MOZ_ASSERT(aStart <= aEnd);
  value.Replace(aStart, aEnd - aStart, aReplacement);
  nsresult rv = mTextCtrlElement->SetValueFromSetRangeText(value);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  uint32_t newEnd = aStart + aReplacement.Length();
  int32_t delta =  aReplacement.Length() - (aEnd - aStart);

  switch (aSelectMode) {
    case mozilla::dom::SelectionMode::Select:
    {
      selectionStart = aStart;
      selectionEnd = newEnd;
    }
    break;
    case mozilla::dom::SelectionMode::Start:
    {
      selectionStart = selectionEnd = aStart;
    }
    break;
    case mozilla::dom::SelectionMode::End:
    {
      selectionStart = selectionEnd = newEnd;
    }
    break;
    case mozilla::dom::SelectionMode::Preserve:
    {
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
    }
    break;
    default:
      MOZ_CRASH("Unknown mode!");
  }

  SetSelectionRange(selectionStart, selectionEnd, Optional<nsAString>(), aRv);
}

HTMLInputElement*
nsTextEditorState::GetParentNumberControl(nsFrame* aFrame) const
{
  MOZ_ASSERT(aFrame);
  nsIContent* content = aFrame->GetContent();
  MOZ_ASSERT(content);
  nsIContent* parent = content->GetParent();
  if (!parent) {
    return nullptr;
  }
  nsIContent* parentOfParent = parent->GetParent();
  if (!parentOfParent) {
    return nullptr;
  }
  HTMLInputElement* input = HTMLInputElement::FromNode(parentOfParent);
  if (input) {
    // This function might be called during frame reconstruction as a result
    // of changing the input control's type from number to something else. In
    // that situation, the type of the control has changed, but its frame has
    // not been reconstructed yet.  So we need to check the type of the input
    // control in addition to the type of the frame.
    return (input->ControlType() == NS_FORM_INPUT_NUMBER) ? input : nullptr;
  }

  return nullptr;
}

void
nsTextEditorState::DestroyEditor()
{
  // notify the editor that we are going away
  if (mEditorInitialized) {
    mTextEditor->PreDestroy(true);
    mEditorInitialized = false;
  }
}

void
nsTextEditorState::UnbindFromFrame(nsTextControlFrame* aFrame)
{
  NS_ENSURE_TRUE_VOID(mBoundFrame);

  // If it was, however, it should be unbounded from the same frame.
  MOZ_ASSERT(aFrame == mBoundFrame, "Unbinding from the wrong frame");
  NS_ENSURE_TRUE_VOID(!aFrame || aFrame == mBoundFrame);

  // If the editor is modified but nsIEditorObserver::EditAction() hasn't been
  // called yet, we need to notify it here because editor may be destroyed
  // before EditAction() is called if selection listener causes flushing layout.
  if (mTextListener && mTextEditor && mEditorInitialized &&
      mTextEditor->IsInEditAction()) {
    mTextListener->OnEditActionHandled();
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
    props.SetStart(start);
    props.SetEnd(end);
    props.SetDirection(direction);
    HTMLInputElement* number = GetParentNumberControl(aFrame);
    if (number) {
      // If we are inside a number control, cache the selection on the
      // parent control, because this text editor state will be destroyed
      // together with the native anonymous text control.
      number->SetSelectionCached();
    } else {
      mSelectionCached = true;
    }
  }

  // Destroy our editor
  DestroyEditor();

  // Clean up the controller
  if (!SuppressEventHandlers(mBoundFrame->PresContext()))
  {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    HTMLInputElement* inputElement =
      HTMLInputElement::FromNodeOrNull(content);
    if (inputElement)
      inputElement->GetControllers(getter_AddRefs(controllers));
    else
    {
      HTMLTextAreaElement* textAreaElement =
        HTMLTextAreaElement::FromNodeOrNull(content);
      if (textAreaElement) {
        textAreaElement->GetControllers(getter_AddRefs(controllers));
      }
    }

    if (controllers)
    {
      uint32_t numControllers;
      nsresult rv = controllers->GetControllerCount(&numControllers);
      NS_ASSERTION((NS_SUCCEEDED(rv)), "bad result in gfx text control destructor");
      for (uint32_t i = 0; i < numControllers; i ++)
      {
        nsCOMPtr<nsIController> controller;
        rv = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(rv) && controller)
        {
          nsCOMPtr<nsIControllerContext> editController = do_QueryInterface(controller);
          if (editController)
          {
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

  if (mTextListener)
  {
    mTextListener->SetFrame(nullptr);

    nsCOMPtr<EventTarget> target = do_QueryInterface(mTextCtrlElement);
    EventListenerManager* manager = target->GetExistingListenerManager();
    if (manager) {
      manager->RemoveEventListenerByType(mTextListener,
        NS_LITERAL_STRING("keydown"),
        TrustedEventsAtSystemGroupBubble());
      manager->RemoveEventListenerByType(mTextListener,
        NS_LITERAL_STRING("keypress"),
        TrustedEventsAtSystemGroupBubble());
      manager->RemoveEventListenerByType(mTextListener,
        NS_LITERAL_STRING("keyup"),
        TrustedEventsAtSystemGroupBubble());
    }

    mTextListener = nullptr;
  }

  mBoundFrame = nullptr;

  // Now that we don't have a frame any more, store the value in the text buffer.
  // The only case where we don't do this is if a value transfer is in progress.
  if (!mValueTransferInProgress) {
    bool success = SetValue(value, eSetValue_Internal);
    // TODO Find something better to do if this fails...
    NS_ENSURE_TRUE_VOID(success);
  }
}

int32_t
nsTextEditorState::GetMaxLength()
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  nsGenericHTMLElement* element =
    nsGenericHTMLElement::FromNodeOrNull(content);
  if (NS_WARN_IF(!element)) {
    return -1;
  }

  const nsAttrValue* attr = element->GetParsedAttr(nsGkAtoms::maxlength);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    return attr->GetIntegerValue();
  }

  return -1;
}

void
nsTextEditorState::GetValue(nsAString& aValue, bool aIgnoreWrap) const
{
  // While SetValue() is being called and requesting to commit composition to
  // IME, GetValue() may be called for appending text or something.  Then, we
  // need to return the latest aValue of SetValue() since the value hasn't
  // been set to the editor yet.
  if (mIsCommittingComposition) {
    aValue = mValueBeingSet;
    return;
  }

  if (mTextEditor && mBoundFrame &&
      (mEditorInitialized || !IsSingleLineTextControl())) {
    if (aIgnoreWrap && !mBoundFrame->CachedValue().IsVoid()) {
      aValue = mBoundFrame->CachedValue();
      return;
    }

    aValue.Truncate(); // initialize out param

    uint32_t flags = (nsIDocumentEncoder::OutputLFLineBreak |
                      nsIDocumentEncoder::OutputPreformatted |
                      nsIDocumentEncoder::OutputPersistNBSP |
                      nsIDocumentEncoder::OutputBodyOnly);
    if (!aIgnoreWrap) {
      nsITextControlElement::nsHTMLTextWrap wrapProp;
      nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
      if (content &&
          nsITextControlElement::GetWrapPropertyEnum(content, wrapProp) &&
          wrapProp == nsITextControlElement::eHTMLTextWrap_Hard) {
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

      mTextEditor->OutputToString(NS_LITERAL_STRING("text/plain"), flags,
                                  aValue);
    }
    // Only when the result doesn't include line breaks caused by hard-wrap,
    // mCacheValue should cache the value.
    if (!(flags & nsIDocumentEncoder::OutputWrap)) {
      mBoundFrame->CacheValue(aValue);
    } else {
      mBoundFrame->ClearCachedValue();
    }
  } else {
    if (!mTextCtrlElement->ValueChanged() || !mValue) {
      mTextCtrlElement->GetDefaultValueFromContent(aValue);
    } else {
      aValue = *mValue;
    }
  }
}

bool
nsTextEditorState::SetValue(const nsAString& aValue, const nsAString* aOldValue,
                            uint32_t aFlags)
{
  nsAutoString newValue(aValue);

  // While mIsCommittingComposition is true (that means that some event
  // handlers which are fired during committing composition are the caller of
  // this method), GetValue() uses mValueBeingSet for its result because the
  // first calls of this methods hasn't set the value yet.  So, when it's true,
  // we need to modify mValueBeingSet.  In this case, we will back to the first
  // call of this method, then, mValueBeingSet will be truncated when
  // mIsCommittingComposition is set false.  See below.
  if (mIsCommittingComposition) {
    mValueBeingSet = aValue;
    // GetValue doesn't return current text frame's content during committing.
    // So we cannot trust this old value
    aOldValue = nullptr;
  }

  // Note that if this may be called during reframe of the editor.  In such
  // case, we shouldn't commit composition.  Therefore, when this is called
  // for internal processing, we shouldn't commit the composition.
  if (aFlags & (eSetValue_BySetUserInput | eSetValue_ByContent)) {
    if (EditorHasComposition()) {
      // When this is called recursively, there shouldn't be composition.
      if (NS_WARN_IF(mIsCommittingComposition)) {
        // Don't request to commit composition again.  But if it occurs,
        // we should skip to set the new value to the editor here.  It should
        // be set later with the updated mValueBeingSet.
        return true;
      }
      if (NS_WARN_IF(!mBoundFrame)) {
        // We're not sure if this case is possible.
      } else {
        // If setting value won't change current value, we shouldn't commit
        // composition for compatibility with the other browsers.
        nsAutoString currentValue;
        if (aOldValue) {
#ifdef DEBUG
          mBoundFrame->GetText(currentValue);
          MOZ_ASSERT(currentValue.Equals(*aOldValue));
#endif
          currentValue.Assign(*aOldValue);
        } else {
          mBoundFrame->GetText(currentValue);
        }
        if (newValue == currentValue) {
          // Note that in this case, we shouldn't fire any events with setting
          // value because event handlers may try to set value recursively but
          // we cannot commit composition at that time due to unsafe to run
          // script (see below).
          return true;
        }
        // IME might commit composition, then change value, so we cannot
        // trust old value from parameter.
        aOldValue = nullptr;
      }
      // If there is composition, need to commit composition first because
      // other browsers do that.
      // NOTE: We don't need to block nested calls of this because input nor
      //       other events won't be fired by setting values and script blocker
      //       is used during setting the value to the editor.  IE also allows
      //       to set the editor value on the input event which is caused by
      //       forcibly committing composition.
      if (nsContentUtils::IsSafeToRunScript()) {
        WeakPtr<nsTextEditorState> self(this);
        // WARNING: During this call, compositionupdate, compositionend, input
        // events will be fired.  Therefore, everything can occur.  E.g., the
        // document may be unloaded.
        mValueBeingSet = aValue;
        mIsCommittingComposition = true;
        RefPtr<TextEditor> textEditor = mTextEditor;
        nsresult rv = textEditor->CommitComposition();
        if (!self.get()) {
          return true;
        }
        mIsCommittingComposition = false;
        // If this is called recursively during committing composition and
        // some of them may be skipped above.  Therefore, we need to set
        // value to the editor with the aValue of the latest call.
        newValue = mValueBeingSet;
        // When mIsCommittingComposition is false, mValueBeingSet won't be
        // used.  Therefore, let's clear it.
        mValueBeingSet.Truncate();
        if (NS_FAILED(rv)) {
          NS_WARNING("nsTextEditorState failed to commit composition");
          return true;
        }
      } else {
        NS_WARNING("SetValue() is called when there is composition but "
                   "it's not safe to request to commit the composition");
      }
    }
  }

  // \r is an illegal character in the dom, but people use them,
  // so convert windows and mac platform linebreaks to \n:
  if (!nsContentUtils::PlatformToDOMLineBreaks(newValue, fallible)) {
    return false;
  }

  if (mTextEditor && mBoundFrame) {
    // The InsertText call below might flush pending notifications, which
    // could lead into a scheduled PrepareEditor to be called.  That will
    // lead to crashes (or worse) because we'd be initializing the editor
    // before InsertText returns.  This script blocker makes sure that
    // PrepareEditor cannot be called prematurely.
    nsAutoScriptBlocker scriptBlocker;

#ifdef DEBUG
    if (IsSingleLineTextControl()) {
      NS_ASSERTION(mEditorInitialized || mInitializing,
                   "We should never try to use the editor if we're not initialized unless we're being initialized");
    }
#endif

    nsAutoString currentValue;
    if (aOldValue) {
#ifdef DEBUG
      mBoundFrame->GetText(currentValue);
      MOZ_ASSERT(currentValue.Equals(*aOldValue));
#endif
      currentValue.Assign(*aOldValue);
    } else {
      mBoundFrame->GetText(currentValue);
    }

    AutoWeakFrame weakFrame(mBoundFrame);

    // this is necessary to avoid infinite recursion
    if (!currentValue.Equals(newValue)) {
      RefPtr<TextEditor> textEditor = mTextEditor;
      ValueSetter valueSetter(textEditor);

      nsCOMPtr<nsIDocument> document = textEditor->GetDocument();
      if (NS_WARN_IF(!document)) {
        return true;
      }

      // Time to mess with our security context... See comments in GetValue()
      // for why this is needed.  Note that we have to do this up here, because
      // otherwise SelectAll() will fail.
      {
        AutoNoJSAPI nojsapi;

        // FYI: It's safe to use raw pointer for selection here because
        //      SelectionBatcher will grab it with RefPtr.
        Selection* selection =
          mSelCon->GetSelection(SelectionType::eNormal);
        SelectionBatcher selectionBatcher(selection);

        if (NS_WARN_IF(!weakFrame.IsAlive())) {
          return true;
        }

        valueSetter.Init();

        // get the flags, remove readonly, disabled and max-length,
        // set the value, restore flags
        {
          AutoRestoreEditorState restoreState(textEditor);

          mTextListener->SettingValue(true);
          bool notifyValueChanged = !!(aFlags & eSetValue_Notify);
          mTextListener->SetValueChanged(notifyValueChanged);

          // We preserve the undo history if we are explicitly setting the
          // value for the user's input, or if we are setting the value for a
          // XUL text control.
          if (aFlags & (eSetValue_BySetUserInput | eSetValue_ForXUL)) {
            nsCOMPtr<nsISelectionController> kungFuDeathGrip = mSelCon.get();
            uint32_t currentLength = currentValue.Length();
            uint32_t newlength = newValue.Length();
            if (!currentLength ||
                !StringBeginsWith(newValue, currentValue)) {
              // Replace the whole text.
              currentLength = 0;
              kungFuDeathGrip->SelectAll();
            } else {
              // Collapse selection to the end so that we can append data.
              mBoundFrame->SelectAllOrCollapseToEndOfText(false);
            }
            const nsAString& insertValue =
              StringTail(newValue, newlength - currentLength);

            if (insertValue.IsEmpty()) {
              DebugOnly<nsresult> rv =
                textEditor->DeleteSelectionAsAction(nsIEditor::eNone,
                                                    nsIEditor::eStrip);
              NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                "Failed to remove the text");
            } else {
              DebugOnly<nsresult> rv =
                textEditor->InsertTextAsAction(insertValue);
              NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                "Failed to insert the new value");
            }
          } else {
            AutoDisableUndo disableUndo(textEditor);
            if (selection) {
              // Since we don't use undo transaction, we don't need to store
              // selection state.  SetText will set selection to tail.
              // Note that textEditor will collapse selection to the end.
              // Therefore, it's safe to use RemoveAllRangesTemporarily() here.
              selection->RemoveAllRangesTemporarily();
            }

            textEditor->SetText(newValue);

            // Call the listener's HandleValueChanged() callback manually, since
            // we don't use the transaction manager in this path and it could be
            // that the editor would bypass calling the listener for that reason.
            mTextListener->HandleValueChanged();
          }

          mTextListener->SetValueChanged(true);
          mTextListener->SettingValue(false);

          if (!notifyValueChanged) {
            // Listener doesn't update frame, but it is required for placeholder
            ValueWasChanged(true);
          }
        }

        if (!weakFrame.IsAlive()) {
          // If the frame was destroyed because of a flush somewhere inside
          // InsertText, mBoundFrame here will be false.  But it's also possible
          // for the frame to go away because of another reason (such as deleting
          // the existing selection -- see bug 574558), in which case we don't
          // need to reset the value here.
          if (!mBoundFrame) {
            return SetValue(newValue, aFlags & eSetValue_Notify);
          }
          return true;
        }

        // The new value never includes line breaks caused by hard-wrap.
        // So, mCachedValue can always cache the new value.
        if (!mBoundFrame->CacheValue(newValue, fallible)) {
          return false;
        }
      }
    }
  } else {
    if (!mValue) {
      mValue.emplace();
    }

    // We can't just early-return here if mValue->Equals(newValue), because
    // ValueWasChanged and OnValueChanged below still need to be called.
    if (!mValue->Equals(newValue) ||
        !nsContentUtils::SkipCursorMoveForSameValueSet()) {
      if (!mValue->Assign(newValue, fallible)) {
        return false;
      }

      // Since we have no editor we presumably have cached selection state.
      if (IsSelectionCached()) {
        SelectionProperties& props = GetSelectionProperties();
        if (aFlags & eSetValue_MoveCursorToEndIfValueChanged) {
          props.SetStart(newValue.Length());
          props.SetEnd(newValue.Length());
          props.SetDirection(nsITextControlFrame::eForward);
        } else {
          // Make sure our cached selection position is not outside the new value.
          props.SetStart(std::min(props.GetStart(), newValue.Length()));
          props.SetEnd(std::min(props.GetEnd(), newValue.Length()));
        }
      }

      // Update the frame display if needed
      if (mBoundFrame) {
        mBoundFrame->UpdateValueDisplay(true);
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

    // If we've reached the point where the root node has been created, we
    // can assume that it's safe to notify.
    ValueWasChanged(!!mBoundFrame);
  }

  mTextCtrlElement->OnValueChanged(/* aNotify = */ !!mBoundFrame,
                                   /* aWasInteractiveUserChange = */ false);

  return true;
}

bool
nsTextEditorState::HasNonEmptyValue()
{
  if (mTextEditor && mBoundFrame && mEditorInitialized &&
      !mIsCommittingComposition) {
    bool empty;
    nsresult rv = mTextEditor->DocumentIsEmpty(&empty);
    if (NS_SUCCEEDED(rv)) {
      return !empty;
    }
  }

  nsAutoString value;
  GetValue(value, true);
  return !value.IsEmpty();
}

void
nsTextEditorState::InitializeKeyboardEventListeners()
{
  //register key listeners
  nsCOMPtr<EventTarget> target = do_QueryInterface(mTextCtrlElement);
  EventListenerManager* manager = target->GetOrCreateListenerManager();
  if (manager) {
    manager->AddEventListenerByType(mTextListener,
                                    NS_LITERAL_STRING("keydown"),
                                    TrustedEventsAtSystemGroupBubble());
    manager->AddEventListenerByType(mTextListener,
                                    NS_LITERAL_STRING("keypress"),
                                    TrustedEventsAtSystemGroupBubble());
    manager->AddEventListenerByType(mTextListener,
                                    NS_LITERAL_STRING("keyup"),
                                    TrustedEventsAtSystemGroupBubble());
  }

  mSelCon->SetScrollableFrame(do_QueryFrame(mBoundFrame->PrincipalChildList().FirstChild()));
}

void
nsTextEditorState::ValueWasChanged(bool aNotify)
{
  UpdateOverlayTextVisibility(aNotify);
}

void
nsTextEditorState::SetPreviewText(const nsAString& aValue, bool aNotify)
{
  // If we don't have a preview div, there's nothing to do.
  Element* previewDiv = GetPreviewNode();
  if (!previewDiv)
    return;

  nsAutoString previewValue(aValue);

  nsContentUtils::RemoveNewlines(previewValue);
  MOZ_ASSERT(previewDiv->GetFirstChild(), "preview div has no child");
  previewDiv->GetFirstChild()->AsText()->SetText(previewValue, aNotify);

  UpdateOverlayTextVisibility(aNotify);
}

void
nsTextEditorState::GetPreviewText(nsAString& aValue)
{
  // If we don't have a preview div, there's nothing to do.
  Element* previewDiv = GetPreviewNode();
  if (!previewDiv)
    return;

  MOZ_ASSERT(previewDiv->GetFirstChild(), "preview div has no child");
  const nsTextFragment *text = previewDiv->GetFirstChild()->GetText();

  aValue.Truncate();
  text->AppendTo(aValue);
}

void
nsTextEditorState::UpdateOverlayTextVisibility(bool aNotify)
{
  nsAutoString value, previewValue;
  bool valueIsEmpty = !HasNonEmptyValue();
  GetPreviewText(previewValue);

  mPreviewVisibility = valueIsEmpty && !previewValue.IsEmpty();
  mPlaceholderVisibility = valueIsEmpty && previewValue.IsEmpty();

  if (mPlaceholderVisibility &&
      !nsContentUtils::ShowInputPlaceholderOnFocus()) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    mPlaceholderVisibility = !nsContentUtils::IsFocusedContent(content);
  }

  if (mBoundFrame && aNotify) {
    mBoundFrame->InvalidateFrame();
  }
}

void
nsTextEditorState::HideSelectionIfBlurred()
{
  MOZ_ASSERT(mSelCon, "Should have a selection controller if we have a frame!");
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  if (!nsContentUtils::IsFocusedContent(content)) {
    mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
  }
}

bool
nsTextEditorState::EditorHasComposition()
{
  return mTextEditor && mTextEditor->IsIMEComposing();
}
