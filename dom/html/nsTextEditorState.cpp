/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextEditorState.h"

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsView.h"
#include "nsCaret.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsITextControlFrame.h" 
#include "nsIPlaintextEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsContentCreatorFunctions.h"
#include "nsTextControlFrame.h"
#include "nsIControllers.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsITransactionManager.h"
#include "nsIControllerContext.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMEventListener.h"
#include "nsIEditorIMESupport.h"
#include "nsIEditorObserver.h"
#include "nsIWidget.h"
#include "nsIDocumentEncoder.h"
#include "nsISelectionPrivate.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsIEditor.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/EventListenerManager.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsTextNode.h"
#include "nsIController.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "nsNumberControlFrame.h"
#include "nsFrameSelection.h"
#include "mozilla/Telemetry.h"
#include "mozilla/layers/ScrollInputMethods.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::layers::ScrollInputMethod;

static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);

class MOZ_STACK_CLASS ValueSetter
{
public:
  explicit ValueSetter(nsIEditor* aEditor)
    : mEditor(aEditor)
  {
    MOZ_ASSERT(aEditor);
  
    // To protect against a reentrant call to SetValue, we check whether
    // another SetValue is already happening for this editor.  If it is,
    // we must wait until we unwind to re-enable oninput events.
    mEditor->GetSuppressDispatchingInputEvent(&mOuterTransaction);
  }
  ~ValueSetter()
  {
    mEditor->SetSuppressDispatchingInputEvent(mOuterTransaction);
  }
  void Init()
  {
    mEditor->SetSuppressDispatchingInputEvent(true);
  }

private:
  nsCOMPtr<nsIEditor> mEditor;
  bool mOuterTransaction;
};

class RestoreSelectionState : public Runnable {
public:
  RestoreSelectionState(nsTextEditorState *aState, nsTextControlFrame *aFrame)
    : mFrame(aFrame),
      mTextEditorState(aState)
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

  // Let the text editor tell us we're no longer relevant - avoids use of nsWeakFrame
  void Revoke() {
    mFrame = nullptr;
    mTextEditorState = nullptr;
  }

private:
  nsTextControlFrame* mFrame;
  nsTextEditorState* mTextEditorState;
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
    static nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::HARD, &nsGkAtoms::OFF, nullptr};

    switch (aContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::wrap,
                                      strings, eIgnoreCase)) {
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

  //NSISELECTIONCONTROLLER INTERFACES
  NS_IMETHOD SetDisplaySelection(int16_t toggle) override;
  NS_IMETHOD GetDisplaySelection(int16_t* _retval) override;
  NS_IMETHOD SetSelectionFlags(int16_t aInEnable) override;
  NS_IMETHOD GetSelectionFlags(int16_t *aOutEnable) override;
  NS_IMETHOD GetSelection(RawSelectionType aRawSelectionType,
                          nsISelection** aSelection) override;
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
    mFrameSelection->Init(aShell, mLimiter);
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
nsTextInputSelectionImpl::GetSelection(RawSelectionType aRawSelectionType,
                                       nsISelection** aSelection)
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

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollSelectionIntoView(
                            RawSelectionType aRawSelectionType,
                            int16_t aRegion,
                            int16_t aFlags)
{
  if (!mFrameSelection) 
    return NS_ERROR_FAILURE; 

  return mFrameSelection->ScrollSelectionIntoView(
                            ToSelectionType(aRawSelectionType),
                            aRegion, aFlags);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(RawSelectionType aRawSelectionType)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;

  return mFrameSelection->RepaintSelection(ToSelectionType(aRawSelectionType));
}

nsresult
nsTextInputSelectionImpl::RepaintSelection(nsPresContext* aPresContext,
                                           SelectionType aSelectionType)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;

  return mFrameSelection->RepaintSelection(aSelectionType);
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
      nsISelection* domSel =
        mFrameSelection->GetSelection(SelectionType::eNormal);
      if (domSel)
        caret->SetCaretReadOnly(aReadOnly);
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
      nsISelection* domSel =
        mFrameSelection->GetSelection(SelectionType::eNormal);
      if (domSel)
        caret->SetVisibilityDuringSelection(aVisibility);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::PhysicalMove(int16_t aDirection, int16_t aAmount,
                                       bool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->PhysicalMove(aDirection, aAmount, aExtend);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterMove(bool aForward, bool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->CharacterMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterExtendForDelete()
{
  if (mFrameSelection)
    return mFrameSelection->CharacterExtendForDelete();
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterExtendForBackspace()
{
  if (mFrameSelection)
    return mFrameSelection->CharacterExtendForBackspace();
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::WordMove(bool aForward, bool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->WordMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::WordExtendForDelete(bool aForward)
{
  if (mFrameSelection)
    return mFrameSelection->WordExtendForDelete(aForward);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::LineMove(bool aForward, bool aExtend)
{
  if (mFrameSelection)
  {
    nsresult result = mFrameSelection->LineMove(aForward, aExtend);
    if (NS_FAILED(result))
      result = CompleteMove(aForward,aExtend);
    return result;
  }
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::IntraLineMove(bool aForward, bool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->IntraLineMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::PageMove(bool aForward, bool aExtend)
{
  // expected behavior for PageMove is to scroll AND move the caret
  // and to remain relative position of the caret in view. see Bug 4302.
  if (mScrollFrame)
  {
    mFrameSelection->CommonPageMove(aForward, aExtend, mScrollFrame);
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

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethod::MainThreadCompleteScroll);

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                         nsIScrollableFrame::WHOLE,
                         nsIScrollableFrame::INSTANT);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteMove(bool aForward, bool aExtend)
{
  // grab the parent / root DIV for this text widget
  nsIContent* parentDIV = mFrameSelection->GetLimiter();
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

  mFrameSelection->HandleClick(parentDIV, offset, offset, aExtend,
                               false, hint);

  // if we got this far, attempt to scroll no matter what the above result is
  return CompleteScroll(aForward);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollPage(bool aForward)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethod::MainThreadScrollPage);

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

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethod::MainThreadScrollLine);

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

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethod::MainThreadScrollCharacter);

  mScrollFrame->ScrollBy(nsIntPoint(aRight ? 1 : -1, 0),
                         nsIScrollableFrame::LINES,
                         nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SelectAll()
{
  if (mFrameSelection)
    return mFrameSelection->SelectAll();
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

class nsTextInputListener : public nsISelectionListener,
                            public nsIDOMEventListener,
                            public nsIEditorObserver,
                            public nsSupportsWeakReference
{
public:
  /** the default constructor
   */ 
  explicit nsTextInputListener(nsITextControlElement* aTxtCtrlElement);

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetFrame(nsTextControlFrame *aFrame){mFrame = aFrame;}

  void SettingValue(bool aValue) { mSettingValue = aValue; }
  void SetValueChanged(bool aSetValueChanged) { mSetValueChanged = aSetValueChanged; }

  NS_DECL_ISUPPORTS

  NS_DECL_NSISELECTIONLISTENER

  NS_DECL_NSIDOMEVENTLISTENER

  NS_DECL_NSIEDITOROBSERVER

protected:
  /** the default destructor. virtual due to the possibility of derivation.
   */
  virtual ~nsTextInputListener();

  nsresult  UpdateTextInputCommands(const nsAString& commandsToUpdate,
                                    nsISelection* sel = nullptr,
                                    int16_t reason = 0);

protected:

  nsIFrame* mFrame;

  nsITextControlElement* const mTxtCtrlElement;

  bool            mSelectionWasCollapsed;
  /**
   * Whether we had undo items or not the last time we got EditAction()
   * notification (when this state changes we update undo and redo menus)
   */
  bool            mHadUndoItems;
  /**
   * Whether we had redo items or not the last time we got EditAction()
   * notification (when this state changes we update undo and redo menus)
   */
  bool            mHadRedoItems;
  /**
   * Whether we're in the process of a SetValue call, and should therefore
   * refrain from calling OnValueChanged.
   */
  bool mSettingValue;
  /**
   * Whether we are in the process of a SetValue call that doesn't want
   * |SetValueChanged| to be called.
   */
  bool mSetValueChanged;
};


/*
 * nsTextInputListener implementation
 */

nsTextInputListener::nsTextInputListener(nsITextControlElement* aTxtCtrlElement)
: mFrame(nullptr)
, mTxtCtrlElement(aTxtCtrlElement)
, mSelectionWasCollapsed(true)
, mHadUndoItems(false)
, mHadRedoItems(false)
, mSettingValue(false)
, mSetValueChanged(true)
{
}

nsTextInputListener::~nsTextInputListener() 
{
}

NS_IMPL_ISUPPORTS(nsTextInputListener,
                  nsISelectionListener,
                  nsIEditorObserver,
                  nsISupportsWeakReference,
                  nsIDOMEventListener)

// BEGIN nsIDOMSelectionListener

NS_IMETHODIMP
nsTextInputListener::NotifySelectionChanged(nsIDOMDocument* aDoc, nsISelection* aSel, int16_t aReason)
{
  bool collapsed;
  nsWeakFrame weakFrame = mFrame;

  if (!aDoc || !aSel || NS_FAILED(aSel->GetIsCollapsed(&collapsed)))
    return NS_OK;

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
  if (!collapsed && (aReason & (nsISelectionListener::MOUSEUP_REASON | 
                                nsISelectionListener::KEYPRESS_REASON |
                                nsISelectionListener::SELECTALL_REASON)))
  {
    nsIContent* content = mFrame->GetContent();
    if (content) 
    {
      nsCOMPtr<nsIDocument> doc = content->GetComposedDoc();
      if (doc) 
      {
        nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
        if (presShell) 
        {
          nsEventStatus status = nsEventStatus_eIgnore;
          WidgetEvent event(true, eFormSelect);

          presShell->HandleEventWithTarget(&event, mFrame, content, &status);
        }
      }
    }
  }

  // if the collapsed state did not change, don't fire notifications
  if (collapsed == mSelectionWasCollapsed)
    return NS_OK;

  mSelectionWasCollapsed = collapsed;

  if (!weakFrame.IsAlive() || !nsContentUtils::IsFocusedContent(mFrame->GetContent()))
    return NS_OK;

  return UpdateTextInputCommands(NS_LITERAL_STRING("select"), aSel, aReason);
}

// END nsIDOMSelectionListener

static void
DoCommandCallback(Command aCommand, void* aData)
{
  nsTextControlFrame *frame = static_cast<nsTextControlFrame*>(aData);
  nsIContent *content = frame->GetContent();

  nsCOMPtr<nsIControllers> controllers;
  nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(content);
  if (input) {
    input->GetControllers(getter_AddRefs(controllers));
  } else {
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea =
      do_QueryInterface(content);

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
nsTextInputListener::HandleEvent(nsIDOMEvent* aEvent)
{
  bool defaultPrevented = false;
  nsresult rv = aEvent->GetDefaultPrevented(&defaultPrevented);
  NS_ENSURE_SUCCESS(rv, rv);
  if (defaultPrevented) {
    return NS_OK;
  }

  bool isTrusted = false;
  rv = aEvent->GetIsTrusted(&isTrusted);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isTrusted) {
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
                                         
  if (widget->ExecuteNativeKeyBinding(nativeKeyBindingsType,
                                      *keyEvent, DoCommandCallback, mFrame)) {
    aEvent->PreventDefault();
  }
  return NS_OK;
}

// BEGIN nsIEditorObserver

NS_IMETHODIMP
nsTextInputListener::EditAction()
{
  if (!mFrame) {
    // We've been disconnected from the nsTextEditorState object, nothing to do
    // here.
    return NS_OK;
  }

  nsWeakFrame weakFrame = mFrame;

  nsITextControlFrame* frameBase = do_QueryFrame(mFrame);
  nsTextControlFrame* frame = static_cast<nsTextControlFrame*> (frameBase);
  NS_ASSERTION(frame, "Where is our frame?");
  //
  // Update the undo / redo menus
  //
  nsCOMPtr<nsIEditor> editor;
  frame->GetEditor(getter_AddRefs(editor));

  // Get the number of undo / redo items
  int32_t numUndoItems = 0;
  int32_t numRedoItems = 0;
  editor->GetNumberOfUndoItems(&numUndoItems);
  editor->GetNumberOfRedoItems(&numRedoItems);
  if ((numUndoItems && !mHadUndoItems) || (!numUndoItems && mHadUndoItems) ||
      (numRedoItems && !mHadRedoItems) || (!numRedoItems && mHadRedoItems)) {
    // Modify the menu if undo or redo items are different
    UpdateTextInputCommands(NS_LITERAL_STRING("undo"));

    mHadUndoItems = numUndoItems != 0;
    mHadRedoItems = numRedoItems != 0;
  }

  if (!weakFrame.IsAlive()) {
    return NS_OK;
  }

  // Make sure we know we were changed (do NOT set this to false if there are
  // no undo items; JS could change the value and we'd still need to save it)
  if (mSetValueChanged) {
    frame->SetValueChanged(true);
  }

  if (!mSettingValue) {
    mTxtCtrlElement->OnValueChanged(/* aNotify = */ true,
                                    /* aWasInteractiveUserChange = */ true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextInputListener::BeforeEditAction()
{
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputListener::CancelEditAction()
{
  return NS_OK;
}

// END nsIEditorObserver


nsresult
nsTextInputListener::UpdateTextInputCommands(const nsAString& commandsToUpdate,
                                             nsISelection* sel,
                                             int16_t reason)
{
  nsIContent* content = mFrame->GetContent();
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDocument> doc = content->GetComposedDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsPIDOMWindowOuter *domWindow = doc->GetWindow();
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  return domWindow->UpdateCommands(commandsToUpdate, sel, reason);
}

// END nsTextInputListener

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
  , mIsCommittingComposition(false)
{
  MOZ_COUNT_CTOR(nsTextEditorState);
}

nsTextEditorState::~nsTextEditorState()
{
  MOZ_COUNT_DTOR(nsTextEditorState);
  Clear();
}

void
nsTextEditorState::Clear()
{
  if (mBoundFrame) {
    // Oops, we still have a frame!
    // This should happen when the type of a text input control is being changed
    // to something which is not a text control.  In this case, we should pretend
    // that a frame is being destroyed, and clean up after ourselves properly.
    UnbindFromFrame(mBoundFrame);
    mEditor = nullptr;
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPlaceholderDiv)
}

void nsTextEditorState::Traverse(nsCycleCollectionTraversalCallback& cb)
{
  nsTextEditorState* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelCon)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlaceholderDiv)
}

nsFrameSelection*
nsTextEditorState::GetConstFrameSelection() {
  if (mSelCon)
    return mSelCon->GetConstFrameSelection();
  return nullptr;
}

nsIEditor*
nsTextEditorState::GetEditor()
{
  if (!mEditor) {
    nsresult rv = PrepareEditor();
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  return mEditor;
}

nsISelectionController*
nsTextEditorState::GetSelectionController() const
{
  return mSelCon;
}

// Helper class, used below in BindToFrame().
class PrepareEditorEvent : public Runnable {
public:
  PrepareEditorEvent(nsTextEditorState &aState,
                     nsIContent *aOwnerContent,
                     const nsAString &aCurrentValue)
    : mState(&aState)
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
  if (mEditor) {
    GetValue(currentValue, true);
  }

  mBoundFrame = aFrame;

  nsIContent *rootNode = GetRootNode();

  nsresult rv = InitializeRootNode();
  NS_ENSURE_SUCCESS(rv, rv);

  nsIPresShell *shell = mBoundFrame->PresContext()->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  // Create selection
  RefPtr<nsFrameSelection> frameSel = new nsFrameSelection();

  // Create a SelectionController
  mSelCon = new nsTextInputSelectionImpl(frameSel, shell, rootNode);
  MOZ_ASSERT(!mTextListener, "Should not overwrite the object");
  mTextListener = new nsTextInputListener(mTextCtrlElement);

  mTextListener->SetFrame(mBoundFrame);
  mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  // Get the caret and make it a selection listener.
  RefPtr<nsISelection> domSelection;
  if (NS_SUCCEEDED(mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                         getter_AddRefs(domSelection))) &&
      domSelection) {
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));
    RefPtr<nsCaret> caret = shell->GetCaret();
    nsCOMPtr<nsISelectionListener> listener;
    if (caret) {
      listener = do_QueryInterface(caret);
      if (listener) {
        selPriv->AddSelectionListener(listener);
      }
    }

    selPriv->AddSelectionListener(static_cast<nsISelectionListener*>
                                             (mTextListener));
  }

  // If an editor exists from before, prepare it for usage
  if (mEditor) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

    // Set the correct direction on the newly created root node
    uint32_t flags;
    nsresult rv = mEditor->GetFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (flags & nsIPlaintextEditor::eEditorRightToLeft) {
      rootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), false);
    } else if (flags & nsIPlaintextEditor::eEditorLeftToRight) {
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
  void Init(nsIEditor* aEditor)
  {
    mNewEditor = aEditor;
  }
  ~PreDestroyer()
  {
    if (mNewEditor) {
      mNewEditor->PreDestroy(true);
    }
  }
  void Swap(nsCOMPtr<nsIEditor>& aEditor)
  {
    return mNewEditor.swap(aEditor);
  }
private:
  nsCOMPtr<nsIEditor> mNewEditor;
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

  // Note that we don't check mEditor here, because we might already have one
  // around, in which case we don't create a new one, and we'll just tie the
  // required machinery to it.

  nsPresContext *presContext = mBoundFrame->PresContext();
  nsIPresShell *shell = presContext->GetPresShell();

  // Setup the editor flags
  uint32_t editorFlags = 0;
  if (IsPlainTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorPlaintextMask;
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
  nsCOMPtr<nsIEditor> newEditor; // the editor that we might create
  nsresult rv = NS_OK;
  PreDestroyer preDestroyer;
  if (!mEditor) {
    shouldInitializeEditor = true;

    // Create an editor
    newEditor = do_CreateInstance(kTextEditorCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    preDestroyer.Init(newEditor);

    // Make sure we clear out the non-breaking space before we initialize the editor
    rv = mBoundFrame->UpdateValueDisplay(true, true);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    if (aValue || !mEditorInitialized) {
      // Set the correct value in the root node
      rv = mBoundFrame->UpdateValueDisplay(true, !mEditorInitialized, aValue);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    newEditor = mEditor; // just pretend that we have a new editor!

    // Don't lose application flags in the process.
    uint32_t originalFlags = 0;
    newEditor->GetFlags(&originalFlags);
    if (originalFlags & nsIPlaintextEditor::eEditorMailMask) {
      editorFlags |= nsIPlaintextEditor::eEditorMailMask;
    }
  }

  // Get the current value of the textfield from the content.
  // Note that if we've created a new editor, mEditor is null at this stage,
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
    nsCOMPtr<nsIDOMDocument> domdoc = do_QueryInterface(shell->GetDocument());
    if (!domdoc)
      return NS_ERROR_FAILURE;

    // What follows is a bit of a hack.  The editor uses the public DOM APIs
    // for its content manipulations, and it causes it to fail some security
    // checks deep inside when initializing. So we explictly make it clear that
    // we're native code.
    // Note that any script that's directly trying to access our value
    // has to be going through some scriptable object to do that and that
    // already does the relevant security checks.
    AutoNoJSAPI nojsapi;

    rv = newEditor->Init(domdoc, GetRootNode(), mSelCon, editorFlags,
                         defaultValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Initialize the controller for the editor

  if (!SuppressEventHandlers(presContext)) {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMHTMLInputElement> inputElement =
      do_QueryInterface(mTextCtrlElement);
    if (inputElement) {
      rv = inputElement->GetControllers(getter_AddRefs(controllers));
    } else {
      nsCOMPtr<nsIDOMHTMLTextAreaElement> textAreaElement =
        do_QueryInterface(mTextCtrlElement);

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
            editController->SetCommandContext(newEditor);
            found = true;
          }
        }
      }
      if (!found)
        rv = NS_ERROR_FAILURE;
    }
  }

  if (shouldInitializeEditor) {
    // Initialize the plaintext editor
    nsCOMPtr<nsIPlaintextEditor> textEditor(do_QueryInterface(newEditor));
    if (textEditor) {
      // Set up wrapping
      textEditor->SetWrapColumn(GetWrapCols());

      // Set max text field length
      int32_t maxLength;
      if (GetMaxLength(&maxLength)) { 
        textEditor->SetMaxTextLength(maxLength);
      }
    }
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  if (content) {
    rv = newEditor->GetFlags(&editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check if the readonly attribute is set.
    if (content->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly))
      editorFlags |= nsIPlaintextEditor::eEditorReadonlyMask;

    // Check if the disabled attribute is set.
    // TODO: call IsDisabled() here!
    if (content->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) 
      editorFlags |= nsIPlaintextEditor::eEditorDisabledMask;

    // Disable the selection if necessary.
    if (editorFlags & nsIPlaintextEditor::eEditorDisabledMask)
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);

    newEditor->SetFlags(editorFlags);
  }

  if (shouldInitializeEditor) {
    // Hold on to the newly created editor
    preDestroyer.Swap(mEditor);
  }

  // If we have a default value, insert it under the div we created
  // above, but be sure to use the editor so that '*' characters get
  // displayed for password fields, etc. SetValue() will call the
  // editor for us.

  if (!defaultValue.IsEmpty()) {
    rv = newEditor->SetFlags(editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now call SetValue() which will make the necessary editor calls to set
    // the default value.  Make sure to turn off undo before setting the default
    // value, and turn it back on afterwards. This will make sure we can't undo
    // past the default value.

    rv = newEditor->EnableUndo(false);
    NS_ENSURE_SUCCESS(rv, rv);

    bool success = SetValue(defaultValue, eSetValue_Internal);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

    rv = newEditor->EnableUndo(true);
    NS_ASSERTION(NS_SUCCEEDED(rv),"Transaction Manager must have failed");

    // Now restore the original editor flags.
    rv = newEditor->SetFlags(editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsITransactionManager> transMgr;
  newEditor->GetTransactionManager(getter_AddRefs(transMgr));
  NS_ENSURE_TRUE(transMgr, NS_ERROR_FAILURE);

  transMgr->SetMaxTransactionCount(nsITextControlElement::DEFAULT_UNDO_CAP);

  if (IsPasswordTextControl()) {
    // Disable undo for password textfields.  Note that we want to do this at
    // the very end of InitEditor, so the calls to EnableUndo when setting the
    // default value don't screw us up.
    // Since changing the control type does a reframe, we don't have to worry
    // about dynamic type changes here.
    newEditor->EnableUndo(false);
  }

  if (!mEditorInitialized) {
    newEditor->PostCreate();
    mEverInited = true;
    mEditorInitialized = true;
  }

  if (mTextListener)
    newEditor->AddEditorObserver(mTextListener);

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

  // The selection cache is no longer going to be valid
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
  HTMLInputElement* input = HTMLInputElement::FromContent(parentOfParent);
  if (input) {
    // This function might be called during frame reconstruction as a result
    // of changing the input control's type from number to something else. In
    // that situation, the type of the control has changed, but its frame has
    // not been reconstructed yet.  So we need to check the type of the input
    // control in addition to the type of the frame.
    return (input->GetType() == NS_FORM_INPUT_NUMBER) ? input : nullptr;
  }

  return nullptr;
}

void
nsTextEditorState::DestroyEditor()
{
  // notify the editor that we are going away
  if (mEditorInitialized) {
    if (mTextListener)
      mEditor->RemoveEditorObserver(mTextListener);

    mEditor->PreDestroy(true);
    mEditorInitialized = false;
  }
  ClearValueCache();
}

void
nsTextEditorState::UnbindFromFrame(nsTextControlFrame* aFrame)
{
  NS_ENSURE_TRUE_VOID(mBoundFrame);

  // If it was, however, it should be unbounded from the same frame.
  NS_ASSERTION(!aFrame || aFrame == mBoundFrame, "Unbinding from the wrong frame");
  NS_ENSURE_TRUE_VOID(!aFrame || aFrame == mBoundFrame);

  // If the editor is modified but nsIEditorObserver::EditAction() hasn't been
  // called yet, we need to notify it here because editor may be destroyed
  // before EditAction() is called if selection listener causes flushing layout.
  bool isInEditAction = false;
  if (mTextListener && mEditor && mEditorInitialized &&
      NS_SUCCEEDED(mEditor->GetIsInEditAction(&isInEditAction)) &&
      isInEditAction) {
    mTextListener->EditAction();
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
  // Note that nsTextControlFrame::GetSelectionRange attempts to initialize the
  // editor before grabbing the range, and because this is not an acceptable
  // side effect for unbinding from a text control frame, we need to call
  // GetSelectionRange before calling DestroyEditor, and only if
  // mEditorInitialized indicates that we actually have an editor available.
  int32_t start = 0, end = 0;
  nsITextControlFrame::SelectionDirection direction =
    nsITextControlFrame::eForward;
  if (mEditorInitialized) {
    HTMLInputElement* number = GetParentNumberControl(aFrame);
    if (number) {
      // If we are inside a number control, cache the selection on the
      // parent control, because this text editor state will be destroyed
      // together with the native anonymous text control.
      SelectionProperties props;
      mBoundFrame->GetSelectionRange(&start, &end, &direction);
      props.SetStart(start);
      props.SetEnd(end);
      props.SetDirection(direction);
      number->SetSelectionProperties(props);
    } else {
      mBoundFrame->GetSelectionRange(&start, &end, &direction);
      mSelectionProperties.SetStart(start);
      mSelectionProperties.SetEnd(end);
      mSelectionProperties.SetDirection(direction);
      mSelectionCached = true;
    }
  }

  // Destroy our editor
  DestroyEditor();

  // Clean up the controller
  if (!SuppressEventHandlers(mBoundFrame->PresContext()))
  {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMHTMLInputElement> inputElement =
      do_QueryInterface(mTextCtrlElement);
    if (inputElement)
      inputElement->GetControllers(getter_AddRefs(controllers));
    else
    {
      nsCOMPtr<nsIDOMHTMLTextAreaElement> textAreaElement =
        do_QueryInterface(mTextCtrlElement);
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
      RefPtr<nsISelection> domSelection;
      if (NS_SUCCEEDED(mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                             getter_AddRefs(domSelection))) &&
          domSelection) {
        nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));

        selPriv->RemoveSelectionListener(static_cast<nsISelectionListener*>
                                         (mTextListener));
      }
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

  if (mRootNode && mMutationObserver) {
    mRootNode->RemoveMutationObserver(mMutationObserver);
    mMutationObserver = nullptr;
  }

  // Unbind the anonymous content from the tree.
  // We actually hold a reference to the content nodes so that
  // they're not actually destroyed.
  nsContentUtils::DestroyAnonymousContent(&mRootNode);
  nsContentUtils::DestroyAnonymousContent(&mPlaceholderDiv);
}

nsresult
nsTextEditorState::CreateRootNode()
{
  NS_ENSURE_TRUE(!mRootNode, NS_ERROR_UNEXPECTED);
  NS_ENSURE_ARG_POINTER(mBoundFrame);

  nsIPresShell *shell = mBoundFrame->PresContext()->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsIDocument *doc = shell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // Now create a DIV and add it to the anonymous content child list.
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);

  nsresult rv = NS_NewHTMLElement(getter_AddRefs(mRootNode), nodeInfo.forget(),
                                  NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsSingleLineTextControl()) {
    mMutationObserver = new nsAnonDivObserver(this);
    mRootNode->AddMutationObserver(mMutationObserver);
  }

  return rv;
}

nsresult
nsTextEditorState::InitializeRootNode()
{
  // Make our root node editable
  mRootNode->SetFlags(NODE_IS_EDITABLE);

  // Set the necessary classes on the text control. We use class values
  // instead of a 'style' attribute so that the style comes from a user-agent
  // style sheet and is still applied even if author styles are disabled.
  nsAutoString classValue;
  classValue.AppendLiteral("anonymous-div");
  int32_t wrapCols = GetWrapCols();
  if (wrapCols > 0) {
    classValue.AppendLiteral(" wrap");
  }
  if (!IsSingleLineTextControl()) {
    // We can't just inherit the overflow because setting visible overflow will
    // crash when the number of lines exceeds the height of the textarea and
    // setting -moz-hidden-unscrollable overflow (NS_STYLE_OVERFLOW_CLIP)
    // doesn't paint the caret for some reason.
    const nsStyleDisplay* disp = mBoundFrame->StyleDisplay();
    if (disp->mOverflowX != NS_STYLE_OVERFLOW_VISIBLE &&
        disp->mOverflowX != NS_STYLE_OVERFLOW_CLIP) {
      classValue.AppendLiteral(" inherit-overflow");
    }
  }
  nsresult rv = mRootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                   classValue, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return mBoundFrame->UpdateValueDisplay(false);
}

nsresult
nsTextEditorState::CreatePlaceholderNode()
{
#ifdef DEBUG
  {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    if (content) {
      nsAutoString placeholderTxt;
      content->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder,
                       placeholderTxt);
      nsContentUtils::RemoveNewlines(placeholderTxt);
      NS_ASSERTION(!placeholderTxt.IsEmpty(), "CreatePlaceholderNode() shouldn't \
be called if @placeholder is the empty string when trimmed from line breaks");
    }
  }
#endif // DEBUG

  NS_ENSURE_TRUE(!mPlaceholderDiv, NS_ERROR_UNEXPECTED);
  NS_ENSURE_ARG_POINTER(mBoundFrame);

  nsIPresShell *shell = mBoundFrame->PresContext()->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsIDocument *doc = shell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsNodeInfoManager* pNodeInfoManager = doc->NodeInfoManager();
  NS_ENSURE_TRUE(pNodeInfoManager, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;

  // Create a DIV for the placeholder
  // and add it to the anonymous content child list
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = pNodeInfoManager->GetNodeInfo(nsGkAtoms::div, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  rv = NS_NewHTMLElement(getter_AddRefs(mPlaceholderDiv), nodeInfo.forget(),
                         NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the text node for the placeholder text before doing anything else
  RefPtr<nsTextNode> placeholderText = new nsTextNode(pNodeInfoManager);

  rv = mPlaceholderDiv->AppendChildTo(placeholderText, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // initialize the text
  UpdatePlaceholderText(false);

  return NS_OK;
}

bool
nsTextEditorState::GetMaxLength(int32_t* aMaxLength)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  nsGenericHTMLElement* element =
    nsGenericHTMLElement::FromContentOrNull(content);
  NS_ENSURE_TRUE(element, false);

  const nsAttrValue* attr = element->GetParsedAttr(nsGkAtoms::maxlength);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    *aMaxLength = attr->GetIntegerValue();

    return true;
  }

  return false;
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

  if (mEditor && mBoundFrame && (mEditorInitialized || !IsSingleLineTextControl())) {
    bool canCache = aIgnoreWrap && !IsSingleLineTextControl();
    if (canCache && !mCachedValue.IsEmpty()) {
      aValue = mCachedValue;
      return;
    }

    aValue.Truncate(); // initialize out param

    uint32_t flags = (nsIDocumentEncoder::OutputLFLineBreak |
                      nsIDocumentEncoder::OutputPreformatted |
                      nsIDocumentEncoder::OutputPersistNBSP);

    if (IsPlainTextControl())
    {
      flags |= nsIDocumentEncoder::OutputBodyOnly;
    }

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

      mEditor->OutputToString(NS_LITERAL_STRING("text/plain"), flags,
                              aValue);
    }
    if (canCache) {
      mCachedValue = aValue;
    } else {
      mCachedValue.Truncate();
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
nsTextEditorState::SetValue(const nsAString& aValue, uint32_t aFlags)
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
        mBoundFrame->GetText(currentValue);
        if (newValue == currentValue) {
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
      if (nsContentUtils::IsSafeToRunScript()) {
        WeakPtr<nsTextEditorState> self(this);
        // WARNING: During this call, compositionupdate, compositionend, input
        // events will be fired.  Therefore, everything can occur.  E.g., the
        // document may be unloaded.
        mValueBeingSet = aValue;
        mIsCommittingComposition = true;
        nsCOMPtr<nsIEditorIMESupport> editorIMESupport =
                                        do_QueryInterface(mEditor);
        MOZ_RELEASE_ASSERT(editorIMESupport);
        nsresult rv = editorIMESupport->ForceCompositionEnd();
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

  if (mEditor && mBoundFrame) {
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
    mBoundFrame->GetText(currentValue);

    nsWeakFrame weakFrame(mBoundFrame);

    // this is necessary to avoid infinite recursion
    if (!currentValue.Equals(newValue))
    {
      ValueSetter valueSetter(mEditor);

      // \r is an illegal character in the dom, but people use them,
      // so convert windows and mac platform linebreaks to \n:
      if (newValue.FindChar(char16_t('\r')) != -1) {
        if (!nsContentUtils::PlatformToDOMLineBreaks(newValue, fallible)) {
          return false;
        }
      }

      nsCOMPtr<nsIDOMDocument> domDoc;
      mEditor->GetDocument(getter_AddRefs(domDoc));
      if (!domDoc) {
        NS_WARNING("Why don't we have a document?");
        return true;
      }

      // Time to mess with our security context... See comments in GetValue()
      // for why this is needed.  Note that we have to do this up here, because
      // otherwise SelectAll() will fail.
      {
        AutoNoJSAPI nojsapi;

        nsCOMPtr<nsISelection> domSel;
        nsCOMPtr<nsISelectionPrivate> selPriv;
        mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                              getter_AddRefs(domSel));
        if (domSel)
        {
          selPriv = do_QueryInterface(domSel);
          if (selPriv)
            selPriv->StartBatchChanges();
        }

        nsCOMPtr<nsISelectionController> kungFuDeathGrip = mSelCon.get();
        uint32_t currentLength = currentValue.Length();
        uint32_t newlength = newValue.Length();
        if (!currentLength ||
            !StringBeginsWith(newValue, currentValue)) {
          // Replace the whole text.
          currentLength = 0;
          mSelCon->SelectAll();
        } else {
          // Collapse selection to the end so that we can append data.
          mBoundFrame->SelectAllOrCollapseToEndOfText(false);
        }
        const nsAString& insertValue =
          StringTail(newValue, newlength - currentLength);
        nsCOMPtr<nsIPlaintextEditor> plaintextEditor = do_QueryInterface(mEditor);
        if (!plaintextEditor || !weakFrame.IsAlive()) {
          NS_WARNING("Somehow not a plaintext editor?");
          return true;
        }

        valueSetter.Init();

        // get the flags, remove readonly and disabled, set the value,
        // restore flags
        uint32_t flags, savedFlags;
        mEditor->GetFlags(&savedFlags);
        flags = savedFlags;
        flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
        flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
        flags |= nsIPlaintextEditor::eEditorDontEchoPassword;
        mEditor->SetFlags(flags);

        mTextListener->SettingValue(true);
        bool notifyValueChanged = !!(aFlags & eSetValue_Notify);
        mTextListener->SetValueChanged(notifyValueChanged);

        // Also don't enforce max-length here
        int32_t savedMaxLength;
        plaintextEditor->GetMaxTextLength(&savedMaxLength);
        plaintextEditor->SetMaxTextLength(-1);

        if (insertValue.IsEmpty()) {
          mEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
        } else {
          plaintextEditor->InsertText(insertValue);
        }

        mTextListener->SetValueChanged(true);
        mTextListener->SettingValue(false);

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

        if (!IsSingleLineTextControl()) {
          if (!mCachedValue.Assign(newValue, fallible)) {
            return false;
          }
        }

        plaintextEditor->SetMaxTextLength(savedMaxLength);
        mEditor->SetFlags(savedFlags);
        if (selPriv)
          selPriv->EndBatchChanges();
      }
    }
  } else {
    if (!mValue) {
      mValue.emplace();
    }
    nsString value;
    if (!value.Assign(newValue, fallible)) {
      return false;
    }
    if (!nsContentUtils::PlatformToDOMLineBreaks(value, fallible)) {
      return false;
    }
    if (!mValue->Assign(value, fallible)) {
      return false;
    }

    // Update the frame display if needed
    if (mBoundFrame) {
      mBoundFrame->UpdateValueDisplay(true);
    }
  }

  // If we've reached the point where the root node has been created, we
  // can assume that it's safe to notify.
  ValueWasChanged(!!mRootNode);

  mTextCtrlElement->OnValueChanged(/* aNotify = */ !!mRootNode,
                                   /* aWasInteractiveUserChange = */ false);

  return true;
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
  UpdatePlaceholderVisibility(aNotify);
}

void
nsTextEditorState::UpdatePlaceholderText(bool aNotify)
{
  NS_ASSERTION(mPlaceholderDiv, "This function should not be called if "
                                "mPlaceholderDiv isn't set");

  // If we don't have a placeholder div, there's nothing to do.
  if (!mPlaceholderDiv)
    return;

  nsAutoString placeholderValue;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  content->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder, placeholderValue);
  nsContentUtils::RemoveNewlines(placeholderValue);
  NS_ASSERTION(mPlaceholderDiv->GetFirstChild(), "placeholder div has no child");
  mPlaceholderDiv->GetFirstChild()->SetText(placeholderValue, aNotify);
}

void
nsTextEditorState::UpdatePlaceholderVisibility(bool aNotify)
{
  nsAutoString value;
  GetValue(value, true);

  mPlaceholderVisibility = value.IsEmpty();

  if (mPlaceholderVisibility &&
      !Preferences::GetBool("dom.placeholder.show_on_focus", true)) {
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
  bool isComposing = false;
  nsCOMPtr<nsIEditorIMESupport> editorIMESupport = do_QueryInterface(mEditor);
  return editorIMESupport &&
         NS_SUCCEEDED(editorIMESupport->GetComposing(&isComposing)) &&
         isComposing;
}

NS_IMPL_ISUPPORTS(nsAnonDivObserver, nsIMutationObserver)

void
nsAnonDivObserver::CharacterDataChanged(nsIDocument*             aDocument,
                                        nsIContent*              aContent,
                                        CharacterDataChangeInfo* aInfo)
{
  mTextEditorState->ClearValueCache();
}

void
nsAnonDivObserver::ContentAppended(nsIDocument* aDocument,
                                   nsIContent*  aContainer,
                                   nsIContent*  aFirstNewContent,
                                   int32_t      /* unused */)
{
  mTextEditorState->ClearValueCache();
}

void
nsAnonDivObserver::ContentInserted(nsIDocument* aDocument,
                                   nsIContent*  aContainer,
                                   nsIContent*  aChild,
                                   int32_t      /* unused */)
{
  mTextEditorState->ClearValueCache();
}

void
nsAnonDivObserver::ContentRemoved(nsIDocument* aDocument,
                                  nsIContent*  aContainer,
                                  nsIContent*  aChild,
                                  int32_t      aIndexInContainer,
                                  nsIContent*  aPreviousSibling)
{
  mTextEditorState->ClearValueCache();
}
