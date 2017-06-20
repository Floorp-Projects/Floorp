/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorBase.h"

#include "mozilla/DebugOnly.h"          // for DebugOnly

#include <stdio.h>                      // for nullptr, stdout
#include <string.h>                     // for strcmp

#include "ChangeAttributeTransaction.h" // for ChangeAttributeTransaction
#include "CompositionTransaction.h"     // for CompositionTransaction
#include "CreateElementTransaction.h"   // for CreateElementTransaction
#include "DeleteNodeTransaction.h"      // for DeleteNodeTransaction
#include "DeleteRangeTransaction.h"     // for DeleteRangeTransaction
#include "DeleteTextTransaction.h"      // for DeleteTextTransaction
#include "EditAggregateTransaction.h"   // for EditAggregateTransaction
#include "EditorEventListener.h"        // for EditorEventListener
#include "InsertNodeTransaction.h"      // for InsertNodeTransaction
#include "InsertTextTransaction.h"      // for InsertTextTransaction
#include "JoinNodeTransaction.h"        // for JoinNodeTransaction
#include "PlaceholderTransaction.h"     // for PlaceholderTransaction
#include "SetTextTransaction.h"         // for SetTextTransaction
#include "SplitNodeTransaction.h"       // for SplitNodeTransaction
#include "StyleSheetTransactions.h"     // for AddStyleSheetTransaction, etc.
#include "TextEditUtils.h"              // for TextEditUtils
#include "mozInlineSpellChecker.h"      // for mozInlineSpellChecker
#include "mozilla/CheckedInt.h"         // for CheckedInt
#include "mozilla/EditorUtils.h"        // for AutoRules, etc.
#include "mozilla/EditTransactionBase.h" // for EditTransactionBase
#include "mozilla/FlushType.h"          // for FlushType::Frames
#include "mozilla/IMEStateManager.h"    // for IMEStateManager
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/dom/Selection.h"      // for Selection, etc.
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/TextComposition.h"    // for TextComposition
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"        // for Element, nsINode::AsElement
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/Event.h"
#include "mozilla/mozalloc.h"           // for operator new, etc.
#include "nsAString.h"                  // for nsAString::Length, etc.
#include "nsCCUncollectableMarker.h"    // for nsCCUncollectableMarker
#include "nsCaret.h"                    // for nsCaret
#include "nsCaseTreatment.h"
#include "nsCharTraits.h"               // for NS_IS_HIGH_SURROGATE, etc.
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsComputedDOMStyle.h"         // for nsComputedDOMStyle
#include "nsContentUtils.h"             // for nsContentUtils
#include "nsDOMString.h"                // for DOMStringIsNull
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc.
#include "nsError.h"                    // for NS_OK, etc.
#include "nsFocusManager.h"             // for nsFocusManager
#include "nsFrameSelection.h"           // for nsFrameSelection
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::dir
#include "nsIAbsorbingTransaction.h"    // for nsIAbsorbingTransaction
#include "nsIAtom.h"                    // for nsIAtom
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMAttr.h"                 // for nsIDOMAttr
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIDOMEvent.h"                // for nsIDOMEvent
#include "nsIDOMEventListener.h"        // for nsIDOMEventListener
#include "nsIDOMEventTarget.h"          // for nsIDOMEventTarget
#include "nsIDOMHTMLElement.h"          // for nsIDOMHTMLElement
#include "nsIDOMMozNamedAttrMap.h"      // for nsIDOMMozNamedAttrMap
#include "nsIDOMMouseEvent.h"           // for nsIDOMMouseEvent
#include "nsIDOMNode.h"                 // for nsIDOMNode, etc.
#include "nsIDOMNodeList.h"             // for nsIDOMNodeList
#include "nsIDocumentStateListener.h"   // for nsIDocumentStateListener
#include "nsIEditActionListener.h"      // for nsIEditActionListener
#include "nsIEditorObserver.h"          // for nsIEditorObserver
#include "nsIEditorSpellCheck.h"        // for nsIEditorSpellCheck
#include "nsIFrame.h"                   // for nsIFrame
#include "nsIHTMLDocument.h"            // for nsIHTMLDocument
#include "nsIInlineSpellChecker.h"      // for nsIInlineSpellChecker, etc.
#include "nsNameSpaceManager.h"        // for kNameSpaceID_None, etc.
#include "nsINode.h"                    // for nsINode, etc.
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc.
#include "nsIPresShell.h"               // for nsIPresShell
#include "nsISelectionController.h"     // for nsISelectionController, etc.
#include "nsISelectionDisplay.h"        // for nsISelectionDisplay, etc.
#include "nsISupportsBase.h"            // for nsISupports
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_IF_ADDREF
#include "nsITransaction.h"             // for nsITransaction
#include "nsITransactionManager.h"
#include "nsIWeakReference.h"           // for nsISupportsWeakReference
#include "nsIWidget.h"                  // for nsIWidget, IMEState, etc.
#include "nsPIDOMWindow.h"              // for nsPIDOMWindow
#include "nsPresContext.h"              // for nsPresContext
#include "nsRange.h"                    // for nsRange
#include "nsReadableUtils.h"            // for EmptyString, ToNewCString
#include "nsString.h"                   // for nsAutoString, nsString, etc.
#include "nsStringFwd.h"                // for nsAFlatString
#include "nsStyleConsts.h"              // for NS_STYLE_DIRECTION_RTL, etc.
#include "nsStyleContext.h"             // for nsStyleContext
#include "nsStyleStruct.h"              // for nsStyleDisplay, nsStyleText, etc.
#include "nsStyleStructFwd.h"           // for nsIFrame::StyleUIReset, etc.
#include "nsTextNode.h"                 // for nsTextNode
#include "nsThreadUtils.h"              // for nsRunnable
#include "nsTransactionManager.h"       // for nsTransactionManager
#include "prtime.h"                     // for PR_Now

class nsIOutputStream;
class nsIParserService;
class nsITransferable;

#ifdef DEBUG
#include "nsIDOMHTMLDocument.h"         // for nsIDOMHTMLDocument
#endif

// Defined in nsEditorRegistration.cpp
extern nsIParserService *sParserService;

namespace mozilla {

using namespace dom;
using namespace widget;

/*****************************************************************************
 * mozilla::EditorBase
 *****************************************************************************/

EditorBase::EditorBase()
  : mPlaceholderName(nullptr)
  , mSelState(nullptr)
  , mModCount(0)
  , mFlags(0)
  , mUpdateCount(0)
  , mPlaceholderBatch(0)
  , mAction(EditAction::none)
  , mIMETextOffset(0)
  , mIMETextLength(0)
  , mDirection(eNone)
  , mDocDirtyState(-1)
  , mSpellcheckCheckboxState(eTriUnset)
  , mShouldTxnSetSelection(true)
  , mDidPreDestroy(false)
  , mDidPostCreate(false)
  , mDispatchInputEvent(true)
  , mIsInEditAction(false)
  , mHidingCaret(false)
  , mSpellCheckerDictionaryUpdated(true)
{
}

EditorBase::~EditorBase()
{
  MOZ_ASSERT(!IsInitialized() || mDidPreDestroy,
             "Why PreDestroy hasn't been called?");

  if (mComposition) {
    mComposition->OnEditorDestroyed();
    mComposition = nullptr;
  }
  // If this editor is still hiding the caret, we need to restore it.
  HideCaret(false);
  mTxnMgr = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(EditorBase)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(EditorBase)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mTxnMgr)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventListener)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mSavedSel);
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mRangeUpdater);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EditorBase)
 nsIDocument* currentDoc =
   tmp->mRootElement ? tmp->mRootElement->GetUncomposedDoc() : nullptr;
 if (currentDoc &&
     nsCCUncollectableMarker::InGeneration(cb, currentDoc->GetMarkedCCGeneration())) {
   return NS_SUCCESS_INTERRUPTED_TRAVERSE;
 }
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTxnMgr)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventListener)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSavedSel);
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRangeUpdater);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditorBase)
 NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
 NS_INTERFACE_MAP_ENTRY(nsIEditor)
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(EditorBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EditorBase)


NS_IMETHODIMP
EditorBase::Init(nsIDOMDocument* aDOMDocument,
                 nsIContent* aRoot,
                 nsISelectionController* aSelectionController,
                 uint32_t aFlags,
                 const nsAString& aValue)
{
  MOZ_ASSERT(mAction == EditAction::none,
             "Initializing during an edit action is an error");
  MOZ_ASSERT(aDOMDocument);
  if (!aDOMDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  // First only set flags, but other stuff shouldn't be initialized now.
  // Don't move this call after initializing mDocumentWeak.
  // SetFlags() can check whether it's called during initialization or not by
  // them.  Note that SetFlags() will be called by PostCreate().
#ifdef DEBUG
  nsresult rv =
#endif
  SetFlags(aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetFlags() failed");

  nsCOMPtr<nsIDocument> document = do_QueryInterface(aDOMDocument);
  mDocumentWeak = document.get();
  // HTML editors currently don't have their own selection controller,
  // so they'll pass null as aSelCon, and we'll get the selection controller
  // off of the presshell.
  nsCOMPtr<nsISelectionController> selectionController;
  if (aSelectionController) {
    mSelectionControllerWeak = aSelectionController;
    selectionController = aSelectionController;
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selectionController = do_QueryInterface(presShell);
  }
  MOZ_ASSERT(selectionController,
             "Selection controller should be available at this point");

  //set up root element if we are passed one.
  if (aRoot)
    mRootElement = do_QueryInterface(aRoot);

  mUpdateCount=0;

  // If this is an editor for <input> or <textarea>, mIMETextNode is always
  // recreated with same content. Therefore, we need to forget mIMETextNode,
  // but we need to keep storing mIMETextOffset and mIMETextLength becuase
  // they are necessary to restore IME selection and replacing composing string
  // when this receives eCompositionChange event next time.
  if (mIMETextNode && !mIMETextNode->IsInComposedDoc()) {
    mIMETextNode = nullptr;
  }

  // Show the caret.
  selectionController->SetCaretReadOnly(false);
  selectionController->SetDisplaySelection(
                         nsISelectionController::SELECTION_ON);
  // Show all the selection reflected to user.
  selectionController->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);

  MOZ_ASSERT(IsInitialized());

  // Make sure that the editor will be destroyed properly
  mDidPreDestroy = false;
  // Make sure that the ediotr will be created properly
  mDidPostCreate = false;

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::PostCreate()
{
  // Synchronize some stuff for the flags.  SetFlags() will initialize
  // something by the flag difference.  This is first time of that, so, all
  // initializations must be run.  For such reason, we need to invert mFlags
  // value first.
  mFlags = ~mFlags;
  nsresult rv = SetFlags(~mFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // These operations only need to happen on the first PostCreate call
  if (!mDidPostCreate) {
    mDidPostCreate = true;

    // Set up listeners
    CreateEventListeners();
    rv = InstallEventListeners();
    NS_ENSURE_SUCCESS(rv, rv);

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
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(focusedContent);
    if (target) {
      InitializeSelection(target);
    }

    // If the text control gets reframed during focus, Focus() would not be
    // called, so take a chance here to see if we need to spell check the text
    // control.
    EditorEventListener* listener =
      reinterpret_cast<EditorEventListener*>(mEventListener.get());
    listener->SpellCheckIfNeeded();

    IMEState newState;
    rv = GetPreferredIMEState(&newState);
    NS_ENSURE_SUCCESS(rv, NS_OK);
    nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
    IMEStateManager::UpdateIMEState(newState, content, *this);
  }

  // FYI: This call might cause destroying this editor.
  IMEStateManager::OnEditorInitialized(this);

  return NS_OK;
}

void
EditorBase::CreateEventListeners()
{
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new EditorEventListener();
  }
}

nsresult
EditorBase::InstallEventListeners()
{
  if (NS_WARN_IF(!IsInitialized()) || NS_WARN_IF(!mEventListener)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Initialize the event target.
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  NS_ENSURE_TRUE(rootContent, NS_ERROR_NOT_AVAILABLE);
  mEventTarget = do_QueryInterface(rootContent->GetParent());
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_AVAILABLE);

  EditorEventListener* listener =
    reinterpret_cast<EditorEventListener*>(mEventListener.get());
  nsresult rv = listener->Connect(this);
  if (mComposition) {
    // Restart to handle composition with new editor contents.
    mComposition->StartHandlingComposition(this);
  }
  return rv;
}

void
EditorBase::RemoveEventListeners()
{
  if (!IsInitialized() || !mEventListener) {
    return;
  }
  reinterpret_cast<EditorEventListener*>(mEventListener.get())->Disconnect();
  if (mComposition) {
    // Even if this is called, don't release mComposition because this is
    // may be reused after reframing.
    mComposition->EndHandlingComposition(this);
  }
  mEventTarget = nullptr;
}

bool
EditorBase::GetDesiredSpellCheckState()
{
  // Check user override on this element
  if (mSpellcheckCheckboxState != eTriUnset) {
    return (mSpellcheckCheckboxState == eTriTrue);
  }

  // Check user preferences
  int32_t spellcheckLevel = Preferences::GetInt("layout.spellcheckDefault", 1);

  if (!spellcheckLevel) {
    return false;                    // Spellchecking forced off globally
  }

  if (!CanEnableSpellCheck()) {
    return false;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
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

  nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(content);
  if (!element) {
    return false;
  }

  if (!IsPlaintextEditor()) {
    // Some of the page content might be editable and some not, if spellcheck=
    // is explicitly set anywhere, so if there's anything editable on the page,
    // return true and let the spellchecker figure it out.
    nsCOMPtr<nsIHTMLDocument> doc = do_QueryInterface(content->GetUncomposedDoc());
    return doc && doc->IsEditingOn();
  }

  bool enable;
  element->GetSpellcheck(&enable);

  return enable;
}

NS_IMETHODIMP
EditorBase::PreDestroy(bool aDestroyingFrames)
{
  if (mDidPreDestroy)
    return NS_OK;

  IMEStateManager::OnEditorDestroying(this);

  // Let spellchecker clean up its observers etc. It is important not to
  // actually free the spellchecker here, since the spellchecker could have
  // caused flush notifications, which could have gotten here if a textbox
  // is being removed. Setting the spellchecker to nullptr could free the
  // object that is still in use! It will be freed when the editor is
  // destroyed.
  if (mInlineSpellChecker)
    mInlineSpellChecker->Cleanup(aDestroyingFrames);

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
  mSpellcheckCheckboxState = eTriUnset;
  mRootElement = nullptr;

  // Transaction may grab this instance.  Therefore, they should be released
  // here for stopping the circular reference with this instance.
  if (mTxnMgr) {
    mTxnMgr->Clear();
    mTxnMgr = nullptr;
  }

  mDidPreDestroy = true;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetFlags(uint32_t* aFlags)
{
  *aFlags = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetFlags(uint32_t aFlags)
{
  if (mFlags == aFlags) {
    return NS_OK;
  }

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
    nsresult rv = SyncRealTimeSpell();
    NS_ENSURE_SUCCESS(rv, rv);
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
      IMEStateManager::UpdateIMEState(newState, content, *this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetIsSelectionEditable(bool* aIsSelectionEditable)
{
  NS_ENSURE_ARG_POINTER(aIsSelectionEditable);

  // get current selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // XXX we just check that the anchor node is editable at the moment
  //     we should check that all nodes in the selection are editable
  nsCOMPtr<nsINode> anchorNode = selection->GetAnchorNode();
  *aIsSelectionEditable = anchorNode && IsEditable(anchorNode);

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetIsDocumentEditable(bool* aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);
  nsCOMPtr<nsIDocument> doc = GetDocument();
  *aIsDocumentEditable = doc && IsModifiable();

  return NS_OK;
}

already_AddRefed<nsIDocument>
EditorBase::GetDocument()
{
  nsCOMPtr<nsIDocument> document = mDocumentWeak.get();
  return document.forget();
}

already_AddRefed<nsIDOMDocument>
EditorBase::GetDOMDocument()
{
  nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(mDocumentWeak);
  return domDocument.forget();
}

NS_IMETHODIMP
EditorBase::GetDocument(nsIDOMDocument** aDoc)
{
  *aDoc = GetDOMDocument().take();
  return *aDoc ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

already_AddRefed<nsIPresShell>
EditorBase::GetPresShell()
{
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  return presShell.forget();
}

already_AddRefed<nsIWidget>
EditorBase::GetWidget()
{
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, nullptr);
  nsPresContext* pc = ps->GetPresContext();
  NS_ENSURE_TRUE(pc, nullptr);
  nsCOMPtr<nsIWidget> widget = pc->GetRootWidget();
  NS_ENSURE_TRUE(widget.get(), nullptr);
  return widget.forget();
}

NS_IMETHODIMP
EditorBase::GetContentsMIMEType(char** aContentsMIMEType)
{
  NS_ENSURE_ARG_POINTER(aContentsMIMEType);
  *aContentsMIMEType = ToNewCString(mContentMIMEType);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetContentsMIMEType(const char* aContentsMIMEType)
{
  mContentMIMEType.Assign(aContentsMIMEType ? aContentsMIMEType : "");
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetSelectionController(nsISelectionController** aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  *aSel = nullptr; // init out param
  nsCOMPtr<nsISelectionController> selCon = GetSelectionController();
  if (NS_WARN_IF(!selCon)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  selCon.forget(aSel);
  return NS_OK;
}

already_AddRefed<nsISelectionController>
EditorBase::GetSelectionController()
{
  nsCOMPtr<nsISelectionController> selectionController;
  if (mSelectionControllerWeak) {
    selectionController = mSelectionControllerWeak.get();
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selectionController = do_QueryInterface(presShell);
  }
  return selectionController.forget();
}

NS_IMETHODIMP
EditorBase::DeleteSelection(EDirection aAction,
                            EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);
  return DeleteSelectionImpl(aAction, aStripWrappers);
}

NS_IMETHODIMP
EditorBase::GetSelection(nsISelection** aSelection)
{
  return GetSelection(SelectionType::eNormal, aSelection);
}

nsresult
EditorBase::GetSelection(SelectionType aSelectionType,
                         nsISelection** aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  *aSelection = nullptr;
  nsCOMPtr<nsISelectionController> selcon = GetSelectionController();
  if (!selcon) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return selcon->GetSelection(ToRawSelectionType(aSelectionType), aSelection);
}

Selection*
EditorBase::GetSelection(SelectionType aSelectionType)
{
  nsCOMPtr<nsISelection> sel;
  nsresult rv = GetSelection(aSelectionType, getter_AddRefs(sel));
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!sel)) {
    return nullptr;
  }

  return sel->AsSelection();
}

NS_IMETHODIMP
EditorBase::DoTransaction(nsITransaction* aTxn)
{
  if (mPlaceholderBatch && !mPlaceholderTransactionWeak) {
    RefPtr<PlaceholderTransaction> placeholderTransaction =
      new PlaceholderTransaction(*this, mPlaceholderName, Move(mSelState));

    // Save off weak reference to placeholder transaction
    mPlaceholderTransactionWeak = placeholderTransaction;

    // We will recurse, but will not hit this case in the nested call
    DoTransaction(placeholderTransaction);

    if (mTxnMgr) {
      nsCOMPtr<nsITransaction> topTransaction = mTxnMgr->PeekUndoStack();
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
          mPlaceholderTransactionWeak = topPlaceholderTransaction;
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
    RefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    selection->StartBatchChanges();

    nsresult rv;
    if (mTxnMgr) {
      RefPtr<nsTransactionManager> txnMgr = mTxnMgr;
      rv = txnMgr->DoTransaction(aTxn);
    } else {
      rv = aTxn->DoTransaction();
    }
    if (NS_SUCCEEDED(rv)) {
      DoAfterDoTransaction(aTxn);
    }

    // no need to check rv here, don't lose result of operation
    selection->EndBatchChanges();

    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::EnableUndo(bool aEnable)
{
  if (aEnable) {
    if (!mTxnMgr) {
      mTxnMgr = new nsTransactionManager();
    }
    mTxnMgr->SetMaxTransactionCount(-1);
  } else if (mTxnMgr) {
    // disable the transaction manager if it is enabled
    mTxnMgr->Clear();
    mTxnMgr->SetMaxTransactionCount(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetNumberOfUndoItems(int32_t* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfUndoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
EditorBase::GetNumberOfRedoItems(int32_t* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfRedoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
EditorBase::GetTransactionManager(nsITransactionManager** aTxnManager)
{
  NS_ENSURE_ARG_POINTER(aTxnManager);

  *aTxnManager = nullptr;
  NS_ENSURE_TRUE(mTxnMgr, NS_ERROR_FAILURE);

  NS_ADDREF(*aTxnManager = mTxnMgr);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Undo(uint32_t aCount)
{
  ForceCompositionEnd();

  bool hasTxnMgr, hasTransaction = false;
  CanUndo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  AutoRules beginRulesSniffing(this, EditAction::undo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  RefPtr<nsTransactionManager> txnMgr = mTxnMgr;
  for (uint32_t i = 0; i < aCount; ++i) {
    nsresult rv = txnMgr->UndoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterUndoTransaction();
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::CanUndo(bool* aIsEnabled,
                    bool* aCanUndo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanUndo, NS_ERROR_NULL_POINTER);
  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    int32_t numTxns = 0;
    mTxnMgr->GetNumberOfUndoItems(&numTxns);
    *aCanUndo = !!numTxns;
  } else {
    *aCanUndo = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Redo(uint32_t aCount)
{
  bool hasTxnMgr, hasTransaction = false;
  CanRedo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  AutoRules beginRulesSniffing(this, EditAction::redo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  RefPtr<nsTransactionManager> txnMgr = mTxnMgr;
  for (uint32_t i = 0; i < aCount; ++i) {
    nsresult rv = txnMgr->RedoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterRedoTransaction();
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::CanRedo(bool* aIsEnabled, bool* aCanRedo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanRedo, NS_ERROR_NULL_POINTER);

  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    int32_t numTxns = 0;
    mTxnMgr->GetNumberOfRedoItems(&numTxns);
    *aCanRedo = !!numTxns;
  } else {
    *aCanRedo = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::BeginTransaction()
{
  BeginUpdateViewBatch();

  if (mTxnMgr) {
    RefPtr<nsTransactionManager> txnMgr = mTxnMgr;
    txnMgr->BeginBatch(nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::EndTransaction()
{
  if (mTxnMgr) {
    RefPtr<nsTransactionManager> txnMgr = mTxnMgr;
    txnMgr->EndBatch(false);
  }

  EndUpdateViewBatch();

  return NS_OK;
}


// These two routines are similar to the above, but do not use
// the transaction managers batching feature.  Instead we use
// a placeholder transaction to wrap up any further transaction
// while the batch is open.  The advantage of this is that
// placeholder transactions can later merge, if needed.  Merging
// is unavailable between transaction manager batches.

NS_IMETHODIMP
EditorBase::BeginPlaceHolderTransaction(nsIAtom* aName)
{
  MOZ_ASSERT(mPlaceholderBatch >= 0, "negative placeholder batch count!");
  if (!mPlaceholderBatch) {
    NotifyEditorObservers(eNotifyEditorObserversOfBefore);
    // time to turn on the batch
    BeginUpdateViewBatch();
    mPlaceholderTransactionWeak = nullptr;
    mPlaceholderName = aName;
    RefPtr<Selection> selection = GetSelection();
    if (selection) {
      mSelState = MakeUnique<SelectionState>();
      mSelState->SaveSelection(selection);
      // Composition transaction can modify multiple nodes and it merges text
      // node for ime into single text node.
      // So if current selection is into IME text node, it might be failed
      // to restore selection by UndoTransaction.
      // So we need update selection by range updater.
      if (mPlaceholderName == nsGkAtoms::IMETxnName) {
        mRangeUpdater.RegisterSelectionState(*mSelState);
      }
    }
  }
  mPlaceholderBatch++;

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::EndPlaceHolderTransaction()
{
  MOZ_ASSERT(mPlaceholderBatch > 0,
             "zero or negative placeholder batch count when ending batch!");
  if (mPlaceholderBatch == 1) {
    RefPtr<Selection> selection = GetSelection();

    // By making the assumption that no reflow happens during the calls
    // to EndUpdateViewBatch and ScrollSelectionIntoView, we are able to
    // allow the selection to cache a frame offset which is used by the
    // caret drawing code. We only enable this cache here; at other times,
    // we have no way to know whether reflow invalidates it
    // See bugs 35296 and 199412.
    if (selection) {
      selection->SetCanCacheFrameOffset(true);
    }

    {
      // Hide the caret here to avoid hiding it twice, once in EndUpdateViewBatch
      // and once in ScrollSelectionIntoView.
      RefPtr<nsCaret> caret;
      nsCOMPtr<nsIPresShell> presShell = GetPresShell();

      if (presShell) {
        caret = presShell->GetCaret();
      }

      // time to turn off the batch
      EndUpdateViewBatch();
      // make sure selection is in view

      // After ScrollSelectionIntoView(), the pending notifications might be
      // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
      ScrollSelectionIntoView(false);
    }

    // cached for frame offset are Not available now
    if (selection) {
      selection->SetCanCacheFrameOffset(false);
    }

    if (mSelState) {
      // we saved the selection state, but never got to hand it to placeholder
      // (else we ould have nulled out this pointer), so destroy it to prevent leaks.
      if (mPlaceholderName == nsGkAtoms::IMETxnName) {
        mRangeUpdater.DropSelectionState(*mSelState);
      }
      mSelState = nullptr;
    }
    // We might have never made a placeholder if no action took place.
    if (mPlaceholderTransactionWeak) {
      RefPtr<PlaceholderTransaction> placeholderTransaction =
        mPlaceholderTransactionWeak.get();
      placeholderTransaction->EndPlaceHolderBatch();
      // notify editor observers of action but if composing, it's done by
      // compositionchange event handler.
      if (!mComposition) {
        NotifyEditorObservers(eNotifyEditorObserversOfEnd);
      }
    } else {
      NotifyEditorObservers(eNotifyEditorObserversOfCancel);
    }
  }
  mPlaceholderBatch--;

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::ShouldTxnSetSelection(bool* aResult)
{
  NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
  *aResult = mShouldTxnSetSelection;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetShouldTxnSetSelection(bool aShould)
{
  mShouldTxnSetSelection = aShould;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetDocumentIsEmpty(bool* aDocumentIsEmpty)
{
  *aDocumentIsEmpty = true;

  dom::Element* root = GetRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER);

  *aDocumentIsEmpty = !root->HasChildren();
  return NS_OK;
}

// XXX: The rule system should tell us which node to select all on (ie, the
//      root, or the body)
NS_IMETHODIMP
EditorBase::SelectAll()
{
  // XXX Why doesn't this check if the document is alive?
  if (!IsInitialized()) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  ForceCompositionEnd();

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
  return SelectEntireDocument(selection);
}

NS_IMETHODIMP
EditorBase::BeginningOfDocument()
{
  // XXX Why doesn't this check if the document is alive?
  if (!IsInitialized()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // get the selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  // get the root element
  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  // find first editable thingy
  nsCOMPtr<nsINode> firstNode = GetFirstEditableNode(rootElement);
  if (!firstNode) {
    // just the root node, set selection to inside the root
    return selection->Collapse(rootElement, 0);
  }

  if (firstNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    // If firstNode is text, set selection to beginning of the text node.
    return selection->Collapse(firstNode, 0);
  }

  // Otherwise, it's a leaf node and we set the selection just in front of it.
  nsCOMPtr<nsIContent> parent = firstNode->GetParent();
  if (!parent) {
    return NS_ERROR_NULL_POINTER;
  }

  int32_t offsetInParent = parent->IndexOf(firstNode);
  return selection->Collapse(parent, offsetInParent);
}

NS_IMETHODIMP
EditorBase::EndOfDocument()
{
  // XXX Why doesn't this check if the document is alive?
  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // get the root element
  nsINode* node = GetRoot();
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  nsINode* child = node->GetLastChild();

  while (child && IsContainer(child->AsDOMNode())) {
    node = child;
    child = node->GetLastChild();
  }

  uint32_t length = node->Length();
  return selection->Collapse(node, int32_t(length));
}

NS_IMETHODIMP
EditorBase::GetDocumentModified(bool* outDocModified)
{
  NS_ENSURE_TRUE(outDocModified, NS_ERROR_NULL_POINTER);

  int32_t  modCount = 0;
  GetModificationCount(&modCount);

  *outDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetDocumentCharacterSet(nsACString& characterSet)
{
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_UNEXPECTED;
  }
  characterSet = document->GetDocumentCharacterSet();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetDocumentCharacterSet(const nsACString& characterSet)
{
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_UNEXPECTED;
  }
  document->SetDocumentCharacterSet(characterSet);
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::Cut()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::CanCut(bool* aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::Copy()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::CanCopy(bool* aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::CanDelete(bool* aCanDelete)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::Paste(int32_t aSelectionType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::PasteTransferable(nsITransferable* aTransferable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::CanPaste(int32_t aSelectionType, bool* aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::CanPasteTransferable(nsITransferable* aTransferable,
                                 bool* aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::SetAttribute(nsIDOMElement* aElement,
                         const nsAString& aAttribute,
                         const nsAString& aValue)
{
  if (NS_WARN_IF(aAttribute.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIAtom> attribute = NS_Atomize(aAttribute);

  return SetAttribute(element, attribute, aValue);
}

nsresult
EditorBase::SetAttribute(Element* aElement,
                         nsIAtom* aAttribute,
                         const nsAString& aValue)
{
  RefPtr<ChangeAttributeTransaction> transaction =
    CreateTxnForSetAttribute(*aElement, *aAttribute, aValue);
  return DoTransaction(transaction);
}

NS_IMETHODIMP
EditorBase::GetAttributeValue(nsIDOMElement* aElement,
                              const nsAString& aAttribute,
                              nsAString& aResultValue,
                              bool* aResultIsSet)
{
  NS_ENSURE_TRUE(aResultIsSet, NS_ERROR_NULL_POINTER);
  *aResultIsSet = false;
  if (!aElement) {
    return NS_OK;
  }
  nsAutoString value;
  nsresult rv = aElement->GetAttribute(aAttribute, value);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!DOMStringIsNull(value)) {
    *aResultIsSet = true;
    aResultValue = value;
  }
  return rv;
}

NS_IMETHODIMP
EditorBase::RemoveAttribute(nsIDOMElement* aElement,
                            const nsAString& aAttribute)
{
  if (NS_WARN_IF(aAttribute.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIAtom> attribute = NS_Atomize(aAttribute);

  return RemoveAttribute(element, attribute);
}

nsresult
EditorBase::RemoveAttribute(Element* aElement,
                            nsIAtom* aAttribute)
{
  RefPtr<ChangeAttributeTransaction> transaction =
    CreateTxnForRemoveAttribute(*aElement, *aAttribute);
  return DoTransaction(transaction);
}

bool
EditorBase::OutputsMozDirty()
{
  // Return true for Composer (!eEditorAllowInteraction) or mail
  // (eEditorMailMask), but false for webpages.
  return !(mFlags & nsIPlaintextEditor::eEditorAllowInteraction) ||
          (mFlags & nsIPlaintextEditor::eEditorMailMask);
}

NS_IMETHODIMP
EditorBase::MarkNodeDirty(nsIDOMNode* aNode)
{
  // Mark the node dirty, but not for webpages (bug 599983)
  if (!OutputsMozDirty()) {
    return NS_OK;
  }
  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  if (element) {
    element->SetAttr(kNameSpaceID_None, nsGkAtoms::mozdirty,
                     EmptyString(), false);
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetInlineSpellChecker(bool autoCreate,
                                  nsIInlineSpellChecker** aInlineSpellChecker)
{
  NS_ENSURE_ARG_POINTER(aInlineSpellChecker);

  if (mDidPreDestroy) {
    // Don't allow people to get or create the spell checker once the editor
    // is going away.
    *aInlineSpellChecker = nullptr;
    return autoCreate ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  // We don't want to show the spell checking UI if there are no spell check dictionaries available.
  bool canSpell = mozInlineSpellChecker::CanEnableInlineSpellChecking();
  if (!canSpell) {
    *aInlineSpellChecker = nullptr;
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (!mInlineSpellChecker && autoCreate) {
    mInlineSpellChecker = do_CreateInstance(MOZ_INLINESPELLCHECKER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
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

NS_IMETHODIMP
EditorBase::SyncRealTimeSpell()
{
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

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetSpellcheckUserOverride(bool enable)
{
  mSpellcheckCheckboxState = enable ? eTriTrue : eTriFalse;

  return SyncRealTimeSpell();
}

NS_IMETHODIMP
EditorBase::CreateNode(const nsAString& aTag,
                       nsIDOMNode* aParent,
                       int32_t aPosition,
                       nsIDOMNode** aNewNode)
{
  nsCOMPtr<nsIAtom> tag = NS_Atomize(aTag);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_STATE(parent);
  *aNewNode = GetAsDOMNode(CreateNode(tag, parent, aPosition).take());
  NS_ENSURE_STATE(*aNewNode);
  return NS_OK;
}

already_AddRefed<Element>
EditorBase::CreateNode(nsIAtom* aTag,
                       nsINode* aParent,
                       int32_t aPosition)
{
  MOZ_ASSERT(aTag && aParent);

  AutoRules beginRulesSniffing(this, EditAction::createNode, nsIEditor::eNext);

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillCreateNode(nsDependentAtomString(aTag),
                               GetAsDOMNode(aParent), aPosition);
    }
  }

  nsCOMPtr<Element> ret;

  RefPtr<CreateElementTransaction> transaction =
    CreateTxnForCreateElement(*aTag, *aParent, aPosition);
  nsresult rv = DoTransaction(transaction);
  if (NS_SUCCEEDED(rv)) {
    ret = transaction->GetNewNode();
    MOZ_ASSERT(ret);
  }

  mRangeUpdater.SelAdjCreateNode(aParent, aPosition);

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidCreateNode(nsDependentAtomString(aTag), GetAsDOMNode(ret),
                              GetAsDOMNode(aParent), aPosition, rv);
    }
  }

  return ret.forget();
}

NS_IMETHODIMP
EditorBase::InsertNode(nsIDOMNode* aNode,
                       nsIDOMNode* aParent,
                       int32_t aPosition)
{
  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(node && parent, NS_ERROR_NULL_POINTER);

  return InsertNode(*node, *parent, aPosition);
}

nsresult
EditorBase::InsertNode(nsIContent& aNode,
                       nsINode& aParent,
                       int32_t aPosition)
{
  AutoRules beginRulesSniffing(this, EditAction::insertNode, nsIEditor::eNext);

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillInsertNode(aNode.AsDOMNode(), aParent.AsDOMNode(),
                               aPosition);
    }
  }

  RefPtr<InsertNodeTransaction> transaction =
    CreateTxnForInsertNode(aNode, aParent, aPosition);
  nsresult rv = DoTransaction(transaction);

  mRangeUpdater.SelAdjInsertNode(&aParent, aPosition);

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidInsertNode(aNode.AsDOMNode(), aParent.AsDOMNode(), aPosition,
                              rv);
    }
  }

  return rv;
}

NS_IMETHODIMP
EditorBase::SplitNode(nsIDOMNode* aNode,
                      int32_t aOffset,
                      nsIDOMNode** aNewLeftNode)
{
  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);
  ErrorResult rv;
  nsCOMPtr<nsIContent> newNode = SplitNode(*node, aOffset, rv);
  *aNewLeftNode = GetAsDOMNode(newNode.forget().take());
  return rv.StealNSResult();
}

nsIContent*
EditorBase::SplitNode(nsIContent& aNode,
                      int32_t aOffset,
                      ErrorResult& aResult)
{
  AutoRules beginRulesSniffing(this, EditAction::splitNode, nsIEditor::eNext);

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillSplitNode(aNode.AsDOMNode(), aOffset);
    }
  }

  RefPtr<SplitNodeTransaction> transaction =
    CreateTxnForSplitNode(aNode, aOffset);
  aResult = DoTransaction(transaction);

  nsCOMPtr<nsIContent> newNode = aResult.Failed() ? nullptr
                                                  : transaction->GetNewNode();

  mRangeUpdater.SelAdjSplitNode(aNode, aOffset, newNode);

  nsresult rv = aResult.StealNSResult();
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidSplitNode(aNode.AsDOMNode(), aOffset, GetAsDOMNode(newNode),
                             rv);
    }
  }
  // Note: result might be a success code, so we can't use Throw() to
  // set it on aResult.
  aResult = rv;

  return newNode;
}

NS_IMETHODIMP
EditorBase::JoinNodes(nsIDOMNode* aLeftNode,
                      nsIDOMNode* aRightNode,
                      nsIDOMNode*)
{
  nsCOMPtr<nsINode> leftNode = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsINode> rightNode = do_QueryInterface(aRightNode);
  NS_ENSURE_STATE(leftNode && rightNode && leftNode->GetParentNode());
  return JoinNodes(*leftNode, *rightNode);
}

nsresult
EditorBase::JoinNodes(nsINode& aLeftNode,
                      nsINode& aRightNode)
{
  nsCOMPtr<nsINode> parent = aLeftNode.GetParentNode();
  MOZ_ASSERT(parent);

  AutoRules beginRulesSniffing(this, EditAction::joinNode,
                               nsIEditor::ePrevious);

  // Remember some values; later used for saved selection updating.
  // Find the offset between the nodes to be joined.
  int32_t offset = parent->IndexOf(&aRightNode);
  // Find the number of children of the lefthand node
  uint32_t oldLeftNodeLen = aLeftNode.Length();

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillJoinNodes(aLeftNode.AsDOMNode(), aRightNode.AsDOMNode(),
                              parent->AsDOMNode());
    }
  }

  nsresult rv = NS_OK;
  RefPtr<JoinNodeTransaction> transaction =
    CreateTxnForJoinNode(aLeftNode, aRightNode);
  if (transaction)  {
    rv = DoTransaction(transaction);
  }

  mRangeUpdater.SelAdjJoinNodes(aLeftNode, aRightNode, *parent, offset,
                                (int32_t)oldLeftNodeLen);

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidJoinNodes(aLeftNode.AsDOMNode(), aRightNode.AsDOMNode(),
                             parent->AsDOMNode(), rv);
    }
  }

  return rv;
}

NS_IMETHODIMP
EditorBase::DeleteNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);
  return DeleteNode(node);
}

nsresult
EditorBase::DeleteNode(nsINode* aNode)
{
  AutoRules beginRulesSniffing(this, EditAction::createNode,
                               nsIEditor::ePrevious);

  // save node location for selection updating code.
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillDeleteNode(aNode->AsDOMNode());
    }
  }

  RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
    CreateTxnForDeleteNode(aNode);
  nsresult rv = deleteNodeTransaction ? DoTransaction(deleteNodeTransaction) :
                                        NS_ERROR_FAILURE;

  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidDeleteNode(aNode->AsDOMNode(), rv);
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

/**
 * ReplaceContainer() replaces inNode with a new node (outNode) which is
 * constructed to be of type aNodeType.  Put inNodes children into outNode.
 * Callers responsibility to make sure inNode's children can go in outNode.
 */
already_AddRefed<Element>
EditorBase::ReplaceContainer(Element* aOldContainer,
                             nsIAtom* aNodeType,
                             nsIAtom* aAttribute,
                             const nsAString* aValue,
                             ECloneAttributes aCloneAttributes)
{
  MOZ_ASSERT(aOldContainer && aNodeType);

  nsCOMPtr<nsIContent> parent = aOldContainer->GetParent();
  NS_ENSURE_TRUE(parent, nullptr);

  int32_t offset = parent->IndexOf(aOldContainer);

  // create new container
  nsCOMPtr<Element> ret = CreateHTMLContent(aNodeType);
  NS_ENSURE_TRUE(ret, nullptr);

  // set attribute if needed
  if (aAttribute && aValue && aAttribute != nsGkAtoms::_empty) {
    nsresult rv = ret->SetAttr(kNameSpaceID_None, aAttribute, *aValue, true);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  if (aCloneAttributes == eCloneAttributes) {
    CloneAttributes(ret, aOldContainer);
  }

  // notify our internal selection state listener
  // (Note: An AutoSelectionRestorer object must be created
  //  before calling this to initialize mRangeUpdater)
  AutoReplaceContainerSelNotify selStateNotify(mRangeUpdater, aOldContainer,
                                               ret);
  {
    AutoTransactionsConserveSelection conserveSelection(this);
    while (aOldContainer->HasChildren()) {
      nsCOMPtr<nsIContent> child = aOldContainer->GetFirstChild();

      nsresult rv = DeleteNode(child);
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = InsertNode(*child, *ret, -1);
      NS_ENSURE_SUCCESS(rv, nullptr);
    }
  }

  // insert new container into tree
  nsresult rv = InsertNode(*ret, *parent, offset);
  NS_ENSURE_SUCCESS(rv, nullptr);

  // delete old container
  rv = DeleteNode(aOldContainer);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return ret.forget();
}

/**
 * RemoveContainer() removes inNode, reparenting its children (if any) into the
 * parent of inNode.
 */
nsresult
EditorBase::RemoveContainer(nsIContent* aNode)
{
  MOZ_ASSERT(aNode);

  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  NS_ENSURE_STATE(parent);

  int32_t offset = parent->IndexOf(aNode);

  // Loop through the children of inNode and promote them into inNode's parent
  uint32_t nodeOrigLen = aNode->GetChildCount();

  // notify our internal selection state listener
  AutoRemoveContainerSelNotify selNotify(mRangeUpdater, aNode, parent,
                                         offset, nodeOrigLen);

  while (aNode->HasChildren()) {
    nsCOMPtr<nsIContent> child = aNode->GetLastChild();
    nsresult rv = DeleteNode(child);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InsertNode(*child, *parent, offset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return DeleteNode(aNode);
}

/**
 * InsertContainerAbove() inserts a new parent for inNode, which is contructed
 * to be of type aNodeType.  outNode becomes a child of inNode's earlier
 * parent.  Caller's responsibility to make sure inNode's can be child of
 * outNode, and outNode can be child of old parent.
 */
already_AddRefed<Element>
EditorBase::InsertContainerAbove(nsIContent* aNode,
                                 nsIAtom* aNodeType,
                                 nsIAtom* aAttribute,
                                 const nsAString* aValue)
{
  MOZ_ASSERT(aNode && aNodeType);

  nsCOMPtr<nsIContent> parent = aNode->GetParent();
  NS_ENSURE_TRUE(parent, nullptr);
  int32_t offset = parent->IndexOf(aNode);

  // Create new container
  nsCOMPtr<Element> newContent = CreateHTMLContent(aNodeType);
  NS_ENSURE_TRUE(newContent, nullptr);

  // Set attribute if needed
  if (aAttribute && aValue && aAttribute != nsGkAtoms::_empty) {
    nsresult rv =
      newContent->SetAttr(kNameSpaceID_None, aAttribute, *aValue, true);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  // Notify our internal selection state listener
  AutoInsertContainerSelNotify selNotify(mRangeUpdater);

  // Put inNode in new parent, outNode
  nsresult rv = DeleteNode(aNode);
  NS_ENSURE_SUCCESS(rv, nullptr);

  {
    AutoTransactionsConserveSelection conserveSelection(this);
    rv = InsertNode(*aNode, *newContent, 0);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  // Put new parent in doc
  rv = InsertNode(*newContent, *parent, offset);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return newContent.forget();
}

/**
 * MoveNode() moves aNode to {aParent,aOffset}.
 */
nsresult
EditorBase::MoveNode(nsIContent* aNode,
                     nsINode* aParent,
                     int32_t aOffset)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aOffset == -1 ||
             (0 <= aOffset &&
              AssertedCast<uint32_t>(aOffset) <= aParent->Length()));

  nsCOMPtr<nsINode> oldParent = aNode->GetParentNode();
  int32_t oldOffset = oldParent ? oldParent->IndexOf(aNode) : -1;

  if (aOffset == -1) {
    // Magic value meaning "move to end of aParent"
    aOffset = AssertedCast<int32_t>(aParent->Length());
  }

  // Don't do anything if it's already in right place
  if (aParent == oldParent && aOffset == oldOffset) {
    return NS_OK;
  }

  // Notify our internal selection state listener
  AutoMoveNodeSelNotify selNotify(mRangeUpdater, oldParent, oldOffset,
                                  aParent, aOffset);

  // Need to adjust aOffset if we're moving aNode later in its current parent
  if (aParent == oldParent && oldOffset < aOffset) {
    // When we delete aNode, it will make the offsets after it off by one
    aOffset--;
  }

  // Hold a reference so aNode doesn't go away when we remove it (bug 772282)
  nsCOMPtr<nsINode> kungFuDeathGrip = aNode;

  nsresult rv = DeleteNode(aNode);
  NS_ENSURE_SUCCESS(rv, rv);

  return InsertNode(*aNode, *aParent, aOffset);
}

NS_IMETHODIMP
EditorBase::AddEditorObserver(nsIEditorObserver* aObserver)
{
  // we don't keep ownership of the observers.  They must
  // remove themselves as observers before they are destroyed.

  NS_ENSURE_TRUE(aObserver, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (!mEditorObservers.Contains(aObserver)) {
    mEditorObservers.AppendElement(*aObserver);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::RemoveEditorObserver(nsIEditorObserver* aObserver)
{
  NS_ENSURE_TRUE(aObserver, NS_ERROR_FAILURE);

  mEditorObservers.RemoveElement(aObserver);

  return NS_OK;
}

class EditorInputEventDispatcher final : public Runnable
{
public:
  EditorInputEventDispatcher(EditorBase* aEditorBase,
                             nsIContent* aTarget,
                             bool aIsComposing)
    : mEditorBase(aEditorBase)
    , mTarget(aTarget)
    , mIsComposing(aIsComposing)
  {
  }

  NS_IMETHOD Run() override
  {
    // Note that we don't need to check mDispatchInputEvent here.  We need
    // to check it only when the editor requests to dispatch the input event.

    if (!mTarget->IsInComposedDoc()) {
      return NS_OK;
    }

    nsCOMPtr<nsIPresShell> ps = mEditorBase->GetPresShell();
    if (!ps) {
      return NS_OK;
    }

    nsCOMPtr<nsIWidget> widget = mEditorBase->GetWidget();
    if (!widget) {
      return NS_OK;
    }

    // Even if the change is caused by untrusted event, we need to dispatch
    // trusted input event since it's a fact.
    InternalEditorInputEvent inputEvent(true, eEditorInput, widget);
    inputEvent.mTime = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIsComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    nsresult rv =
      ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
    NS_ENSURE_SUCCESS(rv, NS_OK); // print the warning if error
    return NS_OK;
  }

private:
  RefPtr<EditorBase> mEditorBase;
  nsCOMPtr<nsIContent> mTarget;
  bool mIsComposing;
};

void
EditorBase::NotifyEditorObservers(NotificationForEditorObservers aNotification)
{
  // Copy the observers since EditAction()s can modify mEditorObservers.
  AutoEditorObserverArray observers(mEditorObservers);
  switch (aNotification) {
    case eNotifyEditorObserversOfEnd:
      mIsInEditAction = false;
      for (auto& observer : observers) {
        observer->EditAction();
      }

      if (!mDispatchInputEvent) {
        return;
      }

      FireInputEvent();
      break;
    case eNotifyEditorObserversOfBefore:
      if (NS_WARN_IF(mIsInEditAction)) {
        break;
      }
      mIsInEditAction = true;
      for (auto& observer : observers) {
        observer->BeforeEditAction();
      }
      break;
    case eNotifyEditorObserversOfCancel:
      mIsInEditAction = false;
      for (auto& observer : observers) {
        observer->CancelEditAction();
      }
      break;
    default:
      MOZ_CRASH("Handle all notifications here");
      break;
  }
}

void
EditorBase::FireInputEvent()
{
  // We don't need to dispatch multiple input events if there is a pending
  // input event.  However, it may have different event target.  If we resolved
  // this issue, we need to manage the pending events in an array.  But it's
  // overwork.  We don't need to do it for the very rare case.

  nsCOMPtr<nsIContent> target = GetInputEventTargetContent();
  NS_ENSURE_TRUE_VOID(target);

  // NOTE: Don't refer IsIMEComposing() because it returns false even before
  //       compositionend.  However, DOM Level 3 Events defines it should be
  //       true after compositionstart and before compositionend.
  nsContentUtils::AddScriptRunner(
    new EditorInputEventDispatcher(this, target, !!GetComposition()));
}

NS_IMETHODIMP
EditorBase::AddEditActionListener(nsIEditActionListener* aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (!mActionListeners.Contains(aListener)) {
    mActionListeners.AppendElement(*aListener);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::RemoveEditActionListener(nsIEditActionListener* aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

  mActionListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::AddDocumentStateListener(nsIDocumentStateListener* aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  if (!mDocStateListeners.Contains(aListener)) {
    mDocStateListeners.AppendElement(*aListener);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::RemoveDocumentStateListener(nsIDocumentStateListener* aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  mDocStateListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::OutputToString(const nsAString& aFormatType,
                           uint32_t aFlags,
                           nsAString& aOutputString)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::OutputToStream(nsIOutputStream* aOutputStream,
                           const nsAString& aFormatType,
                           const nsACString& aCharsetOverride,
                           uint32_t aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EditorBase::DumpContentTree()
{
#ifdef DEBUG
  if (mRootElement) {
    mRootElement->List(stdout);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::DebugDumpContent()
{
#ifdef DEBUG
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIDOMHTMLDocument> domHTMLDocument = do_QueryInterface(document);
  if (NS_WARN_IF(!domHTMLDocument)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIDOMHTMLElement> bodyElement;
  domHTMLDocument->GetBody(getter_AddRefs(bodyElement));
  nsCOMPtr<nsIContent> content = do_QueryInterface(bodyElement);
  if (content) {
    content->List();
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::DebugUnitTests(int32_t* outNumTests,
                           int32_t* outNumTestsFailed)
{
#ifdef DEBUG
  NS_NOTREACHED("This should never get called. Overridden by subclasses");
#endif
  return NS_OK;
}

bool
EditorBase::ArePreservingSelection()
{
  return !(mSavedSel.IsEmpty());
}

void
EditorBase::PreserveSelectionAcrossActions(Selection* aSel)
{
  mSavedSel.SaveSelection(aSel);
  mRangeUpdater.RegisterSelectionState(mSavedSel);
}

nsresult
EditorBase::RestorePreservedSelection(Selection* aSel)
{
  if (mSavedSel.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  mSavedSel.RestoreSelection(aSel);
  StopPreservingSelection();
  return NS_OK;
}

void
EditorBase::StopPreservingSelection()
{
  mRangeUpdater.DropSelectionState(mSavedSel);
  mSavedSel.MakeEmpty();
}

bool
EditorBase::EnsureComposition(WidgetCompositionEvent* aCompositionEvent)
{
  if (mComposition) {
    return true;
  }
  // The compositionstart event must cause creating new TextComposition
  // instance at being dispatched by IMEStateManager.
  mComposition = IMEStateManager::GetTextCompositionFor(aCompositionEvent);
  if (!mComposition) {
    // However, TextComposition may be committed before the composition
    // event comes here.
    return false;
  }
  mComposition->StartHandlingComposition(this);
  return true;
}

nsresult
EditorBase::BeginIMEComposition(WidgetCompositionEvent* aCompositionEvent)
{
  MOZ_ASSERT(!mComposition, "There is composition already");
  if (!EnsureComposition(aCompositionEvent)) {
    return NS_OK;
  }
  return NS_OK;
}

void
EditorBase::EndIMEComposition()
{
  NS_ENSURE_TRUE_VOID(mComposition); // nothing to do

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTxnMgr) {
    nsCOMPtr<nsITransaction> txn = mTxnMgr->PeekUndoStack();
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryInterface(txn);
    if (plcTxn) {
      DebugOnly<nsresult> rv = plcTxn->Commit();
      NS_ASSERTION(NS_SUCCEEDED(rv),
                   "nsIAbsorbingTransaction::Commit() failed");
    }
  }

  // Composition string may have hidden the caret.  Therefore, we need to
  // cancel it here.
  HideCaret(false);

  /* reset the data we need to construct a transaction */
  mIMETextNode = nullptr;
  mIMETextOffset = 0;
  mIMETextLength = 0;
  mComposition->EndHandlingComposition(this);
  mComposition = nullptr;

  // notify editor observers of action
  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
}

NS_IMETHODIMP
EditorBase::ForceCompositionEnd()
{
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  if (!ps) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsPresContext* pc = ps->GetPresContext();
  if (!pc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mComposition ?
    IMEStateManager::NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, pc) : NS_OK;
}

NS_IMETHODIMP
EditorBase::GetPreferredIMEState(IMEState* aState)
{
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
      if (IsPasswordEditor())
        aState->mEnabled = IMEState::PASSWORD;
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
EditorBase::GetComposing(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = IsIMEComposing();
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetRootElement(nsIDOMElement** aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);
  NS_ENSURE_TRUE(mRootElement, NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(mRootElement);
  rootElement.forget(aRootElement);
  return NS_OK;
}

/**
 * All editor operations which alter the doc should be prefaced
 * with a call to StartOperation, naming the action and direction.
 */
NS_IMETHODIMP
EditorBase::StartOperation(EditAction opID,
                           nsIEditor::EDirection aDirection)
{
  mAction = opID;
  mDirection = aDirection;
  return NS_OK;
}

/**
 * All editor operations which alter the doc should be followed
 * with a call to EndOperation.
 */
NS_IMETHODIMP
EditorBase::EndOperation()
{
  mAction = EditAction::none;
  mDirection = eNone;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::CloneAttribute(const nsAString& aAttribute,
                           nsIDOMNode* aDestNode,
                           nsIDOMNode* aSourceNode)
{
  NS_ENSURE_TRUE(aDestNode && aSourceNode, NS_ERROR_NULL_POINTER);
  if (NS_WARN_IF(aAttribute.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<Element> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<Element> sourceElement = do_QueryInterface(aSourceNode);
  NS_ENSURE_TRUE(destElement && sourceElement, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIAtom> attribute = NS_Atomize(aAttribute);
  return CloneAttribute(attribute, destElement, sourceElement);
}

nsresult
EditorBase::CloneAttribute(nsIAtom* aAttribute,
                           Element* aDestElement,
                           Element* aSourceElement)
{
  nsAutoString attrValue;
  if (aSourceElement->GetAttr(kNameSpaceID_None, aAttribute, attrValue)) {
    return SetAttribute(aDestElement, aAttribute, attrValue);
  }
  return RemoveAttribute(aDestElement, aAttribute);
}

/**
 * @param aDest     Must be a DOM element.
 * @param aSource   Must be a DOM element.
 */
NS_IMETHODIMP
EditorBase::CloneAttributes(nsIDOMNode* aDest,
                            nsIDOMNode* aSource)
{
  NS_ENSURE_TRUE(aDest && aSource, NS_ERROR_NULL_POINTER);

  nsCOMPtr<Element> dest = do_QueryInterface(aDest);
  nsCOMPtr<Element> source = do_QueryInterface(aSource);
  NS_ENSURE_TRUE(dest && source, NS_ERROR_NO_INTERFACE);

  CloneAttributes(dest, source);

  return NS_OK;
}

void
EditorBase::CloneAttributes(Element* aDest,
                            Element* aSource)
{
  MOZ_ASSERT(aDest && aSource);

  AutoEditBatch beginBatching(this);

  // Use transaction system for undo only if destination is already in the
  // document
  NS_ENSURE_TRUE(GetRoot(), );
  bool destInBody = GetRoot()->Contains(aDest);

  // Clear existing attributes
  RefPtr<nsDOMAttributeMap> destAttributes = aDest->Attributes();
  while (RefPtr<Attr> attr = destAttributes->Item(0)) {
    if (destInBody) {
      RemoveAttribute(aDest, attr->NodeInfo()->NameAtom());
    } else {
      aDest->UnsetAttr(kNameSpaceID_None, attr->NodeInfo()->NameAtom(), true);
    }
  }

  // Set just the attributes that the source element has
  RefPtr<nsDOMAttributeMap> sourceAttributes = aSource->Attributes();
  uint32_t sourceCount = sourceAttributes->Length();
  for (uint32_t i = 0; i < sourceCount; i++) {
    RefPtr<Attr> attr = sourceAttributes->Item(i);
    nsAutoString value;
    attr->GetValue(value);
    if (destInBody) {
      SetAttributeOrEquivalent(aDest, attr->NodeInfo()->NameAtom(), value,
                               false);
    } else {
      // The element is not inserted in the document yet, we don't want to put
      // a transaction on the UndoStack
      SetAttributeOrEquivalent(aDest, attr->NodeInfo()->NameAtom(), value,
                               true);
    }
  }
}

nsresult
EditorBase::ScrollSelectionIntoView(bool aScrollToAnchor)
{
  nsCOMPtr<nsISelectionController> selectionController =
    GetSelectionController();
  if (!selectionController) {
    return NS_OK;
  }

  int16_t region = nsISelectionController::SELECTION_FOCUS_REGION;
  if (aScrollToAnchor) {
    region = nsISelectionController::SELECTION_ANCHOR_REGION;
  }
  selectionController->ScrollSelectionIntoView(
                         nsISelectionController::SELECTION_NORMAL,
                         region,
                         nsISelectionController::SCROLL_OVERFLOW_HIDDEN);
  return NS_OK;
}

void
EditorBase::FindBetterInsertionPoint(nsCOMPtr<nsIDOMNode>& aNode,
                                     int32_t& aOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  FindBetterInsertionPoint(node, aOffset);
  aNode = do_QueryInterface(node);
}

void
EditorBase::FindBetterInsertionPoint(nsCOMPtr<nsINode>& aNode,
                                     int32_t& aOffset)
{
  if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    // There is no "better" insertion point.
    return;
  }

  if (!IsPlaintextEditor()) {
    // We cannot find "better" insertion point in HTML editor.
    // WARNING: When you add some code to find better node in HTML editor,
    //          you need to call this before calling InsertTextImpl() in
    //          HTMLEditRules.
    return;
  }

  nsCOMPtr<nsINode> node = aNode;
  int32_t offset = aOffset;

  nsCOMPtr<nsINode> root = GetRoot();
  if (aNode == root) {
    // In some cases, aNode is the anonymous DIV, and offset is 0.  To avoid
    // injecting unneeded text nodes, we first look to see if we have one
    // available.  In that case, we'll just adjust node and offset accordingly.
    if (!offset && node->HasChildren() &&
        node->GetFirstChild()->IsNodeOfType(nsINode::eTEXT)) {
      aNode = node->GetFirstChild();
      aOffset = 0;
      return;
    }

    // In some other cases, aNode is the anonymous DIV, and offset points to the
    // terminating mozBR.  In that case, we'll adjust aInOutNode and
    // aInOutOffset to the preceding text node, if any.
    if (offset > 0 && node->GetChildAt(offset - 1) &&
        node->GetChildAt(offset - 1)->IsNodeOfType(nsINode::eTEXT)) {
      NS_ENSURE_TRUE_VOID(node->Length() <= INT32_MAX);
      aNode = node->GetChildAt(offset - 1);
      aOffset = static_cast<int32_t>(aNode->Length());
      return;
    }
  }

  // Sometimes, aNode is the mozBR element itself.  In that case, we'll adjust
  // the insertion point to the previous text node, if one exists, or to the
  // parent anonymous DIV.
  if (TextEditUtils::IsMozBR(node) && !offset) {
    if (node->GetPreviousSibling() &&
        node->GetPreviousSibling()->IsNodeOfType(nsINode::eTEXT)) {
      NS_ENSURE_TRUE_VOID(node->Length() <= INT32_MAX);
      aNode = node->GetPreviousSibling();
      aOffset = static_cast<int32_t>(aNode->Length());
      return;
    }

    if (node->GetParentNode() && node->GetParentNode() == root) {
      aNode = node->GetParentNode();
      aOffset = 0;
      return;
    }
  }
}

nsresult
EditorBase::InsertTextImpl(const nsAString& aStringToInsert,
                           nsCOMPtr<nsINode>* aInOutNode,
                           int32_t* aInOutOffset,
                           nsIDocument* aDoc)
{
  // NOTE: caller *must* have already used AutoTransactionsConserveSelection
  // stack-based class to turn off txn selection updating.  Caller also turned
  // on rules sniffing if desired.

  NS_ENSURE_TRUE(aInOutNode && *aInOutNode && aInOutOffset && aDoc,
                 NS_ERROR_NULL_POINTER);

  if (!ShouldHandleIMEComposition() && aStringToInsert.IsEmpty()) {
    return NS_OK;
  }

  // This method doesn't support over INT32_MAX length text since aInOutOffset
  // is int32_t*.
  CheckedInt<int32_t> lengthToInsert(aStringToInsert.Length());
  NS_ENSURE_TRUE(lengthToInsert.isValid(), NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsINode> node = *aInOutNode;
  int32_t offset = *aInOutOffset;

  // In some cases, the node may be the anonymous div elemnt or a mozBR
  // element.  Let's try to look for better insertion point in the nearest
  // text node if there is.
  FindBetterInsertionPoint(node, offset);

  // If a neighboring text node already exists, use that
  if (!node->IsNodeOfType(nsINode::eTEXT)) {
    if (offset && node->GetChildAt(offset - 1)->IsNodeOfType(nsINode::eTEXT)) {
      node = node->GetChildAt(offset - 1);
      offset = node->Length();
    } else if (offset < static_cast<int32_t>(node->Length()) &&
               node->GetChildAt(offset)->IsNodeOfType(nsINode::eTEXT)) {
      node = node->GetChildAt(offset);
      offset = 0;
    }
  }

  if (ShouldHandleIMEComposition()) {
    CheckedInt<int32_t> newOffset;
    if (!node->IsNodeOfType(nsINode::eTEXT)) {
      // create a text node
      RefPtr<nsTextNode> newNode = aDoc->CreateTextNode(EmptyString());
      // then we insert it into the dom tree
      nsresult rv = InsertNode(*newNode, *node, offset);
      NS_ENSURE_SUCCESS(rv, rv);
      node = newNode;
      offset = 0;
      newOffset = lengthToInsert;
    } else {
      newOffset = lengthToInsert + offset;
      NS_ENSURE_TRUE(newOffset.isValid(), NS_ERROR_FAILURE);
    }
    nsresult rv =
      InsertTextIntoTextNodeImpl(aStringToInsert, *node->GetAsText(), offset);
    NS_ENSURE_SUCCESS(rv, rv);
    offset = newOffset.value();
  } else {
    if (node->IsNodeOfType(nsINode::eTEXT)) {
      CheckedInt<int32_t> newOffset = lengthToInsert + offset;
      NS_ENSURE_TRUE(newOffset.isValid(), NS_ERROR_FAILURE);
      // we are inserting text into an existing text node.
      nsresult rv =
        InsertTextIntoTextNodeImpl(aStringToInsert, *node->GetAsText(), offset);
      NS_ENSURE_SUCCESS(rv, rv);
      offset = newOffset.value();
    } else {
      // we are inserting text into a non-text node.  first we have to create a
      // textnode (this also populates it with the text)
      RefPtr<nsTextNode> newNode = aDoc->CreateTextNode(aStringToInsert);
      // then we insert it into the dom tree
      nsresult rv = InsertNode(*newNode, *node, offset);
      NS_ENSURE_SUCCESS(rv, rv);
      node = newNode;
      offset = lengthToInsert.value();
    }
  }

  *aInOutNode = node;
  *aInOutOffset = offset;
  return NS_OK;
}

nsresult
EditorBase::InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert,
                                       Text& aTextNode,
                                       int32_t aOffset,
                                       bool aSuppressIME)
{
  RefPtr<EditTransactionBase> transaction;
  bool isIMETransaction = false;
  RefPtr<Text> insertedTextNode = &aTextNode;
  int32_t insertedOffset = aOffset;
  // aSuppressIME is used when editor must insert text, yet this text is not
  // part of the current IME operation. Example: adjusting whitespace around an
  // IME insertion.
  if (ShouldHandleIMEComposition() && !aSuppressIME) {
    if (!mIMETextNode) {
      mIMETextNode = &aTextNode;
      mIMETextOffset = aOffset;
    }
    transaction = CreateTxnForComposition(aStringToInsert);
    isIMETransaction = true;
    // All characters of the composition string will be replaced with
    // aStringToInsert.  So, we need to emulate to remove the composition
    // string.
    insertedTextNode = mIMETextNode;
    insertedOffset = mIMETextOffset;
    mIMETextLength = aStringToInsert.Length();
  } else {
    transaction = CreateTxnForInsertText(aStringToInsert, aTextNode, aOffset);
  }

  // Let listeners know what's up
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillInsertText(
        static_cast<nsIDOMCharacterData*>(insertedTextNode->AsDOMNode()),
        insertedOffset, aStringToInsert);
    }
  }

  // XXX We may not need these view batches anymore.  This is handled at a
  // higher level now I believe.
  BeginUpdateViewBatch();
  nsresult rv = DoTransaction(transaction);
  EndUpdateViewBatch();

  // let listeners know what happened
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidInsertText(
        static_cast<nsIDOMCharacterData*>(insertedTextNode->AsDOMNode()),
        insertedOffset, aStringToInsert, rv);
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
  if (isIMETransaction && mIMETextNode) {
    uint32_t len = mIMETextNode->Length();
    if (!len) {
      DeleteNode(mIMETextNode);
      mIMETextNode = nullptr;
      static_cast<CompositionTransaction*>(transaction.get())->MarkFixed();
    }
  }

  return rv;
}

nsresult
EditorBase::SelectEntireDocument(Selection* aSelection)
{
  if (!aSelection) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(GetRoot());
  if (!rootElement) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return aSelection->SelectAllChildren(rootElement);
}

nsINode*
EditorBase::GetFirstEditableNode(nsINode* aRoot)
{
  MOZ_ASSERT(aRoot);

  nsIContent* node = GetLeftmostChild(aRoot);
  if (node && !IsEditable(node)) {
    node = GetNextNode(node, /* aEditableNode = */ true);
  }

  return (node != aRoot) ? node : nullptr;
}

nsresult
EditorBase::NotifyDocumentListeners(
              TDocumentListenerNotification aNotificationType)
{
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
      NS_NOTREACHED("Unknown notification");
  }

  return rv;
}

nsresult
EditorBase::SetTextImpl(const nsAString& aString, Text& aCharData)
{
  RefPtr<SetTextTransaction> transaction =
    CreateTxnForSetText(aString, aCharData);
  if (NS_WARN_IF(!transaction)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t length = aCharData.Length();

  AutoRules beginRulesSniffing(this, EditAction::setText,
                               nsIEditor::eNext);

  // Let listeners know what's up
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      if (length) {
        listener->WillDeleteText(
          static_cast<nsIDOMCharacterData*>(aCharData.AsDOMNode()), 0,
          length);
      }
      if (!aString.IsEmpty()) {
        listener->WillInsertText(
          static_cast<nsIDOMCharacterData*>(aCharData.AsDOMNode()), 0,
          aString);
      }
    }
  }

  nsresult rv = DoTransaction(transaction);

  // Let listeners know what happened
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      if (length) {
        listener->DidDeleteText(
          static_cast<nsIDOMCharacterData*>(aCharData.AsDOMNode()), 0,
          length, rv);
      }
      if (!aString.IsEmpty()) {
        listener->DidInsertText(
          static_cast<nsIDOMCharacterData*>(aCharData.AsDOMNode()), 0,
          aString, rv);
      }
    }
  }

  return rv;
}

already_AddRefed<InsertTextTransaction>
EditorBase::CreateTxnForInsertText(const nsAString& aStringToInsert,
                                   Text& aTextNode,
                                   int32_t aOffset)
{
  RefPtr<InsertTextTransaction> transaction =
    new InsertTextTransaction(aTextNode, aOffset, aStringToInsert, *this,
                              &mRangeUpdater);
  return transaction.forget();
}

already_AddRefed<SetTextTransaction>
EditorBase::CreateTxnForSetText(const nsAString& aString,
                                Text& aTextNode)
{
  RefPtr<SetTextTransaction> transaction =
    new SetTextTransaction(aTextNode, aString, *this, &mRangeUpdater);
  return transaction.forget();
}

nsresult
EditorBase::DeleteText(nsGenericDOMDataNode& aCharData,
                       uint32_t aOffset,
                       uint32_t aLength)
{
  RefPtr<DeleteTextTransaction> transaction =
    CreateTxnForDeleteText(aCharData, aOffset, aLength);
  NS_ENSURE_STATE(transaction);

  AutoRules beginRulesSniffing(this, EditAction::deleteText,
                               nsIEditor::ePrevious);

  // Let listeners know what's up
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->WillDeleteText(
          static_cast<nsIDOMCharacterData*>(GetAsDOMNode(&aCharData)), aOffset,
          aLength);
    }
  }

  nsresult rv = DoTransaction(transaction);

  // Let listeners know what happened
  {
    AutoActionListenerArray listeners(mActionListeners);
    for (auto& listener : listeners) {
      listener->DidDeleteText(
          static_cast<nsIDOMCharacterData*>(GetAsDOMNode(&aCharData)), aOffset,
          aLength, rv);
    }
  }

  return rv;
}

already_AddRefed<DeleteTextTransaction>
EditorBase::CreateTxnForDeleteText(nsGenericDOMDataNode& aCharData,
                                   uint32_t aOffset,
                                   uint32_t aLength)
{
  RefPtr<DeleteTextTransaction> deleteTextTransaction =
    new DeleteTextTransaction(*this, aCharData, aOffset, aLength,
                              &mRangeUpdater);
  // If it's not editable, the transaction shouldn't be recorded since it
  // should never be undone/redone.
  if (NS_WARN_IF(!deleteTextTransaction->CanDoIt())) {
    return nullptr;
  }
  return deleteTextTransaction.forget();
}

already_AddRefed<SplitNodeTransaction>
EditorBase::CreateTxnForSplitNode(nsIContent& aNode,
                                  uint32_t aOffset)
{
  RefPtr<SplitNodeTransaction> transaction =
    new SplitNodeTransaction(*this, aNode, aOffset);
  return transaction.forget();
}

already_AddRefed<JoinNodeTransaction>
EditorBase::CreateTxnForJoinNode(nsINode& aLeftNode,
                                 nsINode& aRightNode)
{
  RefPtr<JoinNodeTransaction> joinNodeTransaction =
    new JoinNodeTransaction(*this, aLeftNode, aRightNode);
  // If it's not editable, the transaction shouldn't be recorded since it
  // should never be undone/redone.
  if (NS_WARN_IF(!joinNodeTransaction->CanDoIt())) {
    return nullptr;
  }
  return joinNodeTransaction.forget();
}

struct SavedRange final
{
  RefPtr<Selection> mSelection;
  nsCOMPtr<nsINode> mStartNode;
  nsCOMPtr<nsINode> mEndNode;
  int32_t mStartOffset;
  int32_t mEndOffset;
};

nsresult
EditorBase::SplitNodeImpl(nsIContent& aExistingRightNode,
                          int32_t aOffset,
                          nsIContent& aNewLeftNode)
{
  // Remember all selection points.
  AutoTArray<SavedRange, 10> savedRanges;
  for (size_t i = 0; i < kPresentSelectionTypeCount; ++i) {
    SelectionType selectionType(ToSelectionType(1 << i));
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
      range.mStartNode = r->GetStartParent();
      range.mStartOffset = r->StartOffset();
      range.mEndNode = r->GetEndParent();
      range.mEndOffset = r->EndOffset();

      savedRanges.AppendElement(range);
    }
  }

  nsCOMPtr<nsINode> parent = aExistingRightNode.GetParentNode();
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  ErrorResult rv;
  nsCOMPtr<nsINode> refNode = &aExistingRightNode;
  parent->InsertBefore(aNewLeftNode, refNode, rv);
  NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());

  // Split the children between the two nodes.  At this point,
  // aExistingRightNode has all the children.  Move all the children whose
  // index is < aOffset to aNewLeftNode.
  if (aOffset < 0) {
    // This means move no children
    return NS_OK;
  }

  // If it's a text node, just shuffle around some text
  if (aExistingRightNode.GetAsText() && aNewLeftNode.GetAsText()) {
    // Fix right node
    nsAutoString leftText;
    aExistingRightNode.GetAsText()->SubstringData(0, aOffset, leftText);
    aExistingRightNode.GetAsText()->DeleteData(0, aOffset);
    // Fix left node
    aNewLeftNode.GetAsText()->SetData(leftText);
  } else {
    // Otherwise it's an interior node, so shuffle around the children. Go
    // through list backwards so deletes don't interfere with the iteration.
    nsCOMPtr<nsINodeList> childNodes = aExistingRightNode.ChildNodes();
    for (int32_t i = aOffset - 1; i >= 0; i--) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i);
      if (childNode) {
        aExistingRightNode.RemoveChild(*childNode, rv);
        if (!rv.Failed()) {
          nsCOMPtr<nsIContent> firstChild = aNewLeftNode.GetFirstChild();
          aNewLeftNode.InsertBefore(*childNode, firstChild, rv);
        }
      }
      if (rv.Failed()) {
        break;
      }
    }
  }

  // Handle selection
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  if (ps) {
    ps->FlushPendingNotifications(FlushType::Frames);
  }

  bool shouldSetSelection = GetShouldTxnSetSelection();

  RefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // Adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      nsresult rv = range.mSelection->RemoveAllRanges();
      NS_ENSURE_SUCCESS(rv, rv);
      previousSelection = range.mSelection;
    }

    if (shouldSetSelection &&
        range.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Split the selection into existing node and new node.
    if (range.mStartNode == &aExistingRightNode) {
      if (range.mStartOffset < aOffset) {
        range.mStartNode = &aNewLeftNode;
      } else {
        range.mStartOffset -= aOffset;
      }
    }

    if (range.mEndNode == &aExistingRightNode) {
      if (range.mEndOffset < aOffset) {
        range.mEndNode = &aNewLeftNode;
      } else {
        range.mEndOffset -= aOffset;
      }
    }

    RefPtr<nsRange> newRange;
    nsresult rv = nsRange::CreateRange(range.mStartNode, range.mStartOffset,
                                       range.mEndNode, range.mEndOffset,
                                       getter_AddRefs(newRange));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = range.mSelection->AddRange(newRange);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (shouldSetSelection) {
    // Editor wants us to set selection at split point.
    RefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    selection->Collapse(&aNewLeftNode, aOffset);
  }

  return NS_OK;
}

nsresult
EditorBase::JoinNodesImpl(nsINode* aNodeToKeep,
                          nsINode* aNodeToJoin,
                          nsINode* aParent)
{
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
  for (size_t i = 0; i < kPresentSelectionTypeCount; ++i) {
    SelectionType selectionType(ToSelectionType(1 << i));
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
      range.mStartNode = r->GetStartParent();
      range.mStartOffset = r->StartOffset();
      range.mEndNode = r->GetEndParent();
      range.mEndOffset = r->EndOffset();

      // If selection endpoint is between the nodes, remember it as being
      // in the one that is going away instead.  This simplifies later selection
      // adjustment logic at end of this method.
      if (range.mStartNode) {
        if (range.mStartNode == parent &&
            joinOffset < range.mStartOffset &&
            range.mStartOffset <= keepOffset) {
          range.mStartNode = aNodeToJoin;
          range.mStartOffset = firstNodeLength;
        }
        if (range.mEndNode == parent &&
            joinOffset < range.mEndOffset &&
            range.mEndOffset <= keepOffset) {
          range.mEndNode = aNodeToJoin;
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
    aNodeToKeep->GetAsText()->SetData(leftText);
  } else {
    // Otherwise it's an interior node, so shuffle around the children.
    nsCOMPtr<nsINodeList> childNodes = aNodeToJoin->ChildNodes();
    MOZ_ASSERT(childNodes);

    // Remember the first child in aNodeToKeep, we'll insert all the children of aNodeToJoin in front of it
    // GetFirstChild returns nullptr firstNode if aNodeToKeep has no children, that's OK.
    nsCOMPtr<nsIContent> firstNode = aNodeToKeep->GetFirstChild();

    // Have to go through the list backwards to keep deletes from interfering with iteration.
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

  bool shouldSetSelection = GetShouldTxnSetSelection();

  RefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // And adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      nsresult rv = range.mSelection->RemoveAllRanges();
      NS_ENSURE_SUCCESS(rv, rv);
      previousSelection = range.mSelection;
    }

    if (shouldSetSelection &&
        range.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Check to see if we joined nodes where selection starts.
    if (range.mStartNode == aNodeToJoin) {
      range.mStartNode = aNodeToKeep;
    } else if (range.mStartNode == aNodeToKeep) {
      range.mStartOffset += firstNodeLength;
    }

    // Check to see if we joined nodes where selection ends.
    if (range.mEndNode == aNodeToJoin) {
      range.mEndNode = aNodeToKeep;
    } else if (range.mEndNode == aNodeToKeep) {
      range.mEndOffset += firstNodeLength;
    }

    RefPtr<nsRange> newRange;
    nsresult rv = nsRange::CreateRange(range.mStartNode, range.mStartOffset,
                                       range.mEndNode, range.mEndOffset,
                                       getter_AddRefs(newRange));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = range.mSelection->AddRange(newRange);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (shouldSetSelection) {
    // Editor wants us to set selection at join point.
    RefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    selection->Collapse(aNodeToKeep, AssertedCast<int32_t>(firstNodeLength));
  }

  return err.StealNSResult();
}

int32_t
EditorBase::GetChildOffset(nsIDOMNode* aChild,
                           nsIDOMNode* aParent)
{
  MOZ_ASSERT(aChild && aParent);

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  nsCOMPtr<nsINode> child = do_QueryInterface(aChild);
  MOZ_ASSERT(parent && child);

  int32_t idx = parent->IndexOf(child);
  MOZ_ASSERT(idx != -1);
  return idx;
}

// static
already_AddRefed<nsIDOMNode>
EditorBase::GetNodeLocation(nsIDOMNode* aChild,
                            int32_t* outOffset)
{
  MOZ_ASSERT(aChild && outOffset);
  NS_ENSURE_TRUE(aChild && outOffset, nullptr);
  *outOffset = -1;

  nsCOMPtr<nsIDOMNode> parent;

  MOZ_ALWAYS_SUCCEEDS(aChild->GetParentNode(getter_AddRefs(parent)));
  if (parent) {
    *outOffset = GetChildOffset(aChild, parent);
  }

  return parent.forget();
}

nsINode*
EditorBase::GetNodeLocation(nsINode* aChild,
                            int32_t* aOffset)
{
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(aOffset);

  nsINode* parent = aChild->GetParentNode();
  if (parent) {
    *aOffset = parent->IndexOf(aChild);
    MOZ_ASSERT(*aOffset != -1);
  } else {
    *aOffset = -1;
  }
  return parent;
}

/**
 * Returns the number of things inside aNode.  If aNode is text, returns number
 * of characters. If not, returns number of children nodes.
 */
nsresult
EditorBase::GetLengthOfDOMNode(nsIDOMNode* aNode,
                               uint32_t& aCount)
{
  aCount = 0;
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  aCount = node->Length();
  return NS_OK;
}

nsIContent*
EditorBase::GetPriorNode(nsINode* aParentNode,
                         int32_t aOffset,
                         bool aEditableNode,
                         bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // If we are at the beginning of the node, or it is a text node, then just
  // look before it.
  if (!aOffset || aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    if (aNoBlockCrossing && IsBlockNode(aParentNode)) {
      // If we aren't allowed to cross blocks, don't look before this block.
      return nullptr;
    }
    return GetPriorNode(aParentNode, aEditableNode, aNoBlockCrossing);
  }

  // else look before the child at 'aOffset'
  if (nsIContent* child = aParentNode->GetChildAt(aOffset)) {
    return GetPriorNode(child, aEditableNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the deep-right child.
  nsIContent* resultNode = GetRightmostChild(aParentNode, aNoBlockCrossing);
  if (!resultNode || !aEditableNode || IsEditable(resultNode)) {
    return resultNode;
  }

  // restart the search from the non-editable node we just found
  return GetPriorNode(resultNode, aEditableNode, aNoBlockCrossing);
}

nsIContent*
EditorBase::GetNextNode(nsINode* aParentNode,
                        int32_t aOffset,
                        bool aEditableNode,
                        bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // if aParentNode is a text node, use its location instead
  if (aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    nsINode* parent = aParentNode->GetParentNode();
    NS_ENSURE_TRUE(parent, nullptr);
    aOffset = parent->IndexOf(aParentNode) + 1; // _after_ the text node
    aParentNode = parent;
  }

  // look at the child at 'aOffset'
  nsIContent* child = aParentNode->GetChildAt(aOffset);
  if (child) {
    if (aNoBlockCrossing && IsBlockNode(child)) {
      return child;
    }

    nsIContent* resultNode = GetLeftmostChild(child, aNoBlockCrossing);
    if (!resultNode) {
      return child;
    }

    if (!IsDescendantOfEditorRoot(resultNode)) {
      return nullptr;
    }

    if (!aEditableNode || IsEditable(resultNode)) {
      return resultNode;
    }

    // restart the search from the non-editable node we just found
    return GetNextNode(resultNode, aEditableNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  if (aNoBlockCrossing && IsBlockNode(aParentNode)) {
    // don't cross out of parent block
    return nullptr;
  }

  return GetNextNode(aParentNode, aEditableNode, aNoBlockCrossing);
}

nsIContent*
EditorBase::GetPriorNode(nsINode* aCurrentNode,
                         bool aEditableNode,
                         bool aNoBlockCrossing /* = false */)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nullptr;
  }

  return FindNode(aCurrentNode, false, aEditableNode, aNoBlockCrossing);
}

nsIContent*
EditorBase::FindNextLeafNode(nsINode* aCurrentNode,
                             bool aGoForward,
                             bool bNoBlockCrossing)
{
  // called only by GetPriorNode so we don't need to check params.
  NS_PRECONDITION(IsDescendantOfEditorRoot(aCurrentNode) &&
                  !IsEditorRoot(aCurrentNode),
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
      nsIContent *leaf =
        aGoForward ? GetLeftmostChild(sibling, bNoBlockCrossing) :
                     GetRightmostChild(sibling, bNoBlockCrossing);
      if (!leaf) {
        return sibling;
      }

      return leaf;
    }

    nsINode *parent = cur->GetParentNode();
    if (!parent) {
      return nullptr;
    }

    NS_ASSERTION(IsDescendantOfEditorRoot(parent),
                 "We started with a proper descendant of root, and should stop "
                 "if we ever hit the root, so we better have a descendant of "
                 "root now!");
    if (IsEditorRoot(parent) ||
        (bNoBlockCrossing && IsBlockNode(parent))) {
      return nullptr;
    }

    cur = parent;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent*
EditorBase::GetNextNode(nsINode* aCurrentNode,
                        bool aEditableNode,
                        bool bNoBlockCrossing)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nullptr;
  }

  return FindNode(aCurrentNode, true, aEditableNode, bNoBlockCrossing);
}

nsIContent*
EditorBase::FindNode(nsINode* aCurrentNode,
                     bool aGoForward,
                     bool aEditableNode,
                     bool bNoBlockCrossing)
{
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

  if (!aEditableNode || IsEditable(candidate)) {
    return candidate;
  }

  return FindNode(candidate, aGoForward, aEditableNode, bNoBlockCrossing);
}

nsIContent*
EditorBase::GetRightmostChild(nsINode* aCurrentNode,
                              bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent *cur = aCurrentNode->GetLastChild();
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

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent*
EditorBase::GetLeftmostChild(nsINode* aCurrentNode,
                             bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent *cur = aCurrentNode->GetFirstChild();
  if (!cur) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && IsBlockNode(cur)) {
      return cur;
    }
    nsIContent *next = cur->GetFirstChild();
    if (!next) {
      return cur;
    }
    cur = next;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

bool
EditorBase::IsBlockNode(nsINode* aNode)
{
  // stub to be overridden in HTMLEditor.
  // screwing around with the class hierarchy here in order
  // to not duplicate the code in GetNextNode/GetPrevNode
  // across both EditorBase/HTMLEditor.
  return false;
}

bool
EditorBase::CanContain(nsINode& aParent,
                       nsIContent& aChild)
{
  switch (aParent.NodeType()) {
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      return TagCanContain(*aParent.NodeInfo()->NameAtom(), aChild);
  }
  return false;
}

bool
EditorBase::CanContainTag(nsINode& aParent,
                          nsIAtom& aChildTag)
{
  switch (aParent.NodeType()) {
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      return TagCanContainTag(*aParent.NodeInfo()->NameAtom(), aChildTag);
  }
  return false;
}

bool
EditorBase::TagCanContain(nsIAtom& aParentTag,
                          nsIContent& aChild)
{
  switch (aChild.NodeType()) {
    case nsIDOMNode::TEXT_NODE:
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      return TagCanContainTag(aParentTag, *aChild.NodeInfo()->NameAtom());
  }
  return false;
}

bool
EditorBase::TagCanContainTag(nsIAtom& aParentTag,
                             nsIAtom& aChildTag)
{
  return true;
}

bool
EditorBase::IsRoot(nsIDOMNode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetRoot());

  return inNode == rootNode;
}

bool
EditorBase::IsRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsINode> rootNode = GetRoot();

  return inNode == rootNode;
}

bool
EditorBase::IsEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsINode> rootNode = GetEditorRoot();
  return aNode == rootNode;
}

bool
EditorBase::IsDescendantOfRoot(nsIDOMNode* inNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  return IsDescendantOfRoot(node);
}

bool
EditorBase::IsDescendantOfRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);
  nsCOMPtr<nsIContent> root = GetRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(inNode, root);
}

bool
EditorBase::IsDescendantOfEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsIContent> root = GetEditorRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(aNode, root);
}

bool
EditorBase::IsContainer(nsINode* aNode)
{
  return aNode ? true : false;
}

bool
EditorBase::IsContainer(nsIDOMNode* aNode)
{
  return aNode ? true : false;
}

bool
EditorBase::IsEditable(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return IsEditable(content);
}

bool
EditorBase::IsEditable(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);

  if (!aNode->IsNodeOfType(nsINode::eCONTENT) || IsMozEditorBogusNode(aNode) ||
      !IsModifiableNode(aNode)) {
    return false;
  }

  switch (aNode->NodeType()) {
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::TEXT_NODE:
      return true;
    default:
      return false;
  }
}

bool
EditorBase::IsMozEditorBogusNode(nsINode* element)
{
  return element && element->IsElement() &&
         element->AsElement()->AttrValueIs(kNameSpaceID_None,
             kMOZEditorBogusNodeAttrAtom, kMOZEditorBogusNodeValue,
             eCaseMatters);
}

uint32_t
EditorBase::CountEditableChildren(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  uint32_t count = 0;
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (IsEditable(child)) {
      ++count;
    }
  }
  return count;
}

NS_IMETHODIMP
EditorBase::IncrementModificationCount(int32_t inNumMods)
{
  uint32_t oldModCount = mModCount;

  mModCount += inNumMods;

  if ((!oldModCount && mModCount) ||
      (oldModCount && !mModCount)) {
    NotifyDocumentListeners(eDocumentStateChanged);
  }
  return NS_OK;
}


NS_IMETHODIMP
EditorBase::GetModificationCount(int32_t* outModCount)
{
  NS_ENSURE_ARG_POINTER(outModCount);
  *outModCount = mModCount;
  return NS_OK;
}


NS_IMETHODIMP
EditorBase::ResetModificationCount()
{
  bool doNotify = (mModCount != 0);

  mModCount = 0;

  if (doNotify) {
    NotifyDocumentListeners(eDocumentStateChanged);
  }
  return NS_OK;
}

nsIAtom*
EditorBase::GetTag(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);

  if (!content) {
    NS_ASSERTION(aNode, "null node passed to EditorBase::GetTag()");
    return nullptr;
  }

  return content->NodeInfo()->NameAtom();
}

nsresult
EditorBase::GetTagString(nsIDOMNode* aNode,
                         nsAString& outString)
{
  if (!aNode) {
    NS_NOTREACHED("null node passed to EditorBase::GetTagString()");
    return NS_ERROR_NULL_POINTER;
  }

  nsIAtom *atom = GetTag(aNode);
  if (!atom) {
    return NS_ERROR_FAILURE;
  }

  atom->ToString(outString);
  return NS_OK;
}

bool
EditorBase::NodesSameType(nsIDOMNode* aNode1,
                          nsIDOMNode* aNode2)
{
  if (!aNode1 || !aNode2) {
    NS_NOTREACHED("null node passed to EditorBase::NodesSameType()");
    return false;
  }

  nsCOMPtr<nsIContent> content1 = do_QueryInterface(aNode1);
  NS_ENSURE_TRUE(content1, false);

  nsCOMPtr<nsIContent> content2 = do_QueryInterface(aNode2);
  NS_ENSURE_TRUE(content2, false);

  return AreNodesSameType(content1, content2);
}

bool
EditorBase::AreNodesSameType(nsIContent* aNode1,
                             nsIContent* aNode2)
{
  MOZ_ASSERT(aNode1);
  MOZ_ASSERT(aNode2);
  return aNode1->NodeInfo()->NameAtom() == aNode2->NodeInfo()->NameAtom();
}

bool
EditorBase::IsTextNode(nsIDOMNode* aNode)
{
  if (!aNode) {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return false;
  }

  uint16_t nodeType;
  aNode->GetNodeType(&nodeType);
  return (nodeType == nsIDOMNode::TEXT_NODE);
}

bool
EditorBase::IsTextNode(nsINode* aNode)
{
  return aNode->NodeType() == nsIDOMNode::TEXT_NODE;
}

nsCOMPtr<nsIDOMNode>
EditorBase::GetChildAt(nsIDOMNode* aParent, int32_t aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;

  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);

  NS_ENSURE_TRUE(parent, resultNode);

  resultNode = do_QueryInterface(parent->GetChildAt(aOffset));

  return resultNode;
}

/**
 * GetNodeAtRangeOffsetPoint() returns the node at this position in a range,
 * assuming that aParentOrNode is the node itself if it's a text node, or
 * the node's parent otherwise.
 */
nsIContent*
EditorBase::GetNodeAtRangeOffsetPoint(nsIDOMNode* aParentOrNode,
                                      int32_t aOffset)
{
  nsCOMPtr<nsINode> parentOrNode = do_QueryInterface(aParentOrNode);
  NS_ENSURE_TRUE(parentOrNode || !aParentOrNode, nullptr);
  if (parentOrNode->GetAsText()) {
    return parentOrNode->AsContent();
  }
  return parentOrNode->GetChildAt(aOffset);
}

/**
 * GetStartNodeAndOffset() returns whatever the start parent & offset is of
 * the first range in the selection.
 */
nsresult
EditorBase::GetStartNodeAndOffset(Selection* aSelection,
                                  nsIDOMNode** outStartNode,
                                  int32_t* outStartOffset)
{
  NS_ENSURE_TRUE(outStartNode && outStartOffset && aSelection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> startNode;
  nsresult rv = GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode),
                                      outStartOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (startNode) {
    NS_ADDREF(*outStartNode = startNode->AsDOMNode());
  } else {
    *outStartNode = nullptr;
  }
  return NS_OK;
}

nsresult
EditorBase::GetStartNodeAndOffset(Selection* aSelection,
                                  nsINode** aStartNode,
                                  int32_t* aStartOffset)
{
  MOZ_ASSERT(aSelection);
  MOZ_ASSERT(aStartNode);
  MOZ_ASSERT(aStartOffset);

  *aStartNode = nullptr;
  *aStartOffset = 0;

  if (!aSelection->RangeCount()) {
    return NS_ERROR_FAILURE;
  }

  const nsRange* range = aSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  NS_ENSURE_TRUE(range->IsPositioned(), NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aStartNode = range->GetStartParent());
  *aStartOffset = range->StartOffset();
  return NS_OK;
}

/**
 * GetEndNodeAndOffset() returns whatever the end parent & offset is of
 * the first range in the selection.
 */
nsresult
EditorBase::GetEndNodeAndOffset(Selection* aSelection,
                                nsIDOMNode** outEndNode,
                                int32_t* outEndOffset)
{
  NS_ENSURE_TRUE(outEndNode && outEndOffset && aSelection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> endNode;
  nsresult rv = GetEndNodeAndOffset(aSelection, getter_AddRefs(endNode),
                                    outEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (endNode) {
    NS_ADDREF(*outEndNode = endNode->AsDOMNode());
  } else {
    *outEndNode = nullptr;
  }
  return NS_OK;
}

nsresult
EditorBase::GetEndNodeAndOffset(Selection* aSelection,
                                nsINode** aEndNode,
                                int32_t* aEndOffset)
{
  MOZ_ASSERT(aSelection);
  MOZ_ASSERT(aEndNode);
  MOZ_ASSERT(aEndOffset);

  *aEndNode = nullptr;
  *aEndOffset = 0;

  NS_ENSURE_TRUE(aSelection->RangeCount(), NS_ERROR_FAILURE);

  const nsRange* range = aSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  NS_ENSURE_TRUE(range->IsPositioned(), NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aEndNode = range->GetEndParent());
  *aEndOffset = range->EndOffset();
  return NS_OK;
}

/**
 * IsPreformatted() checks the style info for the node for the preformatted
 * text style.
 */
nsresult
EditorBase::IsPreformatted(nsIDOMNode* aNode,
                           bool* aResult)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);

  NS_ENSURE_TRUE(aResult && content, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  // Look at the node (and its parent if it's not an element), and grab its style context
  RefPtr<nsStyleContext> elementStyle;
  if (!content->IsElement()) {
    content = content->GetParent();
  }
  if (content && content->IsElement()) {
    elementStyle =
      nsComputedDOMStyle::GetStyleContextNoFlush(content->AsElement(),
                                                 nullptr, ps);
  }

  if (!elementStyle) {
    // Consider nodes without a style context to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no style context).
    *aResult = false;
    return NS_OK;
  }

  const nsStyleText* styleText = elementStyle->StyleText();

  *aResult = styleText->WhiteSpaceIsSignificant();
  return NS_OK;
}


/**
 * This splits a node "deeply", splitting children as appropriate.  The place
 * to split is represented by a DOM point at {splitPointParent,
 * splitPointOffset}.  That DOM point must be inside aNode, which is the node
 * to split.  We return the offset in the parent of aNode where the split
 * terminates - where you would want to insert a new element, for instance, if
 * that's why you were splitting the node.
 *
 * -1 is returned on failure, in unlikely cases like the selection being
 * unavailable or cloning the node failing.  Make sure not to use the returned
 * offset for anything without checking that it's valid!  If you're not using
 * the offset, it's okay to ignore the return value.
 */
int32_t
EditorBase::SplitNodeDeep(nsIContent& aNode,
                          nsIContent& aSplitPointParent,
                          int32_t aSplitPointOffset,
                          EmptyContainers aEmptyContainers,
                          nsIContent** aOutLeftNode,
                          nsIContent** aOutRightNode)
{
  MOZ_ASSERT(&aSplitPointParent == &aNode ||
             EditorUtils::IsDescendantOf(&aSplitPointParent, &aNode));
  int32_t offset = aSplitPointOffset;

  nsCOMPtr<nsIContent> leftNode, rightNode;
  OwningNonNull<nsIContent> nodeToSplit = aSplitPointParent;
  while (true) {
    // Need to insert rules code call here to do things like not split a list
    // if you are after the last <li> or before the first, etc.  For now we
    // just have some smarts about unneccessarily splitting text nodes, which
    // should be universal enough to put straight in this EditorBase routine.

    bool didSplit = false;

    if ((aEmptyContainers == EmptyContainers::yes &&
         !nodeToSplit->GetAsText()) ||
        (offset && offset != (int32_t)nodeToSplit->Length())) {
      didSplit = true;
      ErrorResult rv;
      nsCOMPtr<nsIContent> newLeftNode = SplitNode(nodeToSplit, offset, rv);
      NS_ENSURE_TRUE(!NS_FAILED(rv.StealNSResult()), -1);

      rightNode = nodeToSplit;
      leftNode = newLeftNode;
    }

    NS_ENSURE_TRUE(nodeToSplit->GetParent(), -1);
    OwningNonNull<nsIContent> parentNode = *nodeToSplit->GetParent();

    if (!didSplit && offset) {
      // Must be "end of text node" case, we didn't split it, just move past it
      offset = parentNode->IndexOf(nodeToSplit) + 1;
      leftNode = nodeToSplit;
    } else {
      offset = parentNode->IndexOf(nodeToSplit);
      rightNode = nodeToSplit;
    }

    if (nodeToSplit == &aNode) {
      // we split all the way up to (and including) aNode; we're done
      break;
    }

    nodeToSplit = parentNode;
  }

  if (aOutLeftNode) {
    leftNode.forget(aOutLeftNode);
  }
  if (aOutRightNode) {
    rightNode.forget(aOutRightNode);
  }

  return offset;
}

/**
 * This joins two like nodes "deeply", joining children as appropriate.
 * Returns the point of the join, or (nullptr, -1) in case of error.
 */
EditorDOMPoint
EditorBase::JoinNodeDeep(nsIContent& aLeftNode,
                         nsIContent& aRightNode)
{
  // While the rightmost children and their descendants of the left node match
  // the leftmost children and their descendants of the right node, join them
  // up.

  nsCOMPtr<nsIContent> leftNodeToJoin = &aLeftNode;
  nsCOMPtr<nsIContent> rightNodeToJoin = &aRightNode;
  nsCOMPtr<nsINode> parentNode = aRightNode.GetParentNode();

  EditorDOMPoint ret;

  while (leftNodeToJoin && rightNodeToJoin && parentNode &&
         AreNodesSameType(leftNodeToJoin, rightNodeToJoin)) {
    uint32_t length = leftNodeToJoin->Length();

    ret.node = rightNodeToJoin;
    ret.offset = length;

    // Do the join
    nsresult rv = JoinNodes(*leftNodeToJoin, *rightNodeToJoin);
    NS_ENSURE_SUCCESS(rv, EditorDOMPoint());

    if (parentNode->GetAsText()) {
      // We've joined all the way down to text nodes, we're done!
      return ret;
    }

    // Get new left and right nodes, and begin anew
    parentNode = rightNodeToJoin;
    leftNodeToJoin = parentNode->GetChildAt(length - 1);
    rightNodeToJoin = parentNode->GetChildAt(length);

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

  return ret;
}

void
EditorBase::BeginUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount >= 0, "bad state");

  if (!mUpdateCount) {
    // Turn off selection updates and notifications.
    RefPtr<Selection> selection = GetSelection();
    if (selection) {
      selection->StartBatchChanges();
    }
  }

  mUpdateCount++;
}

nsresult
EditorBase::EndUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount > 0, "bad state");

  if (mUpdateCount <= 0) {
    mUpdateCount = 0;
    return NS_ERROR_FAILURE;
  }

  mUpdateCount--;

  if (!mUpdateCount) {
    // Turn selection updating and notifications back on.
    RefPtr<Selection> selection = GetSelection();
    if (selection) {
      selection->EndBatchChanges();
    }
  }

  return NS_OK;
}

bool
EditorBase::GetShouldTxnSetSelection()
{
  return mShouldTxnSetSelection;
}

NS_IMETHODIMP
EditorBase::DeleteSelectionImpl(EDirection aAction,
                                EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  RefPtr<EditAggregateTransaction> deleteSelectionTransaction;
  nsCOMPtr<nsINode> deleteNode;
  int32_t deleteCharOffset = 0, deleteCharLength = 0;
  if (!selection->Collapsed() || aAction != eNone) {
    deleteSelectionTransaction =
      CreateTxnForDeleteSelection(aAction,
                                  getter_AddRefs(deleteNode),
                                  &deleteCharOffset,
                                  &deleteCharLength);
    if (NS_WARN_IF(!deleteSelectionTransaction)) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIDOMCharacterData> deleteCharData(do_QueryInterface(deleteNode));
  AutoRules beginRulesSniffing(this, EditAction::deleteSelection, aAction);
  // Notify nsIEditActionListener::WillDelete[Selection|Text|Node]
  {
    AutoActionListenerArray listeners(mActionListeners);
    if (!deleteNode) {
      for (auto& listener : listeners) {
        listener->WillDeleteSelection(selection);
      }
    } else if (deleteCharData) {
      for (auto& listener : listeners) {
        listener->WillDeleteText(deleteCharData, deleteCharOffset, 1);
      }
    } else {
      for (auto& listener : listeners) {
        listener->WillDeleteNode(deleteNode->AsDOMNode());
      }
    }
  }

  // Delete the specified amount
  nsresult rv = DoTransaction(deleteSelectionTransaction);

  // Notify nsIEditActionListener::DidDelete[Selection|Text|Node]
  {
    AutoActionListenerArray listeners(mActionListeners);
    if (!deleteNode) {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteSelection(selection);
      }
    } else if (deleteCharData) {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteText(deleteCharData, deleteCharOffset, 1, rv);
      }
    } else {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteNode(deleteNode->AsDOMNode(), rv);
      }
    }
  }

  return rv;
}

already_AddRefed<Element>
EditorBase::DeleteSelectionAndCreateElement(nsIAtom& aTag)
{
  nsresult rv = DeleteSelectionAndPrepareToCreateNode();
  NS_ENSURE_SUCCESS(rv, nullptr);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, nullptr);

  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  uint32_t offset = selection->AnchorOffset();

  nsCOMPtr<Element> newElement = CreateNode(&aTag, node, offset);

  // We want the selection to be just after the new node
  rv = selection->Collapse(node, offset + 1);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return newElement.forget();
}

TextComposition*
EditorBase::GetComposition() const
{
  return mComposition;
}

bool
EditorBase::IsIMEComposing() const
{
  return mComposition && mComposition->IsComposing();
}

bool
EditorBase::ShouldHandleIMEComposition() const
{
  // When the editor is being reframed, the old value may be restored with
  // InsertText().  In this time, the text should be inserted as not a part
  // of the composition.
  return mComposition && mDidPostCreate;
}

nsresult
EditorBase::DeleteSelectionAndPrepareToCreateNode()
{
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  MOZ_ASSERT(selection->GetAnchorFocusRange());

  if (!selection->GetAnchorFocusRange()->Collapsed()) {
    nsresult rv = DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(selection->GetAnchorFocusRange() &&
               selection->GetAnchorFocusRange()->Collapsed(),
               "Selection not collapsed after delete");
  }

  // If the selection is a chardata node, split it if necessary and compute
  // where to put the new node
  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  MOZ_ASSERT(node, "Selection has no ranges in it");

  if (node && node->IsNodeOfType(nsINode::eDATA_NODE)) {
    NS_ASSERTION(node->GetParentNode(),
                 "It's impossible to insert into chardata with no parent -- "
                 "fix the caller");
    NS_ENSURE_STATE(node->GetParentNode());

    uint32_t offset = selection->AnchorOffset();

    if (!offset) {
      nsresult rv = selection->Collapse(node->GetParentNode(),
                                        node->GetParentNode()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (offset == node->Length()) {
      nsresult rv =
        selection->Collapse(node->GetParentNode(),
                            node->GetParentNode()->IndexOf(node) + 1);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      nsCOMPtr<nsIDOMNode> tmp;
      nsresult rv = SplitNode(node->AsDOMNode(), offset, getter_AddRefs(tmp));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = selection->Collapse(node->GetParentNode(),
                               node->GetParentNode()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

void
EditorBase::DoAfterDoTransaction(nsITransaction* aTxn)
{
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

void
EditorBase::DoAfterUndoTransaction()
{
  // all undoable transactions are non-transient
  MOZ_ALWAYS_SUCCEEDS(IncrementModificationCount(-1));
}

void
EditorBase::DoAfterRedoTransaction()
{
  // all redoable transactions are non-transient
  MOZ_ALWAYS_SUCCEEDS(IncrementModificationCount(1));
}

already_AddRefed<ChangeAttributeTransaction>
EditorBase::CreateTxnForSetAttribute(Element& aElement,
                                     nsIAtom& aAttribute,
                                     const nsAString& aValue)
{
  RefPtr<ChangeAttributeTransaction> transaction =
    new ChangeAttributeTransaction(aElement, aAttribute, &aValue);

  return transaction.forget();
}

already_AddRefed<ChangeAttributeTransaction>
EditorBase::CreateTxnForRemoveAttribute(Element& aElement,
                                        nsIAtom& aAttribute)
{
  RefPtr<ChangeAttributeTransaction> transaction =
    new ChangeAttributeTransaction(aElement, aAttribute, nullptr);

  return transaction.forget();
}

already_AddRefed<CreateElementTransaction>
EditorBase::CreateTxnForCreateElement(nsIAtom& aTag,
                                      nsINode& aParent,
                                      int32_t aPosition)
{
  RefPtr<CreateElementTransaction> transaction =
    new CreateElementTransaction(*this, aTag, aParent, aPosition);

  return transaction.forget();
}


already_AddRefed<InsertNodeTransaction>
EditorBase::CreateTxnForInsertNode(nsIContent& aNode,
                                   nsINode& aParent,
                                   int32_t aPosition)
{
  RefPtr<InsertNodeTransaction> transaction =
    new InsertNodeTransaction(aNode, aParent, aPosition, *this);
  return transaction.forget();
}

already_AddRefed<DeleteNodeTransaction>
EditorBase::CreateTxnForDeleteNode(nsINode* aNode)
{
  if (NS_WARN_IF(!aNode)) {
    return nullptr;
  }

  RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
    new DeleteNodeTransaction(*this, *aNode, &mRangeUpdater);
  // This should be OK because if currently it cannot delete the node,
  // it should never be able to undo/redo.
  if (!deleteNodeTransaction->CanDoIt()) {
    return nullptr;
  }
  return deleteNodeTransaction.forget();
}

already_AddRefed<CompositionTransaction>
EditorBase::CreateTxnForComposition(const nsAString& aStringToInsert)
{
  MOZ_ASSERT(mIMETextNode);
  // During handling IME composition, mComposition must have been initialized.
  // TODO: We can simplify CompositionTransaction::Init() with TextComposition
  //       class.
  RefPtr<CompositionTransaction> transaction =
    new CompositionTransaction(*mIMETextNode, mIMETextOffset, mIMETextLength,
                               mComposition->GetRanges(), aStringToInsert,
                               *this, &mRangeUpdater);
  return transaction.forget();
}

already_AddRefed<AddStyleSheetTransaction>
EditorBase::CreateTxnForAddStyleSheet(StyleSheet* aSheet)
{
  RefPtr<AddStyleSheetTransaction> transaction =
    new AddStyleSheetTransaction(*this, aSheet);

  return transaction.forget();
}

already_AddRefed<RemoveStyleSheetTransaction>
EditorBase::CreateTxnForRemoveStyleSheet(StyleSheet* aSheet)
{
  RefPtr<RemoveStyleSheetTransaction> transaction =
    new RemoveStyleSheetTransaction(*this, aSheet);

  return transaction.forget();
}

already_AddRefed<EditAggregateTransaction>
EditorBase::CreateTxnForDeleteSelection(EDirection aAction,
                                        nsINode** aRemovingNode,
                                        int32_t* aOffset,
                                        int32_t* aLength)
{
  RefPtr<Selection> selection = GetSelection();
  if (NS_WARN_IF(!selection)) {
    return nullptr;
  }

  // Check whether the selection is collapsed and we should do nothing:
  if (NS_WARN_IF(selection->Collapsed() && aAction == eNone)) {
    return nullptr;
  }

  // allocate the out-param transaction
  RefPtr<EditAggregateTransaction> aggregateTransaction =
    new EditAggregateTransaction();

  for (uint32_t rangeIdx = 0; rangeIdx < selection->RangeCount(); ++rangeIdx) {
    RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
    if (NS_WARN_IF(!range)) {
      return nullptr;
    }

    // Same with range as with selection; if it is collapsed and action
    // is eNone, do nothing.
    if (!range->Collapsed()) {
      RefPtr<DeleteRangeTransaction> deleteRangeTransaction =
        new DeleteRangeTransaction(*this, *range, &mRangeUpdater);
      // XXX Oh, not checking if deleteRangeTransaction can modify the range...
      aggregateTransaction->AppendChild(deleteRangeTransaction);
    } else if (aAction != eNone) {
      // we have an insertion point.  delete the thing in front of it or
      // behind it, depending on aAction
      // XXX Odd, when there are two or more ranges, this returns the last
      //     range information with aRemovingNode, aOffset and aLength.
      RefPtr<EditTransactionBase> deleteRangeTransaction =
        CreateTxnForDeleteRange(range, aAction,
                                aRemovingNode, aOffset, aLength);
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

already_AddRefed<DeleteTextTransaction>
EditorBase::CreateTxnForDeleteCharacter(nsGenericDOMDataNode& aData,
                                        uint32_t aOffset,
                                        EDirection aDirection)
{
  NS_ASSERTION(aDirection == eNext || aDirection == ePrevious,
               "Invalid direction");
  nsAutoString data;
  aData.GetData(data);
  NS_ASSERTION(data.Length(), "Trying to delete from a zero-length node");
  NS_ENSURE_TRUE(data.Length(), nullptr);

  uint32_t segOffset = aOffset, segLength = 1;
  if (aDirection == eNext) {
    if (segOffset + 1 < data.Length() &&
        NS_IS_HIGH_SURROGATE(data[segOffset]) &&
        NS_IS_LOW_SURROGATE(data[segOffset+1])) {
      // Delete both halves of the surrogate pair
      ++segLength;
    }
  } else if (aOffset > 0) {
    --segOffset;
    if (segOffset > 0 &&
      NS_IS_LOW_SURROGATE(data[segOffset]) &&
      NS_IS_HIGH_SURROGATE(data[segOffset-1])) {
      ++segLength;
      --segOffset;
    }
  } else {
    return nullptr;
  }
  return CreateTxnForDeleteText(aData, segOffset, segLength);
}

//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior
//are not implemented
already_AddRefed<EditTransactionBase>
EditorBase::CreateTxnForDeleteRange(nsRange* aRangeToDelete,
                                    EDirection aAction,
                                    nsINode** aRemovingNode,
                                    int32_t* aOffset,
                                    int32_t* aLength)
{
  MOZ_ASSERT(aAction != eNone);

  // get the node and offset of the insertion point
  nsCOMPtr<nsINode> node = aRangeToDelete->GetStartParent();
  if (NS_WARN_IF(!node)) {
    return nullptr;
  }

  int32_t offset = aRangeToDelete->StartOffset();

  // determine if the insertion point is at the beginning, middle, or end of
  // the node

  uint32_t count = node->Length();

  bool isFirst = !offset;
  bool isLast  = (count == (uint32_t)offset);

  // XXX: if isFirst && isLast, then we'll need to delete the node
  //      as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if (aAction == ePrevious && isFirst) {
    // we're backspacing from the beginning of the node.  Delete the first
    // thing to our left
    nsCOMPtr<nsIContent> priorNode = GetPriorNode(node, true);
    if (NS_WARN_IF(!priorNode)) {
      return nullptr;
    }

    // there is a priorNode, so delete its last child (if chardata, delete the
    // last char). if it has no children, delete it
    if (priorNode->IsNodeOfType(nsINode::eDATA_NODE)) {
      RefPtr<nsGenericDOMDataNode> priorNodeAsCharData =
        static_cast<nsGenericDOMDataNode*>(priorNode.get());
      uint32_t length = priorNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      if (NS_WARN_IF(!length)) {
        return nullptr;
      }
      RefPtr<DeleteTextTransaction> deleteTextTransaction =
        CreateTxnForDeleteCharacter(*priorNodeAsCharData, length, ePrevious);
      if (NS_WARN_IF(!deleteTextTransaction)) {
        return nullptr;
      }
      *aOffset = deleteTextTransaction->GetOffset();
      *aLength = deleteTextTransaction->GetNumCharsToDelete();
      priorNode.forget(aRemovingNode);
      return deleteTextTransaction.forget();
    }

    // priorNode is not chardata, so tell its parent to delete it
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      CreateTxnForDeleteNode(priorNode);
    if (NS_WARN_IF(!deleteNodeTransaction)) {
      return nullptr;
    }
    priorNode.forget(aRemovingNode);
    return deleteNodeTransaction.forget();
  }

  if (aAction == eNext && isLast) {
    // we're deleting from the end of the node.  Delete the first thing to our
    // right
    nsCOMPtr<nsIContent> nextNode = GetNextNode(node, true);
    if (NS_WARN_IF(!nextNode)) {
      return nullptr;
    }

    // there is a nextNode, so delete its first child (if chardata, delete the
    // first char). if it has no children, delete it
    if (nextNode->IsNodeOfType(nsINode::eDATA_NODE)) {
      RefPtr<nsGenericDOMDataNode> nextNodeAsCharData =
        static_cast<nsGenericDOMDataNode*>(nextNode.get());
      uint32_t length = nextNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      if (NS_WARN_IF(!length)) {
        return nullptr;
      }
      RefPtr<DeleteTextTransaction> deleteTextTransaction =
        CreateTxnForDeleteCharacter(*nextNodeAsCharData, 0, eNext);
      if (NS_WARN_IF(!deleteTextTransaction)) {
        return nullptr;
      }
      *aOffset = deleteTextTransaction->GetOffset();
      *aLength = deleteTextTransaction->GetNumCharsToDelete();
      nextNode.forget(aRemovingNode);
      return deleteTextTransaction.forget();
    }

    // nextNode is not chardata, so tell its parent to delete it
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      CreateTxnForDeleteNode(nextNode);
    if (NS_WARN_IF(!deleteNodeTransaction)) {
      return nullptr;
    }
    nextNode.forget(aRemovingNode);
    return deleteNodeTransaction.forget();
  }

  if (node->IsNodeOfType(nsINode::eDATA_NODE)) {
    RefPtr<nsGenericDOMDataNode> nodeAsCharData =
      static_cast<nsGenericDOMDataNode*>(node.get());
    // we have chardata, so delete a char at the proper offset
    RefPtr<DeleteTextTransaction> deleteTextTransaction =
      CreateTxnForDeleteCharacter(*nodeAsCharData, offset, aAction);
    if (NS_WARN_IF(!deleteTextTransaction)) {
      return nullptr;
    }
    *aOffset = deleteTextTransaction->GetOffset();
    *aLength = deleteTextTransaction->GetNumCharsToDelete();
    node.forget(aRemovingNode);
    return deleteTextTransaction.forget();
  }

  // we're either deleting a node or chardata, need to dig into the next/prev
  // node to find out
  nsCOMPtr<nsINode> selectedNode;
  if (aAction == ePrevious) {
    selectedNode = GetPriorNode(node, offset, true);
  } else if (aAction == eNext) {
    selectedNode = GetNextNode(node, offset, true);
  }

  while (selectedNode &&
         selectedNode->IsNodeOfType(nsINode::eDATA_NODE) &&
         !selectedNode->Length()) {
    // Can't delete an empty chardata node (bug 762183)
    if (aAction == ePrevious) {
      selectedNode = GetPriorNode(selectedNode, true);
    } else if (aAction == eNext) {
      selectedNode = GetNextNode(selectedNode, true);
    }
  }

  if (NS_WARN_IF(!selectedNode)) {
    return nullptr;
  }

  if (selectedNode->IsNodeOfType(nsINode::eDATA_NODE)) {
    RefPtr<nsGenericDOMDataNode> selectedNodeAsCharData =
      static_cast<nsGenericDOMDataNode*>(selectedNode.get());
    // we are deleting from a chardata node, so do a character deletion
    uint32_t position = 0;
    if (aAction == ePrevious) {
      position = selectedNode->Length();
    }
    RefPtr<DeleteTextTransaction> deleteTextTransaction =
      CreateTxnForDeleteCharacter(*selectedNodeAsCharData, position,
                                  aAction);
    if (NS_WARN_IF(!deleteTextTransaction)) {
      return nullptr;
    }
    *aOffset = deleteTextTransaction->GetOffset();
    *aLength = deleteTextTransaction->GetNumCharsToDelete();
    selectedNode.forget(aRemovingNode);
    return deleteTextTransaction.forget();
  }

  RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
    CreateTxnForDeleteNode(selectedNode);
  if (NS_WARN_IF(!deleteNodeTransaction)) {
    return nullptr;
  }
  selectedNode.forget(aRemovingNode);
  return deleteNodeTransaction.forget();
}

nsresult
EditorBase::CreateRange(nsIDOMNode* aStartParent,
                        int32_t aStartOffset,
                        nsIDOMNode* aEndParent,
                        int32_t aEndOffset,
                        nsRange** aRange)
{
  return nsRange::CreateRange(aStartParent, aStartOffset, aEndParent,
                              aEndOffset, aRange);
}

nsresult
EditorBase::AppendNodeToSelectionAsRange(nsIDOMNode* aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNode> parentNode;
  nsresult rv = aNode->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);

  int32_t offset = GetChildOffset(aNode, parentNode);

  RefPtr<nsRange> range;
  rv = CreateRange(parentNode, offset, parentNode, offset + 1,
                   getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

  return selection->AddRange(range);
}

nsresult
EditorBase::ClearSelection()
{
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  return selection->RemoveAllRanges();
}

already_AddRefed<Element>
EditorBase::CreateHTMLContent(nsIAtom* aTag)
{
  MOZ_ASSERT(aTag);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (!doc) {
    return nullptr;
  }

  // XXX Wallpaper over editor bug (editor tries to create elements with an
  //     empty nodename).
  if (aTag == nsGkAtoms::_empty) {
    NS_ERROR("Don't pass an empty tag to EditorBase::CreateHTMLContent, "
             "check caller.");
    return nullptr;
  }

  return doc->CreateElem(nsDependentAtomString(aTag), nullptr,
                         kNameSpaceID_XHTML);
}

NS_IMETHODIMP
EditorBase::SetAttributeOrEquivalent(nsIDOMElement* aElement,
                                     const nsAString& aAttribute,
                                     const nsAString& aValue,
                                     bool aSuppressTransaction)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCOMPtr<nsIAtom> attribute = NS_Atomize(aAttribute);
  return SetAttributeOrEquivalent(element, attribute, aValue,
                                  aSuppressTransaction);
}

NS_IMETHODIMP
EditorBase::RemoveAttributeOrEquivalent(nsIDOMElement* aElement,
                                        const nsAString& aAttribute,
                                        bool aSuppressTransaction)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCOMPtr<nsIAtom> attribute = NS_Atomize(aAttribute);
  return RemoveAttributeOrEquivalent(element, attribute, aSuppressTransaction);
}

nsresult
EditorBase::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent)
{
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
      aKeyboardEvent->PreventDefault(); // consumed
      return NS_OK;
    case NS_VK_BACK:
      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
      aKeyboardEvent->PreventDefault(); // consumed
      return NS_OK;
    case NS_VK_DELETE:
      // on certain platforms (such as windows) the shift key
      // modifies what delete does (cmd_cut in this case).
      // bailing here to allow the keybindings to do the cut.
      if (aKeyboardEvent->IsShift() || aKeyboardEvent->IsControl() ||
          aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
          aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
      aKeyboardEvent->PreventDefault(); // consumed
      return NS_OK;
  }
  return NS_OK;
}

nsresult
EditorBase::HandleInlineSpellCheck(EditAction action,
                                   Selection* aSelection,
                                   nsIDOMNode* previousSelectedNode,
                                   int32_t previousSelectedOffset,
                                   nsIDOMNode* aStartNode,
                                   int32_t aStartOffset,
                                   nsIDOMNode* aEndNode,
                                   int32_t aEndOffset)
{
  // Have to cast action here because this method is from an IDL
  return mInlineSpellChecker ? mInlineSpellChecker->SpellCheckAfterEditorChange(
                                 (int32_t)action, aSelection,
                                 previousSelectedNode, previousSelectedOffset,
                                 aStartNode, aStartOffset, aEndNode,
                                 aEndOffset)
                             : NS_OK;
}

already_AddRefed<nsIContent>
EditorBase::FindSelectionRoot(nsINode* aNode)
{
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  return rootContent.forget();
}

nsresult
EditorBase::InitializeSelection(nsIDOMEventTarget* aFocusEventTarget)
{
  nsCOMPtr<nsINode> targetNode = do_QueryInterface(aFocusEventTarget);
  NS_ENSURE_TRUE(targetNode, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsIContent> selectionRootContent = FindSelectionRoot(targetNode);
  if (!selectionRootContent) {
    return NS_OK;
  }

  bool isTargetDoc =
    targetNode->NodeType() == nsIDOMNode::DOCUMENT_NODE &&
    targetNode->HasFlag(NODE_IS_EDITABLE);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelectionController> selectionController =
    GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_FAILURE;
  }

  // Init the caret
  RefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE(caret, NS_ERROR_UNEXPECTED);
  caret->SetIgnoreUserModify(false);
  caret->SetSelection(selection);
  selectionController->SetCaretReadOnly(IsReadonly());
  selectionController->SetCaretEnabled(true);

  // Init selection
  selectionController->SetDisplaySelection(
                         nsISelectionController::SELECTION_ON);
  selectionController->SetSelectionFlags(
                         nsISelectionDisplay::DISPLAY_ALL);
  selectionController->RepaintSelection(
                         nsISelectionController::SELECTION_NORMAL);
  // If the computed selection root isn't root content, we should set it
  // as selection ancestor limit.  However, if that is root element, it means
  // there is not limitation of the selection, then, we must set nullptr.
  // NOTE: If we set a root element to the ancestor limit, some selection
  // methods don't work fine.
  if (selectionRootContent->GetParent()) {
    selection->SetAncestorLimiter(selectionRootContent);
  } else {
    selection->SetAncestorLimiter(nullptr);
  }

  // XXX What case needs this?
  if (isTargetDoc) {
    int32_t rangeCount;
    selection->GetRangeCount(&rangeCount);
    if (!rangeCount) {
      BeginningOfDocument();
    }
  }

  // If there is composition when this is called, we may need to restore IME
  // selection because if the editor is reframed, this already forgot IME
  // selection and the transaction.
  if (mComposition && !mIMETextNode && mIMETextLength) {
    // We need to look for the new mIMETextNode from current selection.
    // XXX If selection is changed during reframe, this doesn't work well!
    nsRange* firstRange = selection->GetRangeAt(0);
    NS_ENSURE_TRUE(firstRange, NS_ERROR_FAILURE);
    nsCOMPtr<nsINode> startNode = firstRange->GetStartParent();
    int32_t startOffset = firstRange->StartOffset();
    FindBetterInsertionPoint(startNode, startOffset);
    Text* textNode = startNode->GetAsText();
    MOZ_ASSERT(textNode,
               "There must be text node if mIMETextLength is larger than 0");
    if (textNode) {
      MOZ_ASSERT(textNode->Length() >= mIMETextOffset + mIMETextLength,
                 "The text node must be different from the old mIMETextNode");
      CompositionTransaction::SetIMESelection(*this, textNode, mIMETextOffset,
                                              mIMETextLength,
                                              mComposition->GetRanges());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorBase::FinalizeSelection()
{
  nsCOMPtr<nsISelectionController> selectionController =
    GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  selection->SetAncestorLimiter(nullptr);

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_INITIALIZED);

  selectionController->SetCaretEnabled(false);

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_ERROR_NOT_INITIALIZED);
  fm->UpdateCaretForCaretBrowsingMode();

  if (!HasIndependentSelection()) {
    // If this editor doesn't have an independent selection, i.e., it must
    // mean that it is an HTML editor, the selection controller is shared with
    // presShell.  So, even this editor loses focus, other part of the document
    // may still have focus.
    nsCOMPtr<nsIDocument> doc = GetDocument();
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
  } else if (IsFormWidget() || IsPasswordEditor() ||
             IsReadonly() || IsDisabled() || IsInputFiltered()) {
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

  selectionController->RepaintSelection(
                         nsISelectionController::SELECTION_NORMAL);
  return NS_OK;
}

Element*
EditorBase::GetRoot()
{
  if (!mRootElement) {
    // Let GetRootElement() do the work
    nsCOMPtr<nsIDOMElement> root;
    GetRootElement(getter_AddRefs(root));
  }

  return mRootElement;
}

Element*
EditorBase::GetEditorRoot()
{
  return GetRoot();
}

Element*
EditorBase::GetExposedRoot()
{
  Element* rootElement = GetRoot();

  // For plaintext editors, we need to ask the input/textarea element directly.
  if (rootElement && rootElement->IsRootOfNativeAnonymousSubtree()) {
    rootElement = rootElement->GetParent()->AsElement();
  }

  return rootElement;
}

nsresult
EditorBase::DetermineCurrentDirection()
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);

  // If we don't have an explicit direction, determine our direction
  // from the content's direction
  if (!(mFlags & (nsIPlaintextEditor::eEditorLeftToRight |
                  nsIPlaintextEditor::eEditorRightToLeft))) {
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

NS_IMETHODIMP
EditorBase::SwitchTextDirection()
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();

  nsresult rv = DetermineCurrentDirection();
  NS_ENSURE_SUCCESS(rv, rv);

  // Apply the opposite direction
  if (mFlags & nsIPlaintextEditor::eEditorRightToLeft) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorLeftToRight),
                 "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), true);
  } else if (mFlags & nsIPlaintextEditor::eEditorLeftToRight) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorRightToLeft),
                 "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), true);
  }

  if (NS_SUCCEEDED(rv)) {
    FireInputEvent();
  }

  return rv;
}

void
EditorBase::SwitchTextDirectionTo(uint32_t aDirection)
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();

  nsresult rv = DetermineCurrentDirection();
  NS_ENSURE_SUCCESS_VOID(rv);

  // Apply the requested direction
  if (aDirection == nsIPlaintextEditor::eEditorLeftToRight &&
      (mFlags & nsIPlaintextEditor::eEditorRightToLeft)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorLeftToRight),
                 "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), true);
  } else if (aDirection == nsIPlaintextEditor::eEditorRightToLeft &&
             (mFlags & nsIPlaintextEditor::eEditorLeftToRight)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorRightToLeft),
                 "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), true);
  }

  if (NS_SUCCEEDED(rv)) {
    FireInputEvent();
  }
}

#if DEBUG_JOE
void
EditorBase::DumpNode(nsIDOMNode* aNode,
                     int32_t indent)
{
  for (int32_t i = 0; i < indent; i++) {
    printf("  ");
  }

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMDocumentFragment> docfrag = do_QueryInterface(aNode);

  if (element || docfrag) {
    if (element) {
      nsAutoString tag;
      element->GetTagName(tag);
      printf("<%s>\n", NS_LossyConvertUTF16toASCII(tag).get());
    } else {
      printf("<document fragment>\n");
    }
    nsCOMPtr<nsIDOMNodeList> childList;
    aNode->GetChildNodes(getter_AddRefs(childList));
    NS_ENSURE_TRUE(childList, NS_ERROR_NULL_POINTER);
    uint32_t numChildren;
    childList->GetLength(&numChildren);
    nsCOMPtr<nsIDOMNode> child, tmp;
    aNode->GetFirstChild(getter_AddRefs(child));
    for (uint32_t i = 0; i < numChildren; i++) {
      DumpNode(child, indent + 1);
      child->GetNextSibling(getter_AddRefs(tmp));
      child = tmp;
    }
  } else if (IsTextNode(aNode)) {
    nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aNode);
    nsAutoString str;
    textNode->GetData(str);
    nsAutoCString cstr;
    LossyCopyUTF16toASCII(str, cstr);
    cstr.ReplaceChar('\n', ' ');
    printf("<textnode> %s\n", cstr.get());
  }
}
#endif

bool
EditorBase::IsModifiableNode(nsIDOMNode* aNode)
{
  return true;
}

bool
EditorBase::IsModifiableNode(nsINode* aNode)
{
  return true;
}

already_AddRefed<nsIContent>
EditorBase::GetFocusedContent()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return nullptr;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsIContent* content = fm->GetFocusedContent();
  MOZ_ASSERT((content == piTarget) == SameCOMIdentity(content, piTarget));

  return (content == piTarget) ?
    piTarget.forget().downcast<nsIContent>() : nullptr;
}

already_AddRefed<nsIContent>
EditorBase::GetFocusedContentForIME()
{
  return GetFocusedContent();
}

bool
EditorBase::IsActiveInDOMWindow()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return false;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }
  nsPIDOMWindowOuter* ourWindow = document->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> win;
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(ourWindow, false,
                                         getter_AddRefs(win));
  return SameCOMIdentity(content, piTarget);
}

bool
EditorBase::IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent)
{
  // If the event is trusted, the event should always cause input.
  if (NS_WARN_IF(!aGUIEvent)) {
    return false;
  }

  // If this is dispatched by using cordinates but this editor doesn't have
  // focus, we shouldn't handle it.
  if (aGUIEvent->IsUsingCoordinates()) {
    nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
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

void
EditorBase::OnFocus(nsIDOMEventTarget* aFocusEventTarget)
{
  InitializeSelection(aFocusEventTarget);
  mSpellCheckerDictionaryUpdated = false;
  if (mInlineSpellChecker && CanEnableSpellCheck()) {
    mInlineSpellChecker->UpdateCurrentDictionary();
    mSpellCheckerDictionaryUpdated = true;
  }
}

NS_IMETHODIMP
EditorBase::GetSuppressDispatchingInputEvent(bool* aSuppressed)
{
  NS_ENSURE_ARG_POINTER(aSuppressed);
  *aSuppressed = !mDispatchInputEvent;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::SetSuppressDispatchingInputEvent(bool aSuppress)
{
  mDispatchInputEvent = !aSuppress;
  return NS_OK;
}

NS_IMETHODIMP
EditorBase::GetIsInEditAction(bool* aIsInEditAction)
{
  MOZ_ASSERT(aIsInEditAction, "aIsInEditAction must not be null");
  *aIsInEditAction = mIsInEditAction;
  return NS_OK;
}

int32_t
EditorBase::GetIMESelectionStartOffsetIn(nsINode* aTextNode)
{
  MOZ_ASSERT(aTextNode, "aTextNode must not be nullptr");

  nsCOMPtr<nsISelectionController> selectionController =
    GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return -1;
  }

  int32_t minOffset = INT32_MAX;
  static const SelectionType kIMESelectionTypes[] = {
    SelectionType::eIMERawClause,
    SelectionType::eIMESelectedRawClause,
    SelectionType::eIMEConvertedClause,
    SelectionType::eIMESelectedClause
  };
  for (auto selectionType : kIMESelectionTypes) {
    RefPtr<Selection> selection = GetSelection(selectionType);
    if (!selection) {
      continue;
    }
    for (uint32_t i = 0; i < selection->RangeCount(); i++) {
      RefPtr<nsRange> range = selection->GetRangeAt(i);
      if (NS_WARN_IF(!range)) {
        continue;
      }
      if (NS_WARN_IF(range->GetStartParent() != aTextNode)) {
        // ignore the start offset...
      } else {
        MOZ_ASSERT(range->StartOffset() >= 0,
                   "start offset shouldn't be negative");
        minOffset = std::min(minOffset, range->StartOffset());
      }
      if (NS_WARN_IF(range->GetEndParent() != aTextNode)) {
        // ignore the end offset...
      } else {
        MOZ_ASSERT(range->EndOffset() >= 0,
                   "start offset shouldn't be negative");
        minOffset = std::min(minOffset, range->EndOffset());
      }
    }
  }
  return minOffset < INT32_MAX ? minOffset : -1;
}

void
EditorBase::HideCaret(bool aHide)
{
  if (mHidingCaret == aHide) {
    return;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE_VOID(presShell);
  RefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE_VOID(caret);

  mHidingCaret = aHide;
  if (aHide) {
    caret->AddForceHide();
  } else {
    caret->RemoveForceHide();
  }
}

} // namespace mozilla
