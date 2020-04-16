/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorBase.h"

#include "mozilla/DebugOnly.h"  // for DebugOnly
#include "mozilla/Encoding.h"   // for Encoding

#include <stdio.h>   // for nullptr, stdout
#include <string.h>  // for strcmp

#include "ChangeAttributeTransaction.h"       // for ChangeAttributeTransaction
#include "CompositionTransaction.h"           // for CompositionTransaction
#include "CreateElementTransaction.h"         // for CreateElementTransaction
#include "DeleteNodeTransaction.h"            // for DeleteNodeTransaction
#include "DeleteRangeTransaction.h"           // for DeleteRangeTransaction
#include "DeleteTextTransaction.h"            // for DeleteTextTransaction
#include "EditAggregateTransaction.h"         // for EditAggregateTransaction
#include "EditorEventListener.h"              // for EditorEventListener
#include "HTMLEditUtils.h"                    // for HTMLEditUtils
#include "InsertNodeTransaction.h"            // for InsertNodeTransaction
#include "InsertTextTransaction.h"            // for InsertTextTransaction
#include "JoinNodeTransaction.h"              // for JoinNodeTransaction
#include "PlaceholderTransaction.h"           // for PlaceholderTransaction
#include "SplitNodeTransaction.h"             // for SplitNodeTransaction
#include "mozilla/BasePrincipal.h"            // for BasePrincipal
#include "mozilla/CheckedInt.h"               // for CheckedInt
#include "mozilla/ComposerCommandsUpdater.h"  // for ComposerCommandsUpdater
#include "mozilla/ComputedStyle.h"            // for ComputedStyle
#include "mozilla/CSSEditUtils.h"             // for CSSEditUtils
#include "mozilla/EditAction.h"               // for EditSubAction
#include "mozilla/EditorDOMPoint.h"           // for EditorDOMPoint
#include "mozilla/EditorSpellCheck.h"         // for EditorSpellCheck
#include "mozilla/EditorUtils.h"              // for various helper classes.
#include "mozilla/EditTransactionBase.h"      // for EditTransactionBase
#include "mozilla/FlushType.h"                // for FlushType::Frames
#include "mozilla/IMEContentObserver.h"       // for IMEContentObserver
#include "mozilla/IMEStateManager.h"          // for IMEStateManager
#include "mozilla/InputEventOptions.h"        // for InputEventOptions
#include "mozilla/InternalMutationEvent.h"  // for NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED
#include "mozilla/mozalloc.h"               // for operator new, etc.
#include "mozilla/mozInlineSpellChecker.h"  // for mozInlineSpellChecker
#include "mozilla/mozSpellChecker.h"        // for mozSpellChecker
#include "mozilla/Preferences.h"            // for Preferences
#include "mozilla/PresShell.h"              // for PresShell
#include "mozilla/RangeBoundary.h"      // for RawRangeBoundary, RangeBoundary
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/ServoCSSParser.h"     // for ServoCSSParser
#include "mozilla/StaticPrefs_bidi.h"   // for StaticPrefs::bidi_*
#include "mozilla/StaticPrefs_dom.h"    // for StaticPrefs::dom_*
#include "mozilla/TextComposition.h"    // for TextComposition
#include "mozilla/TextInputListener.h"  // for TextInputListener
#include "mozilla/TextServicesDocument.h"  // for TextServicesDocument
#include "mozilla/TextEvents.h"
#include "mozilla/TransactionManager.h"  // for TransactionManager
#include "mozilla/Tuple.h"
#include "mozilla/dom/AbstractRange.h"  // for AbstractRange
#include "mozilla/dom/CharacterData.h"  // for CharacterData
#include "mozilla/dom/DataTransfer.h"   // for DataTransfer
#include "mozilla/dom/Element.h"        // for Element, nsINode::AsElement
#include "mozilla/dom/EventTarget.h"    // for EventTarget
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"    // for Selection, etc.
#include "mozilla/dom/StaticRange.h"  // for StaticRange
#include "mozilla/dom/Text.h"
#include "mozilla/dom/Event.h"
#include "nsAString.h"                // for nsAString::Length, etc.
#include "nsCCUncollectableMarker.h"  // for nsCCUncollectableMarker
#include "nsCaret.h"                  // for nsCaret
#include "nsCaseTreatment.h"
#include "nsCharTraits.h"              // for NS_IS_HIGH_SURROGATE, etc.
#include "nsComponentManagerUtils.h"   // for do_CreateInstance
#include "nsComputedDOMStyle.h"        // for nsComputedDOMStyle
#include "nsContentUtils.h"            // for nsContentUtils
#include "nsDOMString.h"               // for DOMStringIsNull
#include "nsDebug.h"                   // for NS_WARNING, etc.
#include "nsError.h"                   // for NS_OK, etc.
#include "nsFocusManager.h"            // for nsFocusManager
#include "nsFrameSelection.h"          // for nsFrameSelection
#include "nsGenericHTMLElement.h"      // for nsGenericHTMLElement
#include "nsGkAtoms.h"                 // for nsGkAtoms, nsGkAtoms::dir
#include "nsIAbsorbingTransaction.h"   // for nsIAbsorbingTransaction
#include "nsAtom.h"                    // for nsAtom
#include "nsIContent.h"                // for nsIContent
#include "mozilla/dom/Document.h"      // for Document
#include "nsIDocumentStateListener.h"  // for nsIDocumentStateListener
#include "nsIEditActionListener.h"     // for nsIEditActionListener
#include "nsIEditorObserver.h"         // for nsIEditorObserver
#include "nsIFrame.h"                  // for nsIFrame
#include "nsIInlineSpellChecker.h"     // for nsIInlineSpellChecker, etc.
#include "nsNameSpaceManager.h"        // for kNameSpaceID_None, etc.
#include "nsINode.h"                   // for nsINode, etc.
#include "nsISelectionController.h"    // for nsISelectionController, etc.
#include "nsISelectionDisplay.h"       // for nsISelectionDisplay, etc.
#include "nsISupportsBase.h"           // for nsISupports
#include "nsISupportsUtils.h"          // for NS_ADDREF, NS_IF_ADDREF
#include "nsITransferable.h"           // for nsITransferable
#include "nsITransaction.h"            // for nsITransaction
#include "nsITransactionManager.h"
#include "nsIWeakReference.h"  // for nsISupportsWeakReference
#include "nsIWidget.h"         // for nsIWidget, IMEState, etc.
#include "nsPIDOMWindow.h"     // for nsPIDOMWindow
#include "nsPresContext.h"     // for nsPresContext
#include "nsRange.h"           // for nsRange
#include "nsReadableUtils.h"   // for EmptyString, ToNewCString
#include "nsString.h"          // for nsAutoString, nsString, etc.
#include "nsStringFwd.h"       // for nsString
#include "nsStyleConsts.h"     // for StyleDirection::Rtl, etc.
#include "nsStyleStruct.h"     // for nsStyleDisplay, nsStyleText, etc.
#include "nsStyleStructFwd.h"  // for nsIFrame::StyleUIReset, etc.
#include "nsStyleUtil.h"       // for nsStyleUtil
#include "nsTextNode.h"        // for nsTextNode
#include "nsThreadUtils.h"     // for nsRunnable
#include "prtime.h"            // for PR_Now

class nsIOutputStream;
class nsITransferable;

namespace mozilla {

using namespace dom;
using namespace widget;

/*****************************************************************************
 * mozilla::EditorBase
 *****************************************************************************/
template EditActionResult EditorBase::SetCaretBidiLevelForDeletion(
    const EditorDOMPoint& aPointAtCaret,
    nsIEditor::EDirection aDirectionAndAmount) const;
template EditActionResult EditorBase::SetCaretBidiLevelForDeletion(
    const EditorRawDOMPoint& aPointAtCaret,
    nsIEditor::EDirection aDirectionAndAmount) const;

EditorBase::EditorBase()
    : mEditActionData(nullptr),
      mPlaceholderName(nullptr),
      mModCount(0),
      mFlags(0),
      mUpdateCount(0),
      mPlaceholderBatch(0),
      mWrapColumn(0),
      mNewlineHandling(nsIEditor::eNewlinesPasteToFirst),
      mDocDirtyState(-1),
      mSpellcheckCheckboxState(eTriUnset),
      mInitSucceeded(false),
      mAllowsTransactionsToChangeSelection(true),
      mDidPreDestroy(false),
      mDidPostCreate(false),
      mDispatchInputEvent(true),
      mIsInEditSubAction(false),
      mHidingCaret(false),
      mSpellCheckerDictionaryUpdated(true),
      mIsHTMLEditorClass(false) {}

EditorBase::~EditorBase() {
  MOZ_ASSERT(!IsInitialized() || mDidPreDestroy,
             "Why PreDestroy hasn't been called?");

  if (mComposition) {
    mComposition->OnEditorDestroyed();
    mComposition = nullptr;
  }
  // If this editor is still hiding the caret, we need to restore it.
  HideCaret(false);
  mTransactionManager = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(EditorBase)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(EditorBase)
  // Remove event listeners first since EditorEventListener may need
  // mDocument, mEventTarget, etc.
  if (tmp->mEventListener) {
    tmp->mEventListener->Disconnect();
    tmp->mEventListener = nullptr;
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPaddingBRElementForEmptyEditor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelectionController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIMEContentObserver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInlineSpellChecker)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTextServicesDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTextInputListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransactionManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mActionListeners)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditorObservers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocStateListeners)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPlaceholderTransaction)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EditorBase)
  Document* currentDoc =
      tmp->mRootElement ? tmp->mRootElement->GetUncomposedDoc() : nullptr;
  if (currentDoc && nsCCUncollectableMarker::InGeneration(
                        cb, currentDoc->GetMarkedCCGeneration())) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPaddingBRElementForEmptyEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelectionController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIMEContentObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInlineSpellChecker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTextServicesDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTextInputListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransactionManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActionListeners)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditorObservers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocStateListeners)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlaceholderTransaction)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditorBase)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIEditor)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(EditorBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EditorBase)

nsresult EditorBase::Init(Document& aDocument, Element* aRoot,
                          nsISelectionController* aSelectionController,
                          uint32_t aFlags, const nsAString& aValue) {
  MOZ_ASSERT(GetTopLevelEditSubAction() == EditSubAction::eNone,
             "Initializing during an edit action is an error");

  // First only set flags, but other stuff shouldn't be initialized now.
  // Note that SetFlags() will be called by PostCreate().
  mFlags = aFlags;

  mDocument = &aDocument;
  // HTML editors currently don't have their own selection controller,
  // so they'll pass null as aSelCon, and we'll get the selection controller
  // off of the presshell.
  nsCOMPtr<nsISelectionController> selectionController;
  if (aSelectionController) {
    mSelectionController = aSelectionController;
    selectionController = aSelectionController;
  } else {
    selectionController = GetPresShell();
  }
  MOZ_ASSERT(selectionController,
             "Selection controller should be available at this point");

  if (mEditActionData) {
    // During edit action, selection is cached. But this selection is invalid
    // now since selection controller is updated, so we have to update this
    // cache.
    Selection* selection = selectionController->GetSelection(
        nsISelectionController::SELECTION_NORMAL);
    NS_WARNING_ASSERTION(selection,
                         "SelectionController::GetSelection() failed");
    if (selection) {
      mEditActionData->UpdateSelectionCache(*selection);
    }
  }

  // set up root element if we are passed one.
  if (aRoot) {
    mRootElement = aRoot;
  }

  mUpdateCount = 0;

  // If this is an editor for <input> or <textarea>, the text node which
  // has composition string is always recreated with same content. Therefore,
  // we need to nodify mComposition of text node destruction and replacing
  // composing string when this receives eCompositionChange event next time.
  if (mComposition && mComposition->GetContainerTextNode() &&
      !mComposition->GetContainerTextNode()->IsInComposedDoc()) {
    mComposition->OnTextNodeRemoved();
  }

  // Show the caret.
  DebugOnly<nsresult> rvIgnored = selectionController->SetCaretReadOnly(false);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsISelectionController::SetCaretReadOnly(false) failed, but ignored");
  // Show all the selection reflected to user.
  rvIgnored =
      selectionController->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsISelectionController::SetSelectionFlags("
                       "nsISelectionDisplay::DISPLAY_ALL) failed, but ignored");

  MOZ_ASSERT(IsInitialized());

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  SelectionRefPtr()->AddSelectionListener(this);

  // Make sure that the editor will be destroyed properly
  mDidPreDestroy = false;
  // Make sure that the editor will be created properly
  mDidPostCreate = false;

  return NS_OK;
}

nsresult EditorBase::PostCreate() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Synchronize some stuff for the flags.  SetFlags() will initialize
  // something by the flag difference.  This is first time of that, so, all
  // initializations must be run.  For such reason, we need to invert mFlags
  // value first.
  mFlags = ~mFlags;
  nsresult rv = SetFlags(~mFlags);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::SetFlags() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // These operations only need to happen on the first PostCreate call
  if (!mDidPostCreate) {
    mDidPostCreate = true;

    // Set up listeners
    CreateEventListeners();
    nsresult rv = InstallEventListeners();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InstallEventListeners() failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    // nuke the modification count, so the doc appears unmodified
    // do this before we notify listeners
    DebugOnly<nsresult> rvIgnored = ResetModificationCount();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::ResetModificationCount() failed, but ignored");

    // update the UI with our state
    rvIgnored = NotifyDocumentListeners(eDocumentCreated);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "EditorBase::NotifyDocumentListeners(eDocumentCreated)"
                         " failed, but ignored");
    rvIgnored = NotifyDocumentListeners(eDocumentStateChanged);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "EditorBase::NotifyDocumentListeners("
                         "eDocumentStateChanged) failed, but ignored");
  }

  // update nsTextStateManager and caret if we have focus
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    DebugOnly<nsresult> rvIgnored = InitializeSelection(*focusedContent);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::InitializeSelection() failed, but ignored");

    // If the text control gets reframed during focus, Focus() would not be
    // called, so take a chance here to see if we need to spell check the text
    // control.
    mEventListener->SpellCheckIfNeeded();

    IMEState newState;
    nsresult rv = GetPreferredIMEState(&newState);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::GetPreferredIMEState() failed");
      return NS_OK;
    }
    // May be null in design mode
    nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
    IMEStateManager::UpdateIMEState(newState, content, this);
  }

  // FYI: This call might cause destroying this editor.
  IMEStateManager::OnEditorInitialized(*this);

  return NS_OK;
}

void EditorBase::SetTextInputListener(TextInputListener* aTextInputListener) {
  MOZ_ASSERT(!mTextInputListener || !aTextInputListener ||
             mTextInputListener == aTextInputListener);
  mTextInputListener = aTextInputListener;
}

void EditorBase::SetIMEContentObserver(
    IMEContentObserver* aIMEContentObserver) {
  MOZ_ASSERT(!mIMEContentObserver || !aIMEContentObserver ||
             mIMEContentObserver == aIMEContentObserver);
  mIMEContentObserver = aIMEContentObserver;
}

void EditorBase::CreateEventListeners() {
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new EditorEventListener();
  }
}

nsresult EditorBase::InstallEventListeners() {
  if (NS_WARN_IF(!IsInitialized()) || NS_WARN_IF(!mEventListener)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Initialize the event target.
  mEventTarget = GetExposedRoot();
  if (NS_WARN_IF(!mEventTarget)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = mEventListener->Connect(this);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorEventListener::Connect() failed");
  if (mComposition) {
    // If mComposition has already been destroyed, we should forget it.
    // This may happen if it ended while we don't listen to composition
    // events.
    if (mComposition->Destroyed()) {
      // XXX We may need to fix existing composition transaction here.
      //     However, this may be called when it's not safe.
      //     Perhaps, we should stop handling composition with events.
      mComposition = nullptr;
    }
    // Otherwise, Restart to handle composition with new editor contents.
    else {
      mComposition->StartHandlingComposition(this);
    }
  }
  return rv;
}

void EditorBase::RemoveEventListeners() {
  if (!IsInitialized() || !mEventListener) {
    return;
  }
  mEventListener->Disconnect();
  if (mComposition) {
    // Even if this is called, don't release mComposition because this is
    // may be reused after reframing.
    mComposition->EndHandlingComposition(this);
  }
  mEventTarget = nullptr;
}

bool EditorBase::GetDesiredSpellCheckState() {
  // Check user override on this element
  if (mSpellcheckCheckboxState != eTriUnset) {
    return (mSpellcheckCheckboxState == eTriTrue);
  }

  // Check user preferences
  int32_t spellcheckLevel = Preferences::GetInt("layout.spellcheckDefault", 1);

  if (!spellcheckLevel) {
    return false;  // Spellchecking forced off globally
  }

  if (!CanEnableSpellCheck()) {
    return false;
  }

  PresShell* presShell = GetPresShell();
  if (presShell) {
    nsPresContext* context = presShell->GetPresContext();
    if (context && !context->IsDynamic()) {
      return false;
    }
  }

  // Check DOM state
  nsCOMPtr<nsIContent> content = GetExposedRoot();
  if (!content) {
    return false;
  }

  auto element = nsGenericHTMLElement::FromNode(content);
  if (!element) {
    return false;
  }

  if (!IsPlaintextEditor()) {
    // Some of the page content might be editable and some not, if spellcheck=
    // is explicitly set anywhere, so if there's anything editable on the page,
    // return true and let the spellchecker figure it out.
    Document* doc = content->GetComposedDoc();
    return doc && doc->IsEditingOn();
  }

  return element->Spellcheck();
}

void EditorBase::PreDestroy(bool aDestroyingFrames) {
  if (mDidPreDestroy) {
    return;
  }

  if (IsPasswordEditor() && !AsTextEditor()->IsAllMasked()) {
    // Mask all range for now because if layout accessed the range, that
    // would cause showing password accidentary or crash.
    AsTextEditor()->MaskAllCharacters();
  }

  mInitSucceeded = false;

  Selection* selection = GetSelection();
  if (selection) {
    selection->RemoveSelectionListener(this);
  }

  IMEStateManager::OnEditorDestroying(*this);

  // Let spellchecker clean up its observers etc. It is important not to
  // actually free the spellchecker here, since the spellchecker could have
  // caused flush notifications, which could have gotten here if a textbox
  // is being removed. Setting the spellchecker to nullptr could free the
  // object that is still in use! It will be freed when the editor is
  // destroyed.
  if (mInlineSpellChecker) {
    DebugOnly<nsresult> rvIgnored =
        mInlineSpellChecker->Cleanup(aDestroyingFrames);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "mozInlineSpellChecker::Cleanup() failed, but ignored");
  }

  // tell our listeners that the doc is going away
  DebugOnly<nsresult> rvIgnored =
      NotifyDocumentListeners(eDocumentToBeDestroyed);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::NotifyDocumentListeners("
                       "eDocumentToBeDestroyed) failed, but ignored");

  // Unregister event listeners
  RemoveEventListeners();
  // If this editor is still hiding the caret, we need to restore it.
  HideCaret(false);
  mActionListeners.Clear();
  mEditorObservers.Clear();
  mDocStateListeners.Clear();
  mInlineSpellChecker = nullptr;
  mTextServicesDocument = nullptr;
  mTextInputListener = nullptr;
  mSpellcheckCheckboxState = eTriUnset;
  mRootElement = nullptr;
  mPaddingBRElementForEmptyEditor = nullptr;

  // Transaction may grab this instance.  Therefore, they should be released
  // here for stopping the circular reference with this instance.
  if (mTransactionManager) {
    DebugOnly<bool> disabledUndoRedo = DisableUndoRedo();
    NS_WARNING_ASSERTION(disabledUndoRedo,
                         "EditorBase::DisableUndoRedo() failed, but ignored");
    mTransactionManager = nullptr;
  }

  mDidPreDestroy = true;
}

NS_IMETHODIMP EditorBase::GetFlags(uint32_t* aFlags) {
  // NOTE: If you need to override this method, you need to make Flags()
  //       virtual.
  *aFlags = Flags();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::SetFlags(uint32_t aFlags) {
  if (mFlags == aFlags) {
    return NS_OK;
  }

  DebugOnly<bool> changingPasswordEditorFlagDynamically =
      mFlags != ~aFlags && ((mFlags ^ aFlags) & nsIEditor::eEditorPasswordMask);
  MOZ_ASSERT(
      !changingPasswordEditorFlagDynamically,
      "TextEditor does not support dynamic eEditorPasswordMask flag change");
  bool spellcheckerWasEnabled = CanEnableSpellCheck();
  mFlags = aFlags;

  if (!IsInitialized()) {
    // If we're initializing, we shouldn't do anything now.
    // SetFlags() will be called by PostCreate(),
    // we should synchronize some stuff for the flags at that time.
    return NS_OK;
  }

  // The flag change may cause the spellchecker state change
  if (CanEnableSpellCheck() != spellcheckerWasEnabled) {
    SyncRealTimeSpell();
  }

  // If this is called from PostCreate(), it will update the IME state if it's
  // necessary.
  if (!mDidPostCreate) {
    return NS_OK;
  }

  // Might be changing editable state, so, we need to reset current IME state
  // if we're focused and the flag change causes IME state change.
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    IMEState newState;
    nsresult rv = GetPreferredIMEState(&newState);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::GetPreferredIMEState() failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      // NOTE: When the enabled state isn't going to be modified, this method
      // is going to do nothing.
      nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
      IMEStateManager::UpdateIMEState(newState, content, this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetIsSelectionEditable(bool* aIsSelectionEditable) {
  if (NS_WARN_IF(!aIsSelectionEditable)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aIsSelectionEditable = IsSelectionEditable();
  return NS_OK;
}

bool EditorBase::IsSelectionEditable() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  if (IsTextEditor()) {
    // XXX we just check that the anchor node is editable at the moment
    //     we should check that all nodes in the selection are editable
    nsCOMPtr<nsINode> anchorNode = SelectionRefPtr()->GetAnchorNode();
    return anchorNode && anchorNode->IsContent() &&
           EditorUtils::IsEditableContent(*anchorNode->AsContent(),
                                          GetEditorType());
  }

  nsINode* anchorNode = SelectionRefPtr()->GetAnchorNode();
  nsINode* focusNode = SelectionRefPtr()->GetFocusNode();
  if (!anchorNode || !focusNode) {
    return false;
  }

  // Per the editing spec as of June 2012: we have to have a selection whose
  // start and end nodes are editable, and which share an ancestor editing
  // host.  (Bug 766387.)
  bool isSelectionEditable = SelectionRefPtr()->RangeCount() &&
                             anchorNode->IsEditable() &&
                             focusNode->IsEditable();
  if (!isSelectionEditable) {
    return false;
  }

  nsINode* commonAncestor = SelectionRefPtr()
                                ->GetAnchorFocusRange()
                                ->GetClosestCommonInclusiveAncestor();
  while (commonAncestor && !commonAncestor->IsEditable()) {
    commonAncestor = commonAncestor->GetParentNode();
  }
  // If there is no editable common ancestor, return false.
  return !!commonAncestor;
}

NS_IMETHODIMP EditorBase::GetIsDocumentEditable(bool* aIsDocumentEditable) {
  if (NS_WARN_IF(!aIsDocumentEditable)) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<Document> document = GetDocument();
  *aIsDocumentEditable = document && IsModifiable();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetDocument(Document** aDocument) {
  if (NS_WARN_IF(!aDocument)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aDocument = do_AddRef(mDocument).take();
  return NS_WARN_IF(!*aDocument) ? NS_ERROR_NOT_INITIALIZED : NS_OK;
}

already_AddRefed<nsIWidget> EditorBase::GetWidget() {
  nsPresContext* presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget = presContext->GetRootWidget();
  return NS_WARN_IF(!widget) ? nullptr : widget.forget();
}

NS_IMETHODIMP EditorBase::GetContentsMIMEType(char** aContentsMIMEType) {
  if (NS_WARN_IF(!aContentsMIMEType)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aContentsMIMEType = ToNewCString(mContentMIMEType);
  return NS_OK;
}

NS_IMETHODIMP EditorBase::SetContentsMIMEType(const char* aContentsMIMEType) {
  mContentMIMEType.Assign(aContentsMIMEType ? aContentsMIMEType : "");
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetSelectionController(
    nsISelectionController** aSelectionController) {
  if (NS_WARN_IF(!aSelectionController)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aSelectionController = do_AddRef(GetSelectionController()).take();
  return NS_WARN_IF(!*aSelectionController) ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP EditorBase::DeleteSelection(EDirection aAction,
                                          EStripWrappers aStripWrappers) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EditorBase::GetSelection(Selection** aSelection) {
  nsresult rv = GetSelection(SelectionType::eNormal, aSelection);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::GetSelection(SelectionType::eNormal) failed");
  return rv;
}

nsresult EditorBase::GetSelection(SelectionType aSelectionType,
                                  Selection** aSelection) const {
  if (NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (IsEditActionDataAvailable()) {
    *aSelection = do_AddRef(SelectionRefPtr()).take();
    return NS_OK;
  }
  nsISelectionController* selectionController = GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    *aSelection = nullptr;
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aSelection = do_AddRef(selectionController->GetSelection(
                              ToRawSelectionType(aSelectionType)))
                    .take();
  return NS_WARN_IF(!*aSelection) ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP EditorBase::DoTransaction(nsITransaction* aTxn) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eUnknown);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }
  // This is a low level API.  So, the caller might require raw error code.
  // Therefore, don't need to use EditorBase::ToGenericNSResult().
  nsresult rv = DoTransactionInternal(aTxn);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DoTransactionInternal() failed");
  return rv;
}

nsresult EditorBase::DoTransactionInternal(nsITransaction* aTxn) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!NeedsToDispatchBeforeInputEvent(),
             "beforeinput event hasn't been dispatched yet");

  if (mPlaceholderBatch && !mPlaceholderTransaction) {
    mPlaceholderTransaction = PlaceholderTransaction::Create(
        *this, mPlaceholderName, std::move(mSelState));
    MOZ_ASSERT(mSelState.isNothing());

    // We will recurse, but will not hit this case in the nested call
    RefPtr<PlaceholderTransaction> placeholderTransaction =
        mPlaceholderTransaction;
    DebugOnly<nsresult> rvIgnored =
        DoTransactionInternal(placeholderTransaction);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::DoTransactionInternal() failed, but ignored");

    if (mTransactionManager) {
      nsCOMPtr<nsITransaction> topTransaction =
          mTransactionManager->PeekUndoStack();
      nsCOMPtr<nsIAbsorbingTransaction> topAbsorbingTransaction =
          do_QueryInterface(topTransaction);
      if (topAbsorbingTransaction) {
        RefPtr<PlaceholderTransaction> topPlaceholderTransaction =
            topAbsorbingTransaction->AsPlaceholderTransaction();
        if (topPlaceholderTransaction) {
          // there is a placeholder transaction on top of the undo stack.  It
          // is either the one we just created, or an earlier one that we are
          // now merging into.  From here on out remember this placeholder
          // instead of the one we just created.
          mPlaceholderTransaction = topPlaceholderTransaction;
        }
      }
    }
  }

  if (aTxn) {
    // XXX: Why are we doing selection specific batching stuff here?
    // XXX: Most entry points into the editor have auto variables that
    // XXX: should trigger Begin/EndUpdateViewBatch() calls that will make
    // XXX: these selection batch calls no-ops.
    // XXX:
    // XXX: I suspect that this was placed here to avoid multiple
    // XXX: selection changed notifications from happening until after
    // XXX: the transaction was done. I suppose that can still happen
    // XXX: if an embedding application called DoTransaction() directly
    // XXX: to pump its own transactions through the system, but in that
    // XXX: case, wouldn't we want to use Begin/EndUpdateViewBatch() or
    // XXX: its auto equivalent AutoUpdateViewBatch to ensure that
    // XXX: selection listeners have access to accurate frame data?
    // XXX:
    // XXX: Note that if we did add Begin/EndUpdateViewBatch() calls
    // XXX: we will need to make sure that they are disabled during
    // XXX: the init of the editor for text widgets to avoid layout
    // XXX: re-entry during initial reflow. - kin

    // get the selection and start a batch change
    SelectionBatcher selectionBatcher(SelectionRefPtr());

    if (mTransactionManager) {
      RefPtr<TransactionManager> transactionManager(mTransactionManager);
      nsresult rv = transactionManager->DoTransaction(aTxn);
      if (NS_FAILED(rv)) {
        NS_WARNING("TransactionManager::DoTransaction() failed");
        return rv;
      }
    } else {
      nsresult rv = aTxn->DoTransaction();
      if (NS_FAILED(rv)) {
        NS_WARNING("nsITransaction::DoTransaction() failed");
        return rv;
      }
    }

    DoAfterDoTransaction(aTxn);
  }

  return NS_OK;
}

NS_IMETHODIMP EditorBase::EnableUndo(bool aEnable) {
  // XXX Should we return NS_ERROR_FAILURE if EdnableUndoRedo() or
  //     DisableUndoRedo() returns false?
  if (aEnable) {
    DebugOnly<bool> enabledUndoRedo = EnableUndoRedo();
    NS_WARNING_ASSERTION(enabledUndoRedo,
                         "EditorBase::EnableUndoRedo() failed, but ignored");
    return NS_OK;
  }
  DebugOnly<bool> disabledUndoRedo = DisableUndoRedo();
  NS_WARNING_ASSERTION(disabledUndoRedo,
                       "EditorBase::DisableUndoRedo() failed, but ignored");
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetTransactionManager(
    nsITransactionManager** aTransactionManager) {
  if (NS_WARN_IF(!aTransactionManager)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!mTransactionManager)) {
    return NS_ERROR_FAILURE;
  }
  *aTransactionManager = do_AddRef(mTransactionManager).take();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::Undo(uint32_t aCount) {
  nsresult rv = MOZ_KnownLive(AsTextEditor())->UndoAsAction(aCount);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "TextEditor::UndoAsAction() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::CanUndo(bool* aIsEnabled, bool* aCanUndo) {
  if (NS_WARN_IF(!aIsEnabled) || NS_WARN_IF(!aCanUndo)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanUndo = CanUndo();
  *aIsEnabled = IsUndoRedoEnabled();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::Redo(uint32_t aCount) {
  nsresult rv = MOZ_KnownLive(AsTextEditor())->RedoAsAction(aCount);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "TextEditor::RedoAsAction() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::CanRedo(bool* aIsEnabled, bool* aCanRedo) {
  if (NS_WARN_IF(!aIsEnabled) || NS_WARN_IF(!aCanRedo)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanRedo = CanRedo();
  *aIsEnabled = IsUndoRedoEnabled();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::BeginTransaction() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eUnknown);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  BeginTransactionInternal();
  return NS_OK;
}

void EditorBase::BeginTransactionInternal() {
  BeginUpdateViewBatch();

  if (NS_WARN_IF(!mTransactionManager)) {
    return;
  }

  RefPtr<TransactionManager> transactionManager(mTransactionManager);
  DebugOnly<nsresult> rvIgnored = transactionManager->BeginBatch(nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TransactionManager::BeginBatch() failed, but ignored");
}

NS_IMETHODIMP EditorBase::EndTransaction() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eUnknown);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  EndTransactionInternal();
  return NS_OK;
}

void EditorBase::EndTransactionInternal() {
  if (mTransactionManager) {
    RefPtr<TransactionManager> transactionManager(mTransactionManager);
    DebugOnly<nsresult> rvIgnored = transactionManager->EndBatch(false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TransactionManager::EndBatch() failed, but ignored");
  }

  EndUpdateViewBatch();
}

void EditorBase::BeginPlaceholderTransaction(nsAtom* aTransactionName) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch >= 0, "negative placeholder batch count!");

  if (!mPlaceholderBatch) {
    NotifyEditorObservers(eNotifyEditorObserversOfBefore);
    // time to turn on the batch
    BeginUpdateViewBatch();
    mPlaceholderTransaction = nullptr;
    mPlaceholderName = aTransactionName;
    mSelState.emplace();
    mSelState->SaveSelection(*SelectionRefPtr());
    // Composition transaction can modify multiple nodes and it merges text
    // node for ime into single text node.
    // So if current selection is into IME text node, it might be failed
    // to restore selection by UndoTransaction.
    // So we need update selection by range updater.
    if (mPlaceholderName == nsGkAtoms::IMETxnName) {
      RangeUpdaterRef().RegisterSelectionState(*mSelState);
    }
  }
  mPlaceholderBatch++;
}

void EditorBase::EndPlaceholderTransaction() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch > 0,
             "zero or negative placeholder batch count when ending batch!");

  if (mPlaceholderBatch == 1) {
    // By making the assumption that no reflow happens during the calls
    // to EndUpdateViewBatch and ScrollSelectionFocusIntoView, we are able to
    // allow the selection to cache a frame offset which is used by the
    // caret drawing code. We only enable this cache here; at other times,
    // we have no way to know whether reflow invalidates it
    // See bugs 35296 and 199412.
    SelectionRefPtr()->SetCanCacheFrameOffset(true);

    // time to turn off the batch
    EndUpdateViewBatch();
    // make sure selection is in view

    // After ScrollSelectionFocusIntoView(), the pending notifications might be
    // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
    // XXX Even if we're destroyed, we need to keep handling below because
    //     this method changes a lot of status.  We should rewrite this safer.
    DebugOnly<nsresult> rvIgnored = ScrollSelectionFocusIntoView();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::ScrollSelectionFocusIntoView() failed, but Ignored");

    // cached for frame offset are Not available now
    SelectionRefPtr()->SetCanCacheFrameOffset(false);

    if (mSelState) {
      // we saved the selection state, but never got to hand it to placeholder
      // (else we ould have nulled out this pointer), so destroy it to prevent
      // leaks.
      if (mPlaceholderName == nsGkAtoms::IMETxnName) {
        RangeUpdaterRef().DropSelectionState(*mSelState);
      }
      mSelState.reset();
    }
    // We might have never made a placeholder if no action took place.
    if (mPlaceholderTransaction) {
      DebugOnly<nsresult> rvIgnored =
          mPlaceholderTransaction->EndPlaceHolderBatch();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "PlaceholderTransaction::EndPlaceHolderBatch() failed, but ignored");
      // notify editor observers of action but if composing, it's done by
      // compositionchange event handler.
      if (!mComposition) {
        NotifyEditorObservers(eNotifyEditorObserversOfEnd);
      }
      mPlaceholderTransaction = nullptr;
    } else {
      NotifyEditorObservers(eNotifyEditorObserversOfCancel);
    }
  }
  mPlaceholderBatch--;
}

NS_IMETHODIMP EditorBase::SetShouldTxnSetSelection(bool aShould) {
  MakeThisAllowTransactionsToChangeSelection(aShould);
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetDocumentIsEmpty(bool* aDocumentIsEmpty) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// XXX: The rule system should tell us which node to select all on (ie, the
//      root, or the body)
NS_IMETHODIMP EditorBase::SelectAll() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = SelectAllInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SelectAllInternal() failed");
  // This is low level API for XUL applcation.  So, we should return raw
  // error code here.
  return rv;
}

nsresult EditorBase::SelectAllInternal() {
  MOZ_ASSERT(IsInitialized());

  DebugOnly<nsresult> rvIgnored = CommitComposition();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::CommitComposition() failed, but ignored");

  // XXX Do we need to keep handling after committing composition causes moving
  //     focus to different element?  Although TextEditor has independent
  //     selection, so, we may not see any odd behavior even in such case.

  nsresult rv = SelectEntireDocument();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::SelectEntireDocument() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::BeginningOfDocument() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // get the root element
  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  // find first editable thingy
  nsCOMPtr<nsINode> firstNode = GetFirstEditableNode(rootElement);
  if (!firstNode) {
    // just the root node, set selection to inside the root
    nsresult rv = SelectionRefPtr()->Collapse(rootElement, 0);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Selection::Collapse() failed");
    return rv;
  }

  if (firstNode->IsText()) {
    // If firstNode is text, set selection to beginning of the text node.
    nsresult rv = SelectionRefPtr()->Collapse(firstNode, 0);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Selection::Collapse() failed");
    return rv;
  }

  // Otherwise, it's a leaf node and we set the selection just in front of it.
  nsCOMPtr<nsIContent> parent = firstNode->GetParent();
  if (NS_WARN_IF(!parent)) {
    return NS_ERROR_NULL_POINTER;
  }

  MOZ_ASSERT(
      parent->ComputeIndexOf(firstNode) == 0,
      "How come the first node isn't the left most child in its parent?");
  nsresult rv = SelectionRefPtr()->Collapse(parent, 0);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Selection::Collapse() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::EndOfDocument() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv = CollapseSelectionToEnd();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionToEnd() failed");
  // This is low level API for XUL applcation.  So, we should return raw
  // error code here.
  return rv;
}

nsresult EditorBase::CollapseSelectionToEnd() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // XXX Why doesn't this check if the document is alive?
  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // get the root element
  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  nsINode* node = rootElement;
  nsINode* child = node->GetLastChild();
  while (child && IsContainer(child)) {
    node = child;
    child = node->GetLastChild();
  }

  uint32_t length = node->Length();
  nsresult rv = SelectionRefPtr()->Collapse(node, static_cast<int32_t>(length));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Selection::Collapse() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::GetDocumentModified(bool* aOutDocModified) {
  if (NS_WARN_IF(!aOutDocModified)) {
    return NS_ERROR_INVALID_ARG;
  }

  int32_t modCount = 0;
  DebugOnly<nsresult> rvIgnored = GetModificationCount(&modCount);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::GetModificationCount() failed, but ignored");

  *aOutDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetDocumentCharacterSet(nsACString& aCharset) {
  nsresult rv = GetDocumentCharsetInternal(aCharset);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::GetDocumentCharsetInternal() failed");
  return rv;
}

nsresult EditorBase::GetDocumentCharsetInternal(nsACString& aCharset) const {
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_UNEXPECTED;
  }
  document->GetDocumentCharacterSet()->Name(aCharset);
  return NS_OK;
}

NS_IMETHODIMP EditorBase::SetDocumentCharacterSet(
    const nsACString& characterSet) {
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_UNEXPECTED;
  }
  // This method is scriptable, so add-ons could pass in something other
  // than a canonical name.
  auto encoding = Encoding::ForLabelNoReplacement(characterSet);
  if (!encoding) {
    NS_WARNING("Encoding::ForLabelNoReplacement() failed");
    return NS_ERROR_INVALID_ARG;
  }
  document->SetDocumentCharacterSet(WrapNotNull(encoding));
  return NS_OK;
}

NS_IMETHODIMP EditorBase::Cut() {
  nsresult rv = MOZ_KnownLive(AsTextEditor())->CutAsAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "TextEditor::CutAsAction() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::CanCut(bool* aCanCut) {
  if (NS_WARN_IF(!aCanCut)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanCut = AsTextEditor()->IsCutCommandEnabled();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::Copy() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP EditorBase::CanCopy(bool* aCanCopy) {
  if (NS_WARN_IF(!aCanCopy)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanCopy = AsTextEditor()->IsCopyCommandEnabled();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::Paste(int32_t aClipboardType) {
  nsresult rv =
      MOZ_KnownLive(AsTextEditor())->PasteAsAction(aClipboardType, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "TextEditor::PasteAsAction() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::PasteTransferable(nsITransferable* aTransferable) {
  nsresult rv =
      MOZ_KnownLive(AsTextEditor())->PasteTransferableAsAction(aTransferable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::PasteTransferableAsAction() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::CanPaste(int32_t aClipboardType, bool* aCanPaste) {
  if (NS_WARN_IF(!aCanPaste)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanPaste = AsTextEditor()->CanPaste(aClipboardType);
  return NS_OK;
}

NS_IMETHODIMP EditorBase::SetAttribute(Element* aElement,
                                       const nsAString& aAttribute,
                                       const nsAString& aValue) {
  if (NS_WARN_IF(aAttribute.IsEmpty()) || NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  rv = SetAttributeWithTransaction(*aElement, *attribute, aValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::SetAttributeWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult EditorBase::SetAttributeWithTransaction(Element& aElement,
                                                 nsAtom& aAttribute,
                                                 const nsAString& aValue) {
  RefPtr<ChangeAttributeTransaction> transaction =
      ChangeAttributeTransaction::Create(aElement, aAttribute, aValue);
  nsresult rv = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DoTransactionInternal() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::RemoveAttribute(Element* aElement,
                                          const nsAString& aAttribute) {
  if (NS_WARN_IF(aAttribute.IsEmpty()) || NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveAttribute);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  rv = RemoveAttributeWithTransaction(*aElement, *attribute);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::RemoveAttributeWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult EditorBase::RemoveAttributeWithTransaction(Element& aElement,
                                                    nsAtom& aAttribute) {
  // XXX If aElement doesn't have aAttribute, shouldn't we stop creating
  //     the transaction?  Otherwise, there will be added a transaction
  //     which does nothing at doing undo/redo.
  RefPtr<ChangeAttributeTransaction> transaction =
      ChangeAttributeTransaction::CreateToRemove(aElement, aAttribute);
  nsresult rv = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DoTransactionInternal() failed");
  return rv;
}

nsresult EditorBase::MarkElementDirty(Element& aElement) {
  // Mark the node dirty, but not for webpages (bug 599983)
  if (!OutputsMozDirty()) {
    return NS_OK;
  }
  DebugOnly<nsresult> rvIgnored = aElement.SetAttr(
      kNameSpaceID_None, nsGkAtoms::mozdirty, EmptyString(), false);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::SetAttr(nsGkAtoms::mozdirty) failed, but ignored");
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

NS_IMETHODIMP EditorBase::GetInlineSpellChecker(
    bool aAutoCreate, nsIInlineSpellChecker** aInlineSpellChecker) {
  if (NS_WARN_IF(!aInlineSpellChecker)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mDidPreDestroy) {
    // Don't allow people to get or create the spell checker once the editor
    // is going away.
    *aInlineSpellChecker = nullptr;
    return aAutoCreate ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  // We don't want to show the spell checking UI if there are no spell check
  // dictionaries available.
  if (!mozInlineSpellChecker::CanEnableInlineSpellChecking()) {
    *aInlineSpellChecker = nullptr;
    return NS_ERROR_FAILURE;
  }

  if (!mInlineSpellChecker && aAutoCreate) {
    mInlineSpellChecker = new mozInlineSpellChecker();
  }

  if (mInlineSpellChecker) {
    nsresult rv = mInlineSpellChecker->Init(this);
    if (NS_FAILED(rv)) {
      NS_WARNING("mozInlineSpellChecker::Init() failed");
      mInlineSpellChecker = nullptr;
      return rv;
    }
  }

  *aInlineSpellChecker = do_AddRef(mInlineSpellChecker).take();
  return NS_OK;
}

void EditorBase::SyncRealTimeSpell() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  bool enable = GetDesiredSpellCheckState();

  // Initializes mInlineSpellChecker
  nsCOMPtr<nsIInlineSpellChecker> spellChecker;
  DebugOnly<nsresult> rvIgnored =
      GetInlineSpellChecker(enable, getter_AddRefs(spellChecker));
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::GetInlineSpellChecker() failed, but ignored");

  if (mInlineSpellChecker) {
    if (!mSpellCheckerDictionaryUpdated && enable) {
      DebugOnly<nsresult> rvIgnored =
          mInlineSpellChecker->UpdateCurrentDictionary();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "mozInlineSpellChecker::UpdateCurrentDictionary() "
                           "failed, but ignored");
      mSpellCheckerDictionaryUpdated = true;
    }

    // We might have a mInlineSpellChecker even if there are no dictionaries
    // available since we don't destroy the mInlineSpellChecker when the last
    // dictionariy is removed, but in that case spellChecker is null
    DebugOnly<nsresult> rvIgnored =
        mInlineSpellChecker->SetEnableRealTimeSpell(enable && spellChecker);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "mozInlineSpellChecker::SetEnableRealTimeSpell() failed, but ignored");
  }
}

NS_IMETHODIMP EditorBase::SetSpellcheckUserOverride(bool enable) {
  mSpellcheckCheckboxState = enable ? eTriTrue : eTriFalse;
  SyncRealTimeSpell();
  return NS_OK;
}

already_AddRefed<Element> EditorBase::CreateNodeWithTransaction(
    nsAtom& aTagName, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  // XXX We need offset at new node for RangeUpdaterRef().  Therefore, we need
  //     to compute the offset now but this is expensive.  So, if it's possible,
  //     we need to redesign RangeUpdaterRef() as avoiding using indices.
  Unused << aPointToInsert.Offset();

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eCreateNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return nullptr;
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  RefPtr<Element> newElement;

  RefPtr<CreateElementTransaction> transaction =
      CreateElementTransaction::Create(*this, aTagName, aPointToInsert);
  nsresult rv = DoTransactionInternal(transaction);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DoTransactionInternal() failed");
    // XXX Why do we do this even when DoTransaction() returned error?
    DebugOnly<nsresult> rvIgnored =
        RangeUpdaterRef().SelAdjCreateNode(aPointToInsert);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Rangeupdater::SelAdjCreateNode() failed, but ignored");
  } else {
    newElement = transaction->GetNewElement();
    MOZ_ASSERT(newElement);

    // If we succeeded to create and insert new element, we need to adjust
    // ranges in RangeUpdaterRef().  It currently requires offset of the new
    // node.  So, let's call it with original offset.  Note that if
    // aPointToInsert stores child node, it may not be at the offset since new
    // element must be inserted before the old child.  Although, mutation
    // observer can do anything, but currently, we don't check it.
    DebugOnly<nsresult> rvIgnored =
        RangeUpdaterRef().SelAdjCreateNode(EditorRawDOMPoint(
            aPointToInsert.GetContainer(), aPointToInsert.Offset()));
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Rangeupdater::SelAdjCreateNode() failed, but ignored");
  }

  if (AsHTMLEditor() && newElement) {
    TopLevelEditSubActionDataRef().DidCreateElement(*this, *newElement);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored = listener->DidCreateNode(
          nsDependentAtomString(&aTagName), newElement, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidCreateNode() failed, but ignored");
    }
  }

  return newElement.forget();
}

NS_IMETHODIMP EditorBase::InsertNode(nsINode* aNodeToInsert,
                                     nsINode* aContainer, int32_t aOffset) {
  nsCOMPtr<nsIContent> contentToInsert = do_QueryInterface(aNodeToInsert);
  if (NS_WARN_IF(!contentToInsert) || NS_WARN_IF(!aContainer)) {
    return NS_ERROR_NULL_POINTER;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertNode);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  int32_t offset =
      aOffset < 0
          ? static_cast<int32_t>(aContainer->Length())
          : std::min(aOffset, static_cast<int32_t>(aContainer->Length()));
  rv = InsertNodeWithTransaction(*contentToInsert,
                                 EditorDOMPoint(aContainer, offset));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertNodeWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult EditorBase::InsertNodeWithTransaction(
    nsIContent& aContentToInsert, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  RefPtr<InsertNodeTransaction> transaction =
      InsertNodeTransaction::Create(*this, aContentToInsert, aPointToInsert);
  nsresult rv = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DoTransactionInternal() failed");

  DebugOnly<nsresult> rvIgnored =
      RangeUpdaterRef().SelAdjInsertNode(aPointToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "RangeUpdater::SelAdjInsertNode() failed, but ignored");

  if (AsHTMLEditor()) {
    TopLevelEditSubActionDataRef().DidInsertContent(*this, aContentToInsert);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored =
          listener->DidInsertNode(&aContentToInsert, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidInsertNode() failed, but ignored");
    }
  }

  return rv;
}

EditorDOMPoint EditorBase::PrepareToInsertBRElement(
    const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return EditorDOMPoint();
  }

  if (!aPointToInsert.IsInTextNode()) {
    return aPointToInsert;
  }

  if (aPointToInsert.IsStartOfContainer()) {
    // Insert before the text node.
    EditorDOMPoint pointInContainer(aPointToInsert.GetContainer());
    NS_WARNING_ASSERTION(pointInContainer.IsSet(),
                         "Failed to climb up the DOM tree from text node");
    return pointInContainer;
  }

  if (aPointToInsert.IsEndOfContainer()) {
    // Insert after the text node.
    EditorDOMPoint pointInContainer(aPointToInsert.GetContainer());
    if (NS_WARN_IF(!pointInContainer.IsSet())) {
      return pointInContainer;
    }
    DebugOnly<bool> advanced = pointInContainer.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset to after the text node");
    return pointInContainer;
  }

  MOZ_DIAGNOSTIC_ASSERT(aPointToInsert.IsSetAndValid());

  // Unfortunately, we need to split the text node at the offset.
  IgnoredErrorResult ignoredError;
  nsCOMPtr<nsIContent> newLeftNode =
      SplitNodeWithTransaction(aPointToInsert, ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "EditorBase::SplitNodeWithTransaction() failed");
  if (ignoredError.Failed()) {
    return EditorDOMPoint();
  }
  Unused << newLeftNode;
  // Insert new <br> before the right node.
  EditorDOMPoint pointInContainer(aPointToInsert.GetContainer());
  NS_WARNING_ASSERTION(pointInContainer.IsSet(),
                       "Failed to split the text node");
  return pointInContainer;
}

CreateElementResult
EditorBase::InsertPaddingBRElementForEmptyLastLineWithTransaction(
    const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditorDOMPoint pointToInsert = PrepareToInsertBRElement(aPointToInsert);
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }

  RefPtr<Element> newBRElement = CreateHTMLContent(nsGkAtoms::br);
  if (NS_WARN_IF(!newBRElement)) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }
  newBRElement->SetFlags(NS_PADDING_FOR_EMPTY_LAST_LINE);

  nsresult rv = InsertNodeWithTransaction(*newBRElement, pointToInsert);
  if (NS_WARN_IF(Destroyed())) {
    return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return CreateElementResult(rv);
  }

  return CreateElementResult(std::move(newBRElement));
}

already_AddRefed<nsIContent> EditorBase::SplitNodeWithTransaction(
    const EditorDOMPoint& aStartOfRightNode, ErrorResult& aError) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aStartOfRightNode.IsInContentNode())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }
  MOZ_ASSERT(aStartOfRightNode.IsSetAndValid());

  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSplitNode, nsIEditor::eNext, aError);
  if (NS_WARN_IF(aError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return nullptr;
  }
  NS_WARNING_ASSERTION(
      !aError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  aError.SuppressException();

  // XXX Unfortunately, storing offset of the split point in
  //     SplitNodeTransaction is necessary for now.  We should fix this
  //     in a follow up bug.
  Unused << aStartOfRightNode.Offset();

  RefPtr<SplitNodeTransaction> transaction =
      SplitNodeTransaction::Create(*this, aStartOfRightNode);
  aError = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(!aError.Failed(),
                       "EditorBase::DoTransactionInternal() failed");

  nsCOMPtr<nsIContent> newLeftContent = transaction->GetNewLeftContent();
  NS_WARNING_ASSERTION(newLeftContent, "Failed to create a new left node");

  if (newLeftContent) {
    // XXX Some other transactions manage range updater by themselves.
    //     Why doesn't SplitNodeTransaction do it?
    DebugOnly<nsresult> rvIgnored = RangeUpdaterRef().SelAdjSplitNode(
        *aStartOfRightNode.GetContainerAsContent(), *newLeftContent);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "RangeUpdater::SelAdjSplitNode() failed, but ignored");
  }
  if (AsHTMLEditor() && newLeftContent) {
    TopLevelEditSubActionDataRef().DidSplitContent(
        *this, *aStartOfRightNode.GetContainerAsContent(), *newLeftContent);
  }

  if (mInlineSpellChecker) {
    RefPtr<mozInlineSpellChecker> spellChecker = mInlineSpellChecker;
    spellChecker->DidSplitNode(aStartOfRightNode.GetContainer(),
                               newLeftContent);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored = listener->DidSplitNode(
          aStartOfRightNode.GetContainer(), newLeftContent);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidSplitNode() failed, but ignored");
    }
  }

  if (aError.Failed()) {
    return nullptr;
  }

  return newLeftContent.forget();
}

nsresult EditorBase::JoinNodesWithTransaction(nsINode& aLeftNode,
                                              nsINode& aRightNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aLeftNode.IsContent());
  MOZ_ASSERT(aRightNode.IsContent());

  nsCOMPtr<nsINode> parent = aLeftNode.GetParentNode();
  MOZ_ASSERT(parent);

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eJoinNodes, nsIEditor::ePrevious, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Remember some values; later used for saved selection updating.
  // Find the offset between the nodes to be joined.
  int32_t offset = parent->ComputeIndexOf(&aRightNode);
  // Find the number of children of the lefthand node
  uint32_t oldLeftNodeLen = aLeftNode.Length();

  if (AsHTMLEditor()) {
    TopLevelEditSubActionDataRef().WillJoinContents(
        *this, *aLeftNode.AsContent(), *aRightNode.AsContent());
  }

  RefPtr<JoinNodeTransaction> transaction = JoinNodeTransaction::MaybeCreate(
      *this, *aLeftNode.AsContent(), *aRightNode.AsContent());
  NS_WARNING_ASSERTION(
      transaction, "JoinNodeTransaction::MaybeCreate() failed, but ignored");

  nsresult rv = NS_OK;
  if (transaction) {
    rv = DoTransactionInternal(transaction);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::DoTransactionInternal() failed");
  }

  // XXX Some other transactions manage range updater by themselves.
  //     Why doesn't JoinNodeTransaction do it?
  DebugOnly<nsresult> rvIgnored =
      RangeUpdaterRef().SelAdjJoinNodes(aLeftNode, aRightNode, *parent, offset,
                                        static_cast<int32_t>(oldLeftNodeLen));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "RangeUpdater::SelAdjJoinNodes() failed, but ignored");

  if (AsHTMLEditor()) {
    TopLevelEditSubActionDataRef().DidJoinContents(
        *this, *aLeftNode.AsContent(), *aRightNode.AsContent());
  }

  if (mInlineSpellChecker) {
    RefPtr<mozInlineSpellChecker> spellChecker = mInlineSpellChecker;
    spellChecker->DidJoinNodes(aLeftNode, aRightNode);
  }

  if (mTextServicesDocument && NS_SUCCEEDED(rv)) {
    RefPtr<TextServicesDocument> textServicesDocument = mTextServicesDocument;
    textServicesDocument->DidJoinNodes(aLeftNode, aRightNode);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored =
          listener->DidJoinNodes(&aLeftNode, &aRightNode, parent, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidJoinNodes() failed, but ignored");
    }
  }

  return rv;
}

NS_IMETHODIMP EditorBase::DeleteNode(nsINode* aNode) {
  if (NS_WARN_IF(!aNode) || NS_WARN_IF(!aNode->IsContent())) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveNode);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = DeleteNodeWithTransaction(MOZ_KnownLive(*aNode->AsContent()));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DeleteNodeWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult EditorBase::DeleteNodeWithTransaction(nsIContent& aContent) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::ePrevious, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  if (AsHTMLEditor()) {
    TopLevelEditSubActionDataRef().WillDeleteContent(*this, aContent);
  }

  // FYI: DeleteNodeTransaction grabs aContent while it's alive.  So, it's safe
  //      to refer aContent even after calling DoTransaction().
  RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      DeleteNodeTransaction::MaybeCreate(*this, aContent);
  NS_WARNING_ASSERTION(deleteNodeTransaction,
                       "DeleteNodeTransaction::MaybeCreate() failed");
  nsresult rv;
  if (deleteNodeTransaction) {
    rv = DoTransactionInternal(deleteNodeTransaction);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::DoTransactionInternal() failed");

    if (mTextServicesDocument && NS_SUCCEEDED(rv)) {
      RefPtr<TextServicesDocument> textServicesDocument = mTextServicesDocument;
      textServicesDocument->DidDeleteNode(&aContent);
    }
  } else {
    rv = NS_ERROR_FAILURE;
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored = listener->DidDeleteNode(&aContent, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidDeleteNode() failed, but ignored");
    }
  }

  return rv;
}

already_AddRefed<Element> EditorBase::ReplaceContainerWithTransactionInternal(
    Element& aOldContainer, nsAtom& aTagName, nsAtom& aAttribute,
    const nsAString& aAttributeValue, bool aCloneAllAttributes) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditorDOMPoint atOldContainer(&aOldContainer);
  if (NS_WARN_IF(!atOldContainer.IsSet())) {
    return nullptr;
  }

  RefPtr<Element> newContainer = CreateHTMLContent(&aTagName);
  if (NS_WARN_IF(!newContainer)) {
    return nullptr;
  }

  // Set or clone attribute if needed.
  if (aCloneAllAttributes) {
    MOZ_ASSERT(&aAttribute == nsGkAtoms::_empty);
    CloneAttributesWithTransaction(*newContainer, aOldContainer);
  } else if (&aAttribute != nsGkAtoms::_empty) {
    nsresult rv = newContainer->SetAttr(kNameSpaceID_None, &aAttribute,
                                        aAttributeValue, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::SetAttr() failed");
      return nullptr;
    }
  }

  // Notify our internal selection state listener.
  // Note: An AutoSelectionRestorer object must be created before calling this
  // to initialize RangeUpdaterRef().
  AutoReplaceContainerSelNotify selStateNotify(RangeUpdaterRef(), aOldContainer,
                                               *newContainer);
  {
    AutoTransactionsConserveSelection conserveSelection(*this);
    // Move all children from the old container to the new container.
    while (aOldContainer.HasChildren()) {
      nsCOMPtr<nsIContent> child = aOldContainer.GetFirstChild();
      if (NS_WARN_IF(!child)) {
        return nullptr;
      }
      nsresult rv = DeleteNodeWithTransaction(*child);
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
        return nullptr;
      }

      rv = InsertNodeWithTransaction(
          *child, EditorDOMPoint(newContainer, newContainer->Length()));
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
        return nullptr;
      }
    }
  }

  // Insert new container into tree.
  NS_WARNING_ASSERTION(atOldContainer.IsSetAndValid(),
                       "The old container might be moved by mutation observer");
  nsresult rv = InsertNodeWithTransaction(*newContainer, atOldContainer);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return nullptr;
  }

  // Delete old container.
  rv = DeleteNodeWithTransaction(aOldContainer);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return nullptr;
  }

  return newContainer.forget();
}

nsresult EditorBase::RemoveContainerWithTransaction(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditorDOMPoint pointToInsertChildren(&aElement);
  if (NS_WARN_IF(!pointToInsertChildren.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Notify our internal selection state listener.
  AutoRemoveContainerSelNotify selNotify(RangeUpdaterRef(),
                                         pointToInsertChildren);

  // Move all children from aNode to its parent.
  while (aElement.HasChildren()) {
    nsCOMPtr<nsIContent> child = aElement.GetLastChild();
    if (NS_WARN_IF(!child)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return rv;
    }

    // Insert the last child before the previous last child.  So, we need to
    // use offset here because previous child might have been moved to
    // container.
    rv = InsertNodeWithTransaction(
        *child, EditorDOMPoint(pointToInsertChildren.GetContainer(),
                               pointToInsertChildren.Offset()));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return rv;
    }
  }

  nsresult rv = DeleteNodeWithTransaction(aElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DeleteNodeWithTransaction() failed");
  return rv;
}

already_AddRefed<Element> EditorBase::InsertContainerWithTransactionInternal(
    nsIContent& aContent, nsAtom& aTagName, nsAtom& aAttribute,
    const nsAString& aAttributeValue) {
  EditorDOMPoint pointToInsertNewContainer(&aContent);
  if (NS_WARN_IF(!pointToInsertNewContainer.IsSet())) {
    return nullptr;
  }
  // aContent will be moved to the new container before inserting the new
  // container.  So, when we insert the container, the insertion point
  // is before the next sibling of aContent.
  // XXX If pointerToInsertNewContainer stores offset here, the offset and
  //     referring child node become mismatched.  Although, currently this
  //     is not a problem since InsertNodeTransaction refers only child node.
  DebugOnly<bool> advanced = pointToInsertNewContainer.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced, "Failed to advance offset to after aContent");

  // Create new container.
  RefPtr<Element> newContainer = CreateHTMLContent(&aTagName);
  if (NS_WARN_IF(!newContainer)) {
    return nullptr;
  }

  // Set attribute if needed.
  if (&aAttribute != nsGkAtoms::_empty) {
    nsresult rv = newContainer->SetAttr(kNameSpaceID_None, &aAttribute,
                                        aAttributeValue, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::SetAttr() failed");
      return nullptr;
    }
  }

  // Notify our internal selection state listener
  AutoInsertContainerSelNotify selNotify(RangeUpdaterRef());

  // Put aNode in the new container, first.
  nsresult rv = DeleteNodeWithTransaction(aContent);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return nullptr;
  }

  {
    AutoTransactionsConserveSelection conserveSelection(*this);
    rv = InsertNodeWithTransaction(aContent, EditorDOMPoint(newContainer, 0));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return nullptr;
    }
  }

  // Put the new container where aNode was.
  rv = InsertNodeWithTransaction(*newContainer, pointToInsertNewContainer);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return nullptr;
  }

  return newContainer.forget();
}

nsresult EditorBase::MoveNodeWithTransaction(
    nsIContent& aContent, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  EditorDOMPoint oldPoint(&aContent);
  if (NS_WARN_IF(!oldPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Don't do anything if it's already in right place.
  if (aPointToInsert == oldPoint) {
    return NS_OK;
  }

  // Notify our internal selection state listener
  AutoMoveNodeSelNotify selNotify(RangeUpdaterRef(), oldPoint, aPointToInsert);

  // Hold a reference so aNode doesn't go away when we remove it (bug 772282)
  nsresult rv = DeleteNodeWithTransaction(aContent);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return rv;
  }

  // Mutation event listener could break insertion point. Let's check it.
  EditorDOMPoint pointToInsert(selNotify.ComputeInsertionPoint());
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  // If some children have removed from the container, let's append to the
  // container.
  // XXX Perhaps, if mutation event listener inserts or removes some children
  //     but the child node referring with aPointToInsert is still available,
  //     we should insert aContent before it.  However, we should keep
  //     traditional behavior for now.
  if (NS_WARN_IF(!pointToInsert.IsSetAndValid())) {
    pointToInsert.SetToEndOf(pointToInsert.GetContainer());
  }
  rv = InsertNodeWithTransaction(aContent, pointToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertNodeWithTransaction() failed");
  return rv;
}

void EditorBase::MoveAllChildren(nsINode& aContainer,
                                 const EditorRawDOMPoint& aPointToInsert,
                                 ErrorResult& aError) {
  MOZ_ASSERT(!aError.Failed());

  if (!aContainer.HasChildren()) {
    return;
  }
  nsIContent* firstChild = aContainer.GetFirstChild();
  if (NS_WARN_IF(!firstChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsIContent* lastChild = aContainer.GetLastChild();
  if (NS_WARN_IF(!lastChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  MoveChildren(*firstChild, *lastChild, aPointToInsert, aError);
  NS_WARNING_ASSERTION(!aError.Failed(), "EditorBase::MoveChildren() failed");
}

void EditorBase::MovePreviousSiblings(nsIContent& aChild,
                                      const EditorRawDOMPoint& aPointToInsert,
                                      ErrorResult& aError) {
  MOZ_ASSERT(!aError.Failed());

  if (NS_WARN_IF(!aChild.GetParentNode())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  nsIContent* firstChild = aChild.GetParentNode()->GetFirstChild();
  if (NS_WARN_IF(!firstChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsIContent* lastChild =
      &aChild == firstChild ? firstChild : aChild.GetPreviousSibling();
  if (NS_WARN_IF(!lastChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  MoveChildren(*firstChild, *lastChild, aPointToInsert, aError);
  NS_WARNING_ASSERTION(!aError.Failed(), "EditorBase::MoveChildren() failed");
}

void EditorBase::MoveChildren(nsIContent& aFirstChild, nsIContent& aLastChild,
                              const EditorRawDOMPoint& aPointToInsert,
                              ErrorResult& aError) {
  nsCOMPtr<nsINode> oldContainer = aFirstChild.GetParentNode();
  if (NS_WARN_IF(oldContainer != aLastChild.GetParentNode()) ||
      NS_WARN_IF(!aPointToInsert.IsSet()) ||
      NS_WARN_IF(!aPointToInsert.CanContainerHaveChildren())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // First, store all children which should be moved to the new container.
  AutoTArray<nsCOMPtr<nsIContent>, 10> children;
  for (nsIContent* child = &aFirstChild; child;
       child = child->GetNextSibling()) {
    children.AppendElement(child);
    if (child == &aLastChild) {
      break;
    }
  }

  if (NS_WARN_IF(children.LastElement() != &aLastChild)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsCOMPtr<nsINode> newContainer = aPointToInsert.GetContainer();
  nsCOMPtr<nsIContent> nextNode = aPointToInsert.GetChild();
  for (size_t i = children.Length(); i > 0; --i) {
    nsCOMPtr<nsIContent>& child = children[i - 1];
    if (child->GetParentNode() != oldContainer) {
      // If the child has been moved to different container, we shouldn't
      // touch it.
      continue;
    }
    oldContainer->RemoveChild(*child, aError);
    if (aError.Failed()) {
      NS_WARNING("nsINode::RemoveChild() failed");
      return;
    }
    if (nextNode) {
      // If we're not appending the children to the new container, we should
      // check if referring next node of insertion point is still in the new
      // container.
      EditorRawDOMPoint pointToInsert(nextNode);
      if (NS_WARN_IF(!pointToInsert.IsSet()) ||
          NS_WARN_IF(pointToInsert.GetContainer() != newContainer)) {
        // The next node of insertion point has been moved by mutation observer.
        // Let's stop moving the remaining nodes.
        // XXX Or should we move remaining children after the last moved child?
        aError.Throw(NS_ERROR_FAILURE);
        return;
      }
    }
    newContainer->InsertBefore(*child, nextNode, aError);
    if (aError.Failed()) {
      NS_WARNING("nsINode::InsertBefore() failed");
      return;
    }
    // If the child was inserted or appended properly, the following children
    // should be inserted before it.  Otherwise, keep using current position.
    if (child->GetParentNode() == newContainer) {
      nextNode = child;
    }
  }
}

NS_IMETHODIMP EditorBase::AddEditorObserver(nsIEditorObserver* aObserver) {
  // we don't keep ownership of the observers.  They must
  // remove themselves as observers before they are destroyed.

  if (NS_WARN_IF(!aObserver)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Make sure the listener isn't already on the list
  if (mEditorObservers.Contains(aObserver)) {
    return NS_OK;
  }

  mEditorObservers.AppendElement(*aObserver);
  NS_WARNING_ASSERTION(
      mEditorObservers.Length() != 1,
      "nsIEditorObserver installed, this editor becomes slower");
  return NS_OK;
}

NS_IMETHODIMP EditorBase::NotifySelectionChanged(Document* aDocument,
                                                 Selection* aSelection,
                                                 int16_t aReason) {
  if (NS_WARN_IF(!aDocument) || NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mTextInputListener) {
    RefPtr<TextInputListener> textInputListener = mTextInputListener;
    textInputListener->OnSelectionChange(*aSelection, aReason);
  }

  if (mIMEContentObserver) {
    RefPtr<IMEContentObserver> observer = mIMEContentObserver;
    observer->OnSelectionChange(*aSelection);
  }

  return NS_OK;
}

void EditorBase::NotifyEditorObservers(
    NotificationForEditorObservers aNotification) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  switch (aNotification) {
    case eNotifyEditorObserversOfEnd:
      mIsInEditSubAction = false;

      if (mTextInputListener) {
        // TODO: TextInputListener::OnEditActionHandled() may return
        //       NS_ERROR_OUT_OF_MEMORY.  If so and if
        //       TextControlState::SetValue() setting value with us, we should
        //       return the result to TextEditor::ReplaceTextAsAction(),
        //       TextEditor::DeleteSelectionAsAction() and
        //       TextEditor::InsertTextAsAction().  However, it requires a lot
        //       of changes in editor classes, but it's not so important since
        //       editor does not use fallible allocation.  Therefore, normally,
        //       the process must be crashed anyway.
        RefPtr<TextInputListener> listener = mTextInputListener;
        nsresult rv =
            listener->OnEditActionHandled(MOZ_KnownLive(*AsTextEditor()));
        MOZ_RELEASE_ASSERT(rv != NS_ERROR_OUT_OF_MEMORY,
                           "Setting value failed due to out of memory");
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "TextInputListener::OnEditActionHandled() failed, but ignored");
      }

      if (mIMEContentObserver) {
        RefPtr<IMEContentObserver> observer = mIMEContentObserver;
        observer->OnEditActionHandled();
      }

      if (!mEditorObservers.IsEmpty()) {
        // Copy the observers since EditAction()s can modify mEditorObservers.
        AutoEditorObserverArray observers(mEditorObservers);
        for (auto& observer : observers) {
          DebugOnly<nsresult> rvIgnored = observer->EditAction();
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rvIgnored),
              "nsIEditorObserver::EditAction() failed, but ignored");
        }
      }

      if (!mDispatchInputEvent || IsEditActionAborted() ||
          IsEditActionCanceled()) {
        return;
      }

      DispatchInputEvent();
      break;
    case eNotifyEditorObserversOfBefore:
      if (NS_WARN_IF(mIsInEditSubAction)) {
        break;
      }

      mIsInEditSubAction = true;

      if (mIMEContentObserver) {
        RefPtr<IMEContentObserver> observer = mIMEContentObserver;
        observer->BeforeEditAction();
      }
      break;
    case eNotifyEditorObserversOfCancel:
      mIsInEditSubAction = false;

      if (mIMEContentObserver) {
        RefPtr<IMEContentObserver> observer = mIMEContentObserver;
        observer->CancelEditAction();
      }
      break;
    default:
      MOZ_CRASH("Handle all notifications here");
      break;
  }
}

void EditorBase::DispatchInputEvent() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsEditActionCanceled(),
             "If preceding beforeinput event is canceled, we shouldn't "
             "dispatch input event");
  MOZ_ASSERT(
      !NeedsToDispatchBeforeInputEvent(),
      "We've not handled beforeinput event but trying to dispatch input event");

  // We don't need to dispatch multiple input events if there is a pending
  // input event.  However, it may have different event target.  If we resolved
  // this issue, we need to manage the pending events in an array.  But it's
  // overwork.  We don't need to do it for the very rare case.
  // TODO: However, we start to set InputEvent.inputType.  So, each "input"
  //       event now notifies web app each change.  So, perhaps, we should
  //       not omit input events.

  RefPtr<Element> targetElement = GetInputEventTargetElement();
  if (NS_WARN_IF(!targetElement)) {
    return;
  }
  RefPtr<TextEditor> textEditor = AsTextEditor();
  RefPtr<DataTransfer> dataTransfer = GetInputEventDataTransfer();
  DebugOnly<nsresult> rvIgnored = nsContentUtils::DispatchInputEvent(
      targetElement, eEditorInput, ToInputType(GetEditAction()), textEditor,
      dataTransfer ? InputEventOptions(dataTransfer)
                   : InputEventOptions(GetInputEventData()));
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsContentUtils::DispatchInputEvent() failed, but ignored");
}

NS_IMETHODIMP EditorBase::AddEditActionListener(
    nsIEditActionListener* aListener) {
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  // If given edit action listener is text services document for the inline
  // spell checker, store it as reference of concrete class for performance
  // reason.
  if (mInlineSpellChecker) {
    EditorSpellCheck* editorSpellCheck =
        mInlineSpellChecker->GetEditorSpellCheck();
    if (editorSpellCheck) {
      mozSpellChecker* spellChecker = editorSpellCheck->GetSpellChecker();
      if (spellChecker) {
        TextServicesDocument* textServicesDocument =
            spellChecker->GetTextServicesDocument();
        if (static_cast<nsIEditActionListener*>(textServicesDocument) ==
            aListener) {
          mTextServicesDocument = textServicesDocument;
          return NS_OK;
        }
      }
    }
  }

  // Make sure the listener isn't already on the list
  if (!mActionListeners.Contains(aListener)) {
    mActionListeners.AppendElement(*aListener);
    NS_WARNING_ASSERTION(
        mActionListeners.Length() != 1,
        "nsIEditActionListener installed, this editor becomes slower");
  }

  return NS_OK;
}

NS_IMETHODIMP EditorBase::RemoveEditActionListener(
    nsIEditActionListener* aListener) {
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (static_cast<nsIEditActionListener*>(mTextServicesDocument) == aListener) {
    mTextServicesDocument = nullptr;
    return NS_OK;
  }

  NS_WARNING_ASSERTION(mActionListeners.Length() != 1,
                       "All nsIEditActionListeners have been removed, this "
                       "editor becomes faster");
  mActionListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP EditorBase::AddDocumentStateListener(
    nsIDocumentStateListener* aListener) {
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!mDocStateListeners.Contains(aListener)) {
    mDocStateListeners.AppendElement(*aListener);
  }

  return NS_OK;
}

NS_IMETHODIMP EditorBase::RemoveDocumentStateListener(
    nsIDocumentStateListener* aListener) {
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mDocStateListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP EditorBase::OutputToString(const nsAString& aFormatType,
                                         uint32_t aFlags,
                                         nsAString& aOutputString) {
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool EditorBase::ArePreservingSelection() {
  return IsEditActionDataAvailable() && !SavedSelectionRef().IsEmpty();
}

void EditorBase::PreserveSelectionAcrossActions() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SavedSelectionRef().SaveSelection(*SelectionRefPtr());
  RangeUpdaterRef().RegisterSelectionState(SavedSelectionRef());
}

nsresult EditorBase::RestorePreservedSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (SavedSelectionRef().IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  DebugOnly<nsresult> rvIgnored =
      SavedSelectionRef().RestoreSelection(*SelectionRefPtr());
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "SelectionState::RestoreSelection() failed, but ignored");
  StopPreservingSelection();
  return NS_OK;
}

void EditorBase::StopPreservingSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RangeUpdaterRef().DropSelectionState(SavedSelectionRef());
  SavedSelectionRef().Clear();
}

NS_IMETHODIMP EditorBase::ForceCompositionEnd() {
  nsresult rv = CommitComposition();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CommitComposition() failed");
  return rv;
}

nsresult EditorBase::CommitComposition() {
  nsPresContext* presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mComposition) {
    return NS_OK;
  }
  nsresult rv =
      IMEStateManager::NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, presContext);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "IMEStateManager::NotifyIME() failed");
  return rv;
}

nsresult EditorBase::GetPreferredIMEState(IMEState* aState) {
  if (NS_WARN_IF(!aState)) {
    return NS_ERROR_INVALID_ARG;
  }

  aState->mEnabled = IMEState::ENABLED;
  aState->mOpen = IMEState::DONT_CHANGE_OPEN_STATE;

  if (IsReadonly()) {
    aState->mEnabled = IMEState::DISABLED;
    return NS_OK;
  }

  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* frameForRootElement = rootElement->GetPrimaryFrame();
  if (NS_WARN_IF(!frameForRootElement)) {
    return NS_ERROR_FAILURE;
  }

  switch (frameForRootElement->StyleUIReset()->mIMEMode) {
    case StyleImeMode::Auto:
      if (IsPasswordEditor()) {
        aState->mEnabled = IMEState::PASSWORD;
      }
      break;
    case StyleImeMode::Disabled:
      // we should use password state for |ime-mode: disabled;|.
      aState->mEnabled = IMEState::PASSWORD;
      break;
    case StyleImeMode::Active:
      aState->mOpen = IMEState::OPEN;
      break;
    case StyleImeMode::Inactive:
      aState->mOpen = IMEState::CLOSED;
      break;
    case StyleImeMode::Normal:
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetComposing(bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = IsIMEComposing();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetRootElement(Element** aRootElement) {
  if (NS_WARN_IF(!aRootElement)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aRootElement = do_AddRef(mRootElement).take();
  return NS_WARN_IF(!*aRootElement) ? NS_ERROR_NOT_AVAILABLE : NS_OK;
}

void EditorBase::OnStartToHandleTopLevelEditSubAction(
    EditSubAction aTopLevelEditSubAction,
    nsIEditor::EDirection aDirectionOfTopLevelEditSubAction, ErrorResult& aRv) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!aRv.Failed());
  mEditActionData->SetTopLevelEditSubAction(aTopLevelEditSubAction,
                                            aDirectionOfTopLevelEditSubAction);
}

nsresult EditorBase::OnEndHandlingTopLevelEditSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  mEditActionData->SetTopLevelEditSubAction(EditSubAction::eNone, eNone);
  return NS_OK;
}

void EditorBase::DoInsertText(Text& aText, uint32_t aOffset,
                              const nsAString& aStringToInsert,
                              ErrorResult& aRv) {
  aText.InsertData(aOffset, aStringToInsert, aRv);
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
    return;
  }
  if (aRv.Failed()) {
    NS_WARNING("Text::InsertData() failed");
    return;
  }
  if (!AsHTMLEditor() && !aStringToInsert.IsEmpty()) {
    aRv = MOZ_KnownLive(AsTextEditor())
              ->DidInsertText(aText.TextLength(), aOffset,
                              aStringToInsert.Length());
    NS_WARNING_ASSERTION(!aRv.Failed(), "TextEditor::DidInsertText() failed");
  }
}

void EditorBase::DoDeleteText(Text& aText, uint32_t aOffset, uint32_t aCount,
                              ErrorResult& aRv) {
  if (!AsHTMLEditor() && aCount > 0) {
    AsTextEditor()->WillDeleteText(aText.TextLength(), aOffset, aCount);
  }
  aText.DeleteData(aOffset, aCount, aRv);
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
    return;
  }
  NS_WARNING_ASSERTION(!aRv.Failed(), "Text::DeleteData() failed");
}

void EditorBase::DoReplaceText(Text& aText, uint32_t aOffset, uint32_t aCount,
                               const nsAString& aStringToInsert,
                               ErrorResult& aRv) {
  if (!AsHTMLEditor() && aCount > 0) {
    AsTextEditor()->WillDeleteText(aText.TextLength(), aOffset, aCount);
  }
  aText.ReplaceData(aOffset, aCount, aStringToInsert, aRv);
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
    return;
  }
  if (aRv.Failed()) {
    NS_WARNING("Text::ReplaceData() failed");
    return;
  }
  if (!AsHTMLEditor() && !aStringToInsert.IsEmpty()) {
    aRv = MOZ_KnownLive(AsTextEditor())
              ->DidInsertText(aText.TextLength(), aOffset,
                              aStringToInsert.Length());
    NS_WARNING_ASSERTION(!aRv.Failed(), "TextEditor::DidInsertText() failed");
  }
}

void EditorBase::DoSetText(Text& aText, const nsAString& aStringToSet,
                           ErrorResult& aRv) {
  if (!AsHTMLEditor()) {
    uint32_t length = aText.TextLength();
    if (length > 0) {
      AsTextEditor()->WillDeleteText(length, 0, length);
    }
  }
  aText.SetData(aStringToSet, aRv);
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
    return;
  }
  if (aRv.Failed()) {
    NS_WARNING("Text::SetData() failed");
    return;
  }
  if (!AsHTMLEditor() && !aStringToSet.IsEmpty()) {
    aRv = MOZ_KnownLive(AsTextEditor())
              ->DidInsertText(aText.Length(), 0, aStringToSet.Length());
    NS_WARNING_ASSERTION(!aRv.Failed(), "TextEditor::DidInsertText() failed");
  }
}

nsresult EditorBase::CloneAttributeWithTransaction(nsAtom& aAttribute,
                                                   Element& aDestElement,
                                                   Element& aSourceElement) {
  nsAutoString attrValue;
  if (aSourceElement.GetAttr(kNameSpaceID_None, &aAttribute, attrValue)) {
    nsresult rv =
        SetAttributeWithTransaction(aDestElement, aAttribute, attrValue);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::SetAttributeWithTransaction() failed");
    return rv;
  }
  nsresult rv = RemoveAttributeWithTransaction(aDestElement, aAttribute);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::RemoveAttributeWithTransaction() failed");
  return rv;
}

NS_IMETHODIMP EditorBase::CloneAttributes(Element* aDestElement,
                                          Element* aSourceElement) {
  if (NS_WARN_IF(!aDestElement) || NS_WARN_IF(!aSourceElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  CloneAttributesWithTransaction(*aDestElement, *aSourceElement);

  return NS_OK;
}

void EditorBase::CloneAttributesWithTransaction(Element& aDestElement,
                                                Element& aSourceElement) {
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  // Use transaction system for undo only if destination is already in the
  // document
  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return;
  }

  OwningNonNull<Element> destElement(aDestElement);
  OwningNonNull<Element> sourceElement(aSourceElement);
  bool isDestElementInBody = rootElement->Contains(destElement);

  // Clear existing attributes
  RefPtr<nsDOMAttributeMap> destAttributes = destElement->Attributes();
  while (RefPtr<Attr> attr = destAttributes->Item(0)) {
    if (isDestElementInBody) {
      DebugOnly<nsresult> rvIgnored = RemoveAttributeWithTransaction(
          destElement, MOZ_KnownLive(*attr->NodeInfo()->NameAtom()));
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::RemoveAttributeWithTransaction() failed, but ignored");
    } else {
      DebugOnly<nsresult> rvIgnored = destElement->UnsetAttr(
          kNameSpaceID_None, attr->NodeInfo()->NameAtom(), true);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "Element::UnsetAttr() failed, but ignored");
    }
  }

  // Set just the attributes that the source element has
  RefPtr<nsDOMAttributeMap> sourceAttributes = sourceElement->Attributes();
  uint32_t sourceCount = sourceAttributes->Length();
  for (uint32_t i = 0; i < sourceCount; i++) {
    RefPtr<Attr> attr = sourceAttributes->Item(i);
    nsAutoString value;
    attr->GetValue(value);
    if (isDestElementInBody) {
      DebugOnly<nsresult> rvIgnored = SetAttributeOrEquivalent(
          destElement, MOZ_KnownLive(attr->NodeInfo()->NameAtom()), value,
          false);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::SetAttributeOrEquivalent() failed, but ignored");
    } else {
      // The element is not inserted in the document yet, we don't want to put
      // a transaction on the UndoStack
      DebugOnly<nsresult> rvIgnored = SetAttributeOrEquivalent(
          destElement, MOZ_KnownLive(attr->NodeInfo()->NameAtom()), value,
          true);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::SetAttributeOrEquivalent() failed, but ignored");
    }
  }
}

nsresult EditorBase::ScrollSelectionFocusIntoView() {
  nsISelectionController* selectionController = GetSelectionController();
  if (!selectionController) {
    return NS_OK;
  }

  DebugOnly<nsresult> rvIgnored = selectionController->ScrollSelectionIntoView(
      nsISelectionController::SELECTION_NORMAL,
      nsISelectionController::SELECTION_FOCUS_REGION,
      nsISelectionController::SCROLL_OVERFLOW_HIDDEN);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsISelectionController::ScrollSelectionIntoView() failed, but ignored");
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

EditorRawDOMPoint EditorBase::FindBetterInsertionPoint(
    const EditorRawDOMPoint& aPoint) {
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return aPoint;
  }

  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (aPoint.IsInTextNode()) {
    // There is no "better" insertion point.
    return aPoint;
  }

  if (!IsPlaintextEditor()) {
    // We cannot find "better" insertion point in HTML editor.
    // WARNING: When you add some code to find better node in HTML editor,
    //          you need to call this before calling InsertTextWithTransaction()
    //          in HTMLEditor.
    return aPoint;
  }

  RefPtr<Element> rootElement = GetRoot();
  if (aPoint.GetContainer() == rootElement) {
    // In some cases, aNode is the anonymous DIV, and offset is 0.  To avoid
    // injecting unneeded text nodes, we first look to see if we have one
    // available.  In that case, we'll just adjust node and offset accordingly.
    if (aPoint.IsStartOfContainer() && aPoint.GetContainer()->HasChildren() &&
        aPoint.GetContainer()->GetFirstChild()->IsText()) {
      return EditorRawDOMPoint(aPoint.GetContainer()->GetFirstChild(), 0);
    }

    // In some other cases, aNode is the anonymous DIV, and offset points to
    // the terminating padding <br> element for empty last line.  In that case,
    // we'll adjust aInOutNode and aInOutOffset to the preceding text node,
    // if any.
    if (!aPoint.IsStartOfContainer()) {
      if (AsHTMLEditor()) {
        // Fall back to a slow path that uses GetChildAt_Deprecated() for
        // Thunderbird's plaintext editor.
        nsIContent* child = aPoint.GetPreviousSiblingOfChild();
        if (child && child->IsText()) {
          if (NS_WARN_IF(child->Length() > INT32_MAX)) {
            return aPoint;
          }
          return EditorRawDOMPoint(child, child->Length());
        }
      } else {
        // If we're in a real plaintext editor, use a fast path that avoids
        // calling GetChildAt_Deprecated() which may perform a linear search.
        nsIContent* child = aPoint.GetContainer()->GetLastChild();
        while (child) {
          if (child->IsText()) {
            if (NS_WARN_IF(child->Length() > INT32_MAX)) {
              return aPoint;
            }
            return EditorRawDOMPoint(child, child->Length());
          }
          child = child->GetPreviousSibling();
        }
      }
    }
  }

  // Sometimes, aNode is the padding <br> element itself.  In that case, we'll
  // adjust the insertion point to the previous text node, if one exists, or
  // to the parent anonymous DIV.
  if (EditorBase::IsPaddingBRElementForEmptyLastLine(*aPoint.GetContainer()) &&
      aPoint.IsStartOfContainer()) {
    nsIContent* previousSibling = aPoint.GetContainer()->GetPreviousSibling();
    if (previousSibling && previousSibling->IsText()) {
      if (NS_WARN_IF(previousSibling->Length() > INT32_MAX)) {
        return aPoint;
      }
      return EditorRawDOMPoint(previousSibling, previousSibling->Length());
    }

    nsINode* parentOfContainer = aPoint.GetContainer()->GetParentNode();
    if (parentOfContainer && parentOfContainer == rootElement) {
      return EditorRawDOMPoint(parentOfContainer,
                               aPoint.GetContainerAsContent(), 0);
    }
  }

  return aPoint;
}

nsresult EditorBase::InsertTextWithTransaction(
    Document& aDocument, const nsAString& aStringToInsert,
    const EditorRawDOMPoint& aPointToInsert,
    EditorRawDOMPoint* aPointAfterInsertedString) {
  MOZ_ASSERT(
      ShouldHandleIMEComposition() || !AllowsTransactionsToChangeSelection(),
      "caller must have already used AutoTransactionsConserveSelection "
      "if this is not for updating composition string");

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  if (!ShouldHandleIMEComposition() && aStringToInsert.IsEmpty()) {
    if (aPointAfterInsertedString) {
      *aPointAfterInsertedString = aPointToInsert;
    }
    return NS_OK;
  }

  // This method doesn't support over INT32_MAX length text since aInOutOffset
  // is int32_t*.
  CheckedInt<int32_t> lengthToInsert(aStringToInsert.Length());
  if (NS_WARN_IF(!lengthToInsert.isValid())) {
    return NS_ERROR_INVALID_ARG;
  }

  // In some cases, the node may be the anonymous div element or a padding
  // <br> element for empty last line.  Let's try to look for better insertion
  // point in the nearest text node if there is.
  EditorDOMPoint pointToInsert = FindBetterInsertionPoint(aPointToInsert);

  // If a neighboring text node already exists, use that
  if (!pointToInsert.IsInTextNode()) {
    nsIContent* child = nullptr;
    if (!pointToInsert.IsStartOfContainer() &&
        (child = pointToInsert.GetPreviousSiblingOfChild()) &&
        child->IsText()) {
      pointToInsert.Set(child, child->Length());
    } else if (!pointToInsert.IsEndOfContainer() &&
               (child = pointToInsert.GetChild()) && child->IsText()) {
      pointToInsert.Set(child, 0);
    }
  }

  if (ShouldHandleIMEComposition()) {
    CheckedInt<int32_t> newOffset;
    if (!pointToInsert.IsInTextNode()) {
      // create a text node
      RefPtr<nsTextNode> newNode = CreateTextNode(EmptyString());
      if (NS_WARN_IF(!newNode)) {
        return NS_ERROR_FAILURE;
      }
      // then we insert it into the dom tree
      nsresult rv = InsertNodeWithTransaction(*newNode, pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
        return rv;
      }
      pointToInsert.Set(newNode, 0);
      newOffset = lengthToInsert;
    } else {
      newOffset = lengthToInsert + pointToInsert.Offset();
      if (NS_WARN_IF(!newOffset.isValid())) {
        return NS_ERROR_FAILURE;
      }
    }
    nsresult rv = InsertTextIntoTextNodeWithTransaction(
        aStringToInsert, EditorDOMPointInText(pointToInsert.ContainerAsText(),
                                              pointToInsert.Offset()));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertTextIntoTextNodeWithTransaction() failed");
      return rv;
    }
    if (aPointAfterInsertedString) {
      aPointAfterInsertedString->Set(pointToInsert.GetContainer(),
                                     newOffset.value());
      NS_WARNING_ASSERTION(
          aPointAfterInsertedString->IsSetAndValid(),
          "Failed to set aPointAfterInsertedString, but ignored");
    }
    return NS_OK;
  }

  if (pointToInsert.IsInTextNode()) {
    CheckedInt<int32_t> newOffset = lengthToInsert + pointToInsert.Offset();
    if (NS_WARN_IF(!newOffset.isValid())) {
      return NS_ERROR_FAILURE;
    }
    // we are inserting text into an existing text node.
    nsresult rv = InsertTextIntoTextNodeWithTransaction(
        aStringToInsert, EditorDOMPointInText(pointToInsert.ContainerAsText(),
                                              pointToInsert.Offset()));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertTextIntoTextNodeWithTransaction() failed");
      return rv;
    }
    if (aPointAfterInsertedString) {
      aPointAfterInsertedString->Set(pointToInsert.GetContainer(),
                                     newOffset.value());
      NS_WARNING_ASSERTION(
          aPointAfterInsertedString->IsSetAndValid(),
          "Failed to set aPointAfterInsertedString, but ignored");
    }
    return NS_OK;
  }

  // we are inserting text into a non-text node.  first we have to create a
  // textnode (this also populates it with the text)
  RefPtr<nsTextNode> newNode = CreateTextNode(aStringToInsert);
  if (NS_WARN_IF(!newNode)) {
    return NS_ERROR_FAILURE;
  }
  // then we insert it into the dom tree
  nsresult rv = InsertNodeWithTransaction(*newNode, pointToInsert);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return rv;
  }
  if (aPointAfterInsertedString) {
    aPointAfterInsertedString->Set(newNode, lengthToInsert.value());
    NS_WARNING_ASSERTION(
        aPointAfterInsertedString->IsSetAndValid(),
        "Failed to set aPointAfterInsertedString, but ignored");
  }
  return NS_OK;
}

static bool TextFragmentBeginsWithStringAtOffset(
    const nsTextFragment& aTextFragment, const int32_t aOffset,
    const nsAString& aString) {
  const uint32_t stringLength = aString.Length();

  if (aOffset + stringLength > aTextFragment.GetLength()) {
    return false;
  }

  if (aTextFragment.Is2b()) {
    return aString.Equals(aTextFragment.Get2b() + aOffset);
  }

  return aString.EqualsLatin1(aTextFragment.Get1b() + aOffset, stringLength);
}

static Tuple<EditorDOMPointInText, EditorDOMPointInText>
AdjustTextInsertionRange(const EditorDOMPointInText& aInsertedPoint,
                         const nsAString& aInsertedString) {
  if (TextFragmentBeginsWithStringAtOffset(
          aInsertedPoint.ContainerAsText()->TextFragment(),
          aInsertedPoint.Offset(), aInsertedString)) {
    return MakeTuple(aInsertedPoint,
                     EditorDOMPointInText(
                         aInsertedPoint.ContainerAsText(),
                         aInsertedPoint.Offset() + aInsertedString.Length()));
  }

  return MakeTuple(
      EditorDOMPointInText(aInsertedPoint.ContainerAsText(), 0),
      EditorDOMPointInText::AtEndOf(*aInsertedPoint.ContainerAsText()));
}

nsresult EditorBase::InsertTextIntoTextNodeWithTransaction(
    const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert, bool aSuppressIME) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  EditorDOMPointInText pointToInsert(aPointToInsert);
  RefPtr<EditTransactionBase> transaction;
  bool isIMETransaction = false;
  // aSuppressIME is used when editor must insert text, yet this text is not
  // part of the current IME operation. Example: adjusting whitespace around an
  // IME insertion.
  if (ShouldHandleIMEComposition() && !aSuppressIME) {
    transaction =
        CompositionTransaction::Create(*this, aStringToInsert, pointToInsert);
    isIMETransaction = true;
    // All characters of the composition string will be replaced with
    // aStringToInsert.  So, we need to emulate to remove the composition
    // string.
    // FYI: The text node information in mComposition has been updated by
    //      CompositionTransaction::Create().
    pointToInsert.Set(mComposition->GetContainerTextNode(),
                      mComposition->XPOffsetInTextNode());
  } else {
    transaction =
        InsertTextTransaction::Create(*this, aStringToInsert, pointToInsert);
  }

  // XXX We may not need these view batches anymore.  This is handled at a
  // higher level now I believe.
  BeginUpdateViewBatch();
  nsresult rv = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DoTransactionInternal() failed");
  EndUpdateViewBatch();

  if (AsHTMLEditor() && pointToInsert.IsSet()) {
    // The DOM was potentially modified during the transaction. This is possible
    // through mutation event listeners. That is, the node could've been removed
    // from the doc or otherwise modified.
    if (!MaybeHasMutationEventListeners(
            NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED)) {
      EditorDOMPointInText endOfInsertion(
          pointToInsert.ContainerAsText(),
          pointToInsert.Offset() + aStringToInsert.Length());
      TopLevelEditSubActionDataRef().DidInsertText(*this, pointToInsert,
                                                   endOfInsertion);
    } else if (pointToInsert.ContainerAsText()->IsInComposedDoc()) {
      EditorDOMPointInText begin, end;
      Tie(begin, end) =
          AdjustTextInsertionRange(pointToInsert, aStringToInsert);
      TopLevelEditSubActionDataRef().DidInsertText(*this, begin, end);
    }
  }

  // let listeners know what happened
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      // TODO: might need adaptation because of mutation event listeners called
      // during `DoTransactionInternal`.
      DebugOnly<nsresult> rvIgnored =
          listener->DidInsertText(pointToInsert.ContainerAsText(),
                                  pointToInsert.Offset(), aStringToInsert, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidInsertText() failed, but ignored");
    }
  }

  // Added some cruft here for bug 43366.  Layout was crashing because we left
  // an empty text node lying around in the document.  So I delete empty text
  // nodes caused by IME.  I have to mark the IME transaction as "fixed", which
  // means that furure IME txns won't merge with it.  This is because we don't
  // want future IME txns trying to put their text into a node that is no
  // longer in the document.  This does not break undo/redo, because all these
  // txns are wrapped in a parent PlaceHolder txn, and placeholder txns are
  // already savvy to having multiple ime txns inside them.

  // Delete empty IME text node if there is one
  if (isIMETransaction && mComposition) {
    RefPtr<Text> textNode = mComposition->GetContainerTextNode();
    if (textNode && !textNode->Length()) {
      DeleteNodeWithTransaction(*textNode);
      mComposition->OnTextNodeRemoved();
      static_cast<CompositionTransaction*>(transaction.get())->MarkFixed();
    }
  }

  return rv;
}

nsINode* EditorBase::GetFirstEditableNode(nsINode* aRoot) {
  MOZ_ASSERT(aRoot);

  EditorType editorType = GetEditorType();
  nsIContent* content = GetLeftmostChild(aRoot);
  if (content && !EditorUtils::IsEditableContent(*content, editorType)) {
    content = GetNextEditableNode(*content);
  }

  return (content != aRoot) ? content : nullptr;
}

nsresult EditorBase::NotifyDocumentListeners(
    TDocumentListenerNotification aNotificationType) {
  switch (aNotificationType) {
    case eDocumentCreated:
      if (!AsHTMLEditor()) {
        return NS_OK;
      }
      if (RefPtr<ComposerCommandsUpdater> composerCommandsUpdate =
              AsHTMLEditor()->mComposerCommandsUpdater) {
        composerCommandsUpdate->OnHTMLEditorCreated();
      }
      return NS_OK;

    case eDocumentToBeDestroyed: {
      RefPtr<ComposerCommandsUpdater> composerCommandsUpdate =
          AsHTMLEditor() ? AsHTMLEditor()->mComposerCommandsUpdater : nullptr;
      if (!mDocStateListeners.Length() && !composerCommandsUpdate) {
        return NS_OK;
      }
      // Needs to store all listeners before notifying ComposerCommandsUpdate
      // since notifying it might change mDocStateListeners.
      const AutoDocumentStateListenerArray listeners(mDocStateListeners);
      if (composerCommandsUpdate) {
        composerCommandsUpdate->OnBeforeHTMLEditorDestroyed();
      }
      for (auto& listener : listeners) {
        // MOZ_KnownLive because 'listeners' is guaranteed to
        // keep it alive.
        //
        // This can go away once
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 is fixed.
        nsresult rv = MOZ_KnownLive(listener)->NotifyDocumentWillBeDestroyed();
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "nsIDocumentStateListener::NotifyDocumentWillBeDestroyed() "
              "failed");
          return rv;
        }
      }
      return NS_OK;
    }
    case eDocumentStateChanged: {
      bool docIsDirty;
      nsresult rv = GetDocumentModified(&docIsDirty);
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::GetDocumentModified() failed");
        return rv;
      }

      if (static_cast<int8_t>(docIsDirty) == mDocDirtyState) {
        return NS_OK;
      }

      mDocDirtyState = docIsDirty;

      RefPtr<ComposerCommandsUpdater> composerCommandsUpdate =
          AsHTMLEditor() ? AsHTMLEditor()->mComposerCommandsUpdater : nullptr;
      if (!mDocStateListeners.Length() && !composerCommandsUpdate) {
        return NS_OK;
      }
      // Needs to store all listeners before notifying ComposerCommandsUpdate
      // since notifying it might change mDocStateListeners.
      const AutoDocumentStateListenerArray listeners(mDocStateListeners);
      if (composerCommandsUpdate) {
        composerCommandsUpdate->OnHTMLEditorDirtyStateChanged(mDocDirtyState);
      }
      for (auto& listener : listeners) {
        // MOZ_KnownLive because 'listeners' is guaranteed to
        // keep it alive.
        //
        // This can go away once
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 is fixed.
        nsresult rv =
            MOZ_KnownLive(listener)->NotifyDocumentStateChanged(mDocDirtyState);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "nsIDocumentStateListener::NotifyDocumentStateChanged() failed");
          return rv;
        }
      }
      return NS_OK;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown notification");
      return NS_ERROR_FAILURE;
  }
}

nsresult EditorBase::SetTextNodeWithoutTransaction(const nsAString& aString,
                                                   Text& aTextNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());
  MOZ_ASSERT(IsPlaintextEditor());
  MOZ_ASSERT(!IsUndoRedoEnabled());

  const uint32_t length = aTextNode.Length();

  // Let listeners know what's up
  if (!mActionListeners.IsEmpty() && length) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored =
          listener->WillDeleteText(&aTextNode, 0, length);
      if (NS_WARN_IF(Destroyed())) {
        NS_WARNING(
            "nsIEditActionListener::WillDeleteText() failed, but ignored");
        return NS_ERROR_EDITOR_DESTROYED;
      }
    }
  }

  // We don't support undo here, so we don't really need all of the transaction
  // machinery, therefore we can run our transaction directly, breaking all of
  // the rules!
  ErrorResult error;
  DoSetText(aTextNode, aString, error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoSetText() failed");
    return error.StealNSResult();
  }

  DebugOnly<nsresult> rvIgnored =
      SelectionRefPtr()->Collapse(&aTextNode, aString.Length());
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_ASSERTION(NS_SUCCEEDED(rvIgnored),
               "Selection::Collapse() failed, but ignored");

  rvIgnored = RangeUpdaterRef().SelAdjDeleteText(aTextNode, 0, length);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "RangeUpdater::SelAdjDeleteText() failed, but ignored");
  RangeUpdaterRef().SelAdjInsertText(aTextNode, 0, aString);

  // Let listeners know what happened
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      if (length) {
        DebugOnly<nsresult> rvIgnored =
            listener->DidDeleteText(&aTextNode, 0, length, NS_OK);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            "nsIEditActionListener::DidDeleteText() failed, but ignored");
      }
      if (!aString.IsEmpty()) {
        DebugOnly<nsresult> rvIgnored =
            listener->DidInsertText(&aTextNode, 0, aString, NS_OK);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            "nsIEditActionListener::DidInsertText() failed, but ignored");
      }
    }
  }

  return NS_OK;
}

nsresult EditorBase::DeleteTextWithTransaction(Text& aTextNode,
                                               uint32_t aOffset,
                                               uint32_t aLength) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<DeleteTextTransaction> transaction =
      DeleteTextTransaction::MaybeCreate(*this, aTextNode, aOffset, aLength);
  if (!transaction) {
    NS_WARNING("DeleteTextTransaction::MaybeCreate() failed");
    return NS_ERROR_FAILURE;
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteText, nsIEditor::ePrevious, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Let listeners know what's up
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored =
          listener->WillDeleteText(&aTextNode, aOffset, aLength);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::WillDeleteText() failed, but ignored");
    }
  }

  nsresult rv = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DoTransactionInternal() failed");

  if (AsHTMLEditor()) {
    TopLevelEditSubActionDataRef().DidDeleteText(
        *this, EditorRawDOMPoint(&aTextNode, aOffset));
  }

  // Let listeners know what happened
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      DebugOnly<nsresult> rvIgnored =
          listener->DidDeleteText(&aTextNode, aOffset, aLength, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::WillDeleteText() failed, but ignored");
    }
  }

  return rv;
}

struct SavedRange final {
  RefPtr<Selection> mSelection;
  nsCOMPtr<nsINode> mStartContainer;
  nsCOMPtr<nsINode> mEndContainer;
  int32_t mStartOffset;
  int32_t mEndOffset;
};

void EditorBase::DoSplitNode(const EditorDOMPoint& aStartOfRightNode,
                             nsIContent& aNewLeftNode, ErrorResult& aError) {
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  // XXX Perhaps, aStartOfRightNode may be invalid if this is a redo
  //     operation after modifying DOM node with JS.
  if (NS_WARN_IF(!aStartOfRightNode.IsSet())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  MOZ_ASSERT(aStartOfRightNode.IsSetAndValid());

  // Remember all selection points.
  AutoTArray<SavedRange, 10> savedRanges;
  for (SelectionType selectionType : kPresentSelectionTypes) {
    SavedRange range;
    range.mSelection = GetSelection(selectionType);
    if (NS_WARN_IF(!range.mSelection &&
                   selectionType == SelectionType::eNormal)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    } else if (!range.mSelection) {
      // For non-normal selections, skip over the non-existing ones.
      continue;
    }

    for (uint32_t j = 0; j < range.mSelection->RangeCount(); ++j) {
      RefPtr<nsRange> r = range.mSelection->GetRangeAt(j);
      MOZ_ASSERT(r->IsPositioned());
      // XXX Looks like that SavedRange should have mStart and mEnd which
      //     are RangeBoundary.  Then, we can avoid to compute offset here.
      range.mStartContainer = r->GetStartContainer();
      range.mStartOffset = r->StartOffset();
      range.mEndContainer = r->GetEndContainer();
      range.mEndOffset = r->EndOffset();

      savedRanges.AppendElement(range);
    }
  }

  nsCOMPtr<nsINode> parent = aStartOfRightNode.GetContainerParent();
  if (NS_WARN_IF(!parent)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Fix the child before mutation observer may touch the DOM tree.
  nsIContent* firstChildOfRightNode = aStartOfRightNode.GetChild();
  parent->InsertBefore(aNewLeftNode, aStartOfRightNode.GetContainer(), aError);
  if (aError.Failed()) {
    NS_WARNING("nsINode::InsertBefore() failed");
    return;
  }

  // At this point, the existing right node has all the children.  Move all
  // the children which are before aStartOfRightNode.
  if (!aStartOfRightNode.IsStartOfContainer()) {
    // If it's a text node, just shuffle around some text
    Text* rightAsText = aStartOfRightNode.GetContainerAsText();
    Text* leftAsText = aNewLeftNode.GetAsText();
    if (rightAsText && leftAsText) {
      MOZ_DIAGNOSTIC_ASSERT(AsHTMLEditor(),
                            "Text node in TextEditor shouldn't be split");
      // Fix right node
      nsAutoString leftText;
      IgnoredErrorResult ignoredError;
      rightAsText->SubstringData(0, aStartOfRightNode.Offset(), leftText,
                                 ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "Text::SubstringData() failed, but ignored");
      ignoredError.SuppressException();

      // XXX This call may destroy us.
      DoDeleteText(MOZ_KnownLive(*rightAsText), 0, aStartOfRightNode.Offset(),
                   ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "EditorBase::DoDeleteText() failed, but ignored");
      ignoredError.SuppressException();

      // Fix left node
      // XXX This call may destroy us.
      DoSetText(MOZ_KnownLive(*leftAsText), leftText, ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "EditorBase::DoSetText() failed, but ignored");
    } else {
      MOZ_DIAGNOSTIC_ASSERT(!rightAsText && !leftAsText);
      // Otherwise it's an interior node, so shuffle around the children. Go
      // through list backwards so deletes don't interfere with the iteration.
      if (!firstChildOfRightNode) {
        MoveAllChildren(*aStartOfRightNode.GetContainer(),
                        EditorRawDOMPoint(&aNewLeftNode, 0), aError);
        NS_WARNING_ASSERTION(!aError.Failed(),
                             "EditorBase::MoveAllChildren() failed");
      } else if (NS_WARN_IF(aStartOfRightNode.GetContainer() !=
                            firstChildOfRightNode->GetParentNode())) {
        // firstChildOfRightNode has been moved by mutation observer.
        // In this case, we what should we do?  Use offset?  But we cannot
        // check if the offset is still expected.
      } else {
        MovePreviousSiblings(*firstChildOfRightNode,
                             EditorRawDOMPoint(&aNewLeftNode, 0), aError);
        NS_WARNING_ASSERTION(!aError.Failed(),
                             "EditorBase::MovePreviousSiblings() failed");
      }
    }
  }

  // XXX Why do we ignore an error while moving nodes from the right node to
  //     the left node?
  NS_WARNING_ASSERTION(!aError.Failed(), "The previous error is ignored");
  aError.SuppressException();

  // Handle selection
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(FlushType::Frames);
  }
  NS_WARNING_ASSERTION(!Destroyed(),
                       "The editor is destroyed during splitting a node");

  bool allowedTransactionsToChangeSelection =
      AllowsTransactionsToChangeSelection();

  RefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // Adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      range.mSelection->RemoveAllRanges(aError);
      if (aError.Failed()) {
        NS_WARNING("Selection::RemoveAllRanges() failed");
        return;
      }
      previousSelection = range.mSelection;
    }

    // XXX Looks like that we don't need to modify normal selection here
    //     because selection will be modified by the caller if
    //     AllowsTransactionsToChangeSelection() will return true.
    if (allowedTransactionsToChangeSelection &&
        range.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Split the selection into existing node and new node.
    if (range.mStartContainer == aStartOfRightNode.GetContainer()) {
      if (static_cast<uint32_t>(range.mStartOffset) <
          aStartOfRightNode.Offset()) {
        range.mStartContainer = &aNewLeftNode;
      } else {
        range.mStartOffset -= aStartOfRightNode.Offset();
      }
    }

    if (range.mEndContainer == aStartOfRightNode.GetContainer()) {
      if (static_cast<uint32_t>(range.mEndOffset) <
          aStartOfRightNode.Offset()) {
        range.mEndContainer = &aNewLeftNode;
      } else {
        range.mEndOffset -= aStartOfRightNode.Offset();
      }
    }

    RefPtr<nsRange> newRange =
        nsRange::Create(range.mStartContainer, range.mStartOffset,
                        range.mEndContainer, range.mEndOffset, aError);
    if (aError.Failed()) {
      NS_WARNING("nsRange::Create() failed");
      return;
    }
    // The `MOZ_KnownLive` annotation is only necessary because of a bug
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1622253) in the
    // static analyzer.
    MOZ_KnownLive(range.mSelection)
        ->AddRangeAndSelectFramesAndNotifyListeners(*newRange, aError);
    if (aError.Failed()) {
      NS_WARNING(
          "Selection::AddRangeAndSelectFramesAndNotifyListeners() failed");
      return;
    }
  }

  // We don't need to set selection here because the caller should do that
  // in any case.

  // If splitting the node causes running mutation event listener and we've
  // got unexpected result, we should return error because callers will
  // continue to do their work without complicated DOM tree result.
  // NOTE: Perhaps, we shouldn't do this immediately after each DOM tree change
  //       because stopping handling it causes some data loss.  E.g., user
  //       may loose the text which is moved to the new text node.
  // XXX We cannot check all descendants in the right node and the new left
  //     node for performance reason.  I think that if caller needs to access
  //     some of the descendants, they should check by themselves.
  if (NS_WARN_IF(parent != aStartOfRightNode.GetContainer()->GetParentNode()) ||
      NS_WARN_IF(parent != aNewLeftNode.GetParentNode()) ||
      NS_WARN_IF(aNewLeftNode.GetNextSibling() !=
                 aStartOfRightNode.GetContainer())) {
    aError.Throw(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
}

nsresult EditorBase::DoJoinNodes(nsIContent& aContentToKeep,
                                 nsIContent& aContentToJoin) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_DIAGNOSTIC_ASSERT(AsHTMLEditor());

  uint32_t firstNodeLength = aContentToJoin.Length();

  EditorRawDOMPoint atNodeToJoin(&aContentToJoin);
  EditorRawDOMPoint atNodeToKeep(&aContentToKeep);

  // Remember all selection points.
  // XXX Do we need to restore all types of selections by ourselves?  Normal
  //     selection should be modified later as result of handling edit action.
  //     IME selections shouldn't be there when nodes are joined.  Spellcheck
  //     selections should be recreated with newer text.  URL selections
  //     shouldn't be there because of used only by the URL bar.
  AutoTArray<SavedRange, 10> savedRanges;
  for (SelectionType selectionType : kPresentSelectionTypes) {
    SavedRange range;
    range.mSelection = GetSelection(selectionType);
    if (selectionType == SelectionType::eNormal) {
      if (NS_WARN_IF(!range.mSelection)) {
        return NS_ERROR_FAILURE;
      }
    } else if (!range.mSelection) {
      // For non-normal selections, skip over the non-existing ones.
      continue;
    }

    for (uint32_t j = 0; j < range.mSelection->RangeCount(); ++j) {
      RefPtr<nsRange> r = range.mSelection->GetRangeAt(j);
      MOZ_ASSERT(r->IsPositioned());
      range.mStartContainer = r->GetStartContainer();
      range.mStartOffset = r->StartOffset();
      range.mEndContainer = r->GetEndContainer();
      range.mEndOffset = r->EndOffset();

      // If selection endpoint is between the nodes, remember it as being
      // in the one that is going away instead.  This simplifies later selection
      // adjustment logic at end of this method.
      if (range.mStartContainer) {
        if (range.mStartContainer == atNodeToKeep.GetContainer() &&
            atNodeToJoin.Offset() < static_cast<uint32_t>(range.mStartOffset) &&
            static_cast<uint32_t>(range.mStartOffset) <=
                atNodeToKeep.Offset()) {
          range.mStartContainer = &aContentToJoin;
          range.mStartOffset = firstNodeLength;
        }
        if (range.mEndContainer == atNodeToKeep.GetContainer() &&
            atNodeToJoin.Offset() < static_cast<uint32_t>(range.mEndOffset) &&
            static_cast<uint32_t>(range.mEndOffset) <= atNodeToKeep.Offset()) {
          range.mEndContainer = &aContentToJoin;
          range.mEndOffset = firstNodeLength;
        }
      }

      savedRanges.AppendElement(range);
    }
  }

  // OK, ready to do join now.
  // If it's a text node, just shuffle around some text.
  if (aContentToKeep.IsText() && aContentToJoin.IsText()) {
    nsAutoString rightText;
    nsAutoString leftText;
    aContentToKeep.AsText()->GetData(rightText);
    aContentToJoin.AsText()->GetData(leftText);
    leftText += rightText;
    IgnoredErrorResult ignoredError;
    DoSetText(MOZ_KnownLive(*aContentToKeep.AsText()), leftText, ignoredError);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "EditorBase::DoSetText() failed, but ignored");
  } else {
    // Otherwise it's an interior node, so shuffle around the children.
    nsCOMPtr<nsINodeList> childNodes = aContentToJoin.ChildNodes();
    MOZ_ASSERT(childNodes);

    // Remember the first child in aContentToKeep, we'll insert all the children
    // of aContentToJoin in front of it GetFirstChild returns nullptr firstNode
    // if aContentToKeep has no children, that's OK.
    nsCOMPtr<nsIContent> firstNode = aContentToKeep.GetFirstChild();

    // Have to go through the list backwards to keep deletes from interfering
    // with iteration.
    for (uint32_t i = childNodes->Length(); i; --i) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i - 1);
      if (childNode) {
        // prepend children of aContentToJoin
        ErrorResult error;
        aContentToKeep.InsertBefore(*childNode, firstNode, error);
        if (NS_WARN_IF(Destroyed())) {
          error.SuppressException();
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (error.Failed()) {
          NS_WARNING("nsINode::InsertBefore() failed");
          return error.StealNSResult();
        }
        firstNode = std::move(childNode);
      }
    }
  }

  // Delete the extra node.
  aContentToJoin.Remove();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  bool allowedTransactionsToChangeSelection =
      AllowsTransactionsToChangeSelection();

  RefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // And adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      ErrorResult error;
      range.mSelection->RemoveAllRanges(error);
      if (NS_WARN_IF(Destroyed())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (error.Failed()) {
        NS_WARNING("Selection::RemoveAllRanges() failed");
        return error.StealNSResult();
      }
      previousSelection = range.mSelection;
    }

    if (allowedTransactionsToChangeSelection &&
        range.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Check to see if we joined nodes where selection starts.
    if (range.mStartContainer == &aContentToJoin) {
      range.mStartContainer = &aContentToKeep;
    } else if (range.mStartContainer == &aContentToKeep) {
      range.mStartOffset += firstNodeLength;
    }

    // Check to see if we joined nodes where selection ends.
    if (range.mEndContainer == &aContentToJoin) {
      range.mEndContainer = &aContentToKeep;
    } else if (range.mEndContainer == &aContentToKeep) {
      range.mEndOffset += firstNodeLength;
    }

    RefPtr<nsRange> newRange =
        nsRange::Create(range.mStartContainer, range.mStartOffset,
                        range.mEndContainer, range.mEndOffset, IgnoreErrors());
    if (!newRange) {
      NS_WARNING("nsRange::Create() failed");
      return NS_ERROR_FAILURE;
    }

    ErrorResult error;
    // The `MOZ_KnownLive` annotation is only necessary because of a bug
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1622253) in the
    // static analyzer.
    MOZ_KnownLive(range.mSelection)
        ->AddRangeAndSelectFramesAndNotifyListeners(*newRange, error);
    if (NS_WARN_IF(Destroyed())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  if (allowedTransactionsToChangeSelection) {
    // Editor wants us to set selection at join point.
    DebugOnly<nsresult> rvIgnored = SelectionRefPtr()->Collapse(
        &aContentToKeep, AssertedCast<int32_t>(firstNodeLength));
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Selection::Collapse() failed, but ignored");
  }

  return NS_OK;
}

nsIContent* EditorBase::GetPreviousNodeInternal(nsINode& aNode,
                                                bool aFindEditableNode,
                                                bool aFindAnyDataNode,
                                                bool aNoBlockCrossing) const {
  if (!IsDescendantOfEditorRoot(&aNode)) {
    return nullptr;
  }
  return FindNode(&aNode, false, aFindEditableNode, aFindAnyDataNode,
                  aNoBlockCrossing);
}

nsIContent* EditorBase::GetPreviousNodeInternal(const EditorRawDOMPoint& aPoint,
                                                bool aFindEditableNode,
                                                bool aFindAnyDataNode,
                                                bool aNoBlockCrossing) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  NS_WARNING_ASSERTION(
      !aPoint.IsInDataNode() || aPoint.IsInTextNode(),
      "GetPreviousNodeInternal() doesn't assume that the start point is a "
      "data node except text node");

  // If we are at the beginning of the node, or it is a text node, then just
  // look before it.
  if (aPoint.IsStartOfContainer() || aPoint.IsInTextNode()) {
    if (aNoBlockCrossing && aPoint.IsInContentNode() &&
        HTMLEditUtils::IsBlockElement(*aPoint.ContainerAsContent())) {
      // If we aren't allowed to cross blocks, don't look before this block.
      return nullptr;
    }
    return GetPreviousNodeInternal(*aPoint.GetContainer(), aFindEditableNode,
                                   aFindAnyDataNode, aNoBlockCrossing);
  }

  // else look before the child at 'aOffset'
  if (aPoint.GetChild()) {
    return GetPreviousNodeInternal(*aPoint.GetChild(), aFindEditableNode,
                                   aFindAnyDataNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the deep-right child.
  nsIContent* rightMostContent =
      GetRightmostChild(aPoint.GetContainer(), aNoBlockCrossing);
  if (!rightMostContent) {
    return nullptr;
  }

  if ((!aFindEditableNode ||
       EditorUtils::IsEditableContent(*rightMostContent, GetEditorType())) &&
      (aFindAnyDataNode || EditorUtils::IsElementOrText(*rightMostContent))) {
    return rightMostContent;
  }

  // restart the search from the non-editable node we just found
  return GetPreviousNodeInternal(*rightMostContent, aFindEditableNode,
                                 aFindAnyDataNode, aNoBlockCrossing);
}

nsIContent* EditorBase::GetNextNodeInternal(nsINode& aNode,
                                            bool aFindEditableNode,
                                            bool aFindAnyDataNode,
                                            bool aNoBlockCrossing) const {
  if (!IsDescendantOfEditorRoot(&aNode)) {
    return nullptr;
  }
  return FindNode(&aNode, true, aFindEditableNode, aFindAnyDataNode,
                  aNoBlockCrossing);
}

nsIContent* EditorBase::GetNextNodeInternal(const EditorRawDOMPoint& aPoint,
                                            bool aFindEditableNode,
                                            bool aFindAnyDataNode,
                                            bool aNoBlockCrossing) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  NS_WARNING_ASSERTION(
      !aPoint.IsInDataNode() || aPoint.IsInTextNode(),
      "GetNextNodeInternal() doesn't assume that the start point is a "
      "data node except text node");

  EditorRawDOMPoint point(aPoint);

  // if the container is a text node, use its location instead
  if (point.IsInTextNode()) {
    point.SetAfter(point.GetContainer());
    if (NS_WARN_IF(!point.IsSet())) {
      return nullptr;
    }
  }

  if (point.GetChild()) {
    if (aNoBlockCrossing && HTMLEditUtils::IsBlockElement(*point.GetChild())) {
      return point.GetChild();
    }

    nsIContent* leftMostContent =
        GetLeftmostChild(point.GetChild(), aNoBlockCrossing);
    if (!leftMostContent) {
      return point.GetChild();
    }

    if (!IsDescendantOfEditorRoot(leftMostContent)) {
      return nullptr;
    }

    if ((!aFindEditableNode ||
         EditorUtils::IsEditableContent(*leftMostContent, GetEditorType())) &&
        (aFindAnyDataNode || EditorUtils::IsElementOrText(*leftMostContent))) {
      return leftMostContent;
    }

    // restart the search from the non-editable node we just found
    return GetNextNodeInternal(*leftMostContent, aFindEditableNode,
                               aFindAnyDataNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  if (aNoBlockCrossing && point.IsInContentNode() &&
      HTMLEditUtils::IsBlockElement(*point.ContainerAsContent())) {
    // don't cross out of parent block
    return nullptr;
  }

  return GetNextNodeInternal(*point.GetContainer(), aFindEditableNode,
                             aFindAnyDataNode, aNoBlockCrossing);
}

nsIContent* EditorBase::FindNextLeafNode(nsINode* aCurrentNode, bool aGoForward,
                                         bool bNoBlockCrossing) const {
  // called only by GetPriorNode so we don't need to check params.
  MOZ_ASSERT(
      IsDescendantOfEditorRoot(aCurrentNode) && !IsEditorRoot(aCurrentNode),
      "Bogus arguments");

  nsINode* cur = aCurrentNode;
  for (;;) {
    // if aCurrentNode has a sibling in the right direction, return
    // that sibling's closest child (or itself if it has no children)
    nsIContent* sibling =
        aGoForward ? cur->GetNextSibling() : cur->GetPreviousSibling();
    if (sibling) {
      if (bNoBlockCrossing && HTMLEditUtils::IsBlockElement(*sibling)) {
        // don't look inside prevsib, since it is a block
        return sibling;
      }
      nsIContent* leaf = aGoForward
                             ? GetLeftmostChild(sibling, bNoBlockCrossing)
                             : GetRightmostChild(sibling, bNoBlockCrossing);
      if (!leaf) {
        return sibling;
      }

      return leaf;
    }

    nsINode* parent = cur->GetParentNode();
    if (!parent) {
      return nullptr;
    }

    NS_ASSERTION(IsDescendantOfEditorRoot(parent),
                 "We started with a proper descendant of root, and should stop "
                 "if we ever hit the root, so we better have a descendant of "
                 "root now!");
    if (IsEditorRoot(parent) ||
        (bNoBlockCrossing && parent->IsContent() &&
         HTMLEditUtils::IsBlockElement(*parent->AsContent()))) {
      return nullptr;
    }

    cur = parent;
  }

  MOZ_ASSERT_UNREACHABLE("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent* EditorBase::FindNode(nsINode* aCurrentNode, bool aGoForward,
                                 bool aEditableNode, bool aFindAnyDataNode,
                                 bool bNoBlockCrossing) const {
  if (IsEditorRoot(aCurrentNode)) {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.

    return nullptr;
  }

  nsIContent* candidate =
      FindNextLeafNode(aCurrentNode, aGoForward, bNoBlockCrossing);

  if (!candidate) {
    return nullptr;
  }

  if ((!aEditableNode ||
       EditorUtils::IsEditableContent(*candidate, GetEditorType())) &&
      (aFindAnyDataNode || EditorUtils::IsElementOrText(*candidate))) {
    return candidate;
  }

  return FindNode(candidate, aGoForward, aEditableNode, aFindAnyDataNode,
                  bNoBlockCrossing);
}

nsIContent* EditorBase::GetRightmostChild(nsINode* aCurrentNode,
                                          bool bNoBlockCrossing) const {
  if (NS_WARN_IF(!aCurrentNode)) {
    return nullptr;
  }
  nsIContent* content = aCurrentNode->GetLastChild();
  if (!content) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && HTMLEditUtils::IsBlockElement(*content)) {
      return content;
    }
    nsIContent* nextContent = content->GetLastChild();
    if (!nextContent) {
      return content;
    }
    content = nextContent;
  }

  MOZ_ASSERT_UNREACHABLE("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent* EditorBase::GetLeftmostChild(nsINode* aCurrentNode,
                                         bool bNoBlockCrossing) const {
  if (NS_WARN_IF(!aCurrentNode)) {
    return nullptr;
  }
  nsIContent* content = aCurrentNode->GetFirstChild();
  if (!content) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && HTMLEditUtils::IsBlockElement(*content)) {
      return content;
    }
    nsIContent* next = content->GetFirstChild();
    if (!next) {
      return content;
    }
    content = next;
  }

  MOZ_ASSERT_UNREACHABLE("What part of for(;;) do you not understand?");
  return nullptr;
}

bool EditorBase::IsRoot(nsINode* inNode) const {
  if (NS_WARN_IF(!inNode)) {
    return false;
  }
  nsINode* rootNode = GetRoot();
  return inNode == rootNode;
}

bool EditorBase::IsEditorRoot(nsINode* aNode) const {
  if (NS_WARN_IF(!aNode)) {
    return false;
  }
  nsINode* rootNode = GetEditorRoot();
  return aNode == rootNode;
}

bool EditorBase::IsDescendantOfRoot(nsINode* inNode) const {
  if (NS_WARN_IF(!inNode)) {
    return false;
  }
  nsIContent* root = GetRoot();
  if (NS_WARN_IF(!root)) {
    return false;
  }

  return inNode->IsInclusiveDescendantOf(root);
}

bool EditorBase::IsDescendantOfEditorRoot(nsINode* aNode) const {
  if (NS_WARN_IF(!aNode)) {
    return false;
  }
  nsIContent* root = GetEditorRoot();
  if (NS_WARN_IF(!root)) {
    return false;
  }

  return aNode->IsInclusiveDescendantOf(root);
}

bool EditorBase::IsContainer(nsINode* aNode) const {
  return aNode ? true : false;
}

uint32_t EditorBase::CountEditableChildren(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  uint32_t count = 0;
  EditorType editorType = GetEditorType();
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (EditorUtils::IsEditableContent(*child, editorType)) {
      ++count;
    }
  }
  return count;
}

NS_IMETHODIMP EditorBase::IncrementModificationCount(int32_t inNumMods) {
  uint32_t oldModCount = mModCount;

  mModCount += inNumMods;

  if ((!oldModCount && mModCount) || (oldModCount && !mModCount)) {
    DebugOnly<nsresult> rvIgnored =
        NotifyDocumentListeners(eDocumentStateChanged);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::NotifyDocumentListeners() failed, but ignored");
  }
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetModificationCount(int32_t* aOutModCount) {
  if (NS_WARN_IF(!aOutModCount)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aOutModCount = mModCount;
  return NS_OK;
}

NS_IMETHODIMP EditorBase::ResetModificationCount() {
  bool doNotify = (mModCount != 0);

  mModCount = 0;

  if (!doNotify) {
    return NS_OK;
  }

  DebugOnly<nsresult> rvIgnored =
      NotifyDocumentListeners(eDocumentStateChanged);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::NotifyDocumentListeners() failed, but ignored");
  return NS_OK;
}

// static
nsIContent* EditorBase::GetNodeAtRangeOffsetPoint(
    const RawRangeBoundary& aPoint) {
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return nullptr;
  }
  if (aPoint.Container()->GetAsText()) {
    return aPoint.Container()->AsContent();
  }
  return aPoint.GetChildAtOffset();
}

// static
EditorRawDOMPoint EditorBase::GetStartPoint(const Selection& aSelection) {
  if (NS_WARN_IF(!aSelection.RangeCount())) {
    return EditorRawDOMPoint();
  }

  const nsRange* range = aSelection.GetRangeAt(0);
  if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
    return EditorRawDOMPoint();
  }

  return EditorRawDOMPoint(range->StartRef());
}

// static
EditorRawDOMPoint EditorBase::GetEndPoint(const Selection& aSelection) {
  if (NS_WARN_IF(!aSelection.RangeCount())) {
    return EditorRawDOMPoint();
  }

  const nsRange* range = aSelection.GetRangeAt(0);
  if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
    return EditorRawDOMPoint();
  }

  return EditorRawDOMPoint(range->EndRef());
}

// static
nsresult EditorBase::GetEndChildNode(const Selection& aSelection,
                                     nsIContent** aEndNode) {
  MOZ_ASSERT(aEndNode);

  *aEndNode = nullptr;

  if (NS_WARN_IF(!aSelection.RangeCount())) {
    return NS_ERROR_FAILURE;
  }

  const nsRange* range = aSelection.GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!range->IsPositioned())) {
    return NS_ERROR_FAILURE;
  }

  NS_IF_ADDREF(*aEndNode = range->GetChildAtEndOffset());
  return NS_OK;
}

/**
 * IsPreformatted() checks the style info for the node for the preformatted
 * text style.
 */
// static
bool EditorBase::IsPreformatted(nsINode* aNode) {
  if (NS_WARN_IF(!aNode)) {
    return false;
  }
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aNode->GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element, nullptr);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  const nsStyleText* styleText = elementStyle->StyleText();

  return styleText->WhiteSpaceIsSignificant();
}

SplitNodeResult EditorBase::SplitNodeDeepWithTransaction(
    nsIContent& aMostAncestorToSplit,
    const EditorDOMPoint& aStartOfDeepestRightNode,
    SplitAtEdges aSplitAtEdges) {
  MOZ_ASSERT(aStartOfDeepestRightNode.IsSetAndValid());
  MOZ_ASSERT(
      aStartOfDeepestRightNode.GetContainer() == &aMostAncestorToSplit ||
      EditorUtils::IsDescendantOf(*aStartOfDeepestRightNode.GetContainer(),
                                  aMostAncestorToSplit));

  if (NS_WARN_IF(!aStartOfDeepestRightNode.IsSet())) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }

  nsCOMPtr<nsIContent> newLeftNodeOfMostAncestor;
  EditorDOMPoint atStartOfRightNode(aStartOfDeepestRightNode);
  while (true) {
    // Need to insert rules code call here to do things like not split a list
    // if you are after the last <li> or before the first, etc.  For now we
    // just have some smarts about unneccessarily splitting text nodes, which
    // should be universal enough to put straight in this EditorBase routine.
    if (NS_WARN_IF(!atStartOfRightNode.GetContainerAsContent())) {
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    // If we meet an orphan node before meeting aMostAncestorToSplit, we need
    // to stop splitting.  This is a bug of the caller.
    if (NS_WARN_IF(atStartOfRightNode.GetContainer() != &aMostAncestorToSplit &&
                   !atStartOfRightNode.GetContainerParentAsContent())) {
      return SplitNodeResult(NS_ERROR_FAILURE);
    }

    nsIContent* currentRightNode = atStartOfRightNode.GetContainerAsContent();

    // If the split point is middle of the node or the node is not a text node
    // and we're allowed to create empty element node, split it.
    if ((aSplitAtEdges == SplitAtEdges::eAllowToCreateEmptyContainer &&
         !atStartOfRightNode.GetContainerAsText()) ||
        (!atStartOfRightNode.IsStartOfContainer() &&
         !atStartOfRightNode.IsEndOfContainer())) {
      ErrorResult error;
      nsCOMPtr<nsIContent> newLeftNode =
          SplitNodeWithTransaction(atStartOfRightNode, error);
      if (error.Failed()) {
        NS_WARNING("EditorBase::SplitNodeWithTransaction() failed");
        return SplitNodeResult(error.StealNSResult());
      }

      if (currentRightNode == &aMostAncestorToSplit) {
        // Actually, we split aMostAncestorToSplit.
        return SplitNodeResult(newLeftNode, &aMostAncestorToSplit);
      }

      // Then, try to split its parent before current node.
      atStartOfRightNode.Set(currentRightNode);
    }
    // If the split point is end of the node and it is a text node or we're not
    // allowed to create empty container node, try to split its parent after it.
    else if (!atStartOfRightNode.IsStartOfContainer()) {
      if (currentRightNode == &aMostAncestorToSplit) {
        return SplitNodeResult(&aMostAncestorToSplit, nullptr);
      }

      // Try to split its parent after current node.
      atStartOfRightNode.Set(currentRightNode);
      DebugOnly<bool> advanced = atStartOfRightNode.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
                           "Failed to advance offset after current node");
    }
    // If the split point is start of the node and it is a text node or we're
    // not allowed to create empty container node, try to split its parent.
    else {
      if (currentRightNode == &aMostAncestorToSplit) {
        return SplitNodeResult(nullptr, &aMostAncestorToSplit);
      }

      // Try to split its parent before current node.
      atStartOfRightNode.Set(currentRightNode);
    }
  }

  return SplitNodeResult(NS_ERROR_FAILURE);
}

nsresult EditorBase::EnsureNoPaddingBRElementForEmptyEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mPaddingBRElementForEmptyEditor) {
    return NS_OK;
  }

  // If we're an HTML editor, a mutation event listener may recreate padding
  // <br> element for empty editor again during the call of
  // DeleteNodeWithTransaction().  So, move it first.
  RefPtr<HTMLBRElement> paddingBRElement(
      std::move(mPaddingBRElementForEmptyEditor));
  nsresult rv = DeleteNodeWithTransaction(*paddingBRElement);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DeleteNodeWithTransaction() failed");
  return rv;
}

nsresult EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (mPaddingBRElementForEmptyEditor) {
    return NS_OK;
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eCreatePaddingBRElementForEmptyEditor,
      nsIEditor::eNone, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  ignoredError.SuppressException();

  RefPtr<Element> rootElement = GetRoot();
  if (!rootElement) {
    return NS_OK;
  }

  // Now we've got the body element. Iterate over the body element's children,
  // looking for editable content. If no editable content is found, insert the
  // padding <br> element.
  EditorType editorType = GetEditorType();
  bool isRootEditable =
      EditorUtils::IsEditableContent(*rootElement, editorType);
  for (nsIContent* rootChild = rootElement->GetFirstChild(); rootChild;
       rootChild = rootChild->GetNextSibling()) {
    if (EditorUtils::IsPaddingBRElementForEmptyEditor(*rootChild) ||
        !isRootEditable ||
        EditorUtils::IsEditableContent(*rootChild, editorType) ||
        HTMLEditUtils::IsBlockElement(*rootChild)) {
      return NS_OK;
    }
  }

  // Skip adding the padding <br> element for empty editor if body
  // is read-only.
  if (IsHTMLEditor() && !HTMLEditUtils::IsSimplyEditableNode(*rootElement)) {
    return NS_OK;
  }

  // Create a br.
  RefPtr<Element> newBRElement = CreateHTMLContent(nsGkAtoms::br);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(!newBRElement)) {
    return NS_ERROR_FAILURE;
  }

  mPaddingBRElementForEmptyEditor =
      static_cast<HTMLBRElement*>(newBRElement.get());

  // Give it a special attribute.
  newBRElement->SetFlags(NS_PADDING_FOR_EMPTY_EDITOR);

  // Put the node in the document.
  nsresult rv =
      InsertNodeWithTransaction(*newBRElement, EditorDOMPoint(rootElement, 0));
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return rv;
  }

  // Set selection.
  SelectionRefPtr()->Collapse(EditorRawDOMPoint(rootElement, 0), ignoredError);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Selection::Collapse() failed, but ignored");
  return NS_OK;
}

void EditorBase::BeginUpdateViewBatch() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mUpdateCount >= 0, "bad state");

  if (!mUpdateCount) {
    // Turn off selection updates and notifications.
    SelectionRefPtr()->StartBatchChanges();
  }

  mUpdateCount++;
}

void EditorBase::EndUpdateViewBatch() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mUpdateCount > 0, "bad state");

  if (mUpdateCount <= 0) {
    mUpdateCount = 0;
    return;
  }

  if (--mUpdateCount) {
    return;
  }

  // Turn selection updating and notifications back on.
  SelectionRefPtr()->EndBatchChanges();

  HTMLEditor* htmlEditor = AsHTMLEditor();
  if (!htmlEditor) {
    return;
  }

  // We may need to show resizing handles or update existing ones after
  // all transactions are done. This way of doing is preferred to DOM
  // mutation events listeners because all the changes the user can apply
  // to a document may result in multiple events, some of them quite hard
  // to listen too (in particular when an ancestor of the selection is
  // changed but the selection itself is not changed).
  DebugOnly<nsresult> rvIgnored = MOZ_KnownLive(htmlEditor)->RefreshEditingUI();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "HTMLEditor::RefreshEditingUI() failed, but ignored");
}

TextComposition* EditorBase::GetComposition() const { return mComposition; }

EditorRawDOMPoint EditorBase::GetCompositionStartPoint() const {
  return mComposition ? EditorRawDOMPoint(mComposition->GetStartRef())
                      : EditorRawDOMPoint();
}

EditorRawDOMPoint EditorBase::GetCompositionEndPoint() const {
  return mComposition ? EditorRawDOMPoint(mComposition->GetEndRef())
                      : EditorRawDOMPoint();
}

bool EditorBase::IsIMEComposing() const {
  return mComposition && mComposition->IsComposing();
}

bool EditorBase::ShouldHandleIMEComposition() const {
  // When the editor is being reframed, the old value may be restored with
  // InsertText().  In this time, the text should be inserted as not a part
  // of the composition.
  return mComposition && mDidPostCreate;
}

void EditorBase::DoAfterDoTransaction(nsITransaction* aTxn) {
  bool isTransientTransaction;
  MOZ_ALWAYS_SUCCEEDS(aTxn->GetIsTransient(&isTransientTransaction));

  if (!isTransientTransaction) {
    // we need to deal here with the case where the user saved after some
    // edits, then undid one or more times. Then, the undo count is -ve,
    // but we can't let a do take it back to zero. So we flip it up to
    // a +ve number.
    int32_t modCount;
    DebugOnly<nsresult> rvIgnored = GetModificationCount(&modCount);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::GetModificationCount() failed, but ignored");
    if (modCount < 0) {
      modCount = -modCount;
    }

    // don't count transient transactions
    MOZ_ALWAYS_SUCCEEDS(IncrementModificationCount(1));
  }
}

void EditorBase::DoAfterUndoTransaction() {
  // all undoable transactions are non-transient
  MOZ_ALWAYS_SUCCEEDS(IncrementModificationCount(-1));
}

void EditorBase::DoAfterRedoTransaction() {
  // all redoable transactions are non-transient
  MOZ_ALWAYS_SUCCEEDS(IncrementModificationCount(1));
}

already_AddRefed<EditAggregateTransaction>
EditorBase::CreateTxnForDeleteSelection(EDirection aAction,
                                        nsINode** aRemovingNode,
                                        int32_t* aOffset, int32_t* aLength) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Check whether the selection is collapsed and we should do nothing:
  if (NS_WARN_IF(SelectionRefPtr()->IsCollapsed() && aAction == eNone)) {
    return nullptr;
  }

  // allocate the out-param transaction
  RefPtr<EditAggregateTransaction> aggregateTransaction =
      EditAggregateTransaction::Create();

  for (uint32_t rangeIdx = 0; rangeIdx < SelectionRefPtr()->RangeCount();
       ++rangeIdx) {
    RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(rangeIdx);
    if (NS_WARN_IF(!range)) {
      return nullptr;
    }

    // Same with range as with selection; if it is collapsed and action
    // is eNone, do nothing.
    if (!range->Collapsed()) {
      RefPtr<DeleteRangeTransaction> deleteRangeTransaction =
          DeleteRangeTransaction::Create(*this, *range);
      // XXX Oh, not checking if deleteRangeTransaction can modify the range...
      DebugOnly<nsresult> rvIgnored =
          aggregateTransaction->AppendChild(deleteRangeTransaction);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditAggregationTransaction::AppendChild() failed, but ignored");
    } else if (aAction != eNone) {
      // we have an insertion point.  delete the thing in front of it or
      // behind it, depending on aAction
      // XXX Odd, when there are two or more ranges, this returns the last
      //     range information with aRemovingNode, aOffset and aLength.
      RefPtr<EditTransactionBase> deleteRangeTransaction =
          CreateTxnForDeleteRange(range, aAction, aRemovingNode, aOffset,
                                  aLength);
      NS_WARNING_ASSERTION(deleteRangeTransaction,
                           "EditorBase::CreateTxnForDeleteRange() failed");
      // XXX When there are two or more ranges and at least one of them is
      //     not editable, deleteRangeTransaction may be nullptr.
      //     In such case, should we stop removing other ranges too?
      if (!deleteRangeTransaction) {
        return nullptr;
      }
      DebugOnly<nsresult> rvIgnored =
          aggregateTransaction->AppendChild(deleteRangeTransaction);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditAggregationTransaction::AppendChild() failed, but ignored");
    }
  }

  return aggregateTransaction.forget();
}

// XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior
// are not implemented
already_AddRefed<EditTransactionBase> EditorBase::CreateTxnForDeleteRange(
    nsRange* aRangeToDelete, EDirection aAction, nsINode** aRemovingNode,
    int32_t* aOffset, int32_t* aLength) {
  MOZ_ASSERT(aAction != eNone);

  // get the node and offset of the insertion point
  nsCOMPtr<nsINode> node = aRangeToDelete->GetStartContainer();
  if (NS_WARN_IF(!node)) {
    return nullptr;
  }

  nsIContent* child = aRangeToDelete->GetChildAtStartOffset();
  int32_t offset = aRangeToDelete->StartOffset();

  // determine if the insertion point is at the beginning, middle, or end of
  // the node

  uint32_t count = node->Length();

  bool isFirst = !offset;
  bool isLast = (count == (uint32_t)offset);

  // XXX: if isFirst && isLast, then we'll need to delete the node
  //      as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if (aAction == ePrevious && isFirst) {
    // we're backspacing from the beginning of the node.  Delete the first
    // thing to our left
    nsCOMPtr<nsIContent> priorNode = GetPreviousEditableNode(*node);
    if (NS_WARN_IF(!priorNode)) {
      return nullptr;
    }

    // there is a priorNode, so delete its last child (if chardata, delete the
    // last char). if it has no children, delete it
    if (RefPtr<Text> priorNodeAsText = Text::FromNode(priorNode)) {
      uint32_t length = priorNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      if (NS_WARN_IF(!length)) {
        return nullptr;
      }
      RefPtr<DeleteTextTransaction> deleteTextTransaction =
          DeleteTextTransaction::MaybeCreateForPreviousCharacter(
              *this, *priorNodeAsText, length);
      if (!deleteTextTransaction) {
        NS_WARNING(
            "DeleteTextTransaction::MaybeCreateForPreviousCharacter() failed");
        return nullptr;
      }
      *aOffset = deleteTextTransaction->Offset();
      *aLength = deleteTextTransaction->LengthToDelete();
      priorNode.forget(aRemovingNode);
      return deleteTextTransaction.forget();
    }

    // priorNode is not chardata, so tell its parent to delete it
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
        DeleteNodeTransaction::MaybeCreate(*this, *priorNode);
    if (!deleteNodeTransaction) {
      NS_WARNING("DeleteNodeTransaction::MaybeCreate() failed");
      return nullptr;
    }
    priorNode.forget(aRemovingNode);
    return deleteNodeTransaction.forget();
  }

  if (aAction == eNext && isLast) {
    // we're deleting from the end of the node.  Delete the first thing to our
    // right
    nsCOMPtr<nsIContent> nextNode = GetNextEditableNode(*node);
    if (NS_WARN_IF(!nextNode)) {
      return nullptr;
    }

    // there is a nextNode, so delete its first child (if chardata, delete the
    // first char). if it has no children, delete it
    if (RefPtr<Text> nextNodeAsText = Text::FromNode(nextNode)) {
      uint32_t length = nextNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      if (NS_WARN_IF(!length)) {
        return nullptr;
      }
      RefPtr<DeleteTextTransaction> deleteTextTransaction =
          DeleteTextTransaction::MaybeCreateForNextCharacter(
              *this, *nextNodeAsText, 0);
      if (!deleteTextTransaction) {
        NS_WARNING(
            "DeleteTextTransaction::MaybeCreateForNextCharacter() failed");
        return nullptr;
      }
      *aOffset = deleteTextTransaction->Offset();
      *aLength = deleteTextTransaction->LengthToDelete();
      nextNode.forget(aRemovingNode);
      return deleteTextTransaction.forget();
    }

    // nextNode is not chardata, so tell its parent to delete it
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
        DeleteNodeTransaction::MaybeCreate(*this, *nextNode);
    if (!deleteNodeTransaction) {
      NS_WARNING("DeleteNodeTransaction::MaybeCreate() failed");
      return nullptr;
    }
    nextNode.forget(aRemovingNode);
    return deleteNodeTransaction.forget();
  }

  if (RefPtr<Text> nodeAsText = Text::FromNode(node)) {
    if (NS_WARN_IF(aAction != ePrevious && aAction != eNext)) {
      return nullptr;
    }
    // We have chardata, so delete a char at the proper offset
    RefPtr<DeleteTextTransaction> deleteTextTransaction;
    if (aAction == ePrevious) {
      deleteTextTransaction =
          DeleteTextTransaction::MaybeCreateForPreviousCharacter(
              *this, *nodeAsText, offset);
      NS_WARNING_ASSERTION(
          deleteTextTransaction,
          "DeleteTextTransaction::MaybeCreateForPreviousCharacter() failed");
    } else {
      deleteTextTransaction =
          DeleteTextTransaction::MaybeCreateForNextCharacter(*this, *nodeAsText,
                                                             offset);
      NS_WARNING_ASSERTION(
          deleteTextTransaction,
          "DeleteTextTransaction::MaybeCreateForNextCharacter() failed");
    }
    if (!deleteTextTransaction) {
      return nullptr;
    }
    *aOffset = deleteTextTransaction->Offset();
    *aLength = deleteTextTransaction->LengthToDelete();
    node.forget(aRemovingNode);
    return deleteTextTransaction.forget();
  }

  // we're either deleting a node or chardata, need to dig into the next/prev
  // node to find out
  nsCOMPtr<nsIContent> selectedContent;
  if (aAction == ePrevious) {
    selectedContent =
        GetPreviousEditableNode(EditorRawDOMPoint(node, child, offset));
  } else if (aAction == eNext) {
    selectedContent =
        GetNextEditableNode(EditorRawDOMPoint(node, child, offset));
  }

  while (selectedContent && selectedContent->IsCharacterData() &&
         !selectedContent->Length()) {
    // Can't delete an empty chardata node (bug 762183)
    if (aAction == ePrevious) {
      selectedContent = GetPreviousEditableNode(*selectedContent);
    } else if (aAction == eNext) {
      selectedContent = GetNextEditableNode(*selectedContent);
    }
  }

  if (NS_WARN_IF(!selectedContent)) {
    return nullptr;
  }

  if (RefPtr<Text> selectedTextNode = Text::FromNode(selectedContent)) {
    if (NS_WARN_IF(aAction != ePrevious && aAction != eNext)) {
      return nullptr;
    }
    // we are deleting from a chardata node, so do a character deletion
    uint32_t position = 0;
    if (aAction == ePrevious) {
      position = selectedTextNode->Length();
    }
    RefPtr<DeleteTextTransaction> deleteTextTransaction;
    if (aAction == ePrevious) {
      deleteTextTransaction =
          DeleteTextTransaction::MaybeCreateForPreviousCharacter(
              *this, *selectedTextNode, position);
      NS_WARNING_ASSERTION(
          deleteTextTransaction,
          "DeleteTextTransaction::MaybeCreateForPreviousCharacter() failed");
    } else {
      deleteTextTransaction =
          DeleteTextTransaction::MaybeCreateForNextCharacter(
              *this, *selectedTextNode, position);
      NS_WARNING_ASSERTION(
          deleteTextTransaction,
          "DeleteTextTransaction::MaybeCreateForNextCharacter() failed");
    }
    if (!deleteTextTransaction) {
      return nullptr;
    }
    *aOffset = deleteTextTransaction->Offset();
    *aLength = deleteTextTransaction->LengthToDelete();
    nsCOMPtr<nsINode> removingNode(selectedTextNode);
    removingNode.forget(aRemovingNode);
    return deleteTextTransaction.forget();
  }

  RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      DeleteNodeTransaction::MaybeCreate(*this, *selectedContent);
  if (NS_WARN_IF(!deleteNodeTransaction)) {
    return nullptr;
  }
  nsCOMPtr<nsINode> removingNode(selectedContent);
  removingNode.forget(aRemovingNode);
  return deleteNodeTransaction.forget();
}

nsresult EditorBase::CreateRange(nsINode* aStartContainer, int32_t aStartOffset,
                                 nsINode* aEndContainer, int32_t aEndOffset,
                                 nsRange** aRange) {
  RefPtr<nsRange> range = nsRange::Create(
      aStartContainer, aStartOffset, aEndContainer, aEndOffset, IgnoreErrors());
  if (!range) {
    NS_WARNING("nsRange::Create() failed");
    return NS_ERROR_FAILURE;
  }
  range.forget(aRange);
  return NS_OK;
}

nsresult EditorBase::AppendNodeToSelectionAsRange(nsINode* aNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aNode) && NS_WARN_IF(!aNode->IsContent())) {
    return NS_ERROR_INVALID_ARG;
  }

  EditorRawDOMPoint atContent(aNode->AsContent());
  if (NS_WARN_IF(!atContent.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsRange> range;
  nsresult rv = CreateRange(atContent.GetContainer(), atContent.Offset(),
                            atContent.GetContainer(), atContent.Offset() + 1,
                            getter_AddRefs(range));
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::CreateRange() failed");
    return rv;
  }
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult err;
  MOZ_KnownLive(SelectionRefPtr())
      ->AddRangeAndSelectFramesAndNotifyListeners(*range, err);
  NS_WARNING_ASSERTION(!err.Failed(), "Failed to add range to Selection");
  return err.StealNSResult();
}

nsresult EditorBase::ClearSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  ErrorResult error;
  SelectionRefPtr()->RemoveAllRanges(error);
  NS_WARNING_ASSERTION(!error.Failed(), "Selection::RemoveAllRanges() failed");
  return error.StealNSResult();
}

already_AddRefed<Element> EditorBase::CreateHTMLContent(const nsAtom* aTag) {
  MOZ_ASSERT(aTag);

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }

  // XXX Wallpaper over editor bug (editor tries to create elements with an
  //     empty nodename).
  if (aTag == nsGkAtoms::_empty) {
    NS_ERROR(
        "Don't pass an empty tag to EditorBase::CreateHTMLContent, "
        "check caller.");
    return nullptr;
  }

  return document->CreateElem(nsDependentAtomString(aTag), nullptr,
                              kNameSpaceID_XHTML);
}

already_AddRefed<nsTextNode> EditorBase::CreateTextNode(
    const nsAString& aData) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  RefPtr<nsTextNode> text = document->CreateEmptyTextNode();
  text->MarkAsMaybeModifiedFrequently();
  if (IsPasswordEditor()) {
    text->MarkAsMaybeMasked();
  }
  // Don't notify; this node is still being created.
  DebugOnly<nsresult> rvIgnored = text->SetText(aData, false);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Text::SetText() failed, but ignored");
  return text.forget();
}

NS_IMETHODIMP EditorBase::SetAttributeOrEquivalent(Element* aElement,
                                                   const nsAString& aAttribute,
                                                   const nsAString& aValue,
                                                   bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  rv = SetAttributeOrEquivalent(aElement, attribute, aValue,
                                aSuppressTransaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::SetAttributeOrEquivalent() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP EditorBase::RemoveAttributeOrEquivalent(
    Element* aElement, const nsAString& aAttribute, bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveAttribute);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  rv = RemoveAttributeOrEquivalent(aElement, attribute, aSuppressTransaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::RemoveAttributeOrEquivalent() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult EditorBase::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent) {
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_ERROR_UNEXPECTED;
  }
  MOZ_ASSERT(aKeyboardEvent->mMessage == eKeyPress,
             "HandleKeyPressEvent gets non-keypress event");

  // if we are readonly or disabled, then do nothing.
  if (IsReadonly()) {
    // consume backspace for disabled and readonly textfields, to prevent
    // back in history, which could be confusing to users
    if (aKeyboardEvent->mKeyCode == NS_VK_BACK) {
      aKeyboardEvent->PreventDefault();
    }
    return NS_OK;
  }

  switch (aKeyboardEvent->mKeyCode) {
    case NS_VK_META:
    case NS_VK_WIN:
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
      aKeyboardEvent->PreventDefault();  // consumed
      return NS_OK;
  }
  return NS_OK;
}

nsresult EditorBase::HandleInlineSpellCheck(
    const EditorDOMPoint& aPreviouslySelectedStart,
    const AbstractRange* aRange) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mInlineSpellChecker) {
    return NS_OK;
  }
  nsresult rv = mInlineSpellChecker->SpellCheckAfterEditorChange(
      GetTopLevelEditSubAction(), *SelectionRefPtr(),
      aPreviouslySelectedStart.GetContainer(),
      aPreviouslySelectedStart.Offset(),
      aRange ? aRange->GetStartContainer() : nullptr,
      aRange ? aRange->StartOffset() : 0,
      aRange ? aRange->GetEndContainer() : nullptr,
      aRange ? aRange->EndOffset() : 0);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "mozInlineSpellChecker::SpellCheckAfterEditorChange() failed");
  return rv;
}

Element* EditorBase::FindSelectionRoot(nsINode* aNode) const {
  return GetRoot();
}

void EditorBase::InitializeSelectionAncestorLimit(nsIContent& aAncestorLimit) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SelectionRefPtr()->SetAncestorLimiter(&aAncestorLimit);
}

nsresult EditorBase::InitializeSelection(nsINode& aFocusEventTargetNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsCOMPtr<nsIContent> selectionRootContent =
      FindSelectionRoot(&aFocusEventTargetNode);
  if (!selectionRootContent) {
    return NS_OK;
  }

  nsCOMPtr<nsISelectionController> selectionController =
      GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_FAILURE;
  }

  // Init the caret
  RefPtr<nsCaret> caret = GetCaret();
  if (NS_WARN_IF(!caret)) {
    return NS_ERROR_FAILURE;
  }
  caret->SetSelection(SelectionRefPtr());
  DebugOnly<nsresult> rvIgnored =
      selectionController->SetCaretReadOnly(IsReadonly());
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsISelectionController::SetCaretReadOnly() failed, but ignored");
  rvIgnored = selectionController->SetCaretEnabled(true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsISelectionController::SetCaretEnabled() failed, but ignored");
  // NOTE(emilio): It's important for this call to be after
  // SetCaretEnabled(true), since that would override mIgnoreUserModify to true.
  //
  // Also, make sure to always ignore it for designMode, since that effectively
  // overrides everything and we allow to edit stuff with
  // contenteditable="false" subtrees in such a document.
  caret->SetIgnoreUserModify(
      aFocusEventTargetNode.OwnerDoc()->HasFlag(NODE_IS_EDITABLE));

  // Init selection
  rvIgnored =
      selectionController->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsISelectionController::SetSelectionFlags() failed, but ignored");

  selectionController->SelectionWillTakeFocus();

  // If the computed selection root isn't root content, we should set it
  // as selection ancestor limit.  However, if that is root element, it means
  // there is not limitation of the selection, then, we must set nullptr.
  // NOTE: If we set a root element to the ancestor limit, some selection
  // methods don't work fine.
  if (selectionRootContent->GetParent()) {
    InitializeSelectionAncestorLimit(*selectionRootContent);
  } else {
    SelectionRefPtr()->SetAncestorLimiter(nullptr);
  }

  // If there is composition when this is called, we may need to restore IME
  // selection because if the editor is reframed, this already forgot IME
  // selection and the transaction.
  if (mComposition && mComposition->IsMovingToNewTextNode()) {
    // We need to look for the new text node from current selection.
    // XXX If selection is changed during reframe, this doesn't work well!
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }
    EditorRawDOMPoint atStartOfFirstRange(firstRange->StartRef());
    EditorRawDOMPoint betterInsertionPoint =
        FindBetterInsertionPoint(atStartOfFirstRange);
    RefPtr<Text> textNode = betterInsertionPoint.GetContainerAsText();
    MOZ_ASSERT(textNode,
               "There must be text node if composition string is not empty");
    if (textNode) {
      MOZ_ASSERT(textNode->Length() >= mComposition->XPEndOffsetInTextNode(),
                 "The text node must be different from the old text node");
      RefPtr<TextRangeArray> ranges = mComposition->GetRanges();
      DebugOnly<nsresult> rvIgnored = CompositionTransaction::SetIMESelection(
          *this, textNode, mComposition->XPOffsetInTextNode(),
          mComposition->XPLengthInTextNode(), ranges);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CompositionTransaction::SetIMESelection() failed, but ignored");
    }
  }

  return NS_OK;
}

nsresult EditorBase::FinalizeSelection() {
  nsCOMPtr<nsISelectionController> selectionController =
      GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_FAILURE;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  SelectionRefPtr()->SetAncestorLimiter(nullptr);

  if (NS_WARN_IF(!GetPresShell())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (RefPtr<nsCaret> caret = GetCaret()) {
    caret->SetIgnoreUserModify(true);
    DebugOnly<nsresult> rvIgnored = selectionController->SetCaretEnabled(false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "nsISelectionController::SetCaretEnabled(false) failed, but ignored");
  }

  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  focusManager->UpdateCaretForCaretBrowsingMode();
  if (nsCOMPtr<nsINode> node = do_QueryInterface(GetDOMEventTarget())) {
    if (node->OwnerDoc()->GetUnretargetedFocusedContent() != node) {
      selectionController->SelectionWillLoseFocus();
    }
  }
  return NS_OK;
}

void EditorBase::ReinitializeSelection(Element& aElement) {
  if (NS_WARN_IF(Destroyed())) {
    return;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  OnFocus(aElement);

  // If previous focused editor turn on spellcheck and this editor doesn't
  // turn on it, spellcheck state is mismatched.  So we need to re-sync it.
  SyncRealTimeSpell();

  nsPresContext* context = GetPresContext();
  if (NS_WARN_IF(!context)) {
    return;
  }
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContentForIME();
  IMEStateManager::OnFocusInEditor(context, focusedContent, *this);
}

Element* EditorBase::GetEditorRoot() const { return GetRoot(); }

Element* EditorBase::GetExposedRoot() const {
  Element* rootElement = GetRoot();
  if (!rootElement || !rootElement->IsInNativeAnonymousSubtree()) {
    return rootElement;
  }
  return Element::FromNodeOrNull(
      rootElement->GetClosestNativeAnonymousSubtreeRootParent());
}

nsresult EditorBase::DetermineCurrentDirection() {
  // Get the current root direction from its frame
  Element* rootElement = GetExposedRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  // If we don't have an explicit direction, determine our direction
  // from the content's direction
  if (!IsRightToLeft() && !IsLeftToRight()) {
    nsIFrame* frameForRootElement = rootElement->GetPrimaryFrame();
    if (NS_WARN_IF(!frameForRootElement)) {
      return NS_ERROR_FAILURE;
    }

    // Set the flag here, to enable us to use the same code path below.
    // It will be flipped before returning from the function.
    if (frameForRootElement->StyleVisibility()->mDirection ==
        StyleDirection::Rtl) {
      mFlags |= nsIEditor::eEditorRightToLeft;
    } else {
      mFlags |= nsIEditor::eEditorLeftToRight;
    }
  }

  return NS_OK;
}

nsresult EditorBase::ToggleTextDirectionAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eSetTextDirection,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = DetermineCurrentDirection();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DetermineCurrentDirection() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  MOZ_ASSERT(IsRightToLeft() || IsLeftToRight());
  // Note that we need to consider new direction before dispatching
  // "beforeinput" event since "beforeinput" event listener may change it
  // but not canceled.
  TextDirection newDirection =
      IsRightToLeft() ? TextDirection::eLTR : TextDirection::eRTL;
  editActionData.SetData(IsRightToLeft() ? NS_LITERAL_STRING("ltr")
                                         : NS_LITERAL_STRING("rtl"));

  // FYI: Oddly, Chrome does not dispatch beforeinput event in this case but
  //      dispatches input event.
  rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = SetTextDirectionTo(newDirection);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::SetTextDirectionTo() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // XXX When we don't change the text direction, do we really need to
  //     dispatch input event?
  DispatchInputEvent();

  return NS_OK;
}

void EditorBase::SwitchTextDirectionTo(TextDirection aTextDirection) {
  MOZ_ASSERT(aTextDirection == TextDirection::eLTR ||
             aTextDirection == TextDirection::eRTL);

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetTextDirection);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  nsresult rv = DetermineCurrentDirection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  editActionData.SetData(aTextDirection == TextDirection::eLTR
                             ? NS_LITERAL_STRING("ltr")
                             : NS_LITERAL_STRING("rtl"));

  // FYI: Oddly, Chrome does not dispatch beforeinput event in this case but
  //      dispatches input event.
  rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent() failed");
    return;
  }

  if ((aTextDirection == TextDirection::eLTR && IsRightToLeft()) ||
      (aTextDirection == TextDirection::eRTL && IsLeftToRight())) {
    // Do it only when the direction is still different from the original
    // new direction.  Note that "beforeinput" event listener may have already
    // changed the direction here, but they may not cancel the event.
    nsresult rv = SetTextDirectionTo(aTextDirection);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::SetTextDirectionTo() failed");
      return;
    }
  }

  // XXX When we don't change the text direction, do we really need to
  //     dispatch input event?
  DispatchInputEvent();
}

nsresult EditorBase::SetTextDirectionTo(TextDirection aTextDirection) {
  Element* rootElement = GetExposedRoot();

  if (aTextDirection == TextDirection::eLTR) {
    NS_ASSERTION(!IsLeftToRight(), "Unexpected mutually exclusive flag");
    mFlags &= ~nsIEditor::eEditorRightToLeft;
    mFlags |= nsIEditor::eEditorLeftToRight;
    nsresult rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir,
                                       NS_LITERAL_STRING("ltr"), true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Element::SetAttr(nsGkAtoms::dir, ltr) failed");
    return rv;
  }

  if (aTextDirection == TextDirection::eRTL) {
    NS_ASSERTION(!IsRightToLeft(), "Unexpected mutually exclusive flag");
    mFlags |= nsIEditor::eEditorRightToLeft;
    mFlags &= ~nsIEditor::eEditorLeftToRight;
    nsresult rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir,
                                       NS_LITERAL_STRING("rtl"), true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Element::SetAttr(nsGkAtoms::dir, rtl) failed");
    return rv;
  }

  return NS_OK;
}

nsIContent* EditorBase::GetFocusedContent() const {
  EventTarget* piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return nullptr;
  }

  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return nullptr;
  }

  nsIContent* content = focusManager->GetFocusedElement();
  MOZ_ASSERT((content == piTarget) == SameCOMIdentity(content, piTarget));

  return (content == piTarget) ? content : nullptr;
}

nsIContent* EditorBase::GetFocusedContentForIME() const {
  return GetFocusedContent();
}

bool EditorBase::IsActiveInDOMWindow() const {
  EventTarget* piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return false;
  }

  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return false;  // Do we need to check the singleton instance??
  }

  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }
  nsPIDOMWindowOuter* ourWindow = document->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> win;
  nsIContent* content = nsFocusManager::GetFocusedDescendant(
      ourWindow, nsFocusManager::eOnlyCurrentWindow, getter_AddRefs(win));
  return SameCOMIdentity(content, piTarget);
}

bool EditorBase::IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent) {
  // If the event is trusted, the event should always cause input.
  if (NS_WARN_IF(!aGUIEvent)) {
    return false;
  }

  // If this is dispatched by using cordinates but this editor doesn't have
  // focus, we shouldn't handle it.
  if (aGUIEvent->IsUsingCoordinates()) {
    nsIContent* focusedContent = GetFocusedContent();
    if (!focusedContent) {
      return false;
    }
  }

  // If a composition event isn't dispatched via widget, we need to ignore them
  // since they cannot be managed by TextComposition. E.g., the event was
  // created by chrome JS.
  // Note that if we allow to handle such events, editor may be confused by
  // strange event order.
  bool needsWidget = false;
  switch (aGUIEvent->mMessage) {
    case eUnidentifiedEvent:
      // If events are not created with proper event interface, their message
      // are initialized with eUnidentifiedEvent.  Let's ignore such event.
      return false;
    case eCompositionStart:
    case eCompositionEnd:
    case eCompositionUpdate:
    case eCompositionChange:
    case eCompositionCommitAsIs:
      // Don't allow composition events whose internal event are not
      // WidgetCompositionEvent.
      if (!aGUIEvent->AsCompositionEvent()) {
        return false;
      }
      needsWidget = true;
      break;
    default:
      break;
  }
  if (needsWidget && !aGUIEvent->mWidget) {
    return false;
  }

  // Accept all trusted events.
  if (aGUIEvent->IsTrusted()) {
    return true;
  }

  // Ignore untrusted mouse event.
  // XXX Why are we handling other untrusted input events?
  if (aGUIEvent->AsMouseEventBase()) {
    return false;
  }

  // Otherwise, we shouldn't handle any input events when we're not an active
  // element of the DOM window.
  return IsActiveInDOMWindow();
}

void EditorBase::OnFocus(nsINode& aFocusEventTargetNode) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  InitializeSelection(aFocusEventTargetNode);
  mSpellCheckerDictionaryUpdated = false;
  if (mInlineSpellChecker && CanEnableSpellCheck()) {
    DebugOnly<nsresult> rvIgnored =
        mInlineSpellChecker->UpdateCurrentDictionary();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "mozInlineSpellCHecker::UpdateCurrentDictionary() failed, but ignored");
    mSpellCheckerDictionaryUpdated = true;
  }
}

void EditorBase::HideCaret(bool aHide) {
  if (mHidingCaret == aHide) {
    return;
  }

  RefPtr<nsCaret> caret = GetCaret();
  if (NS_WARN_IF(!caret)) {
    return;
  }

  mHidingCaret = aHide;
  if (aHide) {
    caret->AddForceHide();
  } else {
    caret->RemoveForceHide();
  }
}

NS_IMETHODIMP EditorBase::Unmask(uint32_t aStart, int64_t aEnd,
                                 uint32_t aTimeout, uint8_t aArgc) {
  if (NS_WARN_IF(!IsPasswordEditor())) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(aArgc >= 1 && aStart == UINT32_MAX) ||
      NS_WARN_IF(aArgc >= 2 && aEnd == 0) ||
      NS_WARN_IF(aArgc >= 2 && aEnd > 0 && aStart >= aEnd)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eHidePassword);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  uint32_t start = aArgc < 1 ? 0 : aStart;
  uint32_t length = aArgc < 2 || aEnd < 0 ? UINT32_MAX : aEnd - start;
  uint32_t timeout = aArgc < 3 ? 0 : aTimeout;
  nsresult rv = MOZ_KnownLive(AsTextEditor())
                    ->SetUnmaskRangeAndNotify(start, length, timeout);
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::SetUnmaskRangeAndNotify() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // Flush pending layout right now since the caller may access us before
  // doing it.
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(FlushType::Layout);
  }

  return NS_OK;
}

NS_IMETHODIMP EditorBase::Mask() {
  if (NS_WARN_IF(!IsPasswordEditor())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eHidePassword);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = MOZ_KnownLive(AsTextEditor())->MaskAllCharactersAndNotify();
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::MaskAllCharactersAndNotify() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // Flush pending layout right now since the caller may access us before
  // doing it.
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(FlushType::Layout);
  }

  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetUnmaskedStart(uint32_t* aResult) {
  if (NS_WARN_IF(!IsPasswordEditor())) {
    *aResult = 0;
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aResult =
      AsTextEditor()->IsAllMasked() ? 0 : AsTextEditor()->UnmaskedStart();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetUnmaskedEnd(uint32_t* aResult) {
  if (NS_WARN_IF(!IsPasswordEditor())) {
    *aResult = 0;
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aResult = AsTextEditor()->IsAllMasked() ? 0 : AsTextEditor()->UnmaskedEnd();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetAutoMaskingEnabled(bool* aResult) {
  if (NS_WARN_IF(!IsPasswordEditor())) {
    *aResult = false;
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aResult = AsTextEditor()->IsMaskingPassword();
  return NS_OK;
}

NS_IMETHODIMP EditorBase::GetPasswordMask(nsAString& aPasswordMask) {
  aPasswordMask.Assign(TextEditor::PasswordMask());
  return NS_OK;
}

template <typename PT, typename CT>
EditActionResult EditorBase::SetCaretBidiLevelForDeletion(
    const EditorDOMPointBase<PT, CT>& aPointAtCaret,
    nsIEditor::EDirection aDirectionAndAmount) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsPresContext* presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  if (!presContext->BidiEnabled()) {
    return EditActionIgnored();  // Perform the deletion
  }

  if (!aPointAtCaret.GetContainerAsContent()) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  // XXX Not sure whether this requires strong reference here.
  RefPtr<nsFrameSelection> frameSelection =
      SelectionRefPtr()->GetFrameSelection();
  if (NS_WARN_IF(!frameSelection)) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  nsPrevNextBidiLevels levels = frameSelection->GetPrevNextBidiLevels(
      aPointAtCaret.GetContainerAsContent(), aPointAtCaret.Offset(), true);

  nsBidiLevel levelBefore = levels.mLevelBefore;
  nsBidiLevel levelAfter = levels.mLevelAfter;

  nsBidiLevel currentCaretLevel = frameSelection->GetCaretBidiLevel();

  nsBidiLevel levelOfDeletion;
  levelOfDeletion = (nsIEditor::eNext == aDirectionAndAmount ||
                     nsIEditor::eNextWord == aDirectionAndAmount)
                        ? levelAfter
                        : levelBefore;

  if (currentCaretLevel == levelOfDeletion) {
    return EditActionIgnored();  // Perform the deletion
  }

  // Set the bidi level of the caret to that of the
  // character that will be (or would have been) deleted
  frameSelection->SetCaretBidiLevel(levelOfDeletion);

  if (!StaticPrefs::bidi_edit_delete_immediately() &&
      levelBefore != levelAfter) {
    return EditActionCanceled();  // Cancel deletion due to the bidi boundary
  }

  return EditActionIgnored();  // Perform the deletion
}

void EditorBase::UndefineCaretBidiLevel() const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  /**
   * After inserting text the caret Bidi level must be set to the level of the
   * inserted text.This is difficult, because we cannot know what the level is
   * until after the Bidi algorithm is applied to the whole paragraph.
   *
   * So we set the caret Bidi level to UNDEFINED here, and the caret code will
   * set it correctly later
   */
  nsFrameSelection* frameSelection = SelectionRefPtr()->GetFrameSelection();
  if (frameSelection) {
    frameSelection->UndefineCaretBidiLevel();
  }
}

NS_IMETHODIMP EditorBase::GetTextLength(int32_t* aCount) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EditorBase::GetWrapWidth(int32_t* aWrapColumn) {
  if (NS_WARN_IF(!aWrapColumn)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aWrapColumn = WrapWidth();
  return NS_OK;
}

//
// See if the style value includes this attribute, and if it does,
// cut out everything from the attribute to the next semicolon.
//
static void CutStyle(const char* stylename, nsString& styleValue) {
  // Find the current wrapping type:
  int32_t styleStart = styleValue.Find(stylename, true);
  if (styleStart >= 0) {
    int32_t styleEnd = styleValue.Find(";", false, styleStart);
    if (styleEnd > styleStart) {
      styleValue.Cut(styleStart, styleEnd - styleStart + 1);
    } else {
      styleValue.Cut(styleStart, styleValue.Length() - styleStart);
    }
  }
}

NS_IMETHODIMP EditorBase::SetWrapWidth(int32_t aWrapColumn) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eSetWrapWidth);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  SetWrapColumn(aWrapColumn);

  // Make sure we're a plaintext editor, otherwise we shouldn't
  // do the rest of this.
  if (!IsPlaintextEditor()) {
    return NS_OK;
  }

  // Ought to set a style sheet here ...
  // Probably should keep around an mPlaintextStyleSheet for this purpose.
  RefPtr<Element> rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Get the current style for this root element:
  nsAutoString styleValue;
  rootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::style, styleValue);

  // We'll replace styles for these values:
  CutStyle("white-space", styleValue);
  CutStyle("width", styleValue);
  CutStyle("font-family", styleValue);

  // If we have other style left, trim off any existing semicolons
  // or whitespace, then add a known semicolon-space:
  if (!styleValue.IsEmpty()) {
    styleValue.Trim("; \t", false, true);
    styleValue.AppendLiteral("; ");
  }

  // Make sure we have fixed-width font.  This should be done for us,
  // but it isn't, see bug 22502, so we have to add "font: -moz-fixed;".
  // Only do this if we're wrapping.
  if (IsWrapHackEnabled() && aWrapColumn >= 0) {
    styleValue.AppendLiteral("font-family: -moz-fixed; ");
  }

  // and now we're ready to set the new whitespace/wrapping style.
  if (aWrapColumn > 0) {
    // Wrap to a fixed column.
    styleValue.AppendLiteral("white-space: pre-wrap; width: ");
    styleValue.AppendInt(aWrapColumn);
    styleValue.AppendLiteral("ch;");
  } else if (!aWrapColumn) {
    styleValue.AppendLiteral("white-space: pre-wrap;");
  } else {
    styleValue.AppendLiteral("white-space: pre;");
  }

  nsresult rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                                     styleValue, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Element::SetAttr(nsGkAtoms::style) failed");
  return rv;
}

NS_IMETHODIMP EditorBase::GetNewlineHandling(int32_t* aNewlineHandling) {
  if (NS_WARN_IF(!aNewlineHandling)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aNewlineHandling = mNewlineHandling;
  return NS_OK;
}

NS_IMETHODIMP EditorBase::SetNewlineHandling(int32_t aNewlineHandling) {
  switch (aNewlineHandling) {
    case nsIEditor::eNewlinesPasteIntact:
    case nsIEditor::eNewlinesPasteToFirst:
    case nsIEditor::eNewlinesReplaceWithSpaces:
    case nsIEditor::eNewlinesStrip:
    case nsIEditor::eNewlinesReplaceWithCommas:
    case nsIEditor::eNewlinesStripSurroundingWhitespace:
      mNewlineHandling = aNewlineHandling;
      return NS_OK;
    default:
      NS_ERROR("SetNewlineHandling() is called with wrong value");
      return NS_ERROR_INVALID_ARG;
  }
}

bool EditorBase::IsSelectionRangeContainerNotContent() const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  for (uint32_t i = 0; i < SelectionRefPtr()->RangeCount(); i++) {
    nsRange* range = SelectionRefPtr()->GetRangeAt(i);
    MOZ_ASSERT(range);
    if (!range || !range->GetStartContainer() ||
        !range->GetStartContainer()->IsContent() || !range->GetEndContainer() ||
        !range->GetEndContainer()->IsContent()) {
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP EditorBase::InsertText(const nsAString& aStringToInsert) {
  nsresult rv = InsertTextAsAction(aStringToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsAction() failed");
  return rv;
}

nsresult EditorBase::InsertTextAsAction(const nsAString& aStringToInsert,
                                        nsIPrincipal* aPrincipal) {
  // Showing this assertion is fine if this method is called by outside via
  // mutation event listener or something.  Otherwise, this is called by
  // wrong method.
  NS_ASSERTION(!mPlaceholderBatch,
               "Should be called only when this is the only edit action of the "
               "operation "
               "unless mutation event listener nests some operations");

  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText,
                                          aPrincipal);
  // Note that we don't need to replace native line breaks with XP line breaks
  // here because Chrome does not do it.
  MOZ_ASSERT(!aStringToInsert.IsVoid());
  editActionData.SetData(aStringToInsert);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  nsString stringToInsert(aStringToInsert);
  if (!AsHTMLEditor()) {
    nsContentUtils::PlatformToDOMLineBreaks(stringToInsert);
  }
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  rv = InsertTextAsSubAction(stringToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult EditorBase::InsertTextAsSubAction(const nsAString& aStringToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch);
  MOZ_ASSERT(AsHTMLEditor() ||
             aStringToInsert.FindChar(nsCRT::CR) == kNotFound);

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  EditSubAction editSubAction = ShouldHandleIMEComposition()
                                    ? EditSubAction::eInsertTextComingFromIME
                                    : EditSubAction::eInsertText;

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, editSubAction, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result =
      MOZ_KnownLive(AsTextEditor())
          ->HandleInsertText(editSubAction, aStringToInsert);
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "TextEditor::HandleInsertText() failed");
  return result.Rv();
}

NS_IMETHODIMP EditorBase::InsertLineBreak() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLineBreak);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (NS_WARN_IF(IsSingleLineEditor())) {
    return NS_ERROR_FAILURE;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  rv = InsertLineBreakAsSubAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertLineBreakAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult EditorBase::InsertLineBreakAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertLineBreak, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result =
      MOZ_KnownLive(AsTextEditor())->InsertLineFeedCharacterAtSelection();
  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "TextEditor::InsertLineFeedCharacterAtSelection() failed, but ignored");
  return result.Rv();
}

/******************************************************************************
 * EditorBase::AutoSelectionRestorer
 *****************************************************************************/

EditorBase::AutoSelectionRestorer::AutoSelectionRestorer(
    EditorBase& aEditorBase MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : mEditorBase(nullptr) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  if (aEditorBase.ArePreservingSelection()) {
    // We already have initialized mParentData::mSavedSelection, so this must
    // be nested call.
    return;
  }
  MOZ_ASSERT(aEditorBase.IsEditActionDataAvailable());
  mEditorBase = &aEditorBase;
  mEditorBase->PreserveSelectionAcrossActions();
}

EditorBase::AutoSelectionRestorer::~AutoSelectionRestorer() {
  if (mEditorBase && mEditorBase->ArePreservingSelection()) {
    DebugOnly<nsresult> rvIgnored = mEditorBase->RestorePreservedSelection();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::RestorePreservedSelection() failed, but ignored");
  }
}

void EditorBase::AutoSelectionRestorer::Abort() {
  if (mEditorBase) {
    mEditorBase->StopPreservingSelection();
  }
}

/*****************************************************************************
 * mozilla::EditorBase::AutoEditActionDataSetter
 *****************************************************************************/

EditorBase::AutoEditActionDataSetter::AutoEditActionDataSetter(
    const EditorBase& aEditorBase, EditAction aEditAction,
    nsIPrincipal* aPrincipal /* = nullptr */)
    : mEditorBase(const_cast<EditorBase&>(aEditorBase)),
      mPrincipal(aPrincipal),
      mParentData(aEditorBase.mEditActionData),
      mData(VoidString()),
      mTopLevelEditSubAction(EditSubAction::eNone),
      mAborted(false),
      mHasTriedToDispatchBeforeInputEvent(false),
      mBeforeInputEventCanceled(false) {
  // If we're nested edit action, copies necessary data from the parent.
  if (mParentData) {
    mSelection = mParentData->mSelection;
    MOZ_ASSERT(!mSelection ||
               (mSelection->GetType() == SelectionType::eNormal));

    // If we're eNotEditing, we should inherit the parent's edit action.
    // This may occur if creator or its callee use public methods which
    // just returns something.
    if (aEditAction != EditAction::eNotEditing) {
      mEditAction = aEditAction;
    }
    mTopLevelEditSubAction = mParentData->mTopLevelEditSubAction;

    // Parent's mTopLevelEditSubActionData should be referred instead so that
    // we don't need to set mTopLevelEditSubActionData.mSelectedRange nor
    // mTopLevelEditActionData.mChangedRange here.

    mDirectionOfTopLevelEditSubAction =
        mParentData->mDirectionOfTopLevelEditSubAction;
  } else {
    mSelection = mEditorBase.GetSelection();
    if (NS_WARN_IF(!mSelection)) {
      return;
    }

    MOZ_ASSERT(mSelection->GetType() == SelectionType::eNormal);

    mEditAction = aEditAction;
    mDirectionOfTopLevelEditSubAction = eNone;
    if (mEditorBase.IsHTMLEditor()) {
      mTopLevelEditSubActionData.mSelectedRange =
          mEditorBase.AsHTMLEditor()
              ->GetSelectedRangeItemForTopLevelEditSubAction();
      mTopLevelEditSubActionData.mChangedRange =
          mEditorBase.AsHTMLEditor()->GetChangedRangeForTopLevelEditSubAction();
      mTopLevelEditSubActionData.mCachedInlineStyles.emplace();
    }
  }
  mEditorBase.mEditActionData = this;
}

EditorBase::AutoEditActionDataSetter::~AutoEditActionDataSetter() {
  MOZ_ASSERT(mHasCanHandleChecked);

  if (!mSelection || NS_WARN_IF(mEditorBase.mEditActionData != this)) {
    return;
  }
  mEditorBase.mEditActionData = mParentData;

  MOZ_ASSERT(
      !mTopLevelEditSubActionData.mSelectedRange ||
          (!mTopLevelEditSubActionData.mSelectedRange->mStartContainer &&
           !mTopLevelEditSubActionData.mSelectedRange->mEndContainer),
      "mTopLevelEditSubActionData.mSelectedRange should've been cleared");
}

void EditorBase::AutoEditActionDataSetter::SetColorData(
    const nsAString& aData) {
  MOZ_ASSERT(!HasTriedToDispatchBeforeInputEvent(),
             "It's too late to set data since this may have already dispatched "
             "a beforeinput event");

  if (aData.IsEmpty()) {
    // When removing color/background-color, let's use empty string.
    MOZ_ASSERT(!EmptyString().IsVoid());
    mData = EmptyString();
    return;
  }

  bool wasCurrentColor = false;
  nscolor color = NS_RGB(0, 0, 0);
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0),
                                    NS_ConvertUTF16toUTF8(aData), &color,
                                    &wasCurrentColor)) {
    // If we cannot parse aData, let's set original value as-is.  It could be
    // new format defined by newer spec.
    MOZ_ASSERT(!aData.IsVoid());
    mData = aData;
    return;
  }

  // If it's current color, we cannot resolve actual current color here.
  // So, let's return "currentcolor" keyword, but let's use it as-is because
  // there is no agreement between browser vendors.
  if (wasCurrentColor) {
    MOZ_ASSERT(!aData.IsVoid());
    mData = aData;
    return;
  }

  // Get serialized color value (i.e., "rgb()" or "rgba()").
  nsStyleUtil::GetSerializedColorValue(color, mData);
  MOZ_ASSERT(!mData.IsVoid());
}

void EditorBase::AutoEditActionDataSetter::InitializeDataTransfer(
    DataTransfer* aDataTransfer) {
  MOZ_ASSERT(aDataTransfer);
  MOZ_ASSERT(aDataTransfer->IsReadOnly());
  MOZ_ASSERT(!HasTriedToDispatchBeforeInputEvent(),
             "It's too late to set dataTransfer since this may have already "
             "dispatched a beforeinput event");

  mDataTransfer = aDataTransfer;
}

void EditorBase::AutoEditActionDataSetter::InitializeDataTransfer(
    nsITransferable* aTransferable) {
  MOZ_ASSERT(aTransferable);
  MOZ_ASSERT(!HasTriedToDispatchBeforeInputEvent(),
             "It's too late to set dataTransfer since this may have already "
             "dispatched a beforeinput event");

  Document* document = mEditorBase.GetDocument();
  nsIGlobalObject* scopeObject =
      document ? document->GetScopeObject() : nullptr;
  mDataTransfer = new DataTransfer(scopeObject, eEditorInput, aTransferable);
}

void EditorBase::AutoEditActionDataSetter::InitializeDataTransfer(
    const nsAString& aString) {
  MOZ_ASSERT(!HasTriedToDispatchBeforeInputEvent(),
             "It's too late to set dataTransfer since this may have already "
             "dispatched a beforeinput event");
  Document* document = mEditorBase.GetDocument();
  nsIGlobalObject* scopeObject =
      document ? document->GetScopeObject() : nullptr;
  mDataTransfer = new DataTransfer(scopeObject, eEditorInput, aString);
}

void EditorBase::AutoEditActionDataSetter::InitializeDataTransferWithClipboard(
    SettingDataTransfer aSettingDataTransfer, int32_t aClipboardType) {
  MOZ_ASSERT(!HasTriedToDispatchBeforeInputEvent(),
             "It's too late to set dataTransfer since this may have already "
             "dispatched a beforeinput event");

  Document* document = mEditorBase.GetDocument();
  nsIGlobalObject* scopeObject =
      document ? document->GetScopeObject() : nullptr;
  // mDataTransfer will be used for eEditorInput event, but we can keep
  // using ePaste and ePasteNoFormatting here.  If we need to use eEditorInput,
  // we need to create eEditorInputNoFormatting or something...
  mDataTransfer =
      new DataTransfer(scopeObject,
                       aSettingDataTransfer == SettingDataTransfer::eWithFormat
                           ? ePaste
                           : ePasteNoFormatting,
                       true /* is external */, aClipboardType);
}

void EditorBase::AutoEditActionDataSetter::AppendTargetRange(
    StaticRange& aTargetRange) {
  mTargetRanges.AppendElement(aTargetRange);
}

nsresult EditorBase::AutoEditActionDataSetter::MaybeDispatchBeforeInputEvent() {
  MOZ_ASSERT(!HasTriedToDispatchBeforeInputEvent(),
             "We've already handled beforeinput event");
  MOZ_ASSERT(CanHandle());
  MOZ_ASSERT(NeedsToDispatchBeforeInputEvent());

  mHasTriedToDispatchBeforeInputEvent = true;

  if (!StaticPrefs::dom_input_events_beforeinput_enabled()) {
    return NS_OK;
  }

  // Don't dispatch "beforeinput" event when the editor user makes us stop
  // dispatching input event.
  if (mEditorBase.IsSuppressingDispatchingInputEvent()) {
    return NS_OK;
  }

  // If mPrincipal has set, it means that we're handling an edit action
  // which is requested by JS.  If it's not chrome script, we shouldn't
  // dispatch "beforeinput" event.
  if (mPrincipal && !mPrincipal->IsSystemPrincipal()) {
    // But if it's content script of an addon, `execCommand` calls are a
    // part of browser's default action from point of view of web apps.
    // Therefore, we should dispatch `beforeinput` event.
    // https://github.com/w3c/input-events/issues/91
    if (!mPrincipal->GetIsAddonOrExpandedAddonPrincipal()) {
      return NS_OK;
    }
  }

  // If we're called from OnCompositionEnd(), we shouldn't dispatch
  // "beforeinput" event since the preceding OnCompositionChange() call has
  // already dispatched "beforeinput" event for this.
  if (mEditAction == EditAction::eCommitComposition ||
      mEditAction == EditAction::eCancelComposition) {
    return NS_OK;
  }

  RefPtr<Element> targetElement = mEditorBase.GetInputEventTargetElement();
  if (NS_WARN_IF(!targetElement)) {
    return NS_ERROR_FAILURE;
  }
  OwningNonNull<TextEditor> textEditor = *mEditorBase.AsTextEditor();
  EditorInputType inputType = ToInputType(mEditAction);
  // If mTargetRanges has not been initialized yet, it means that we may need
  // to set it to selection ranges.
  if (textEditor->AsHTMLEditor() && mTargetRanges.IsEmpty() &&
      MayHaveTargetRangesOnHTMLEditor(inputType)) {
    if (uint32_t rangeCount = textEditor->SelectionRefPtr()->RangeCount()) {
      mTargetRanges.SetCapacity(rangeCount);
      for (uint32_t i = 0; i < rangeCount; i++) {
        nsRange* range = textEditor->SelectionRefPtr()->GetRangeAt(i);
        if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
          continue;
        }
        // Now, we need to fix the offset of target range because it may
        // be referred after modifying the DOM tree and range boundaries
        // of `range` may have not computed offset yet.
        RefPtr<StaticRange> targetRange = StaticRange::Create(
            range->GetStartContainer(), range->StartOffset(),
            range->GetEndContainer(), range->EndOffset(), IgnoreErrors());
        if (NS_WARN_IF(!targetRange) ||
            NS_WARN_IF(!targetRange->IsPositioned())) {
          continue;
        }
        mTargetRanges.AppendElement(std::move(targetRange));
      }
    }
  }
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = nsContentUtils::DispatchInputEvent(
      targetElement, eEditorBeforeInput, inputType, textEditor,
      mDataTransfer ? InputEventOptions(mDataTransfer, std::move(mTargetRanges))
                    : InputEventOptions(mData, std::move(mTargetRanges)),
      &status);
  if (NS_WARN_IF(mEditorBase.Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("nsContentUtils::DispatchInputEvent() failed");
    return rv;
  }
  mBeforeInputEventCanceled = status == nsEventStatus_eConsumeNoDefault;
  return mBeforeInputEventCanceled ? NS_ERROR_EDITOR_ACTION_CANCELED : NS_OK;
}

/*****************************************************************************
 * mozilla::EditorBase::TopLevelEditSubActionData
 *****************************************************************************/

nsresult EditorBase::TopLevelEditSubActionData::AddNodeToChangedRange(
    const HTMLEditor& aHTMLEditor, nsINode& aNode) {
  EditorRawDOMPoint startPoint(&aNode);
  EditorRawDOMPoint endPoint(&aNode);
  DebugOnly<bool> advanced = endPoint.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced, "Failed to set endPoint to next to aNode");
  nsresult rv = AddRangeToChangedRange(aHTMLEditor, startPoint, endPoint);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "TopLevelEditSubActionData::AddRangeToChangedRange() failed");
  return rv;
}

nsresult EditorBase::TopLevelEditSubActionData::AddPointToChangedRange(
    const HTMLEditor& aHTMLEditor, const EditorRawDOMPoint& aPoint) {
  nsresult rv = AddRangeToChangedRange(aHTMLEditor, aPoint, aPoint);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "TopLevelEditSubActionData::AddRangeToChangedRange() failed");
  return rv;
}

nsresult EditorBase::TopLevelEditSubActionData::AddRangeToChangedRange(
    const HTMLEditor& aHTMLEditor, const EditorRawDOMPoint& aStart,
    const EditorRawDOMPoint& aEnd) {
  if (NS_WARN_IF(!aStart.IsSet()) || NS_WARN_IF(!aEnd.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aHTMLEditor.IsDescendantOfRoot(aStart.GetContainer()) ||
      (aStart.GetContainer() != aEnd.GetContainer() &&
       !aHTMLEditor.IsDescendantOfRoot(aEnd.GetContainer()))) {
    return NS_OK;
  }

  // If mChangedRange hasn't been set, we can just set it to `aStart` and
  // `aEnd`.
  if (!mChangedRange->IsPositioned()) {
    nsresult rv = mChangedRange->SetStartAndEnd(aStart.ToRawRangeBoundary(),
                                                aEnd.ToRawRangeBoundary());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsRange::SetStartAndEnd() failed");
    return rv;
  }

  Maybe<int32_t> relation =
      mChangedRange->StartRef().IsSet()
          ? nsContentUtils::ComparePoints(mChangedRange->StartRef(),
                                          aStart.ToRawRangeBoundary())
          : Some(1);
  if (NS_WARN_IF(!relation)) {
    return NS_ERROR_FAILURE;
  }

  // If aStart is before start of mChangedRange, reset the start.
  if (*relation > 0) {
    ErrorResult error;
    mChangedRange->SetStart(aStart.ToRawRangeBoundary(), error);
    if (error.Failed()) {
      NS_WARNING("nsRange::SetStart() failed");
      return error.StealNSResult();
    }
  }

  relation = mChangedRange->EndRef().IsSet()
                 ? nsContentUtils::ComparePoints(mChangedRange->EndRef(),
                                                 aEnd.ToRawRangeBoundary())
                 : Some(1);
  if (NS_WARN_IF(!relation)) {
    return NS_ERROR_FAILURE;
  }

  // If aEnd is after end of mChangedRange, reset the end.
  if (*relation < 0) {
    ErrorResult error;
    mChangedRange->SetEnd(aEnd.ToRawRangeBoundary(), error);
    if (error.Failed()) {
      NS_WARNING("nsRange::SetEnd() failed");
      return error.StealNSResult();
    }
  }

  return NS_OK;
}

void EditorBase::TopLevelEditSubActionData::DidCreateElement(
    EditorBase& aEditorBase, Element& aNewElement) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rvIgnored =
      AddNodeToChangedRange(*aEditorBase.AsHTMLEditor(), aNewElement);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "TopLevelEditSubActionData::AddNodeToChangedRange() failed, but ignored");
}

void EditorBase::TopLevelEditSubActionData::DidInsertContent(
    EditorBase& aEditorBase, nsIContent& aNewContent) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rvIgnored =
      AddNodeToChangedRange(*aEditorBase.AsHTMLEditor(), aNewContent);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "TopLevelEditSubActionData::AddNodeToChangedRange() failed, but ignored");
}

void EditorBase::TopLevelEditSubActionData::WillDeleteContent(
    EditorBase& aEditorBase, nsIContent& aRemovingContent) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rvIgnored =
      AddNodeToChangedRange(*aEditorBase.AsHTMLEditor(), aRemovingContent);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "TopLevelEditSubActionData::AddNodeToChangedRange() failed, but ignored");
}

void EditorBase::TopLevelEditSubActionData::DidSplitContent(
    EditorBase& aEditorBase, nsIContent& aExistingRightContent,
    nsIContent& aNewLeftContent) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rvIgnored = AddRangeToChangedRange(
      *aEditorBase.AsHTMLEditor(), EditorRawDOMPoint(&aNewLeftContent, 0),
      EditorRawDOMPoint(&aExistingRightContent, 0));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TopLevelEditSubActionData::AddRangeToChangedRange() "
                       "failed, but ignored");
}

void EditorBase::TopLevelEditSubActionData::WillJoinContents(
    EditorBase& aEditorBase, nsIContent& aLeftContent,
    nsIContent& aRightContent) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  // remember split point
  aEditorBase.EditSubActionDataRef().mJoinedLeftNodeLength =
      aLeftContent.Length();
}

void EditorBase::TopLevelEditSubActionData::DidJoinContents(
    EditorBase& aEditorBase, nsIContent& aLeftContent,
    nsIContent& aRightContent) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rvIgnored = AddPointToChangedRange(
      *aEditorBase.AsHTMLEditor(),
      EditorRawDOMPoint(
          &aRightContent,
          aEditorBase.EditSubActionDataRef().mJoinedLeftNodeLength));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TopLevelEditSubActionData::AddPointToChangedRange() "
                       "failed, but ignored");
}

void EditorBase::TopLevelEditSubActionData::DidInsertText(
    EditorBase& aEditorBase, const EditorRawDOMPoint& aInsertionBegin,
    const EditorRawDOMPoint& aInsertionEnd) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rvIgnored = AddRangeToChangedRange(
      *aEditorBase.AsHTMLEditor(), aInsertionBegin, aInsertionEnd);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TopLevelEditSubActionData::AddRangeToChangedRange() "
                       "failed, but ignored");
}

void EditorBase::TopLevelEditSubActionData::DidDeleteText(
    EditorBase& aEditorBase, const EditorRawDOMPoint& aStartInTextNode) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rvIgnored =
      AddPointToChangedRange(*aEditorBase.AsHTMLEditor(), aStartInTextNode);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TopLevelEditSubActionData::AddPointToChangedRange() "
                       "failed, but ignored");
}

void EditorBase::TopLevelEditSubActionData::WillDeleteRange(
    EditorBase& aEditorBase, const EditorRawDOMPoint& aStart,
    const EditorRawDOMPoint& aEnd) {
  MOZ_ASSERT(aEditorBase.AsHTMLEditor());
  MOZ_ASSERT(aStart.IsSet());
  MOZ_ASSERT(aEnd.IsSet());

  if (!aEditorBase.mInitSucceeded || aEditorBase.Destroyed()) {
    return;  // We have not been initialized yet or already been destroyed.
  }

  if (!aEditorBase.EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  // XXX Looks like that this is wrong.  We delete multiple selection ranges
  //     once, but this adds only first range into the changed range.
  //     Anyway, we should take the range as an argument.
  DebugOnly<nsresult> rvIgnored =
      AddRangeToChangedRange(*aEditorBase.AsHTMLEditor(), aStart, aEnd);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TopLevelEditSubActionData::AddRangeToChangedRange() "
                       "failed, but ignored");
}

}  // namespace mozilla
