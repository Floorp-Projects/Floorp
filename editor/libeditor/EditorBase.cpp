/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorBase.h"

#include "mozilla/DebugOnly.h"  // for DebugOnly
#include "mozilla/Encoding.h"   // for Encoding

#include <stdio.h>   // for nullptr, stdout
#include <string.h>  // for strcmp

#include "ChangeAttributeTransaction.h"     // for ChangeAttributeTransaction
#include "CompositionTransaction.h"         // for CompositionTransaction
#include "CreateElementTransaction.h"       // for CreateElementTransaction
#include "DeleteNodeTransaction.h"          // for DeleteNodeTransaction
#include "DeleteRangeTransaction.h"         // for DeleteRangeTransaction
#include "DeleteTextTransaction.h"          // for DeleteTextTransaction
#include "EditAggregateTransaction.h"       // for EditAggregateTransaction
#include "EditorEventListener.h"            // for EditorEventListener
#include "HTMLEditRules.h"                  // for HTMLEditRules
#include "InsertNodeTransaction.h"          // for InsertNodeTransaction
#include "InsertTextTransaction.h"          // for InsertTextTransaction
#include "JoinNodeTransaction.h"            // for JoinNodeTransaction
#include "PlaceholderTransaction.h"         // for PlaceholderTransaction
#include "SplitNodeTransaction.h"           // for SplitNodeTransaction
#include "TextEditUtils.h"                  // for TextEditUtils
#include "mozilla/CheckedInt.h"             // for CheckedInt
#include "mozilla/ComputedStyle.h"          // for ComputedStyle
#include "mozilla/CSSEditUtils.h"           // for CSSEditUtils
#include "mozilla/EditAction.h"             // for EditSubAction
#include "mozilla/EditorDOMPoint.h"         // for EditorDOMPoint
#include "mozilla/EditorSpellCheck.h"       // for EditorSpellCheck
#include "mozilla/EditorUtils.h"            // for various helper classes.
#include "mozilla/EditTransactionBase.h"    // for EditTransactionBase
#include "mozilla/FlushType.h"              // for FlushType::Frames
#include "mozilla/IMEContentObserver.h"     // for IMEContentObserver
#include "mozilla/IMEStateManager.h"        // for IMEStateManager
#include "mozilla/mozalloc.h"               // for operator new, etc.
#include "mozilla/mozInlineSpellChecker.h"  // for mozInlineSpellChecker
#include "mozilla/mozSpellChecker.h"        // for mozSpellChecker
#include "mozilla/Preferences.h"            // for Preferences
#include "mozilla/PresShell.h"              // for PresShell
#include "mozilla/RangeBoundary.h"      // for RawRangeBoundary, RangeBoundary
#include "mozilla/dom/Selection.h"      // for Selection, etc.
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/ServoCSSParser.h"     // for ServoCSSParser
#include "mozilla/TextComposition.h"    // for TextComposition
#include "mozilla/TextInputListener.h"  // for TextInputListener
#include "mozilla/TextServicesDocument.h"  // for TextServicesDocument
#include "mozilla/TextEvents.h"
#include "mozilla/TransactionManager.h"  // for TransactionManager
#include "mozilla/dom/CharacterData.h"   // for CharacterData
#include "mozilla/dom/DataTransfer.h"    // for DataTransfer
#include "mozilla/dom/Element.h"         // for Element, nsINode::AsElement
#include "mozilla/dom/EventTarget.h"     // for EventTarget
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLBRElement.h"
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
#include "nsDebug.h"                   // for NS_ENSURE_TRUE, etc.
#include "nsError.h"                   // for NS_OK, etc.
#include "nsFocusManager.h"            // for nsFocusManager
#include "nsFrameSelection.h"          // for nsFrameSelection
#include "nsGenericHTMLElement.h"      // for nsGenericHTMLElement
#include "nsGkAtoms.h"                 // for nsGkAtoms, nsGkAtoms::dir
#include "nsIAbsorbingTransaction.h"   // for nsIAbsorbingTransaction
#include "nsAtom.h"                    // for nsAtom
#include "nsIContent.h"                // for nsIContent
#include "mozilla/dom/Document.h"      // for Document
#include "nsIDOMEventListener.h"       // for nsIDOMEventListener
#include "nsIDocumentStateListener.h"  // for nsIDocumentStateListener
#include "nsIEditActionListener.h"     // for nsIEditActionListener
#include "nsIEditorObserver.h"         // for nsIEditorObserver
#include "nsIEditorSpellCheck.h"       // for nsIEditorSpellCheck
#include "nsIFrame.h"                  // for nsIFrame
#include "nsIInlineSpellChecker.h"     // for nsIInlineSpellChecker, etc.
#include "nsNameSpaceManager.h"        // for kNameSpaceID_None, etc.
#include "nsINode.h"                   // for nsINode, etc.
#include "nsIPlaintextEditor.h"        // for nsIPlaintextEditor, etc.
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
#include "nsStyleConsts.h"     // for NS_STYLE_DIRECTION_RTL, etc.
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

EditorBase::EditorBase()
    : mEditActionData(nullptr),
      mPlaceholderName(nullptr),
      mModCount(0),
      mFlags(0),
      mUpdateCount(0),
      mPlaceholderBatch(0),
      mDocDirtyState(-1),
      mSpellcheckCheckboxState(eTriUnset),
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
                         "We cannot update selection cache in the edit action");
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
  selectionController->SetCaretReadOnly(false);
  selectionController->SetDisplaySelection(
      nsISelectionController::SELECTION_ON);
  // Show all the selection reflected to user.
  selectionController->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);

  MOZ_ASSERT(IsInitialized());

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  SelectionRefPtr()->AddSelectionListener(this);

  // Make sure that the editor will be destroyed properly
  mDidPreDestroy = false;
  // Make sure that the ediotr will be created properly
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  // These operations only need to happen on the first PostCreate call
  if (!mDidPostCreate) {
    mDidPostCreate = true;

    // Set up listeners
    CreateEventListeners();
    rv = InstallEventListeners();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }

    // nuke the modification count, so the doc appears unmodified
    // do this before we notify listeners
    ResetModificationCount();

    // update the UI with our state
    NotifyDocumentListeners(eDocumentCreated);
    NotifyDocumentListeners(eDocumentStateChanged);
  }

  // update nsTextStateManager and caret if we have focus
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    InitializeSelection(focusedContent);

    // If the text control gets reframed during focus, Focus() would not be
    // called, so take a chance here to see if we need to spell check the text
    // control.
    mEventListener->SpellCheckIfNeeded();

    IMEState newState;
    if (NS_WARN_IF(NS_FAILED(GetPreferredIMEState(&newState)))) {
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
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  NS_ENSURE_TRUE(rootContent, NS_ERROR_NOT_AVAILABLE);
  mEventTarget = rootContent->GetParent();
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_AVAILABLE);

  nsresult rv = mEventListener->Connect(this);
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
  if (mInlineSpellChecker) mInlineSpellChecker->Cleanup(aDestroyingFrames);

  // tell our listeners that the doc is going away
  NotifyDocumentListeners(eDocumentToBeDestroyed);

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
                         "Failed to disable undo/redo transactions");
    mTransactionManager = nullptr;
  }

  mDidPreDestroy = true;
}

NS_IMETHODIMP
EditorBase::GetFlags(uint32_t* aFlags) {
  // NOTE: If you need to override this method, you need to make Flags()
  //       virtual.
  *aFlags = Flags();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetFlags(uint32_t aFlags) {
  if (mFlags == aFlags) {
    return NS_OK;
  }

  DebugOnly<bool> changingPasswordEditorFlagDynamically =
      mFlags != ~aFlags &&
      ((mFlags ^ aFlags) & nsIPlaintextEditor::eEditorPasswordMask);
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
    if (NS_SUCCEEDED(rv)) {
      // NOTE: When the enabled state isn't going to be modified, this method
      // is going to do nothing.
      nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
      IMEStateManager::UpdateIMEState(newState, content, this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetIsSelectionEditable(bool* aIsSelectionEditable) {
  NS_ENSURE_ARG_POINTER(aIsSelectionEditable);
  *aIsSelectionEditable = IsSelectionEditable();
  return NS_OK;
}

bool EditorBase::IsSelectionEditable() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  if (!mIsHTMLEditorClass) {
    // XXX we just check that the anchor node is editable at the moment
    //     we should check that all nodes in the selection are editable
    nsCOMPtr<nsINode> anchorNode = SelectionRefPtr()->GetAnchorNode();
    return anchorNode && IsEditable(anchorNode);
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

  nsINode* commonAncestor =
      SelectionRefPtr()->GetAnchorFocusRange()->GetCommonAncestor();
  while (commonAncestor && !commonAncestor->IsEditable()) {
    commonAncestor = commonAncestor->GetParentNode();
  }
  // If there is no editable common ancestor, return false.
  return !!commonAncestor;
}

NS_IMETHODIMP
EditorBase::GetIsDocumentEditable(bool* aIsDocumentEditable) {
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);
  RefPtr<Document> doc = GetDocument();
  *aIsDocumentEditable = doc && IsModifiable();

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetDocument(Document** aDoc) {
  NS_IF_ADDREF(*aDoc = mDocument);
  return *aDoc ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

already_AddRefed<nsIWidget> EditorBase::GetWidget() {
  nsPresContext* presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget = presContext->GetRootWidget();
  NS_ENSURE_TRUE(widget.get(), nullptr);
  return widget.forget();
}

NS_IMETHODIMP
EditorBase::GetContentsMIMEType(char** aContentsMIMEType) {
  NS_ENSURE_ARG_POINTER(aContentsMIMEType);
  *aContentsMIMEType = ToNewCString(mContentMIMEType);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetContentsMIMEType(const char* aContentsMIMEType) {
  mContentMIMEType.Assign(aContentsMIMEType ? aContentsMIMEType : "");
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetSelectionController(nsISelectionController** aSel) {
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  *aSel = nullptr;  // init out param
  nsCOMPtr<nsISelectionController> selCon = GetSelectionController();
  if (NS_WARN_IF(!selCon)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  selCon.forget(aSel);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::DeleteSelection(EDirection aAction, EStripWrappers aStripWrappers) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::GetSelection(Selection** aSelection) {
  return GetSelection(SelectionType::eNormal, aSelection);
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
  *aSelection = nullptr;
  nsISelectionController* selcon = GetSelectionController();
  if (!selcon) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  RefPtr<Selection> selection =
      selcon->GetSelection(ToRawSelectionType(aSelectionType));
  if (!selection) {
    return NS_ERROR_INVALID_ARG;
  }
  selection.forget(aSelection);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::DoTransaction(nsITransaction* aTxn) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eUnknown);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }
  // This is a low level API.  So, the caller might require raw error code.
  // Therefore, don't need to use EditorBase::ToGenericNSResult().
  return DoTransactionInternal(aTxn);
}

nsresult EditorBase::DoTransactionInternal(nsITransaction* aTxn) {
  if (mPlaceholderBatch && !mPlaceholderTransaction) {
    mPlaceholderTransaction = PlaceholderTransaction::Create(
        *this, mPlaceholderName, std::move(mSelState));
    MOZ_ASSERT(mSelState.isNothing());

    // We will recurse, but will not hit this case in the nested call
    RefPtr<PlaceholderTransaction> placeholderTransaction =
        mPlaceholderTransaction;
    DoTransactionInternal(placeholderTransaction);

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

    nsresult rv;
    if (mTransactionManager) {
      RefPtr<TransactionManager> transactionManager(mTransactionManager);
      rv = transactionManager->DoTransaction(aTxn);
    } else {
      rv = aTxn->DoTransaction();
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    DoAfterDoTransaction(aTxn);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::EnableUndo(bool aEnable) {
  // XXX Should we return NS_ERROR_FAILURE if EdnableUndoRedo() or
  //     DisableUndoRedo() returns false?
  if (aEnable) {
    DebugOnly<bool> enabledUndoRedo = EnableUndoRedo();
    NS_WARNING_ASSERTION(enabledUndoRedo,
                         "Failed to enable undo/redo transactions");
    return NS_OK;
  }
  DebugOnly<bool> disabledUndoRedo = DisableUndoRedo();
  NS_WARNING_ASSERTION(disabledUndoRedo,
                       "Failed to disable undo/redo transactions");
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetTransactionManager(nsITransactionManager** aTransactionManager) {
  if (NS_WARN_IF(!aTransactionManager)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!mTransactionManager)) {
    return NS_ERROR_FAILURE;
  }
  NS_IF_ADDREF(*aTransactionManager = mTransactionManager);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Undo(uint32_t aCount) {
  nsresult rv = MOZ_KnownLive(AsTextEditor())->UndoAsAction(aCount);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to do Undo");
  return rv;
}

NS_IMETHODIMP
EditorBase::CanUndo(bool* aIsEnabled, bool* aCanUndo) {
  if (NS_WARN_IF(!aIsEnabled) || NS_WARN_IF(!aCanUndo)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanUndo = CanUndo();
  *aIsEnabled = IsUndoRedoEnabled();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Redo(uint32_t aCount) {
  nsresult rv = MOZ_KnownLive(AsTextEditor())->RedoAsAction(aCount);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to do Redo");
  return rv;
}

NS_IMETHODIMP
EditorBase::CanRedo(bool* aIsEnabled, bool* aCanRedo) {
  if (NS_WARN_IF(!aIsEnabled) || NS_WARN_IF(!aCanRedo)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanRedo = CanRedo();
  *aIsEnabled = IsUndoRedoEnabled();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::BeginTransaction() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eUnknown);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  BeginTransactionInternal();
  return NS_OK;
}

void EditorBase::BeginTransactionInternal() {
  BeginUpdateViewBatch();

  if (mTransactionManager) {
    RefPtr<TransactionManager> transactionManager(mTransactionManager);
    transactionManager->BeginBatch(nullptr);
  }
}

NS_IMETHODIMP
EditorBase::EndTransaction() {
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
    transactionManager->EndBatch(false);
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
    mSelState->SaveSelection(SelectionRefPtr());
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
    // to EndUpdateViewBatch and ScrollSelectionIntoView, we are able to
    // allow the selection to cache a frame offset which is used by the
    // caret drawing code. We only enable this cache here; at other times,
    // we have no way to know whether reflow invalidates it
    // See bugs 35296 and 199412.
    SelectionRefPtr()->SetCanCacheFrameOffset(true);

    // time to turn off the batch
    EndUpdateViewBatch();
    // make sure selection is in view

    // After ScrollSelectionIntoView(), the pending notifications might be
    // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
    ScrollSelectionIntoView(false);

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
      mPlaceholderTransaction->EndPlaceHolderBatch();
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

NS_IMETHODIMP
EditorBase::SetShouldTxnSetSelection(bool aShould) {
  MakeThisAllowTransactionsToChangeSelection(aShould);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetDocumentIsEmpty(bool* aDocumentIsEmpty) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// XXX: The rule system should tell us which node to select all on (ie, the
//      root, or the body)
NS_IMETHODIMP
EditorBase::SelectAll() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = SelectAllInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // This is low level API for XUL applcation.  So, we should return raw
    // error code here.
    return rv;
  }
  return NS_OK;
}

nsresult EditorBase::SelectAllInternal() {
  MOZ_ASSERT(IsInitialized());

  CommitComposition();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  // XXX Do we need to keep handling after committing composition causes moving
  //     focus to different element?  Although TextEditor has independent
  //     selection, so, we may not see any odd behavior even in such case.

  nsresult rv = SelectEntireDocument();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::BeginningOfDocument() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // get the root element
  dom::Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  // find first editable thingy
  nsCOMPtr<nsINode> firstNode = GetFirstEditableNode(rootElement);
  if (!firstNode) {
    // just the root node, set selection to inside the root
    return SelectionRefPtr()->Collapse(rootElement, 0);
  }

  if (firstNode->NodeType() == nsINode::TEXT_NODE) {
    // If firstNode is text, set selection to beginning of the text node.
    return SelectionRefPtr()->Collapse(firstNode, 0);
  }

  // Otherwise, it's a leaf node and we set the selection just in front of it.
  nsCOMPtr<nsIContent> parent = firstNode->GetParent();
  if (!parent) {
    return NS_ERROR_NULL_POINTER;
  }

  MOZ_ASSERT(
      parent->ComputeIndexOf(firstNode) == 0,
      "How come the first node isn't the left most child in its parent?");
  return SelectionRefPtr()->Collapse(parent, 0);
}

NS_IMETHODIMP
EditorBase::EndOfDocument() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv = CollapseSelectionToEnd();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // This is low level API for XUL applcation.  So, we should return raw
    // error code here.
    return rv;
  }
  return NS_OK;
}

nsresult EditorBase::CollapseSelectionToEnd() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // XXX Why doesn't this check if the document is alive?
  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // get the root element
  nsINode* node = GetRoot();
  if (NS_WARN_IF(!node)) {
    return NS_ERROR_NULL_POINTER;
  }

  nsINode* child = node->GetLastChild();
  while (child && IsContainer(child)) {
    node = child;
    child = node->GetLastChild();
  }

  uint32_t length = node->Length();
  return SelectionRefPtr()->Collapse(node, static_cast<int32_t>(length));
}

NS_IMETHODIMP
EditorBase::GetDocumentModified(bool* outDocModified) {
  NS_ENSURE_TRUE(outDocModified, NS_ERROR_NULL_POINTER);

  int32_t modCount = 0;
  GetModificationCount(&modCount);

  *outDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetDocumentCharacterSet(nsACString& aCharset) {
  return GetDocumentCharsetInternal(aCharset);
}

nsresult EditorBase::GetDocumentCharsetInternal(nsACString& aCharset) const {
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_UNEXPECTED;
  }
  document->GetDocumentCharacterSet()->Name(aCharset);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetDocumentCharacterSet(const nsACString& characterSet) {
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_UNEXPECTED;
  }
  // This method is scriptable, so add-ons could pass in something other
  // than a canonical name.
  auto encoding = Encoding::ForLabelNoReplacement(characterSet);
  if (!encoding) {
    return NS_ERROR_INVALID_ARG;
  }
  document->SetDocumentCharacterSet(WrapNotNull(encoding));
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Cut() {
  nsresult rv = MOZ_KnownLive(AsTextEditor())->CutAsAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to do Cut");
  return rv;
}

NS_IMETHODIMP
EditorBase::CanCut(bool* aCanCut) {
  if (NS_WARN_IF(!aCanCut)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanCut = AsTextEditor()->IsCutCommandEnabled();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Copy() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
EditorBase::CanCopy(bool* aCanCopy) {
  if (NS_WARN_IF(!aCanCopy)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanCopy = AsTextEditor()->IsCopyCommandEnabled();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Paste(int32_t aClipboardType) {
  nsresult rv =
      MOZ_KnownLive(AsTextEditor())->PasteAsAction(aClipboardType, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to do Paste");
  return rv;
}

NS_IMETHODIMP
EditorBase::PasteTransferable(nsITransferable* aTransferable) {
  nsresult rv =
      MOZ_KnownLive(AsTextEditor())->PasteTransferableAsAction(aTransferable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to do Paste transferable");
  return rv;
}

NS_IMETHODIMP
EditorBase::CanPaste(int32_t aClipboardType, bool* aCanPaste) {
  if (NS_WARN_IF(!aCanPaste)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aCanPaste = AsTextEditor()->CanPaste(aClipboardType);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetAttribute(Element* aElement, const nsAString& aAttribute,
                         const nsAString& aValue) {
  if (NS_WARN_IF(aAttribute.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  nsresult rv = SetAttributeWithTransaction(*aElement, *attribute, aValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult EditorBase::SetAttributeWithTransaction(Element& aElement,
                                                 nsAtom& aAttribute,
                                                 const nsAString& aValue) {
  RefPtr<ChangeAttributeTransaction> transaction =
      ChangeAttributeTransaction::Create(aElement, aAttribute, aValue);
  return DoTransactionInternal(transaction);
}

NS_IMETHODIMP
EditorBase::GetAttributeValue(Element* aElement, const nsAString& aAttribute,
                              nsAString& aResultValue, bool* aResultIsSet) {
  NS_ENSURE_TRUE(aResultIsSet, NS_ERROR_NULL_POINTER);
  *aResultIsSet = false;
  if (!aElement) {
    return NS_OK;
  }
  nsAutoString value;
  aElement->GetAttribute(aAttribute, value);
  if (!DOMStringIsNull(value)) {
    *aResultIsSet = true;
    aResultValue = value;
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::RemoveAttribute(Element* aElement, const nsAString& aAttribute) {
  if (NS_WARN_IF(aAttribute.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  nsresult rv = RemoveAttributeWithTransaction(*aElement, *attribute);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult EditorBase::RemoveAttributeWithTransaction(Element& aElement,
                                                    nsAtom& aAttribute) {
  // XXX If aElement doesn't have aAttribute, shouldn't we stop creating
  //     the transaction?  Otherwise, there will be added a transaction
  //     which does nothing at doing undo/redo.
  RefPtr<ChangeAttributeTransaction> transaction =
      ChangeAttributeTransaction::CreateToRemove(aElement, aAttribute);
  return DoTransactionInternal(transaction);
}

NS_IMETHODIMP
EditorBase::MarkNodeDirty(nsINode* aNode) {
  // Mark the node dirty, but not for webpages (bug 599983)
  if (!OutputsMozDirty()) {
    return NS_OK;
  }
  if (RefPtr<Element> element = Element::FromNodeOrNull(aNode)) {
    element->SetAttr(kNameSpaceID_None, nsGkAtoms::mozdirty, EmptyString(),
                     false);
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetInlineSpellChecker(bool autoCreate,
                                  nsIInlineSpellChecker** aInlineSpellChecker) {
  NS_ENSURE_ARG_POINTER(aInlineSpellChecker);

  if (mDidPreDestroy) {
    // Don't allow people to get or create the spell checker once the editor
    // is going away.
    *aInlineSpellChecker = nullptr;
    return autoCreate ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  // We don't want to show the spell checking UI if there are no spell check
  // dictionaries available.
  bool canSpell = mozInlineSpellChecker::CanEnableInlineSpellChecking();
  if (!canSpell) {
    *aInlineSpellChecker = nullptr;
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (!mInlineSpellChecker && autoCreate) {
    mInlineSpellChecker = new mozInlineSpellChecker();
  }

  if (mInlineSpellChecker) {
    rv = mInlineSpellChecker->Init(this);
    if (NS_FAILED(rv)) {
      mInlineSpellChecker = nullptr;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aInlineSpellChecker = mInlineSpellChecker);

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
  GetInlineSpellChecker(enable, getter_AddRefs(spellChecker));

  if (mInlineSpellChecker) {
    if (!mSpellCheckerDictionaryUpdated && enable) {
      mInlineSpellChecker->UpdateCurrentDictionary();
      mSpellCheckerDictionaryUpdated = true;
    }

    // We might have a mInlineSpellChecker even if there are no dictionaries
    // available since we don't destroy the mInlineSpellChecker when the last
    // dictionariy is removed, but in that case spellChecker is null
    mInlineSpellChecker->SetEnableRealTimeSpell(enable && spellChecker);
  }
}

NS_IMETHODIMP
EditorBase::SetSpellcheckUserOverride(bool enable) {
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

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eCreateNode, nsIEditor::eNext);

  RefPtr<Element> newElement;

  RefPtr<CreateElementTransaction> transaction =
      CreateElementTransaction::Create(*this, aTagName, aPointToInsert);
  nsresult rv = DoTransactionInternal(transaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // XXX Why do we do this even when DoTransaction() returned error?
    RangeUpdaterRef().SelAdjCreateNode(aPointToInsert);
  } else {
    newElement = transaction->GetNewNode();
    MOZ_ASSERT(newElement);

    // If we succeeded to create and insert new element, we need to adjust
    // ranges in RangeUpdaterRef().  It currently requires offset of the new
    // node.  So, let's call it with original offset.  Note that if
    // aPointToInsert stores child node, it may not be at the offset since new
    // element must be inserted before the old child.  Although, mutation
    // observer can do anything, but currently, we don't check it.
    RangeUpdaterRef().SelAdjCreateNode(EditorRawDOMPoint(
        aPointToInsert.GetContainer(), aPointToInsert.Offset()));
  }

  if (mRules && mRules->AsHTMLEditRules() && newElement) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->DidCreateNode(*newElement);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidCreateNode(nsDependentAtomString(&aTagName), newElement, rv);
    }
  }

  return newElement.forget();
}

NS_IMETHODIMP
EditorBase::InsertNode(nsINode* aNodeToInsert, nsINode* aContainer,
                       int32_t aOffset) {
  nsCOMPtr<nsIContent> contentToInsert = do_QueryInterface(aNodeToInsert);
  if (NS_WARN_IF(!contentToInsert)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (NS_WARN_IF(!aContainer)) {
    return NS_ERROR_NULL_POINTER;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertNode);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  int32_t offset =
      aOffset < 0
          ? static_cast<int32_t>(aContainer->Length())
          : std::min(aOffset, static_cast<int32_t>(aContainer->Length()));
  nsresult rv = InsertNodeWithTransaction(*contentToInsert,
                                          EditorDOMPoint(aContainer, offset));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult EditorBase::InsertNodeWithTransaction(
    nsIContent& aContentToInsert, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext);

  RefPtr<InsertNodeTransaction> transaction =
      InsertNodeTransaction::Create(*this, aContentToInsert, aPointToInsert);
  nsresult rv = DoTransactionInternal(transaction);

  RangeUpdaterRef().SelAdjInsertNode(aPointToInsert);

  if (mRules && mRules->AsHTMLEditRules()) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->DidInsertNode(aContentToInsert);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidInsertNode(&aContentToInsert, rv);
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
  ErrorResult error;
  nsCOMPtr<nsIContent> newLeftNode =
      SplitNodeWithTransaction(aPointToInsert, error);
  if (NS_WARN_IF(error.Failed())) {
    error.SuppressException();
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return CreateElementResult(rv);
  }

  return CreateElementResult(newBRElement.forget());
}

NS_IMETHODIMP
EditorBase::SplitNode(nsINode* aNode, int32_t aOffset, nsINode** aNewLeftNode) {
  if (NS_WARN_IF(!aNode)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSplitNode);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  int32_t offset =
      std::min(std::max(aOffset, 0), static_cast<int32_t>(aNode->Length()));
  ErrorResult error;
  nsCOMPtr<nsIContent> newNode =
      SplitNodeWithTransaction(EditorDOMPoint(aNode, offset), error);
  newNode.forget(aNewLeftNode);
  if (NS_WARN_IF(error.Failed())) {
    return EditorBase::ToGenericNSResult(error.StealNSResult());
  }
  return NS_OK;
}

already_AddRefed<nsIContent> EditorBase::SplitNodeWithTransaction(
    const EditorDOMPoint& aStartOfRightNode, ErrorResult& aError) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aStartOfRightNode.IsSet()) ||
      NS_WARN_IF(!aStartOfRightNode.GetContainerAsContent())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }
  MOZ_ASSERT(aStartOfRightNode.IsSetAndValid());

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eSplitNode, nsIEditor::eNext);

  // XXX Unfortunately, storing offset of the split point in
  //     SplitNodeTransaction is necessary for now.  We should fix this
  //     in a follow up bug.
  Unused << aStartOfRightNode.Offset();

  RefPtr<SplitNodeTransaction> transaction =
      SplitNodeTransaction::Create(*this, aStartOfRightNode);
  aError = DoTransactionInternal(transaction);

  nsCOMPtr<nsIContent> newNode = transaction->GetNewNode();
  NS_WARNING_ASSERTION(newNode, "Failed to create a new left node");

  // XXX Some other transactions manage range updater by themselves.
  //     Why doesn't SplitNodeTransaction do it?
  RangeUpdaterRef().SelAdjSplitNode(*aStartOfRightNode.GetContainerAsContent(),
                                    newNode);

  if (mRules && mRules->AsHTMLEditRules() && newNode) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->DidSplitNode(*aStartOfRightNode.GetContainer(), *newNode);
  }

  if (mInlineSpellChecker) {
    RefPtr<mozInlineSpellChecker> spellChecker = mInlineSpellChecker;
    spellChecker->DidSplitNode(aStartOfRightNode.GetContainer(), newNode);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidSplitNode(aStartOfRightNode.GetContainer(), newNode);
    }
  }

  if (NS_WARN_IF(aError.Failed())) {
    return nullptr;
  }

  return newNode.forget();
}

NS_IMETHODIMP
EditorBase::JoinNodes(nsINode* aLeftNode, nsINode* aRightNode, nsINode*) {
  if (NS_WARN_IF(!aLeftNode) || NS_WARN_IF(!aRightNode) ||
      NS_WARN_IF(!aLeftNode->GetParentNode())) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eJoinNodes);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = JoinNodesWithTransaction(*aLeftNode, *aRightNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult EditorBase::JoinNodesWithTransaction(nsINode& aLeftNode,
                                              nsINode& aRightNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsCOMPtr<nsINode> parent = aLeftNode.GetParentNode();
  MOZ_ASSERT(parent);

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eJoinNodes, nsIEditor::ePrevious);

  // Remember some values; later used for saved selection updating.
  // Find the offset between the nodes to be joined.
  int32_t offset = parent->ComputeIndexOf(&aRightNode);
  // Find the number of children of the lefthand node
  uint32_t oldLeftNodeLen = aLeftNode.Length();

  if (mRules && mRules->AsHTMLEditRules()) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->WillJoinNodes(aLeftNode, aRightNode);
  }

  nsresult rv = NS_OK;
  RefPtr<JoinNodeTransaction> transaction =
      JoinNodeTransaction::MaybeCreate(*this, aLeftNode, aRightNode);
  if (transaction) {
    rv = DoTransactionInternal(transaction);
  }

  // XXX Some other transactions manage range updater by themselves.
  //     Why doesn't JoinNodeTransaction do it?
  RangeUpdaterRef().SelAdjJoinNodes(aLeftNode, aRightNode, *parent, offset,
                                    static_cast<int32_t>(oldLeftNodeLen));

  if (mRules && mRules->AsHTMLEditRules()) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->DidJoinNodes(aLeftNode, aRightNode);
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
      listener->DidJoinNodes(&aLeftNode, &aRightNode, parent, rv);
    }
  }

  return rv;
}

NS_IMETHODIMP
EditorBase::DeleteNode(nsINode* aNode) {
  if (NS_WARN_IF(!aNode)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveNode);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = DeleteNodeWithTransaction(*aNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult EditorBase::DeleteNodeWithTransaction(nsINode& aNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::ePrevious);

  if (mRules && mRules->AsHTMLEditRules()) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->WillDeleteNode(aNode);
  }

  // FYI: DeleteNodeTransaction grabs aNode while it's alive.  So, it's safe
  //      to refer aNode even after calling DoTransaction().
  RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      DeleteNodeTransaction::MaybeCreate(*this, aNode);
  nsresult rv = deleteNodeTransaction
                    ? DoTransactionInternal(deleteNodeTransaction)
                    : NS_ERROR_FAILURE;

  if (mTextServicesDocument && NS_SUCCEEDED(rv)) {
    RefPtr<TextServicesDocument> textServicesDocument = mTextServicesDocument;
    textServicesDocument->DidDeleteNode(&aNode);
  }

  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidDeleteNode(&aNode, rv);
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
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
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  // Notify our internal selection state listener.
  // Note: An AutoSelectionRestorer object must be created before calling this
  // to initialize RangeUpdaterRef().
  AutoReplaceContainerSelNotify selStateNotify(RangeUpdaterRef(),
                                               &aOldContainer, newContainer);
  {
    AutoTransactionsConserveSelection conserveSelection(*this);
    // Move all children from the old container to the new container.
    while (aOldContainer.HasChildren()) {
      nsCOMPtr<nsIContent> child = aOldContainer.GetFirstChild();
      if (NS_WARN_IF(!child)) {
        return nullptr;
      }
      nsresult rv = DeleteNodeWithTransaction(*child);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      rv = InsertNodeWithTransaction(
          *child, EditorDOMPoint(newContainer, newContainer->Length()));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    }
  }

  // Insert new container into tree.
  NS_WARNING_ASSERTION(atOldContainer.IsSetAndValid(),
                       "The old container might be moved by mutation observer");
  nsresult rv = InsertNodeWithTransaction(*newContainer, atOldContainer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  // Delete old container.
  rv = DeleteNodeWithTransaction(aOldContainer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  AutoRemoveContainerSelNotify selNotify(
      RangeUpdaterRef(), &aElement, pointToInsertChildren.GetContainer(),
      pointToInsertChildren.Offset(), aElement.GetChildCount());

  // Move all children from aNode to its parent.
  while (aElement.HasChildren()) {
    nsCOMPtr<nsIContent> child = aElement.GetLastChild();
    if (NS_WARN_IF(!child)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Insert the last child before the previous last child.  So, we need to
    // use offset here because previous child might have been moved to
    // container.
    rv = InsertNodeWithTransaction(
        *child, EditorDOMPoint(pointToInsertChildren.GetContainer(),
                               pointToInsertChildren.Offset()));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv = DeleteNodeWithTransaction(aElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
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
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  // Notify our internal selection state listener
  AutoInsertContainerSelNotify selNotify(RangeUpdaterRef());

  // Put aNode in the new container, first.
  nsresult rv = DeleteNodeWithTransaction(aContent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  {
    AutoTransactionsConserveSelection conserveSelection(*this);
    rv = InsertNodeWithTransaction(aContent, EditorDOMPoint(newContainer, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  // Put the new container where aNode was.
  rv = InsertNodeWithTransaction(*newContainer, pointToInsertNewContainer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

void EditorBase::MoveAllChildren(nsINode& aContainer,
                                 const EditorRawDOMPoint& aPointToInsert,
                                 ErrorResult& aError) {
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
  return MoveChildren(*firstChild, *lastChild, aPointToInsert, aError);
}

void EditorBase::MovePreviousSiblings(nsIContent& aChild,
                                      const EditorRawDOMPoint& aPointToInsert,
                                      ErrorResult& aError) {
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
  return MoveChildren(*firstChild, *lastChild, aPointToInsert, aError);
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
    if (NS_WARN_IF(aError.Failed())) {
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
    if (NS_WARN_IF(aError.Failed())) {
      return;
    }
    // If the child was inserted or appended properly, the following children
    // should be inserted before it.  Otherwise, keep using current position.
    if (child->GetParentNode() == newContainer) {
      nextNode = child;
    }
  }
}

NS_IMETHODIMP
EditorBase::AddEditorObserver(nsIEditorObserver* aObserver) {
  // we don't keep ownership of the observers.  They must
  // remove themselves as observers before they are destroyed.

  NS_ENSURE_TRUE(aObserver, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (!mEditorObservers.Contains(aObserver)) {
    mEditorObservers.AppendElement(*aObserver);
    NS_WARNING_ASSERTION(
        mEditorObservers.Length() != 1,
        "nsIEditorObserver installed, this editor becomes slower");
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::RemoveEditorObserver(nsIEditorObserver* aObserver) {
  NS_ENSURE_TRUE(aObserver, NS_ERROR_FAILURE);

  NS_WARNING_ASSERTION(
      mEditorObservers.Length() != 1,
      "All nsIEditorObservers have been removed, this editor becomes faster");
  mEditorObservers.RemoveElement(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::NotifySelectionChanged(Document* aDocument, Selection* aSelection,
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
  switch (aNotification) {
    case eNotifyEditorObserversOfEnd:
      mIsInEditSubAction = false;

      if (mTextInputListener) {
        RefPtr<TextInputListener> listener = mTextInputListener;
        listener->OnEditActionHandled();
      }

      if (mIMEContentObserver) {
        RefPtr<IMEContentObserver> observer = mIMEContentObserver;
        observer->OnEditActionHandled();
      }

      if (!mEditorObservers.IsEmpty()) {
        // Copy the observers since EditAction()s can modify mEditorObservers.
        AutoEditorObserverArray observers(mEditorObservers);
        for (auto& observer : observers) {
          observer->EditAction();
        }
      }

      if (!mDispatchInputEvent) {
        return;
      }

      FireInputEvent();
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

void EditorBase::FireInputEvent() {
  RefPtr<DataTransfer> dataTransfer = GetInputEventDataTransfer();
  FireInputEvent(GetEditAction(), GetInputEventData(), dataTransfer);
}

void EditorBase::FireInputEvent(EditAction aEditAction, const nsAString& aData,
                                DataTransfer* aDataTransfer) {
  MOZ_ASSERT(IsEditActionDataAvailable());

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
  DebugOnly<nsresult> rvIgnored = nsContentUtils::DispatchInputEvent(
      targetElement, ToInputType(aEditAction), textEditor,
      aDataTransfer ? nsContentUtils::InputEventOptions(aDataTransfer)
                    : nsContentUtils::InputEventOptions(aData));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Failed to dispatch input event");
}

NS_IMETHODIMP
EditorBase::AddEditActionListener(nsIEditActionListener* aListener) {
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

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

NS_IMETHODIMP
EditorBase::RemoveEditActionListener(nsIEditActionListener* aListener) {
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

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

NS_IMETHODIMP
EditorBase::AddDocumentStateListener(nsIDocumentStateListener* aListener) {
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  if (!mDocStateListeners.Contains(aListener)) {
    mDocStateListeners.AppendElement(*aListener);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::RemoveDocumentStateListener(nsIDocumentStateListener* aListener) {
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  mDocStateListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::OutputToString(const nsAString& aFormatType, uint32_t aFlags,
                           nsAString& aOutputString) {
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool EditorBase::ArePreservingSelection() {
  return IsEditActionDataAvailable() && !SavedSelectionRef().IsEmpty();
}

void EditorBase::PreserveSelectionAcrossActions() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SavedSelectionRef().SaveSelection(SelectionRefPtr());
  RangeUpdaterRef().RegisterSelectionState(SavedSelectionRef());
}

nsresult EditorBase::RestorePreservedSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (SavedSelectionRef().IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  SavedSelectionRef().RestoreSelection(SelectionRefPtr());
  StopPreservingSelection();
  return NS_OK;
}

void EditorBase::StopPreservingSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RangeUpdaterRef().DropSelectionState(SavedSelectionRef());
  SavedSelectionRef().MakeEmpty();
}

NS_IMETHODIMP
EditorBase::ForceCompositionEnd() { return CommitComposition(); }

nsresult EditorBase::CommitComposition() {
  nsPresContext* pc = GetPresContext();
  if (!pc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mComposition
             ? IMEStateManager::NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, pc)
             : NS_OK;
}

nsresult EditorBase::GetPreferredIMEState(IMEState* aState) {
  NS_ENSURE_ARG_POINTER(aState);
  aState->mEnabled = IMEState::ENABLED;
  aState->mOpen = IMEState::DONT_CHANGE_OPEN_STATE;

  if (IsReadonly() || IsDisabled()) {
    aState->mEnabled = IMEState::DISABLED;
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = GetRoot();
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIFrame* frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  switch (frame->StyleUIReset()->mIMEMode) {
    case NS_STYLE_IME_MODE_AUTO:
      if (IsPasswordEditor()) aState->mEnabled = IMEState::PASSWORD;
      break;
    case NS_STYLE_IME_MODE_DISABLED:
      // we should use password state for |ime-mode: disabled;|.
      aState->mEnabled = IMEState::PASSWORD;
      break;
    case NS_STYLE_IME_MODE_ACTIVE:
      aState->mOpen = IMEState::OPEN;
      break;
    case NS_STYLE_IME_MODE_INACTIVE:
      aState->mOpen = IMEState::CLOSED;
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetComposing(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = IsIMEComposing();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetRootElement(Element** aRootElement) {
  NS_ENSURE_ARG_POINTER(aRootElement);
  NS_ENSURE_TRUE(mRootElement, NS_ERROR_NOT_AVAILABLE);
  RefPtr<Element> rootElement = mRootElement;
  rootElement.forget(aRootElement);
  return NS_OK;
}

void EditorBase::OnStartToHandleTopLevelEditSubAction(
    EditSubAction aEditSubAction, nsIEditor::EDirection aDirection) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  mEditActionData->SetTopLevelEditSubAction(aEditSubAction, aDirection);
}

void EditorBase::OnEndHandlingTopLevelEditSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  mEditActionData->SetTopLevelEditSubAction(EditSubAction::eNone, eNone);
}

void EditorBase::DoInsertText(Text& aText, uint32_t aOffset,
                              const nsAString& aStringToInsert,
                              ErrorResult& aRv) {
  aText.InsertData(aOffset, aStringToInsert, aRv);
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
    return;
  }
  if (NS_WARN_IF(aRv.Failed())) {
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
  NS_WARNING_ASSERTION(!aRv.Failed(), "Text::DeleteData() failed");
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
  }
}

void EditorBase::DoReplaceText(Text& aText, uint32_t aOffset, uint32_t aCount,
                               const nsAString& aStringToInsert,
                               ErrorResult& aRv) {
  if (!AsHTMLEditor() && aCount > 0) {
    AsTextEditor()->WillDeleteText(aText.TextLength(), aOffset, aCount);
  }
  aText.ReplaceData(aOffset, aCount, aStringToInsert, aRv);
  NS_WARNING_ASSERTION(!aRv.Failed(), "Text::ReplaceData() failed");
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(aRv.Failed())) {
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
  NS_WARNING_ASSERTION(!aRv.Failed(), "Text::SetData() failed");
  if (NS_WARN_IF(Destroyed())) {
    aRv = NS_ERROR_EDITOR_DESTROYED;
    return;
  }
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  if (!AsHTMLEditor() && !aStringToSet.IsEmpty()) {
    aRv = MOZ_KnownLive(AsTextEditor())
              ->DidInsertText(aText.Length(), 0, aStringToSet.Length());
    NS_WARNING_ASSERTION(!aRv.Failed(), "TextEditor::DidInsertText() failed");
  }
}

NS_IMETHODIMP
EditorBase::CloneAttribute(const nsAString& aAttribute, Element* aDestElement,
                           Element* aSourceElement) {
  NS_ENSURE_TRUE(aDestElement && aSourceElement, NS_ERROR_NULL_POINTER);
  if (NS_WARN_IF(aAttribute.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  nsresult rv =
      CloneAttributeWithTransaction(*attribute, *aDestElement, *aSourceElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult EditorBase::CloneAttributeWithTransaction(nsAtom& aAttribute,
                                                   Element& aDestElement,
                                                   Element& aSourceElement) {
  nsAutoString attrValue;
  if (aSourceElement.GetAttr(kNameSpaceID_None, &aAttribute, attrValue)) {
    return SetAttributeWithTransaction(aDestElement, aAttribute, attrValue);
  }
  return RemoveAttributeWithTransaction(aDestElement, aAttribute);
}

/**
 * @param aDest     Must be a DOM element.
 * @param aSource   Must be a DOM element.
 */
NS_IMETHODIMP
EditorBase::CloneAttributes(Element* aDestElement, Element* aSourceElement) {
  if (NS_WARN_IF(!aDestElement) || NS_WARN_IF(!aSourceElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
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
      RemoveAttributeWithTransaction(
          destElement, MOZ_KnownLive(*attr->NodeInfo()->NameAtom()));
    } else {
      destElement->UnsetAttr(kNameSpaceID_None, attr->NodeInfo()->NameAtom(),
                             true);
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
      SetAttributeOrEquivalent(destElement,
                               MOZ_KnownLive(attr->NodeInfo()->NameAtom()),
                               value, false);
    } else {
      // The element is not inserted in the document yet, we don't want to put
      // a transaction on the UndoStack
      SetAttributeOrEquivalent(destElement,
                               MOZ_KnownLive(attr->NodeInfo()->NameAtom()),
                               value, true);
    }
  }
}

nsresult EditorBase::ScrollSelectionIntoView(bool aScrollToAnchor) {
  nsISelectionController* selectionController = GetSelectionController();
  if (!selectionController) {
    return NS_OK;
  }

  int16_t region = nsISelectionController::SELECTION_FOCUS_REGION;
  if (aScrollToAnchor) {
    region = nsISelectionController::SELECTION_ANCHOR_REGION;
  }
  selectionController->ScrollSelectionIntoView(
      nsISelectionController::SELECTION_NORMAL, region,
      nsISelectionController::SCROLL_OVERFLOW_HIDDEN);
  return NS_OK;
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
    //          in HTMLEditRules.
    return aPoint;
  }

  nsCOMPtr<nsINode> root = GetRoot();
  if (aPoint.GetContainer() == root) {
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
    if (parentOfContainer && parentOfContainer == root) {
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
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      pointToInsert.Set(newNode, 0);
      newOffset = lengthToInsert;
    } else {
      newOffset = lengthToInsert + pointToInsert.Offset();
      NS_ENSURE_TRUE(newOffset.isValid(), NS_ERROR_FAILURE);
    }
    nsresult rv = InsertTextIntoTextNodeWithTransaction(
        aStringToInsert, MOZ_KnownLive(*pointToInsert.GetContainerAsText()),
        pointToInsert.Offset());
    NS_ENSURE_SUCCESS(rv, rv);
    if (aPointAfterInsertedString) {
      aPointAfterInsertedString->Set(pointToInsert.GetContainer(),
                                     newOffset.value());
    }
    return NS_OK;
  }

  if (pointToInsert.IsInTextNode()) {
    CheckedInt<int32_t> newOffset = lengthToInsert + pointToInsert.Offset();
    NS_ENSURE_TRUE(newOffset.isValid(), NS_ERROR_FAILURE);
    // we are inserting text into an existing text node.
    nsresult rv = InsertTextIntoTextNodeWithTransaction(
        aStringToInsert, MOZ_KnownLive(*pointToInsert.GetContainerAsText()),
        pointToInsert.Offset());
    NS_ENSURE_SUCCESS(rv, rv);
    if (aPointAfterInsertedString) {
      aPointAfterInsertedString->Set(pointToInsert.GetContainer(),
                                     newOffset.value());
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (aPointAfterInsertedString) {
    aPointAfterInsertedString->Set(newNode, lengthToInsert.value());
  }
  return NS_OK;
}

nsresult EditorBase::InsertTextIntoTextNodeWithTransaction(
    const nsAString& aStringToInsert, Text& aTextNode, int32_t aOffset,
    bool aSuppressIME) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<EditTransactionBase> transaction;
  bool isIMETransaction = false;
  RefPtr<Text> insertedTextNode = &aTextNode;
  int32_t insertedOffset = aOffset;
  // aSuppressIME is used when editor must insert text, yet this text is not
  // part of the current IME operation. Example: adjusting whitespace around an
  // IME insertion.
  if (ShouldHandleIMEComposition() && !aSuppressIME) {
    transaction = CompositionTransaction::Create(*this, aStringToInsert,
                                                 aTextNode, aOffset);
    isIMETransaction = true;
    // All characters of the composition string will be replaced with
    // aStringToInsert.  So, we need to emulate to remove the composition
    // string.
    // FYI: The text node information in mComposition has been updated by
    //      CompositionTransaction::Create().
    insertedTextNode = mComposition->GetContainerTextNode();
    insertedOffset = mComposition->XPOffsetInTextNode();
  } else {
    transaction = InsertTextTransaction::Create(*this, aStringToInsert,
                                                aTextNode, aOffset);
  }

  // XXX We may not need these view batches anymore.  This is handled at a
  // higher level now I believe.
  BeginUpdateViewBatch();
  nsresult rv = DoTransactionInternal(transaction);
  EndUpdateViewBatch();

  if (mRules && mRules->AsHTMLEditRules() && insertedTextNode) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->DidInsertText(*insertedTextNode, insertedOffset,
                                 aStringToInsert);
  }

  // let listeners know what happened
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidInsertText(insertedTextNode, insertedOffset, aStringToInsert,
                              rv);
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

  nsIContent* node = GetLeftmostChild(aRoot);
  if (node && !IsEditable(node)) {
    node = GetNextEditableNode(*node);
  }

  return (node != aRoot) ? node : nullptr;
}

nsresult EditorBase::NotifyDocumentListeners(
    TDocumentListenerNotification aNotificationType) {
  if (!mDocStateListeners.Length()) {
    // Maybe there just aren't any.
    return NS_OK;
  }

  AutoDocumentStateListenerArray listeners(mDocStateListeners);
  nsresult rv = NS_OK;

  switch (aNotificationType) {
    case eDocumentCreated:
      for (auto& listener : listeners) {
        rv = listener->NotifyDocumentCreated();
        if (NS_FAILED(rv)) {
          break;
        }
      }
      break;

    case eDocumentToBeDestroyed:
      for (auto& listener : listeners) {
        rv = listener->NotifyDocumentWillBeDestroyed();
        if (NS_FAILED(rv)) {
          break;
        }
      }
      break;

    case eDocumentStateChanged: {
      bool docIsDirty;
      rv = GetDocumentModified(&docIsDirty);
      NS_ENSURE_SUCCESS(rv, rv);

      if (static_cast<int8_t>(docIsDirty) == mDocDirtyState) {
        return NS_OK;
      }

      mDocDirtyState = docIsDirty;

      for (auto& listener : listeners) {
        rv = listener->NotifyDocumentStateChanged(mDocDirtyState);
        if (NS_FAILED(rv)) {
          break;
        }
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown notification");
  }

  return rv;
}

nsresult EditorBase::SetTextImpl(const nsAString& aString, Text& aTextNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  const uint32_t length = aTextNode.Length();

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eSetText, nsIEditor::eNext);

  // Let listeners know what's up
  if (!mActionListeners.IsEmpty() && length) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillDeleteText(&aTextNode, 0, length);
    }
  }

  // We don't support undo here, so we don't really need all of the transaction
  // machinery, therefore we can run our transaction directly, breaking all of
  // the rules!
  ErrorResult error;
  DoSetText(aTextNode, aString, error);
  nsresult rv = error.StealNSResult();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  {
    // Create a nested scope to not overwrite rv from the outer scope.
    DebugOnly<nsresult> rv =
        SelectionRefPtr()->Collapse(&aTextNode, aString.Length());
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Selection could not be collapsed after insert");
  }

  RangeUpdaterRef().SelAdjDeleteText(&aTextNode, 0, length);
  RangeUpdaterRef().SelAdjInsertText(aTextNode, 0, aString);

  if (mRules && mRules->AsHTMLEditRules()) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    if (length) {
      htmlEditRules->DidDeleteText(aTextNode, 0, length);
    }
    if (!aString.IsEmpty()) {
      htmlEditRules->DidInsertText(aTextNode, 0, aString);
    }
  }

  // Let listeners know what happened
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      if (length) {
        listener->DidDeleteText(&aTextNode, 0, length, rv);
      }
      if (!aString.IsEmpty()) {
        listener->DidInsertText(&aTextNode, 0, aString, rv);
      }
    }
  }

  return rv;
}

nsresult EditorBase::DeleteTextWithTransaction(Text& aTextNode,
                                               uint32_t aOffset,
                                               uint32_t aLength) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<DeleteTextTransaction> transaction =
      DeleteTextTransaction::MaybeCreate(*this, aTextNode, aOffset, aLength);
  if (NS_WARN_IF(!transaction)) {
    return NS_ERROR_FAILURE;
  }

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eDeleteText, nsIEditor::ePrevious);

  // Let listeners know what's up
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillDeleteText(&aTextNode, aOffset, aLength);
    }
  }

  nsresult rv = DoTransactionInternal(transaction);

  if (mRules && mRules->AsHTMLEditRules()) {
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->DidDeleteText(aTextNode, aOffset, aLength);
  }

  // Let listeners know what happened
  if (!mActionListeners.IsEmpty()) {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidDeleteText(&aTextNode, aOffset, aLength, rv);
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

  nsCOMPtr<nsINode> parent = aStartOfRightNode.GetContainer()->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Fix the child before mutation observer may touch the DOM tree.
  nsIContent* firstChildOfRightNode = aStartOfRightNode.GetChild();
  parent->InsertBefore(aNewLeftNode, aStartOfRightNode.GetContainer(), aError);
  if (NS_WARN_IF(aError.Failed())) {
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
      rightAsText->SubstringData(0, aStartOfRightNode.Offset(), leftText,
                                 IgnoreErrors());
      // XXX This call may destroy us.
      DoDeleteText(MOZ_KnownLive(*rightAsText), 0, aStartOfRightNode.Offset(),
                   IgnoreErrors());
      // Fix left node
      // XXX This call may destroy us.
      DoSetText(MOZ_KnownLive(*leftAsText), leftText, IgnoreErrors());
    } else {
      MOZ_DIAGNOSTIC_ASSERT(!rightAsText && !leftAsText);
      // Otherwise it's an interior node, so shuffle around the children. Go
      // through list backwards so deletes don't interfere with the iteration.
      if (!firstChildOfRightNode) {
        MoveAllChildren(*aStartOfRightNode.GetContainer(),
                        EditorRawDOMPoint(&aNewLeftNode, 0), aError);
        NS_WARNING_ASSERTION(
            !aError.Failed(),
            "Failed to move all children from the right node to the left node");
      } else if (NS_WARN_IF(aStartOfRightNode.GetContainer() !=
                            firstChildOfRightNode->GetParentNode())) {
        // firstChildOfRightNode has been moved by mutation observer.
        // In this case, we what should we do?  Use offset?  But we cannot
        // check if the offset is still expected.
      } else {
        MovePreviousSiblings(*firstChildOfRightNode,
                             EditorRawDOMPoint(&aNewLeftNode, 0), aError);
        NS_WARNING_ASSERTION(!aError.Failed(),
                             "Failed to move some children from the right node "
                             "to the left node");
      }
    }
  }

  // XXX Why do we ignore an error while moving nodes from the right node to
  //     the left node?
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
      if (NS_WARN_IF(aError.Failed())) {
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
    if (NS_WARN_IF(aError.Failed())) {
      return;
    }
    range.mSelection->AddRangeAndSelectFramesAndNotifyListeners(*newRange,
                                                                aError);
    if (NS_WARN_IF(aError.Failed())) {
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

nsresult EditorBase::DoJoinNodes(nsINode* aNodeToKeep, nsINode* aNodeToJoin,
                                 nsINode* aParent) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_DIAGNOSTIC_ASSERT(AsHTMLEditor());

  MOZ_ASSERT(aNodeToKeep);
  MOZ_ASSERT(aNodeToJoin);
  MOZ_ASSERT(aParent);

  uint32_t firstNodeLength = aNodeToJoin->Length();

  int32_t joinOffset;
  GetNodeLocation(aNodeToJoin, &joinOffset);
  int32_t keepOffset;
  nsINode* parent = GetNodeLocation(aNodeToKeep, &keepOffset);

  // Remember all selection points.
  AutoTArray<SavedRange, 10> savedRanges;
  for (SelectionType selectionType : kPresentSelectionTypes) {
    SavedRange range;
    range.mSelection = GetSelection(selectionType);
    if (selectionType == SelectionType::eNormal) {
      NS_ENSURE_TRUE(range.mSelection, NS_ERROR_NULL_POINTER);
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
        if (range.mStartContainer == parent &&
            joinOffset < range.mStartOffset &&
            range.mStartOffset <= keepOffset) {
          range.mStartContainer = aNodeToJoin;
          range.mStartOffset = firstNodeLength;
        }
        if (range.mEndContainer == parent && joinOffset < range.mEndOffset &&
            range.mEndOffset <= keepOffset) {
          range.mEndContainer = aNodeToJoin;
          range.mEndOffset = firstNodeLength;
        }
      }

      savedRanges.AppendElement(range);
    }
  }

  // OK, ready to do join now.
  // If it's a text node, just shuffle around some text.
  if (IsTextNode(aNodeToKeep) && IsTextNode(aNodeToJoin)) {
    nsAutoString rightText;
    nsAutoString leftText;
    aNodeToKeep->GetAsText()->GetData(rightText);
    aNodeToJoin->GetAsText()->GetData(leftText);
    leftText += rightText;
    // XXX This call may destroy us.
    DoSetText(MOZ_KnownLive(*aNodeToKeep->GetAsText()), leftText,
              IgnoreErrors());
  } else {
    // Otherwise it's an interior node, so shuffle around the children.
    nsCOMPtr<nsINodeList> childNodes = aNodeToJoin->ChildNodes();
    MOZ_ASSERT(childNodes);

    // Remember the first child in aNodeToKeep, we'll insert all the children of
    // aNodeToJoin in front of it GetFirstChild returns nullptr firstNode if
    // aNodeToKeep has no children, that's OK.
    nsCOMPtr<nsIContent> firstNode = aNodeToKeep->GetFirstChild();

    // Have to go through the list backwards to keep deletes from interfering
    // with iteration.
    for (uint32_t i = childNodes->Length(); i; --i) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i - 1);
      if (childNode) {
        // prepend children of aNodeToJoin
        ErrorResult err;
        aNodeToKeep->InsertBefore(*childNode, firstNode, err);
        NS_ENSURE_TRUE(!err.Failed(), err.StealNSResult());
        firstNode = childNode.forget();
      }
    }
  }

  // Delete the extra node.
  ErrorResult err;
  aParent->RemoveChild(*aNodeToJoin, err);

  bool allowedTransactionsToChangeSelection =
      AllowsTransactionsToChangeSelection();

  RefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // And adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      ErrorResult rv;
      range.mSelection->RemoveAllRanges(rv);
      if (NS_WARN_IF(rv.Failed())) {
        return rv.StealNSResult();
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
    if (range.mStartContainer == aNodeToJoin) {
      range.mStartContainer = aNodeToKeep;
    } else if (range.mStartContainer == aNodeToKeep) {
      range.mStartOffset += firstNodeLength;
    }

    // Check to see if we joined nodes where selection ends.
    if (range.mEndContainer == aNodeToJoin) {
      range.mEndContainer = aNodeToKeep;
    } else if (range.mEndContainer == aNodeToKeep) {
      range.mEndOffset += firstNodeLength;
    }

    RefPtr<nsRange> newRange =
        nsRange::Create(range.mStartContainer, range.mStartOffset,
                        range.mEndContainer, range.mEndOffset, IgnoreErrors());
    if (NS_WARN_IF(!newRange)) {
      return NS_ERROR_FAILURE;
    }

    ErrorResult err;
    range.mSelection->AddRangeAndSelectFramesAndNotifyListeners(*newRange, err);
    if (NS_WARN_IF(err.Failed())) {
      return err.StealNSResult();
    }
  }

  if (allowedTransactionsToChangeSelection) {
    // Editor wants us to set selection at join point.
    DebugOnly<nsresult> rv = SelectionRefPtr()->Collapse(
        aNodeToKeep, AssertedCast<int32_t>(firstNodeLength));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to collapse Selection at end of the node");
  }

  return err.StealNSResult();
}

// static
int32_t EditorBase::GetChildOffset(nsINode* aChild, nsINode* aParent) {
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(aParent);

  // nsINode::ComputeIndexOf() is expensive.  So, if we can return index
  // without calling it, we should do that.

  // If there is no previous siblings, it means that it's the first child.
  if (aParent->GetFirstChild() == aChild) {
    MOZ_ASSERT(aParent->ComputeIndexOf(aChild) == 0);
    return 0;
  }

  // If there is no next siblings, it means that it's the last child.
  if (aParent->GetLastChild() == aChild) {
    int32_t lastChildIndex = static_cast<int32_t>(aParent->Length() - 1);
    MOZ_ASSERT(aParent->ComputeIndexOf(aChild) == lastChildIndex);
    return lastChildIndex;
  }

  int32_t index = aParent->ComputeIndexOf(aChild);
  MOZ_ASSERT(index != -1);
  return index;
}

// static
nsINode* EditorBase::GetNodeLocation(nsINode* aChild, int32_t* aOffset) {
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(aOffset);

  nsINode* parent = aChild->GetParentNode();
  if (parent) {
    *aOffset = GetChildOffset(aChild, parent);
    MOZ_ASSERT(*aOffset != -1);
  } else {
    *aOffset = -1;
  }
  return parent;
}

nsIContent* EditorBase::GetPreviousNodeInternal(nsINode& aNode,
                                                bool aFindEditableNode,
                                                bool aFindAnyDataNode,
                                                bool aNoBlockCrossing) {
  if (!IsDescendantOfEditorRoot(&aNode)) {
    return nullptr;
  }
  return FindNode(&aNode, false, aFindEditableNode, aFindAnyDataNode,
                  aNoBlockCrossing);
}

nsIContent* EditorBase::GetPreviousNodeInternal(const EditorRawDOMPoint& aPoint,
                                                bool aFindEditableNode,
                                                bool aFindAnyDataNode,
                                                bool aNoBlockCrossing) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  NS_WARNING_ASSERTION(
      !aPoint.IsInDataNode() || aPoint.IsInTextNode(),
      "GetPreviousNodeInternal() doesn't assume that the start point is a "
      "data node except text node");

  // If we are at the beginning of the node, or it is a text node, then just
  // look before it.
  if (aPoint.IsStartOfContainer() || aPoint.IsInTextNode()) {
    if (aNoBlockCrossing && IsBlockNode(aPoint.GetContainer())) {
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
  nsIContent* rightMostNode =
      GetRightmostChild(aPoint.GetContainer(), aNoBlockCrossing);
  if (!rightMostNode) {
    return nullptr;
  }

  if ((!aFindEditableNode || IsEditable(rightMostNode)) &&
      (aFindAnyDataNode || IsElementOrText(*rightMostNode))) {
    return rightMostNode;
  }

  // restart the search from the non-editable node we just found
  return GetPreviousNodeInternal(*rightMostNode, aFindEditableNode,
                                 aFindAnyDataNode, aNoBlockCrossing);
}

nsIContent* EditorBase::GetNextNodeInternal(nsINode& aNode,
                                            bool aFindEditableNode,
                                            bool aFindAnyDataNode,
                                            bool aNoBlockCrossing) {
  if (!IsDescendantOfEditorRoot(&aNode)) {
    return nullptr;
  }
  return FindNode(&aNode, true, aFindEditableNode, aFindAnyDataNode,
                  aNoBlockCrossing);
}

nsIContent* EditorBase::GetNextNodeInternal(const EditorRawDOMPoint& aPoint,
                                            bool aFindEditableNode,
                                            bool aFindAnyDataNode,
                                            bool aNoBlockCrossing) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  NS_WARNING_ASSERTION(
      !aPoint.IsInDataNode() || aPoint.IsInTextNode(),
      "GetNextNodeInternal() doesn't assume that the start point is a "
      "data node except text node");

  EditorRawDOMPoint point(aPoint);

  // if the container is a text node, use its location instead
  if (point.IsInTextNode()) {
    point.Set(point.GetContainer());
    bool advanced = point.AdvanceOffset();
    if (NS_WARN_IF(!advanced)) {
      return nullptr;
    }
  }

  if (point.GetChild()) {
    if (aNoBlockCrossing && IsBlockNode(point.GetChild())) {
      return point.GetChild();
    }

    nsIContent* leftMostNode =
        GetLeftmostChild(point.GetChild(), aNoBlockCrossing);
    if (!leftMostNode) {
      return point.GetChild();
    }

    if (!IsDescendantOfEditorRoot(leftMostNode)) {
      return nullptr;
    }

    if ((!aFindEditableNode || IsEditable(leftMostNode)) &&
        (aFindAnyDataNode || IsElementOrText(*leftMostNode))) {
      return leftMostNode;
    }

    // restart the search from the non-editable node we just found
    return GetNextNodeInternal(*leftMostNode, aFindEditableNode,
                               aFindAnyDataNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  if (aNoBlockCrossing && IsBlockNode(point.GetContainer())) {
    // don't cross out of parent block
    return nullptr;
  }

  return GetNextNodeInternal(*point.GetContainer(), aFindEditableNode,
                             aFindAnyDataNode, aNoBlockCrossing);
}

nsIContent* EditorBase::FindNextLeafNode(nsINode* aCurrentNode, bool aGoForward,
                                         bool bNoBlockCrossing) {
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
      if (bNoBlockCrossing && IsBlockNode(sibling)) {
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
    if (IsEditorRoot(parent) || (bNoBlockCrossing && IsBlockNode(parent))) {
      return nullptr;
    }

    cur = parent;
  }

  MOZ_ASSERT_UNREACHABLE("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent* EditorBase::FindNode(nsINode* aCurrentNode, bool aGoForward,
                                 bool aEditableNode, bool aFindAnyDataNode,
                                 bool bNoBlockCrossing) {
  if (IsEditorRoot(aCurrentNode)) {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.

    return nullptr;
  }

  nsCOMPtr<nsIContent> candidate =
      FindNextLeafNode(aCurrentNode, aGoForward, bNoBlockCrossing);

  if (!candidate) {
    return nullptr;
  }

  if ((!aEditableNode || IsEditable(candidate)) &&
      (aFindAnyDataNode || IsElementOrText(*candidate))) {
    return candidate;
  }

  return FindNode(candidate, aGoForward, aEditableNode, aFindAnyDataNode,
                  bNoBlockCrossing);
}

nsIContent* EditorBase::GetRightmostChild(nsINode* aCurrentNode,
                                          bool bNoBlockCrossing) const {
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent* cur = aCurrentNode->GetLastChild();
  if (!cur) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && IsBlockNode(cur)) {
      return cur;
    }
    nsIContent* next = cur->GetLastChild();
    if (!next) {
      return cur;
    }
    cur = next;
  }

  MOZ_ASSERT_UNREACHABLE("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent* EditorBase::GetLeftmostChild(nsINode* aCurrentNode,
                                         bool bNoBlockCrossing) const {
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent* cur = aCurrentNode->GetFirstChild();
  if (!cur) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && IsBlockNode(cur)) {
      return cur;
    }
    nsIContent* next = cur->GetFirstChild();
    if (!next) {
      return cur;
    }
    cur = next;
  }

  MOZ_ASSERT_UNREACHABLE("What part of for(;;) do you not understand?");
  return nullptr;
}

bool EditorBase::IsBlockNode(nsINode* aNode) const {
  // stub to be overridden in HTMLEditor.
  // screwing around with the class hierarchy here in order
  // to not duplicate the code in GetNextNode/GetPrevNode
  // across both EditorBase/HTMLEditor.
  return false;
}

bool EditorBase::CanContain(nsINode& aParent, nsIContent& aChild) const {
  switch (aParent.NodeType()) {
    case nsINode::ELEMENT_NODE:
    case nsINode::DOCUMENT_FRAGMENT_NODE:
      return TagCanContain(*aParent.NodeInfo()->NameAtom(), aChild);
  }
  return false;
}

bool EditorBase::CanContainTag(nsINode& aParent, nsAtom& aChildTag) const {
  switch (aParent.NodeType()) {
    case nsINode::ELEMENT_NODE:
    case nsINode::DOCUMENT_FRAGMENT_NODE:
      return TagCanContainTag(*aParent.NodeInfo()->NameAtom(), aChildTag);
  }
  return false;
}

bool EditorBase::TagCanContain(nsAtom& aParentTag, nsIContent& aChild) const {
  switch (aChild.NodeType()) {
    case nsINode::TEXT_NODE:
    case nsINode::ELEMENT_NODE:
    case nsINode::DOCUMENT_FRAGMENT_NODE:
      return TagCanContainTag(aParentTag, *aChild.NodeInfo()->NameAtom());
  }
  return false;
}

bool EditorBase::TagCanContainTag(nsAtom& aParentTag, nsAtom& aChildTag) const {
  return true;
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
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (IsEditable(child)) {
      ++count;
    }
  }
  return count;
}

NS_IMETHODIMP
EditorBase::IncrementModificationCount(int32_t inNumMods) {
  uint32_t oldModCount = mModCount;

  mModCount += inNumMods;

  if ((!oldModCount && mModCount) || (oldModCount && !mModCount)) {
    NotifyDocumentListeners(eDocumentStateChanged);
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetModificationCount(int32_t* outModCount) {
  NS_ENSURE_ARG_POINTER(outModCount);
  *outModCount = mModCount;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::ResetModificationCount() {
  bool doNotify = (mModCount != 0);

  mModCount = 0;

  if (doNotify) {
    NotifyDocumentListeners(eDocumentStateChanged);
  }
  return NS_OK;
}

// static
bool EditorBase::AreNodesSameType(nsIContent& aNode1,
                                  nsIContent& aNode2) const {
  if (aNode1.NodeInfo()->NameAtom() != aNode2.NodeInfo()->NameAtom()) {
    return false;
  }
  if (!AsHTMLEditor() || !AsHTMLEditor()->IsCSSEnabled()) {
    return true;
  }
  // If this is an HTMLEditor in CSS mode and they are <span> elements,
  // let's check their styles.
  if (!aNode1.IsHTMLElement(nsGkAtoms::span)) {
    return true;
  }
  if (!aNode1.IsElement() || !aNode2.IsElement()) {
    return false;
  }
  return CSSEditUtils::ElementsSameStyle(aNode1.AsElement(),
                                         aNode2.AsElement());
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
  Element* element = Element::FromNode(aNode);
  if (!element) {
    element = aNode->GetParentElement();
    if (!element) {
      return false;
    }
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
                   !atStartOfRightNode.GetContainer()->GetParent())) {
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
      if (NS_WARN_IF(error.Failed())) {
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

EditorDOMPoint EditorBase::JoinNodesDeepWithTransaction(
    nsIContent& aLeftNode, nsIContent& aRightNode) {
  // While the rightmost children and their descendants of the left node match
  // the leftmost children and their descendants of the right node, join them
  // up.

  nsCOMPtr<nsIContent> leftNodeToJoin = &aLeftNode;
  nsCOMPtr<nsIContent> rightNodeToJoin = &aRightNode;
  nsCOMPtr<nsINode> parentNode = aRightNode.GetParentNode();

  EditorDOMPoint ret;
  while (leftNodeToJoin && rightNodeToJoin && parentNode &&
         AreNodesSameType(*leftNodeToJoin, *rightNodeToJoin)) {
    uint32_t length = leftNodeToJoin->Length();

    // Do the join
    nsresult rv = JoinNodesWithTransaction(*leftNodeToJoin, *rightNodeToJoin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorDOMPoint();
    }

    ret.Set(rightNodeToJoin, length);

    if (parentNode->GetAsText()) {
      // We've joined all the way down to text nodes, we're done!
      return ret;
    }

    // Get new left and right nodes, and begin anew
    parentNode = rightNodeToJoin;
    rightNodeToJoin = parentNode->GetChildAt_Deprecated(length);
    if (rightNodeToJoin) {
      leftNodeToJoin = rightNodeToJoin->GetPreviousSibling();
    } else {
      leftNodeToJoin = nullptr;
    }

    // Skip over non-editable nodes
    while (leftNodeToJoin && !IsEditable(leftNodeToJoin)) {
      leftNodeToJoin = leftNodeToJoin->GetPreviousSibling();
    }
    if (!leftNodeToJoin) {
      return ret;
    }

    while (rightNodeToJoin && !IsEditable(rightNodeToJoin)) {
      rightNodeToJoin = rightNodeToJoin->GetNextSibling();
    }
    if (!rightNodeToJoin) {
      return ret;
    }
  }

  if (NS_WARN_IF(!ret.IsSet())) {
    return EditorDOMPoint();
  }

  return ret;
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
  return rv;
}

nsresult EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (mPaddingBRElementForEmptyEditor) {
    return NS_OK;
  }

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eCreatePaddingBRElementForEmptyEditor,
      nsIEditor::eNone);

  RefPtr<Element> rootElement = GetRoot();
  if (!rootElement) {
    return NS_OK;
  }

  // Now we've got the body element. Iterate over the body element's children,
  // looking for editable content. If no editable content is found, insert the
  // padding <br> element.
  bool isRootEditable = IsEditable(rootElement);
  for (nsIContent* rootChild = rootElement->GetFirstChild(); rootChild;
       rootChild = rootChild->GetNextSibling()) {
    if (EditorBase::IsPaddingBRElementForEmptyEditor(*rootChild) ||
        !isRootEditable || IsEditable(rootChild) || IsBlockNode(rootChild)) {
      return NS_OK;
    }
  }

  // Skip adding the padding <br> element for empty editor if body
  // is read-only.
  if (!IsModifiableNode(*rootElement)) {
    return NS_OK;
  }

  // Create a br.
  RefPtr<Element> newBrElement = CreateHTMLContent(nsGkAtoms::br);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(!newBrElement)) {
    return NS_ERROR_FAILURE;
  }

  mPaddingBRElementForEmptyEditor =
      static_cast<HTMLBRElement*>(newBrElement.get());

  // Give it a special attribute.
  newBrElement->SetFlags(NS_PADDING_FOR_EMPTY_EDITOR);

  // Put the node in the document.
  nsresult rv =
      InsertNodeWithTransaction(*newBrElement, EditorDOMPoint(rootElement, 0));
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Set selection.
  IgnoredErrorResult error;
  SelectionRefPtr()->Collapse(EditorRawDOMPoint(rootElement, 0), error);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      !error.Failed(),
      "Failed to collapse selection at start of the root element");
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
  DebugOnly<nsresult> rv = MOZ_KnownLive(htmlEditor)->RefreshEditingUI();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RefreshEditingUI() failed");
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
    GetModificationCount(&modCount);
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
      aggregateTransaction->AppendChild(deleteRangeTransaction);
    } else if (aAction != eNone) {
      // we have an insertion point.  delete the thing in front of it or
      // behind it, depending on aAction
      // XXX Odd, when there are two or more ranges, this returns the last
      //     range information with aRemovingNode, aOffset and aLength.
      RefPtr<EditTransactionBase> deleteRangeTransaction =
          CreateTxnForDeleteRange(range, aAction, aRemovingNode, aOffset,
                                  aLength);
      // XXX When there are two or more ranges and at least one of them is
      //     not editable, deleteRangeTransaction may be nullptr.
      //     In such case, should we stop removing other ranges too?
      if (NS_WARN_IF(!deleteRangeTransaction)) {
        return nullptr;
      }
      aggregateTransaction->AppendChild(deleteRangeTransaction);
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
      if (NS_WARN_IF(!deleteTextTransaction)) {
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
    if (NS_WARN_IF(!deleteNodeTransaction)) {
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
      if (NS_WARN_IF(!deleteTextTransaction)) {
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
    if (NS_WARN_IF(!deleteNodeTransaction)) {
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
    RefPtr<DeleteTextTransaction> deleteTextTransaction =
        aAction == ePrevious
            ? DeleteTextTransaction::MaybeCreateForPreviousCharacter(
                  *this, *nodeAsText, offset)
            : DeleteTextTransaction::MaybeCreateForNextCharacter(
                  *this, *nodeAsText, offset);
    if (NS_WARN_IF(!deleteTextTransaction)) {
      return nullptr;
    }
    *aOffset = deleteTextTransaction->Offset();
    *aLength = deleteTextTransaction->LengthToDelete();
    node.forget(aRemovingNode);
    return deleteTextTransaction.forget();
  }

  // we're either deleting a node or chardata, need to dig into the next/prev
  // node to find out
  nsCOMPtr<nsINode> selectedNode;
  if (aAction == ePrevious) {
    selectedNode =
        GetPreviousEditableNode(EditorRawDOMPoint(node, child, offset));
  } else if (aAction == eNext) {
    selectedNode = GetNextEditableNode(EditorRawDOMPoint(node, child, offset));
  }

  while (selectedNode && selectedNode->IsCharacterData() &&
         !selectedNode->Length()) {
    // Can't delete an empty chardata node (bug 762183)
    if (aAction == ePrevious) {
      selectedNode = GetPreviousEditableNode(*selectedNode);
    } else if (aAction == eNext) {
      selectedNode = GetNextEditableNode(*selectedNode);
    }
  }

  if (NS_WARN_IF(!selectedNode)) {
    return nullptr;
  }

  if (RefPtr<Text> selectedNodeAsText = Text::FromNode(selectedNode)) {
    if (NS_WARN_IF(aAction != ePrevious && aAction != eNext)) {
      return nullptr;
    }
    // we are deleting from a chardata node, so do a character deletion
    uint32_t position = 0;
    if (aAction == ePrevious) {
      position = selectedNode->Length();
    }
    RefPtr<DeleteTextTransaction> deleteTextTransaction =
        aAction == ePrevious
            ? DeleteTextTransaction::MaybeCreateForPreviousCharacter(
                  *this, *selectedNodeAsText, position)
            : DeleteTextTransaction::MaybeCreateForNextCharacter(
                  *this, *selectedNodeAsText, position);
    if (NS_WARN_IF(!deleteTextTransaction)) {
      return nullptr;
    }
    *aOffset = deleteTextTransaction->Offset();
    *aLength = deleteTextTransaction->LengthToDelete();
    selectedNode.forget(aRemovingNode);
    return deleteTextTransaction.forget();
  }

  RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      DeleteNodeTransaction::MaybeCreate(*this, *selectedNode);
  if (NS_WARN_IF(!deleteNodeTransaction)) {
    return nullptr;
  }
  selectedNode.forget(aRemovingNode);
  return deleteNodeTransaction.forget();
}

nsresult EditorBase::CreateRange(nsINode* aStartContainer, int32_t aStartOffset,
                                 nsINode* aEndContainer, int32_t aEndOffset,
                                 nsRange** aRange) {
  RefPtr<nsRange> range = nsRange::Create(
      aStartContainer, aStartOffset, aEndContainer, aEndOffset, IgnoreErrors());
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }
  range.forget(aRange);
  return NS_OK;
}

nsresult EditorBase::AppendNodeToSelectionAsRange(nsINode* aNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aNode)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsINode> parentNode = aNode->GetParentNode();
  if (NS_WARN_IF(!parentNode)) {
    return NS_ERROR_FAILURE;
  }

  int32_t offset = GetChildOffset(aNode, parentNode);

  RefPtr<nsRange> range;
  nsresult rv = CreateRange(parentNode, offset, parentNode, offset + 1,
                            getter_AddRefs(range));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult err;
  SelectionRefPtr()->AddRangeAndSelectFramesAndNotifyListeners(*range, err);
  NS_WARNING_ASSERTION(!err.Failed(), "Failed to add range to Selection");
  return err.StealNSResult();
}

nsresult EditorBase::ClearSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  ErrorResult rv;
  SelectionRefPtr()->RemoveAllRanges(rv);
  NS_WARNING_ASSERTION(!rv.Failed(),
                       "Failed to remove all ranges from Selection");
  return rv.StealNSResult();
}

already_AddRefed<Element> EditorBase::CreateHTMLContent(const nsAtom* aTag) {
  MOZ_ASSERT(aTag);

  RefPtr<Document> doc = GetDocument();
  if (!doc) {
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

  return doc->CreateElem(nsDependentAtomString(aTag), nullptr,
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
  text->SetText(aData, false);
  return text.forget();
}

NS_IMETHODIMP
EditorBase::SetAttributeOrEquivalent(Element* aElement,
                                     const nsAString& aAttribute,
                                     const nsAString& aValue,
                                     bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  nsresult rv = SetAttributeOrEquivalent(aElement, attribute, aValue,
                                         aSuppressTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::RemoveAttributeOrEquivalent(Element* aElement,
                                        const nsAString& aAttribute,
                                        bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsAtom> attribute = NS_Atomize(aAttribute);
  nsresult rv =
      RemoveAttributeOrEquivalent(aElement, attribute, aSuppressTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
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
  if (IsReadonly() || IsDisabled()) {
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
    EditSubAction aEditSubAction, nsINode* previousSelectedNode,
    uint32_t previousSelectedOffset, nsINode* aStartContainer,
    uint32_t aStartOffset, nsINode* aEndContainer, uint32_t aEndOffset) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mInlineSpellChecker) {
    return NS_OK;
  }
  return mInlineSpellChecker->SpellCheckAfterEditorChange(
      aEditSubAction, *SelectionRefPtr(), previousSelectedNode,
      previousSelectedOffset, aStartContainer, aStartOffset, aEndContainer,
      aEndOffset);
}

Element* EditorBase::FindSelectionRoot(nsINode* aNode) const {
  return GetRoot();
}

void EditorBase::InitializeSelectionAncestorLimit(nsIContent& aAncestorLimit) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SelectionRefPtr()->SetAncestorLimiter(&aAncestorLimit);
}

nsresult EditorBase::InitializeSelection(EventTarget* aFocusEventTarget) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsCOMPtr<nsINode> targetNode = do_QueryInterface(aFocusEventTarget);
  if (NS_WARN_IF(!targetNode)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIContent> selectionRootContent = FindSelectionRoot(targetNode);
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
  selectionController->SetCaretReadOnly(IsReadonly());
  selectionController->SetCaretEnabled(true);
  // NOTE(emilio): It's important for this call to be after
  // SetCaretEnabled(true), since that would override mIgnoreUserModify to true.
  //
  // Also, make sure to always ignore it for designMode, since that effectively
  // overrides everything and we allow to edit stuff with
  // contenteditable="false" subtrees in such a document.
  caret->SetIgnoreUserModify(targetNode->OwnerDoc()->HasFlag(NODE_IS_EDITABLE));

  // Init selection
  selectionController->SetDisplaySelection(
      nsISelectionController::SELECTION_ON);
  selectionController->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);
  selectionController->RepaintSelection(
      nsISelectionController::SELECTION_NORMAL);

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
    Text* textNode = betterInsertionPoint.GetContainerAsText();
    MOZ_ASSERT(textNode,
               "There must be text node if composition string is not empty");
    if (textNode) {
      MOZ_ASSERT(textNode->Length() >= mComposition->XPEndOffsetInTextNode(),
                 "The text node must be different from the old text node");
      CompositionTransaction::SetIMESelection(
          *this, textNode, mComposition->XPOffsetInTextNode(),
          mComposition->XPLengthInTextNode(), mComposition->GetRanges());
    }
  }

  return NS_OK;
}

class RepaintSelectionRunner final : public Runnable {
 public:
  explicit RepaintSelectionRunner(nsISelectionController* aSelectionController)
      : Runnable("RepaintSelectionRunner"),
        mSelectionController(aSelectionController) {}

  NS_IMETHOD Run() override {
    mSelectionController->RepaintSelection(
        nsISelectionController::SELECTION_NORMAL);
    return NS_OK;
  }

 private:
  nsCOMPtr<nsISelectionController> mSelectionController;
};

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
  }

  selectionController->SetCaretEnabled(false);

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!fm)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  fm->UpdateCaretForCaretBrowsingMode();

  if (!HasIndependentSelection()) {
    // If this editor doesn't have an independent selection, i.e., it must
    // mean that it is an HTML editor, the selection controller is shared with
    // presShell.  So, even this editor loses focus, other part of the document
    // may still have focus.
    RefPtr<Document> doc = GetDocument();
    ErrorResult ret;
    if (!doc || !doc->HasFocus(ret)) {
      // If the document already lost focus, mark the selection as disabled.
      selectionController->SetDisplaySelection(
          nsISelectionController::SELECTION_DISABLED);
    } else {
      // Otherwise, mark selection as normal because outside of a
      // contenteditable element should be selected with normal selection
      // color after here.
      selectionController->SetDisplaySelection(
          nsISelectionController::SELECTION_ON);
    }
  } else if (IsFormWidget() || IsPasswordEditor() || IsReadonly() ||
             IsDisabled() || IsInputFiltered()) {
    // In <input> or <textarea>, the independent selection should be hidden
    // while this editor doesn't have focus.
    selectionController->SetDisplaySelection(
        nsISelectionController::SELECTION_HIDDEN);
  } else {
    // Otherwise, although we're not sure how this case happens, the
    // independent selection should be marked as disabled.
    selectionController->SetDisplaySelection(
        nsISelectionController::SELECTION_DISABLED);
  }

  // FinalizeSelection might be called from ContentRemoved even if selection
  // isn't updated.  So we need to call RepaintSelection after updated it.
  nsContentUtils::AddScriptRunner(
      new RepaintSelectionRunner(selectionController));
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

  OnFocus(&aElement);

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

  // For plaintext editors, we need to ask the input/textarea element directly.
  if (rootElement && rootElement->IsRootOfNativeAnonymousSubtree()) {
    rootElement = rootElement->GetParent()->AsElement();
  }

  return rootElement;
}

nsresult EditorBase::DetermineCurrentDirection() {
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);

  // If we don't have an explicit direction, determine our direction
  // from the content's direction
  if (!IsRightToLeft() && !IsLeftToRight()) {
    nsIFrame* frame = rootElement->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

    // Set the flag here, to enable us to use the same code path below.
    // It will be flipped before returning from the function.
    if (frame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    } else {
      mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
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

  // XXX Oddly, Chrome does not dispatch beforeinput event in this case but
  //     dispatches input event.

  nsresult rv = DetermineCurrentDirection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  if (IsRightToLeft()) {
    editActionData.SetData(NS_LITERAL_STRING("ltr"));
    nsresult rv = SetTextDirectionTo(TextDirection::eLTR);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
  } else if (IsLeftToRight()) {
    editActionData.SetData(NS_LITERAL_STRING("rtl"));
    nsresult rv = SetTextDirectionTo(TextDirection::eRTL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
  } else {
    MOZ_ASSERT_UNREACHABLE(
        "Why did DetermineCurrentDirection() not determine current direction?");
  }

  // XXX When we don't change the text direction, do we really need to
  //     dispatch input event?
  FireInputEvent();

  return NS_OK;
}

void EditorBase::SwitchTextDirectionTo(TextDirection aTextDirection) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eSetTextDirection);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  // XXX Oddly, Chrome does not dispatch beforeinput event in this case but
  //     dispatches input event.

  nsresult rv = DetermineCurrentDirection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  switch (aTextDirection) {
    case TextDirection::eLTR:
      editActionData.SetData(NS_LITERAL_STRING("ltr"));
      if (IsRightToLeft() &&
          NS_WARN_IF(NS_FAILED(SetTextDirectionTo(aTextDirection)))) {
        return;
      }
      break;
    case TextDirection::eRTL:
      editActionData.SetData(NS_LITERAL_STRING("rtl"));
      if (IsLeftToRight() &&
          NS_WARN_IF(NS_FAILED(SetTextDirectionTo(aTextDirection)))) {
        return;
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid aTextDirection value");
      break;
  }

  // XXX When we don't change the text direction, do we really need to
  //     dispatch input event?
  FireInputEvent();
}

nsresult EditorBase::SetTextDirectionTo(TextDirection aTextDirection) {
  Element* rootElement = GetExposedRoot();

  if (aTextDirection == TextDirection::eLTR) {
    NS_ASSERTION(!IsLeftToRight(), "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    nsresult rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir,
                                       NS_LITERAL_STRING("ltr"), true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (aTextDirection == TextDirection::eRTL) {
    NS_ASSERTION(!IsRightToLeft(), "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    nsresult rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir,
                                       NS_LITERAL_STRING("rtl"), true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  return NS_OK;
}

bool EditorBase::IsModifiableNode(const nsINode& aNode) const {
  return !AsHTMLEditor() || aNode.IsEditable();
}

nsIContent* EditorBase::GetFocusedContent() {
  EventTarget* piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return nullptr;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsIContent* content = fm->GetFocusedElement();
  MOZ_ASSERT((content == piTarget) == SameCOMIdentity(content, piTarget));

  return (content == piTarget) ? content : nullptr;
}

already_AddRefed<nsIContent> EditorBase::GetFocusedContentForIME() {
  nsCOMPtr<nsIContent> content = GetFocusedContent();
  return content.forget();
}

bool EditorBase::IsActiveInDOMWindow() {
  EventTarget* piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return false;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  RefPtr<Document> document = GetDocument();
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

void EditorBase::OnFocus(EventTarget* aFocusEventTarget) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  InitializeSelection(aFocusEventTarget);
  mSpellCheckerDictionaryUpdated = false;
  if (mInlineSpellChecker && CanEnableSpellCheck()) {
    mInlineSpellChecker->UpdateCurrentDictionary();
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
    mEditorBase->RestorePreservedSelection();
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
      mParentData(aEditorBase.mEditActionData),
      mData(VoidString()),
      mTopLevelEditSubAction(EditSubAction::eNone) {
  // If we're nested edit action, copies necessary data from the parent.
  if (mParentData) {
    mSelection = mParentData->mSelection;
    // If we're eNotEditing, we should inherit the parent's edit action.
    // This may occur if creator or its callee use public methods which
    // just returns something.
    if (aEditAction != EditAction::eNotEditing) {
      mEditAction = aEditAction;
    }
    mTopLevelEditSubAction = mParentData->mTopLevelEditSubAction;
    mDirectionOfTopLevelEditSubAction =
        mParentData->mDirectionOfTopLevelEditSubAction;
  } else {
    mSelection = mEditorBase.GetSelection();
    if (NS_WARN_IF(!mSelection)) {
      return;
    }
    mEditAction = aEditAction;
    mDirectionOfTopLevelEditSubAction = eNone;
  }
  mEditorBase.mEditActionData = this;
}

EditorBase::AutoEditActionDataSetter::~AutoEditActionDataSetter() {
  if (!mSelection || NS_WARN_IF(mEditorBase.mEditActionData != this)) {
    return;
  }
  mEditorBase.mEditActionData = mParentData;
}

void EditorBase::AutoEditActionDataSetter::SetColorData(
    const nsAString& aData) {
  if (aData.IsEmpty()) {
    // When removing color/background-color, let's use empty string.
    MOZ_ASSERT(!EmptyString().IsVoid());
    mData = EmptyString();
    return;
  }

  bool wasCurrentColor = false;
  nscolor color = NS_RGB(0, 0, 0);
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), aData, &color,
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
  mDataTransfer = aDataTransfer;
}

void EditorBase::AutoEditActionDataSetter::InitializeDataTransfer(
    nsITransferable* aTransferable) {
  MOZ_ASSERT(aTransferable);

  Document* document = mEditorBase.GetDocument();
  nsIGlobalObject* scopeObject =
      document ? document->GetScopeObject() : nullptr;
  mDataTransfer = new DataTransfer(scopeObject, eEditorInput, aTransferable);
}

void EditorBase::AutoEditActionDataSetter::InitializeDataTransfer(
    const nsAString& aString) {
  Document* document = mEditorBase.GetDocument();
  nsIGlobalObject* scopeObject =
      document ? document->GetScopeObject() : nullptr;
  mDataTransfer = new DataTransfer(scopeObject, eEditorInput, aString);
}

void EditorBase::AutoEditActionDataSetter::InitializeDataTransferWithClipboard(
    SettingDataTransfer aSettingDataTransfer, int32_t aClipboardType) {
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

}  // namespace mozilla
