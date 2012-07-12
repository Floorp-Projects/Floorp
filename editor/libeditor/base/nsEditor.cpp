/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pratom.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIMEStateManager.h"
#include "nsFocusManager.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozISpellCheckingEngine.h"
#include "nsIEditorSpellCheck.h"
#include "mozInlineSpellChecker.h"

#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDocument.h"
#include "nsITransactionManager.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionController.h"
#include "nsIEnumerator.h"
#include "nsEditProperty.h"
#include "nsIAtom.h"
#include "nsCaret.h"
#include "nsIWidget.h"
#include "nsIPlaintextEditor.h"
#include "nsGUIEvent.h"

#include "nsIFrame.h"  // Needed by IME code

#include "nsCSSStyleSheet.h"

#include "nsIContent.h"
#include "nsDOMString.h"
#include "nsServiceManagerUtils.h"

// transactions the editor knows how to build
#include "EditAggregateTxn.h"
#include "PlaceholderTxn.h"
#include "ChangeAttributeTxn.h"
#include "CreateElementTxn.h"
#include "InsertElementTxn.h"
#include "DeleteNodeTxn.h"
#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"
#include "DeleteRangeTxn.h"
#include "SplitElementTxn.h"
#include "JoinElementTxn.h"
#include "nsStyleSheetTxns.h"
#include "IMETextTxn.h"
#include "nsString.h"

#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsEditorEventListener.h"
#include "nsISelectionDisplay.h"
#include "nsIInlineSpellChecker.h"
#include "nsINameSpaceManager.h"
#include "nsIParserService.h"

#include "nsITransferable.h"
#include "nsComputedDOMStyle.h"
#include "nsTextEditUtils.h"
#include "nsComputedDOMStyle.h"

#include "mozilla/FunctionTimer.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsCCUncollectableMarker.h"

#define NS_ERROR_EDITOR_NO_SELECTION NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,1)
#define NS_ERROR_EDITOR_NO_TEXTNODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,2)

#ifdef NS_DEBUG_EDITOR
static bool gNoisy = false;
#endif

#ifdef DEBUG
#include "nsIDOMHTMLDocument.h"
#endif

using namespace mozilla;
using namespace mozilla::widget;

// Defined in nsEditorRegistration.cpp
extern nsIParserService *sParserService;

//---------------------------------------------------------------------------
//
// nsEditor: base editor class implementation
//
//---------------------------------------------------------------------------

nsEditor::nsEditor()
:  mPlaceHolderName(nsnull)
,  mSelState(nsnull)
,  mPhonetic(nsnull)
,  mModCount(0)
,  mFlags(0)
,  mUpdateCount(0)
,  mPlaceHolderBatch(0)
,  mAction(kOpNone)
,  mHandlingActionCount(0)
,  mIMETextOffset(0)
,  mIMEBufferLength(0)
,  mDirection(eNone)
,  mDocDirtyState(-1)
,  mSpellcheckCheckboxState(eTriUnset)
,  mInIMEMode(false)
,  mIsIMEComposing(false)
,  mShouldTxnSetSelection(true)
,  mDidPreDestroy(false)
,  mDidPostCreate(false)
,  mHandlingTrustedAction(false)
,  mDispatchInputEvent(true)
{
}

nsEditor::~nsEditor()
{
  NS_ASSERTION(!mDocWeak || mDidPreDestroy, "Why PreDestroy hasn't been called?");

  mTxnMgr = nsnull;

  delete mPhonetic;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsEditor)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTxnMgr)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mIMETextRangeList)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEventListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsEditor)
 nsIDocument* currentDoc =
   tmp->mRootElement ? tmp->mRootElement->GetCurrentDoc() : nsnull;
 if (currentDoc &&
     nsCCUncollectableMarker::InGeneration(cb, currentDoc->GetMarkedCCGeneration())) {
   return NS_SUCCESS_INTERRUPTED_TRAVERSE;
 }
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mTxnMgr, nsITransactionManager)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mIMETextRangeList)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mEventListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsEditor)
 NS_INTERFACE_MAP_ENTRY(nsIPhonetic)
 NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
 NS_INTERFACE_MAP_ENTRY(nsIEditorIMESupport)
 NS_INTERFACE_MAP_ENTRY(nsIEditor)
 NS_INTERFACE_MAP_ENTRY(nsIObserver)
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEditor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEditor)


NS_IMETHODIMP
nsEditor::Init(nsIDOMDocument *aDoc, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags)
{
  NS_PRECONDITION(aDoc, "bad arg");
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;

  // First only set flags, but other stuff shouldn't be initialized now.
  // Don't move this call after initializing mDocWeak.
  // SetFlags() can check whether it's called during initialization or not by
  // them.  Note that SetFlags() will be called by PostCreate().
#ifdef DEBUG
  nsresult rv =
#endif
  SetFlags(aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetFlags() failed");

  mDocWeak = do_GetWeakReference(aDoc);  // weak reference to doc
  // HTML editors currently don't have their own selection controller,
  // so they'll pass null as aSelCon, and we'll get the selection controller
  // off of the presshell.
  nsCOMPtr<nsISelectionController> selCon;
  if (aSelCon) {
    mSelConWeak = do_GetWeakReference(aSelCon);   // weak reference to selectioncontroller
    selCon = aSelCon;
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selCon = do_QueryInterface(presShell);
  }
  NS_ASSERTION(selCon, "Selection controller should be available at this point");

  //set up root element if we are passed one.  
  if (aRoot)
    mRootElement = do_QueryInterface(aRoot);

  mUpdateCount=0;

  /* initialize IME stuff */
  mIMETextNode = nsnull;
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  
  /* Show the caret */
  selCon->SetCaretReadOnly(false);
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  selCon->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);//we want to see all the selection reflected to user

  NS_POSTCONDITION(mDocWeak, "bad state");

  // Make sure that the editor will be destroyed properly
  mDidPreDestroy = false;
  // Make sure that the ediotr will be created properly
  mDidPostCreate = false;

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::PostCreate()
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

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this,
                       SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION,
                       false);
    }
  }

  // update nsTextStateManager and caret if we have focus
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();
    NS_ASSERTION(ps, "no pres shell even though we have focus");
    NS_ENSURE_TRUE(ps, NS_ERROR_UNEXPECTED);
    nsPresContext* pc = ps->GetPresContext(); 

    nsIMEStateManager::OnTextStateBlur(pc, nsnull);
    nsIMEStateManager::OnTextStateFocus(pc, focusedContent);

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(focusedContent);
    if (target) {
      InitializeSelection(target);
    }

    // If the text control gets reframed during focus, Focus() would not be
    // called, so take a chance here to see if we need to spell check the text
    // control.
    nsEditorEventListener* listener =
      reinterpret_cast<nsEditorEventListener*> (mEventListener.get());
    listener->SpellCheckIfNeeded();
  }
  return NS_OK;
}

/* virtual */
void
nsEditor::CreateEventListeners()
{
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new nsEditorEventListener();
  }
}

nsresult
nsEditor::InstallEventListeners()
{
  NS_ENSURE_TRUE(mDocWeak && mEventListener,
                 NS_ERROR_NOT_INITIALIZED);

  // Initialize the event target.
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  NS_ENSURE_TRUE(rootContent, NS_ERROR_NOT_AVAILABLE);
  mEventTarget = do_QueryInterface(rootContent->GetParent());
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_AVAILABLE);

  nsEditorEventListener* listener =
    reinterpret_cast<nsEditorEventListener*>(mEventListener.get());
  return listener->Connect(this);
}

void
nsEditor::RemoveEventListeners()
{
  if (!mDocWeak || !mEventListener) {
    return;
  }
  reinterpret_cast<nsEditorEventListener*>(mEventListener.get())->Disconnect();
  mEventTarget = nsnull;
}

bool
nsEditor::GetDesiredSpellCheckState()
{
  // Check user override on this element
  if (mSpellcheckCheckboxState != eTriUnset) {
    return (mSpellcheckCheckboxState == eTriTrue);
  }

  // Check user preferences
  PRInt32 spellcheckLevel = Preferences::GetInt("layout.spellcheckDefault", 1);

  if (spellcheckLevel == 0) {
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
  nsCOMPtr<nsIContent> content = GetRoot();
  if (!content) {
    return false;
  }

  if (content->IsRootOfNativeAnonymousSubtree()) {
    content = content->GetParent();
  }

  nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(content);
  if (!element) {
    return false;
  }

  bool enable;
  element->GetSpellcheck(&enable);

  return enable;
}

NS_IMETHODIMP
nsEditor::PreDestroy(bool aDestroyingFrames)
{
  if (mDidPreDestroy)
    return NS_OK;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this,
                        SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION);
  }

  // Let spellchecker clean up its observers etc. It is important not to
  // actually free the spellchecker here, since the spellchecker could have
  // caused flush notifications, which could have gotten here if a textbox
  // is being removed. Setting the spellchecker to NULL could free the
  // object that is still in use! It will be freed when the editor is
  // destroyed.
  if (mInlineSpellChecker)
    mInlineSpellChecker->Cleanup(aDestroyingFrames);

  // tell our listeners that the doc is going away
  NotifyDocumentListeners(eDocumentToBeDestroyed);

  // Unregister event listeners
  RemoveEventListeners();
  mActionListeners.Clear();
  mEditorObservers.Clear();
  mDocStateListeners.Clear();
  mInlineSpellChecker = nsnull;
  mSpellcheckCheckboxState = eTriUnset;
  mRootElement = nsnull;

  mDidPreDestroy = true;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetFlags(PRUint32 *aFlags)
{
  *aFlags = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetFlags(PRUint32 aFlags)
{
  if (mFlags == aFlags) {
    return NS_OK;
  }

  bool spellcheckerWasEnabled = CanEnableSpellCheck();
  mFlags = aFlags;

  if (!mDocWeak) {
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

  // Might be changing editable state, so, we need to reset current IME state
  // if we're focused and the flag change causes IME state change.
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    IMEState newState;
    nsresult rv = GetPreferredIMEState(&newState);
    if (NS_SUCCEEDED(rv)) {
      // NOTE: When the enabled state isn't going to be modified, this method
      // is going to do nothing.
      nsIMEStateManager::UpdateIMEState(newState, focusedContent);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsSelectionEditable(bool *aIsSelectionEditable)
{
  NS_ENSURE_ARG_POINTER(aIsSelectionEditable);

  // get current selection
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // XXX we just check that the anchor node is editable at the moment
  //     we should check that all nodes in the selection are editable
  nsCOMPtr<nsIDOMNode> anchorNode;
  selection->GetAnchorNode(getter_AddRefs(anchorNode));
  *aIsSelectionEditable = anchorNode && IsEditable(anchorNode);

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsDocumentEditable(bool *aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);
  nsCOMPtr<nsIDOMDocument> doc = GetDOMDocument();
  *aIsDocumentEditable = !!doc;

  return NS_OK;
}

already_AddRefed<nsIDocument>
nsEditor::GetDocument()
{
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  return doc.forget();
}

already_AddRefed<nsIDOMDocument>
nsEditor::GetDOMDocument()
{
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  return doc.forget();
}

NS_IMETHODIMP 
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  nsCOMPtr<nsIDOMDocument> doc = GetDOMDocument();
  doc.forget(aDoc);
  return *aDoc ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

already_AddRefed<nsIPresShell>
nsEditor::GetPresShell()
{
  NS_PRECONDITION(mDocWeak, "bad state, null mDocWeak");
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NULL);
  nsCOMPtr<nsIPresShell> ps = doc->GetShell();
  return ps.forget();
}


/* attribute string contentsMIMEType; */
NS_IMETHODIMP
nsEditor::GetContentsMIMEType(char * *aContentsMIMEType)
{
  NS_ENSURE_ARG_POINTER(aContentsMIMEType);
  *aContentsMIMEType = ToNewCString(mContentMIMEType);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetContentsMIMEType(const char * aContentsMIMEType)
{
  mContentMIMEType.Assign(aContentsMIMEType ? aContentsMIMEType : "");
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetSelectionController(nsISelectionController **aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  *aSel = nsnull; // init out param
  nsCOMPtr<nsISelectionController> selCon;
  if (mSelConWeak) {
    selCon = do_QueryReferent(mSelConWeak);
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selCon = do_QueryInterface(presShell);
  }
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aSel = selCon);
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DeleteSelection(EDirection aAction, EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);
  return DeleteSelectionImpl(aAction, aStripWrappers);
}



NS_IMETHODIMP
nsEditor::GetSelection(nsISelection **aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  *aSelection = nsnull;
  nsCOMPtr<nsISelectionController> selcon;
  GetSelectionController(getter_AddRefs(selcon));
  NS_ENSURE_TRUE(selcon, NS_ERROR_NOT_INITIALIZED);
  return selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);  // does an addref
}

Selection*
nsEditor::GetSelection()
{
  nsCOMPtr<nsISelection> sel;
  nsresult res = GetSelection(getter_AddRefs(sel));
  NS_ENSURE_SUCCESS(res, nsnull);

  nsCOMPtr<nsISelectionPrivate> selPrivate = do_QueryInterface(sel);
  NS_ENSURE_TRUE(selPrivate, nsnull);

  nsRefPtr<nsFrameSelection> frameSel;
  res = selPrivate->GetFrameSelection(getter_AddRefs(frameSel));
  NS_ENSURE_SUCCESS(res, nsnull);

  return frameSel->GetSelection(nsISelectionController::SELECTION_NORMAL);
}

NS_IMETHODIMP
nsEditor::DoTransaction(nsITransaction* aTxn)
{
  if (mPlaceHolderBatch && !mPlaceHolderTxn) {
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = new PlaceholderTxn();

    // save off weak reference to placeholder txn
    mPlaceHolderTxn = do_GetWeakReference(plcTxn);
    plcTxn->Init(mPlaceHolderName, mSelState, this);
    // placeholder txn took ownership of this pointer
    mSelState = nsnull;

    // QI to an nsITransaction since that's what DoTransaction() expects
    nsCOMPtr<nsITransaction> theTxn = do_QueryInterface(plcTxn);
    // we will recurse, but will not hit this case in the nested call
    DoTransaction(theTxn);

    if (mTxnMgr) {
      nsCOMPtr<nsITransaction> topTxn = mTxnMgr->PeekUndoStack();
      if (topTxn) {
        plcTxn = do_QueryInterface(topTxn);
        if (plcTxn) {
          // there is a placeholder transaction on top of the undo stack.  It
          // is either the one we just created, or an earlier one that we are
          // now merging into.  From here on out remember this placeholder
          // instead of the one we just created.
          mPlaceHolderTxn = do_GetWeakReference(plcTxn);
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
    // XXX: its auto equivalent nsAutoUpdateViewBatch to ensure that
    // XXX: selection listeners have access to accurate frame data?
    // XXX:
    // XXX: Note that if we did add Begin/EndUpdateViewBatch() calls
    // XXX: we will need to make sure that they are disabled during
    // XXX: the init of the editor for text widgets to avoid layout
    // XXX: re-entry during initial reflow. - kin

    // get the selection and start a batch change
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    selection->StartBatchChanges();

    nsresult res;
    if (mTxnMgr) {
      res = mTxnMgr->DoTransaction(aTxn);
    } else {
      res = aTxn->DoTransaction();
    }
    if (NS_SUCCEEDED(res)) {
      DoAfterDoTransaction(aTxn);
    }

    // no need to check res here, don't lose result of operation
    selection->EndBatchChanges();

    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::EnableUndo(bool aEnable)
{
  if (aEnable) {
    if (!mTxnMgr) {
      mTxnMgr = new nsTransactionManager();
    }
  } else if (mTxnMgr) {
    // disable the transaction manager if it is enabled
    mTxnMgr->Clear();
    mTxnMgr->SetMaxTransactionCount(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetNumberOfUndoItems(PRInt32* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfUndoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
nsEditor::GetNumberOfRedoItems(PRInt32* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfRedoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
nsEditor::GetTransactionManager(nsITransactionManager* *aTxnManager)
{
  NS_ENSURE_ARG_POINTER(aTxnManager);
  
  *aTxnManager = NULL;
  NS_ENSURE_TRUE(mTxnMgr, NS_ERROR_FAILURE);

  NS_ADDREF(*aTxnManager = mTxnMgr);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetTransactionManager(nsITransactionManager *aTxnManager)
{
  NS_ENSURE_TRUE(aTxnManager, NS_ERROR_FAILURE);

  // nsITransactionManager is builtinclass, so this is safe
  mTxnMgr = static_cast<nsTransactionManager*>(aTxnManager);
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::Undo(PRUint32 aCount)
{
#ifdef NS_DEBUG_EDITOR
  if (gNoisy) { printf("Editor::Undo ----------\n"); }
#endif

  ForceCompositionEnd();

  bool hasTxnMgr, hasTransaction = false;
  CanUndo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  nsAutoRules beginRulesSniffing(this, kOpUndo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  for (PRUint32 i = 0; i < aCount; ++i) {
    nsresult rv = mTxnMgr->UndoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterUndoTransaction();
  }

  return NS_OK;
}


NS_IMETHODIMP nsEditor::CanUndo(bool *aIsEnabled, bool *aCanUndo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanUndo, NS_ERROR_NULL_POINTER);
  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    PRInt32 numTxns = 0;
    mTxnMgr->GetNumberOfUndoItems(&numTxns);
    *aCanUndo = !!numTxns;
  } else {
    *aCanUndo = false;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::Redo(PRUint32 aCount)
{
#ifdef NS_DEBUG_EDITOR
  if (gNoisy) { printf("Editor::Redo ----------\n"); }
#endif

  bool hasTxnMgr, hasTransaction = false;
  CanRedo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  nsAutoRules beginRulesSniffing(this, kOpRedo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  for (PRUint32 i = 0; i < aCount; ++i) {
    nsresult rv = mTxnMgr->RedoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterRedoTransaction();
  }

  return NS_OK;
}


NS_IMETHODIMP nsEditor::CanRedo(bool *aIsEnabled, bool *aCanRedo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanRedo, NS_ERROR_NULL_POINTER);

  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    PRInt32 numTxns = 0;
    mTxnMgr->GetNumberOfRedoItems(&numTxns);
    *aCanRedo = !!numTxns;
  } else {
    *aCanRedo = false;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::BeginTransaction()
{
  BeginUpdateViewBatch();

  if (mTxnMgr) {
    mTxnMgr->BeginBatch();
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndTransaction()
{
  if (mTxnMgr) {
    mTxnMgr->EndBatch();
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
nsEditor::BeginPlaceHolderTransaction(nsIAtom *aName)
{
  NS_PRECONDITION(mPlaceHolderBatch >= 0, "negative placeholder batch count!");
  if (!mPlaceHolderBatch)
  {
    // time to turn on the batch
    BeginUpdateViewBatch();
    mPlaceHolderTxn = nsnull;
    mPlaceHolderName = aName;
    nsCOMPtr<nsISelection> selection;
    nsresult res = GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(res)) {
      mSelState = new nsSelectionState();
      mSelState->SaveSelection(selection);
    }
  }
  mPlaceHolderBatch++;

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndPlaceHolderTransaction()
{
  NS_PRECONDITION(mPlaceHolderBatch > 0, "zero or negative placeholder batch count when ending batch!");
  if (mPlaceHolderBatch == 1)
  {
    nsCOMPtr<nsISelection>selection;
    GetSelection(getter_AddRefs(selection));

    nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(selection));

    // By making the assumption that no reflow happens during the calls
    // to EndUpdateViewBatch and ScrollSelectionIntoView, we are able to
    // allow the selection to cache a frame offset which is used by the
    // caret drawing code. We only enable this cache here; at other times,
    // we have no way to know whether reflow invalidates it
    // See bugs 35296 and 199412.
    if (selPrivate) {
      selPrivate->SetCanCacheFrameOffset(true);
    }

    {
      // Hide the caret here to avoid hiding it twice, once in EndUpdateViewBatch
      // and once in ScrollSelectionIntoView.
      nsRefPtr<nsCaret> caret;
      nsCOMPtr<nsIPresShell> presShell = GetPresShell();

      if (presShell)
        caret = presShell->GetCaret();

      StCaretHider caretHider(caret);

      // time to turn off the batch
      EndUpdateViewBatch();
      // make sure selection is in view

      // After ScrollSelectionIntoView(), the pending notifications might be
      // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
      ScrollSelectionIntoView(false);
    }

    // cached for frame offset are Not available now
    if (selPrivate) {
      selPrivate->SetCanCacheFrameOffset(false);
    }

    if (mSelState)
    {
      // we saved the selection state, but never got to hand it to placeholder 
      // (else we ould have nulled out this pointer), so destroy it to prevent leaks.
      delete mSelState;
      mSelState = nsnull;
    }
    if (mPlaceHolderTxn)  // we might have never made a placeholder if no action took place
    {
      nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryReferent(mPlaceHolderTxn);
      if (plcTxn) 
      {
        plcTxn->EndPlaceHolderBatch();
      }
      else  
      {
        // in the future we will check to make sure undo is off here,
        // since that is the only known case where the placeholdertxn would disappear on us.
        // For now just removing the assert.
      }
      // notify editor observers of action but if composing, it's done by
      // text event handler.
      if (!mInIMEMode) NotifyEditorObservers();
    }
  }
  mPlaceHolderBatch--;
  
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ShouldTxnSetSelection(bool *aResult)
{
  NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
  *aResult = mShouldTxnSetSelection;
  return NS_OK;
}

NS_IMETHODIMP  
nsEditor::SetShouldTxnSetSelection(bool aShould)
{
  mShouldTxnSetSelection = aShould;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentIsEmpty(bool *aDocumentIsEmpty)
{
  *aDocumentIsEmpty = true;

  dom::Element* root = GetRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER); 

  *aDocumentIsEmpty = !root->HasChildren();
  return NS_OK;
}


// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDocWeak) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsCOMPtr<nsISelectionController> selCon;
  GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsISelection> selection;
  nsresult result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    result = SelectEntireDocument(selection);
  }
  return result;
}

NS_IMETHODIMP nsEditor::BeginningOfDocument()
{
  if (!mDocWeak) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection
  nsCOMPtr<nsISelection> selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
    
  // get the root element 
  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER); 
  
  // find first editable thingy
  nsCOMPtr<nsINode> firstNode = GetFirstEditableNode(rootElement);
  if (!firstNode) {
    // just the root node, set selection to inside the root
    return selection->CollapseNative(rootElement, 0);
  }

  if (firstNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    // If firstNode is text, set selection to beginning of the text node.
    return selection->CollapseNative(firstNode, 0);
  }

  // Otherwise, it's a leaf node and we set the selection just in front of it.
  nsCOMPtr<nsIContent> parent = firstNode->GetParent();
  if (!parent) {
    return NS_ERROR_NULL_POINTER;
  }

  PRInt32 offsetInParent = parent->IndexOf(firstNode);
  return selection->CollapseNative(parent, offsetInParent);
}

NS_IMETHODIMP
nsEditor::EndOfDocument()
{ 
  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);

  // get selection 
  nsCOMPtr<nsISelection> selection; 
  nsresult rv = GetSelection(getter_AddRefs(selection)); 
  NS_ENSURE_SUCCESS(rv, rv); 
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER); 
  
  // get the root element 
  nsINode* node = GetRoot();
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER); 
  nsINode* child = node->GetLastChild();

  while (child && IsContainer(child->AsDOMNode())) {
    node = child;
    child = node->GetLastChild();
  }

  PRUint32 length = node->Length();
  return selection->CollapseNative(node, PRInt32(length));
} 
  
NS_IMETHODIMP
nsEditor::GetDocumentModified(bool *outDocModified)
{
  NS_ENSURE_TRUE(outDocModified, NS_ERROR_NULL_POINTER);

  PRInt32  modCount = 0;
  GetModificationCount(&modCount);

  *outDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentCharacterSet(nsACString &characterSet)
{
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  characterSet = doc->GetDocumentCharacterSet();
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetDocumentCharacterSet(const nsACString& characterSet)
{
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  doc->SetDocumentCharacterSet(characterSet);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::Cut()
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanCut(bool *aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::Copy()
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanCopy(bool *aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::Paste(PRInt32 aSelectionType)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::PasteTransferable(nsITransferable *aTransferable)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanPaste(PRInt32 aSelectionType, bool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP 
nsEditor::SetAttribute(nsIDOMElement *aElement, const nsAString & aAttribute, const nsAString & aValue)
{
  nsRefPtr<ChangeAttributeTxn> txn;
  nsresult result = CreateTxnForSetAttribute(aElement, aAttribute, aValue,
                                             getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::GetAttributeValue(nsIDOMElement *aElement, 
                            const nsAString & aAttribute, 
                            nsAString & aResultValue, 
                            bool *aResultIsSet)
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
nsEditor::RemoveAttribute(nsIDOMElement *aElement, const nsAString& aAttribute)
{
  nsRefPtr<ChangeAttributeTxn> txn;
  nsresult result = CreateTxnForRemoveAttribute(aElement, aAttribute,
                                                getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }
  return result;
}


bool
nsEditor::OutputsMozDirty()
{
  // Return true for Composer (!eEditorAllowInteraction) or mail
  // (eEditorMailMask), but false for webpages.
  return !(mFlags & nsIPlaintextEditor::eEditorAllowInteraction) ||
          (mFlags & nsIPlaintextEditor::eEditorMailMask);
}


NS_IMETHODIMP
nsEditor::MarkNodeDirty(nsIDOMNode* aNode)
{  
  // Mark the node dirty, but not for webpages (bug 599983)
  if (!OutputsMozDirty()) {
    return NS_OK;
  }
  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  if (element) {
    element->SetAttr(kNameSpaceID_None, nsEditProperty::mozdirty,
                     EmptyString(), false);
  }
  return NS_OK;
}

NS_IMETHODIMP nsEditor::GetInlineSpellChecker(bool autoCreate,
                                              nsIInlineSpellChecker ** aInlineSpellChecker)
{
  NS_ENSURE_ARG_POINTER(aInlineSpellChecker);

  if (mDidPreDestroy) {
    // Don't allow people to get or create the spell checker once the editor
    // is going away.
    *aInlineSpellChecker = nsnull;
    return autoCreate ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  // We don't want to show the spell checking UI if there are no spell check dictionaries available.
  bool canSpell = mozInlineSpellChecker::CanEnableInlineSpellChecking();
  if (!canSpell) {
    *aInlineSpellChecker = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (!mInlineSpellChecker && autoCreate) {
    mInlineSpellChecker = do_CreateInstance(MOZ_INLINESPELLCHECKER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mInlineSpellChecker) {
    rv = mInlineSpellChecker->Init(this);
    if (NS_FAILED(rv))
      mInlineSpellChecker = nsnull;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aInlineSpellChecker = mInlineSpellChecker);

  return NS_OK;
}

NS_IMETHODIMP nsEditor::Observe(nsISupports* aSubj, const char *aTopic,
                                const PRUnichar *aData)
{
  NS_ASSERTION(!strcmp(aTopic,
                       SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION),
               "Unexpected observer topic");

  // When mozInlineSpellChecker::CanEnableInlineSpellChecking changes
  SyncRealTimeSpell();

  // When nsIEditorSpellCheck::GetCurrentDictionary changes
  if (mInlineSpellChecker) {
    // if the current dictionary is no longer available, find another one
    nsCOMPtr<nsIEditorSpellCheck> editorSpellCheck;
    mInlineSpellChecker->GetSpellChecker(getter_AddRefs(editorSpellCheck));
    if (editorSpellCheck) {
      // Note: This might change the current dictionary, which may call
      // this observer recursively.
      editorSpellCheck->CheckCurrentDictionary();
    }

    // update the inline spell checker to reflect the new current dictionary
    mInlineSpellChecker->SpellCheckRange(nsnull); // causes recheck
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditor::SyncRealTimeSpell()
{
  NS_TIME_FUNCTION;

  bool enable = GetDesiredSpellCheckState();

  // Initializes mInlineSpellChecker
  nsCOMPtr<nsIInlineSpellChecker> spellChecker;
  GetInlineSpellChecker(enable, getter_AddRefs(spellChecker));

  if (mInlineSpellChecker) {
    // We might have a mInlineSpellChecker even if there are no dictionaries
    // available since we don't destroy the mInlineSpellChecker when the last
    // dictionariy is removed, but in that case spellChecker is null
    mInlineSpellChecker->SetEnableRealTimeSpell(enable && spellChecker);
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditor::SetSpellcheckUserOverride(bool enable)
{
  mSpellcheckCheckboxState = enable ? eTriTrue : eTriFalse;

  return SyncRealTimeSpell();
}

NS_IMETHODIMP nsEditor::CreateNode(const nsAString& aTag,
                                   nsIDOMNode *    aParent,
                                   PRInt32         aPosition,
                                   nsIDOMNode **   aNewNode)
{
  PRInt32 i;

  nsAutoRules beginRulesSniffing(this, kOpCreateNode, nsIEditor::eNext);
  
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillCreateNode(aTag, aParent, aPosition);

  nsRefPtr<CreateElementTxn> txn;
  nsresult result = CreateTxnForCreateElement(aTag, aParent, aPosition,
                                              getter_AddRefs(txn));
  if (NS_SUCCEEDED(result)) 
  {
    result = DoTransaction(txn);  
    if (NS_SUCCEEDED(result)) 
    {
      result = txn->GetNewNode(aNewNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "GetNewNode can't fail if txn::DoTransaction succeeded.");
    }
  }
  
  mRangeUpdater.SelAdjCreateNode(aParent, aPosition);
  
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidCreateNode(aTag, *aNewNode, aParent, aPosition, result);

  return result;
}


NS_IMETHODIMP nsEditor::InsertNode(nsIDOMNode * aNode,
                                   nsIDOMNode * aParent,
                                   PRInt32      aPosition)
{
  PRInt32 i;
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillInsertNode(aNode, aParent, aPosition);

  nsRefPtr<InsertElementTxn> txn;
  nsresult result = CreateTxnForInsertElement(aNode, aParent, aPosition,
                                              getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }

  mRangeUpdater.SelAdjInsertNode(aParent, aPosition);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidInsertNode(aNode, aParent, aPosition, result);

  return result;
}


NS_IMETHODIMP 
nsEditor::SplitNode(nsIDOMNode * aNode,
                    PRInt32      aOffset,
                    nsIDOMNode **aNewLeftNode)
{
  PRInt32 i;
  nsAutoRules beginRulesSniffing(this, kOpSplitNode, nsIEditor::eNext);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillSplitNode(aNode, aOffset);

  nsRefPtr<SplitElementTxn> txn;
  nsresult result = CreateTxnForSplitNode(aNode, aOffset, getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  
  {
    result = DoTransaction(txn);
    if (NS_SUCCEEDED(result))
    {
      result = txn->GetNewNode(aNewLeftNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "result must succeeded for GetNewNode");
    }
  }

  mRangeUpdater.SelAdjSplitNode(aNode, aOffset, *aNewLeftNode);

  for (i = 0; i < mActionListeners.Count(); i++)
  {
    nsIDOMNode *ptr = *aNewLeftNode;
    mActionListeners[i]->DidSplitNode(aNode, aOffset, ptr, result);
  }

  return result;
}


nsresult
nsEditor::JoinNodes(nsINode* aNodeToKeep, nsIContent* aNodeToMove)
{
  // We don't really need aNodeToMove's parent to be non-null -- we could just
  // skip adjusting any ranges in aNodeToMove's parent if there is none.  But
  // the current implementation requires it.
  MOZ_ASSERT(aNodeToKeep && aNodeToMove && aNodeToMove->GetNodeParent());
  nsresult res = JoinNodes(aNodeToKeep->AsDOMNode(), aNodeToMove->AsDOMNode(),
                           aNodeToMove->GetNodeParent()->AsDOMNode());
  NS_ASSERTION(NS_SUCCEEDED(res), "JoinNodes failed");
  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::JoinNodes(nsIDOMNode * aLeftNode,
                    nsIDOMNode * aRightNode,
                    nsIDOMNode * aParent)
{
  PRInt32 i;
  nsAutoRules beginRulesSniffing(this, kOpJoinNode, nsIEditor::ePrevious);

  // remember some values; later used for saved selection updating.
  // find the offset between the nodes to be joined.
  PRInt32 offset = GetChildOffset(aRightNode, aParent);
  // find the number of children of the lefthand node
  PRUint32 oldLeftNodeLen;
  nsresult result = GetLengthOfDOMNode(aLeftNode, oldLeftNodeLen);
  NS_ENSURE_SUCCESS(result, result);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillJoinNodes(aLeftNode, aRightNode, aParent);

  nsRefPtr<JoinElementTxn> txn;
  result = CreateTxnForJoinNode(aLeftNode, aRightNode, getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }

  mRangeUpdater.SelAdjJoinNodes(aLeftNode, aRightNode, aParent, offset, (PRInt32)oldLeftNodeLen);
  
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidJoinNodes(aLeftNode, aRightNode, aParent, result);

  return result;
}


NS_IMETHODIMP
nsEditor::DeleteNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);
  return DeleteNode(node);
}

nsresult
nsEditor::DeleteNode(nsINode* aNode)
{
  nsAutoRules beginRulesSniffing(this, kOpCreateNode, nsIEditor::ePrevious);

  // save node location for selection updating code.
  for (PRInt32 i = 0; i < mActionListeners.Count(); i++) {
    mActionListeners[i]->WillDeleteNode(aNode->AsDOMNode());
  }

  nsRefPtr<DeleteNodeTxn> txn;
  nsresult res = CreateTxnForDeleteNode(aNode, getter_AddRefs(txn));
  if (NS_SUCCEEDED(res))  {
    res = DoTransaction(txn);
  }

  for (PRInt32 i = 0; i < mActionListeners.Count(); i++) {
    mActionListeners[i]->DidDeleteNode(aNode->AsDOMNode(), res);
  }

  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// ReplaceContainer: replace inNode with a new node (outNode) which is contructed 
//                   to be of type aNodeType.  Put inNodes children into outNode.
//                   Callers responsibility to make sure inNode's children can 
//                   go in outNode.
nsresult
nsEditor::ReplaceContainer(nsIDOMNode *inNode, 
                           nsCOMPtr<nsIDOMNode> *outNode, 
                           const nsAString &aNodeType,
                           const nsAString *aAttribute,
                           const nsAString *aValue,
                           bool aCloneAttributes)
{
  NS_ENSURE_TRUE(inNode && outNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_STATE(node);

  nsCOMPtr<dom::Element> element;
  nsresult rv = ReplaceContainer(node, getter_AddRefs(element), aNodeType,
                                 aAttribute, aValue, aCloneAttributes);
  *outNode = element ? element->AsDOMNode() : nsnull;
  return rv;
}

nsresult
nsEditor::ReplaceContainer(nsINode* aNode,
                           dom::Element** outNode,
                           const nsAString& aNodeType,
                           const nsAString* aAttribute,
                           const nsAString* aValue,
                           bool aCloneAttributes)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(outNode);

  *outNode = nsnull;

  nsCOMPtr<nsIContent> parent = aNode->GetParent();
  NS_ENSURE_STATE(parent);

  PRInt32 offset = parent->IndexOf(aNode);

  // create new container
  //new call to use instead to get proper HTML element, bug# 39919
  nsresult res = CreateHTMLContent(aNodeType, outNode);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(*outNode);
  
  nsIDOMNode* inNode = aNode->AsDOMNode();

  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty()) {
    res = elem->SetAttribute(*aAttribute, *aValue);
    NS_ENSURE_SUCCESS(res, res);
  }
  if (aCloneAttributes) {
    res = CloneAttributes(elem, inNode);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // notify our internal selection state listener
  // (Note: A nsAutoSelectionReset object must be created
  //  before calling this to initialize mRangeUpdater)
  nsAutoReplaceContainerSelNotify selStateNotify(mRangeUpdater, inNode, elem);
  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    while (aNode->HasChildren()) {
      nsCOMPtr<nsIDOMNode> child = aNode->GetFirstChild()->AsDOMNode();

      res = DeleteNode(child);
      NS_ENSURE_SUCCESS(res, res);

      res = InsertNode(child, elem, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // insert new container into tree
  res = InsertNode(elem, parent->AsDOMNode(), offset);
  NS_ENSURE_SUCCESS(res, res);
  
  // delete old container
  return DeleteNode(inNode);
}

///////////////////////////////////////////////////////////////////////////
// RemoveContainer: remove inNode, reparenting its children into their
//                  the parent of inNode
//
nsresult
nsEditor::RemoveContainer(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return RemoveContainer(node);
}

nsresult
nsEditor::RemoveContainer(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> parent = aNode->GetNodeParent();
  NS_ENSURE_STATE(parent);

  PRInt32 offset = parent->IndexOf(aNode);
  
  // loop through the child nodes of inNode and promote them
  // into inNode's parent.
  PRUint32 nodeOrigLen = aNode->GetChildCount();

  // notify our internal selection state listener
  nsAutoRemoveContainerSelNotify selNotify(mRangeUpdater, aNode, parent, offset, nodeOrigLen);
                                   
  while (aNode->HasChildren()) {
    nsCOMPtr<nsIContent> child = aNode->GetLastChild();
    nsresult rv = DeleteNode(child->AsDOMNode());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InsertNode(child->AsDOMNode(), parent->AsDOMNode(), offset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return DeleteNode(aNode->AsDOMNode());
}


///////////////////////////////////////////////////////////////////////////
// InsertContainerAbove:  insert a new parent for inNode, returned in outNode,
//                   which is contructed to be of type aNodeType.  outNode becomes
//                   a child of inNode's earlier parent.
//                   Callers responsibility to make sure inNode's can be child
//                   of outNode, and outNode can be child of old parent.
nsresult
nsEditor::InsertContainerAbove( nsIDOMNode *inNode, 
                                nsCOMPtr<nsIDOMNode> *outNode, 
                                const nsAString &aNodeType,
                                const nsAString *aAttribute,
                                const nsAString *aValue)
{
  NS_ENSURE_TRUE(inNode && outNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> node = do_QueryInterface(inNode);
  NS_ENSURE_STATE(node);

  nsCOMPtr<dom::Element> element;
  nsresult rv = InsertContainerAbove(node, getter_AddRefs(element), aNodeType,
                                     aAttribute, aValue);
  *outNode = element ? element->AsDOMNode() : nsnull;
  return rv;
}

nsresult
nsEditor::InsertContainerAbove(nsIContent* aNode,
                               dom::Element** aOutNode,
                               const nsAString& aNodeType,
                               const nsAString* aAttribute,
                               const nsAString* aValue)
{
  MOZ_ASSERT(aNode);

  nsCOMPtr<nsIContent> parent = aNode->GetParent();
  NS_ENSURE_STATE(parent);
  PRInt32 offset = parent->IndexOf(aNode);

  // create new container
  nsCOMPtr<dom::Element> newContent;

  //new call to use instead to get proper HTML element, bug# 39919
  nsresult res = CreateHTMLContent(aNodeType, getter_AddRefs(newContent));
  NS_ENSURE_SUCCESS(res, res);

  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty()) {
    nsIDOMNode* elem = newContent->AsDOMNode();
    res = static_cast<nsIDOMElement*>(elem)->SetAttribute(*aAttribute, *aValue);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // notify our internal selection state listener
  nsAutoInsertContainerSelNotify selNotify(mRangeUpdater);
  
  // put inNode in new parent, outNode
  res = DeleteNode(aNode->AsDOMNode());
  NS_ENSURE_SUCCESS(res, res);

  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    res = InsertNode(aNode->AsDOMNode(), newContent->AsDOMNode(), 0);
    NS_ENSURE_SUCCESS(res, res);
  }

  // put new parent in doc
  res = InsertNode(newContent->AsDOMNode(), parent->AsDOMNode(), offset);
  newContent.forget(aOutNode);
  return res;  
}

///////////////////////////////////////////////////////////////////////////
// MoveNode:  move aNode to {aParent,aOffset}
nsresult
nsEditor::MoveNode(nsIContent* aNode, nsINode* aParent, PRInt32 aOffset)
{
  MOZ_ASSERT(aNode && aParent);
  MOZ_ASSERT(aOffset == -1 || (0 <= aOffset &&
                               aOffset <= (PRInt32)aParent->Length()));
  nsresult res = MoveNode(aNode->AsDOMNode(), aParent->AsDOMNode(), aOffset);
  NS_ASSERTION(NS_SUCCEEDED(res), "MoveNode failed");
  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

nsresult
nsEditor::MoveNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);
  nsresult res;

  PRInt32 oldOffset;
  nsCOMPtr<nsIDOMNode> oldParent = GetNodeLocation(aNode, &oldOffset);
  
  if (aOffset == -1)
  {
    PRUint32 unsignedOffset;
    // magic value meaning "move to end of aParent"
    res = GetLengthOfDOMNode(aParent, unsignedOffset);
    NS_ENSURE_SUCCESS(res, res);
    aOffset = (PRInt32)unsignedOffset;
  }
  
  // don't do anything if it's already in right place
  if ((aParent == oldParent.get()) && (oldOffset == aOffset)) return NS_OK;
  
  // notify our internal selection state listener
  nsAutoMoveNodeSelNotify selNotify(mRangeUpdater, oldParent, oldOffset, aParent, aOffset);
  
  // need to adjust aOffset if we are moving aNode further along in its current parent
  if ((aParent == oldParent.get()) && (oldOffset < aOffset)) 
  {
    aOffset--;  // this is because when we delete aNode, it will make the offsets after it off by one
  }

  // Hold a reference so aNode doesn't go away when we remove it (bug 772282)
  nsCOMPtr<nsIDOMNode> node = aNode;
  res = DeleteNode(node);
  NS_ENSURE_SUCCESS(res, res);
  return InsertNode(node, aParent, aOffset);
}


NS_IMETHODIMP
nsEditor::AddEditorObserver(nsIEditorObserver *aObserver)
{
  // we don't keep ownership of the observers.  They must
  // remove themselves as observers before they are destroyed.
  
  NS_ENSURE_TRUE(aObserver, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (mEditorObservers.IndexOf(aObserver) == -1) 
  {
    if (!mEditorObservers.AppendObject(aObserver))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditorObserver(nsIEditorObserver *aObserver)
{
  NS_ENSURE_TRUE(aObserver, NS_ERROR_FAILURE);

  if (!mEditorObservers.RemoveObject(aObserver))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

class EditorInputEventDispatcher : public nsRunnable
{
public:
  EditorInputEventDispatcher(nsEditor* aEditor,
                             bool aIsTrusted,
                             nsIContent* aTarget) :
    mEditor(aEditor), mTarget(aTarget), mIsTrusted(aIsTrusted)
  {
  }

  NS_IMETHOD Run()
  {
    // Note that we don't need to check mDispatchInputEvent here.  We need
    // to check it only when the editor requests to dispatch the input event.

    if (!mTarget->IsInDoc()) {
      return NS_OK;
    }

    nsCOMPtr<nsIPresShell> ps = mEditor->GetPresShell();
    if (!ps) {
      return NS_OK;
    }

    nsEvent inputEvent(mIsTrusted, NS_FORM_INPUT);
    inputEvent.flags |= NS_EVENT_FLAG_CANT_CANCEL;
    inputEvent.time = static_cast<PRUint64>(PR_Now() / 1000);
    nsEventStatus status = nsEventStatus_eIgnore;
    nsresult rv =
      ps->HandleEventWithTarget(&inputEvent, nsnull, mTarget, &status);
    NS_ENSURE_SUCCESS(rv, NS_OK); // print the warning if error
    return NS_OK;
  }

private:
  nsRefPtr<nsEditor> mEditor;
  nsCOMPtr<nsIContent> mTarget;
  bool mIsTrusted;
};

void nsEditor::NotifyEditorObservers(void)
{
  for (PRInt32 i = 0; i < mEditorObservers.Count(); i++) {
    mEditorObservers[i]->EditAction();
  }

  if (!mDispatchInputEvent) {
    return;
  }

  // We don't need to dispatch multiple input events if there is a pending
  // input event.  However, it may have different event target.  If we resolved
  // this issue, we need to manage the pending events in an array.  But it's
  // overwork.  We don't need to do it for the very rare case.

  nsCOMPtr<nsIContent> target = GetInputEventTargetContent();
  NS_ENSURE_TRUE(target, );

  nsContentUtils::AddScriptRunner(
     new EditorInputEventDispatcher(this, mHandlingTrustedAction, target));
}

NS_IMETHODIMP
nsEditor::AddEditActionListener(nsIEditActionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (mActionListeners.IndexOf(aListener) == -1) 
  {
    if (!mActionListeners.AppendObject(aListener))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditActionListener(nsIEditActionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

  if (!mActionListeners.RemoveObject(aListener))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::AddDocumentStateListener(nsIDocumentStateListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  if (mDocStateListeners.IndexOf(aListener) == -1)
  {
    if (!mDocStateListeners.AppendObject(aListener))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveDocumentStateListener(nsIDocumentStateListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  if (!mDocStateListeners.RemoveObject(aListener))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


NS_IMETHODIMP nsEditor::OutputToString(const nsAString& aFormatType,
                                       PRUint32 aFlags,
                                       nsAString& aOutputString)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::OutputToStream(nsIOutputStream* aOutputStream,
                         const nsAString& aFormatType,
                         const nsACString& aCharsetOverride,
                         PRUint32 aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::DumpContentTree()
{
#ifdef DEBUG
  if (mRootElement) {
    mRootElement->List(stdout);
  }
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugDumpContent()
{
#ifdef DEBUG
  nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMHTMLElement>bodyElem;
  doc->GetBody(getter_AddRefs(bodyElem));
  nsCOMPtr<nsIContent> content = do_QueryInterface(bodyElem);
  if (content)
    content->List();
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
#ifdef DEBUG
  NS_NOTREACHED("This should never get called. Overridden by subclasses");
#endif
  return NS_OK;
}


bool     
nsEditor::ArePreservingSelection()
{
  return !(mSavedSel.IsEmpty());
}

void
nsEditor::PreserveSelectionAcrossActions(nsISelection *aSel)
{
  mSavedSel.SaveSelection(aSel);
  mRangeUpdater.RegisterSelectionState(mSavedSel);
}

nsresult 
nsEditor::RestorePreservedSelection(nsISelection *aSel)
{
  if (mSavedSel.IsEmpty()) return NS_ERROR_FAILURE;
  mSavedSel.RestoreSelection(aSel);
  StopPreservingSelection();
  return NS_OK;
}

void     
nsEditor::StopPreservingSelection()
{
  mRangeUpdater.DropSelectionState(mSavedSel);
  mSavedSel.MakeEmpty();
}


nsresult
nsEditor::BeginIMEComposition()
{
  mInIMEMode = true;
  if (mPhonetic) {
    mPhonetic->Truncate(0);
  }
  return NS_OK;
}

void
nsEditor::EndIMEComposition()
{
  NS_ENSURE_TRUE(mInIMEMode, ); // nothing to do

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

  /* reset the data we need to construct a transaction */
  mIMETextNode = nsnull;
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  mInIMEMode = false;
  mIsIMEComposing = false;

  // notify editor observers of action
  NotifyEditorObservers();
}


NS_IMETHODIMP
nsEditor::GetPhonetic(nsAString& aPhonetic)
{
  if (mPhonetic)
    aPhonetic = *mPhonetic;
  else
    aPhonetic.Truncate(0);

  return NS_OK;
}


static nsresult
GetEditorContentWindow(dom::Element *aRoot, nsIWidget **aResult)
{
  NS_ENSURE_TRUE(aRoot && aResult, NS_ERROR_NULL_POINTER);

  *aResult = 0;

  // Not ref counted
  nsIFrame *frame = aRoot->GetPrimaryFrame();

  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  *aResult = frame->GetNearestWidget();
  NS_ENSURE_TRUE(*aResult, NS_ERROR_FAILURE);

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult
nsEditor::GetWidget(nsIWidget **aWidget)
{
  NS_ENSURE_TRUE(aWidget, NS_ERROR_NULL_POINTER);
  *aWidget = nsnull;

  nsCOMPtr<nsIWidget> widget;
  nsresult res = GetEditorContentWindow(GetRoot(), getter_AddRefs(widget));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(widget, NS_ERROR_NOT_AVAILABLE);

  NS_ADDREF(*aWidget = widget);

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ForceCompositionEnd()
{

// We can test mInIMEMode and do some optimization for Mac and Window
// Howerver, since UNIX support over-the-spot, we cannot rely on that 
// flag for Unix.
// We should use LookAndFeel to resolve this

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_OS2)
  // XXXmnakano see bug 558976, ResetInputState() has two meaning which are
  // "commit the composition" and "cursor is moved".  This method name is
  // "ForceCompositionEnd", so, ResetInputState() should be used only for the
  // former here.  However, ResetInputState() is also used for the latter here
  // because even if we don't have composition, we call ResetInputState() on
  // Linux.  Currently, nsGtkIMModule can know the timing of the cursor move,
  // so, the latter meaning should be gone and we should remove this #if.
  if(! mInIMEMode)
    return NS_OK;
#endif

  nsCOMPtr<nsIWidget> widget;
  nsresult res = GetWidget(getter_AddRefs(widget));
  NS_ENSURE_SUCCESS(res, res);

  if (widget) {
    res = widget->ResetInputState();
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetPreferredIMEState(IMEState *aState)
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

  switch (frame->GetStyleUIReset()->mIMEMode) {
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
nsEditor::GetComposing(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = IsIMEComposing();
  return NS_OK;
}


/* Non-interface, public methods */

NS_IMETHODIMP
nsEditor::GetRootElement(nsIDOMElement **aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);
  NS_ENSURE_TRUE(mRootElement, NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(mRootElement);
  rootElement.forget(aRootElement);
  return NS_OK;
}


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsEditor::StartOperation(OperationID opID, nsIEditor::EDirection aDirection)
{
  mAction = opID;
  mDirection = aDirection;
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsEditor::EndOperation()
{
  mAction = kOpNone;
  mDirection = eNone;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::CloneAttribute(const nsAString & aAttribute,
                         nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  NS_ENSURE_TRUE(aDestNode && aSourceNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  NS_ENSURE_TRUE(destElement && sourceElement, NS_ERROR_NO_INTERFACE);

  nsAutoString attrValue;
  bool isAttrSet;
  nsresult rv = GetAttributeValue(sourceElement,
                                  aAttribute,
                                  attrValue,
                                  &isAttrSet);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isAttrSet)
    rv = SetAttribute(destElement, aAttribute, attrValue);
  else
    rv = RemoveAttribute(destElement, aAttribute);

  return rv;
}

// Objects must be DOM elements
NS_IMETHODIMP
nsEditor::CloneAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  NS_ENSURE_TRUE(aDestNode && aSourceNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  NS_ENSURE_TRUE(destElement && sourceElement, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIDOMNamedNodeMap> sourceAttributes;
  sourceElement->GetAttributes(getter_AddRefs(sourceAttributes));
  nsCOMPtr<nsIDOMNamedNodeMap> destAttributes;
  destElement->GetAttributes(getter_AddRefs(destAttributes));
  NS_ENSURE_TRUE(sourceAttributes && destAttributes, NS_ERROR_FAILURE);

  nsAutoEditBatch beginBatching(this);

  // Use transaction system for undo only if destination
  //   is already in the document
  nsCOMPtr<nsIDOMNode> p = aDestNode;
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetRoot());
  NS_ENSURE_TRUE(rootNode, NS_ERROR_NULL_POINTER);
  bool destInBody = true;
  while (p && p != rootNode)
  {
    nsCOMPtr<nsIDOMNode> tmp;
    if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp)
    {
      destInBody = false;
      break;
    }
    p = tmp;
  }

  PRUint32 sourceCount;
  sourceAttributes->GetLength(&sourceCount);
  PRUint32 i, destCount;
  destAttributes->GetLength(&destCount);
  nsCOMPtr<nsIDOMNode> attrNode;

  // Clear existing attributes
  for (i = 0; i < destCount; i++)
  {
    // always remove item number 0 (first item in list)
    if( NS_SUCCEEDED(destAttributes->Item(0, getter_AddRefs(attrNode))) && attrNode)
    {
      nsCOMPtr<nsIDOMAttr> destAttribute = do_QueryInterface(attrNode);
      if (destAttribute)
      {
        nsAutoString str;
        if (NS_SUCCEEDED(destAttribute->GetName(str)))
        {
          if (destInBody)
            RemoveAttribute(destElement, str);
          else
            destElement->RemoveAttribute(str);
        }
      }
    }
  }

  nsresult result = NS_OK;

  // Set just the attributes that the source element has
  for (i = 0; i < sourceCount; i++)
  {
    if( NS_SUCCEEDED(sourceAttributes->Item(i, getter_AddRefs(attrNode))) && attrNode)
    {
      nsCOMPtr<nsIDOMAttr> sourceAttribute = do_QueryInterface(attrNode);
      if (sourceAttribute)
      {
        nsAutoString sourceAttrName;
        if (NS_SUCCEEDED(sourceAttribute->GetName(sourceAttrName)))
        {
          nsAutoString sourceAttrValue;
          /*
          Presence of an attribute in the named node map indicates that it was set on the 
          element even if it has no value.
          */
          if (NS_SUCCEEDED(sourceAttribute->GetValue(sourceAttrValue)))
          {
            if (destInBody) {
              result = SetAttributeOrEquivalent(destElement, sourceAttrName, sourceAttrValue, false);
            }
            else {
              // the element is not inserted in the document yet, we don't want to put a
              // transaction on the UndoStack
              result = SetAttributeOrEquivalent(destElement, sourceAttrName, sourceAttrValue, true);
            }
          } else {
            // Do we ever get here?
#if DEBUG_cmanske
            printf("Attribute in sourceAttribute has empty value in nsEditor::CloneAttributes()\n");
#endif
          }
        }        
      }
    }
  }
  return result;
}


NS_IMETHODIMP nsEditor::ScrollSelectionIntoView(bool aScrollToAnchor)
{
  nsCOMPtr<nsISelectionController> selCon;
  if (NS_SUCCEEDED(GetSelectionController(getter_AddRefs(selCon))) && selCon)
  {
    PRInt16 region = nsISelectionController::SELECTION_FOCUS_REGION;

    if (aScrollToAnchor)
      region = nsISelectionController::SELECTION_ANCHOR_REGION;

    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                    region, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditor::InsertTextImpl(const nsAString& aStringToInsert, 
                                          nsCOMPtr<nsIDOMNode> *aInOutNode, 
                                          PRInt32 *aInOutOffset,
                                          nsIDOMDocument *aDoc)
{
  // NOTE: caller *must* have already used nsAutoTxnsConserveSelection stack-based
  // class to turn off txn selection updating.  Caller also turned on rules sniffing
  // if desired.
  
  nsresult res;
  NS_ENSURE_TRUE(aInOutNode && *aInOutNode && aInOutOffset && aDoc, NS_ERROR_NULL_POINTER);
  if (!mInIMEMode && aStringToInsert.IsEmpty()) return NS_OK;
  nsCOMPtr<nsIDOMText> nodeAsText = do_QueryInterface(*aInOutNode);
  if (!nodeAsText && IsPlaintextEditor()) {
    nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetRoot());
    // In some cases, aInOutNode is the anonymous DIV, and aInOutOffset is 0.
    // To avoid injecting unneeded text nodes, we first look to see if we have
    // one available.  In that case, we'll just adjust aInOutNode and aInOutOffset
    // accordingly.
    if (*aInOutNode == rootNode && *aInOutOffset == 0) {
      nsCOMPtr<nsIDOMNode> possibleTextNode;
      res = (*aInOutNode)->GetFirstChild(getter_AddRefs(possibleTextNode));
      if (NS_SUCCEEDED(res)) {
        nodeAsText = do_QueryInterface(possibleTextNode);
        if (nodeAsText) {
          *aInOutNode = possibleTextNode;
        }
      }
    }
    // In some other cases, aInOutNode is the anonymous DIV, and aInOutOffset points
    // to the terminating mozBR.  In that case, we'll adjust aInOutNode and aInOutOffset
    // to the preceding text node, if any.
    if (!nodeAsText && *aInOutNode == rootNode && *aInOutOffset > 0) {
      nsCOMPtr<nsIDOMNodeList> children;
      res = (*aInOutNode)->GetChildNodes(getter_AddRefs(children));
      if (NS_SUCCEEDED(res)) {
        nsCOMPtr<nsIDOMNode> possibleMozBRNode;
        children->Item(*aInOutOffset, getter_AddRefs(possibleMozBRNode));
        if (possibleMozBRNode && nsTextEditUtils::IsMozBR(possibleMozBRNode)) {
          nsCOMPtr<nsIDOMNode> possibleTextNode;
          res = children->Item(*aInOutOffset - 1, getter_AddRefs(possibleTextNode));
          if (NS_SUCCEEDED(res)) {
            nodeAsText = do_QueryInterface(possibleTextNode);
            if (nodeAsText) {
              PRUint32 length;
              res = nodeAsText->GetLength(&length);
              if (NS_SUCCEEDED(res)) {
                *aInOutOffset = PRInt32(length);
                *aInOutNode = possibleTextNode;
              }
            }
          }
        } else {
          // The selection might be at the end of the last textnode child,
          // in which case we can just append to the textnode in question.
          nsCOMPtr<nsIDOMNode> possibleTextNode;
          res = children->Item(*aInOutOffset - 1, getter_AddRefs(possibleTextNode));
          nodeAsText = do_QueryInterface(possibleTextNode);
          if (nodeAsText) {
            PRUint32 length;
            res = nodeAsText->GetLength(&length);
            if (NS_SUCCEEDED(res)) {
              *aInOutOffset = PRInt32(length);
              *aInOutNode = possibleTextNode;
            }
          }
        }
      }
    }
    // Sometimes, aInOutNode is the mozBR element itself.  In that case, we'll
    // adjust the insertion point to the previous text node, if one exists, or
    // to the parent anonymous DIV.
    if (nsTextEditUtils::IsMozBR(*aInOutNode) && *aInOutOffset == 0) {
      nsCOMPtr<nsIDOMNode> previous;
      (*aInOutNode)->GetPreviousSibling(getter_AddRefs(previous));
      nodeAsText = do_QueryInterface(previous);
      if (nodeAsText) {
        PRUint32 length;
        res = nodeAsText->GetLength(&length);
        if (NS_SUCCEEDED(res)) {
          *aInOutOffset = PRInt32(length);
          *aInOutNode = previous;
        }
      } else {
        nsCOMPtr<nsIDOMNode> parent;
        (*aInOutNode)->GetParentNode(getter_AddRefs(parent));
        if (parent == rootNode) {
          *aInOutNode = parent;
        }
      }
    }
  }
  PRInt32 offset = *aInOutOffset;
  if (mInIMEMode)
  {
    if (!nodeAsText)
    {
      // create a text node
      res = aDoc->CreateTextNode(EmptyString(), getter_AddRefs(nodeAsText));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(nodeAsText, NS_ERROR_NULL_POINTER);
      nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(nodeAsText);
      // then we insert it into the dom tree
      res = InsertNode(newNode, *aInOutNode, offset);
      NS_ENSURE_SUCCESS(res, res);
      offset = 0;
    }
    res = InsertTextIntoTextNodeImpl(aStringToInsert, nodeAsText, offset);
    NS_ENSURE_SUCCESS(res, res);
  }
  else
  {
    if (nodeAsText)
    {
      // we are inserting text into an existing text node.
      res = InsertTextIntoTextNodeImpl(aStringToInsert, nodeAsText, offset);
      NS_ENSURE_SUCCESS(res, res);
      *aInOutOffset += aStringToInsert.Length();
    }
    else
    {
      // we are inserting text into a non-text node
      // first we have to create a textnode (this also populates it with the text)
      res = aDoc->CreateTextNode(aStringToInsert, getter_AddRefs(nodeAsText));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(nodeAsText, NS_ERROR_NULL_POINTER);
      nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(nodeAsText);
      // then we insert it into the dom tree
      res = InsertNode(newNode, *aInOutNode, offset);
      NS_ENSURE_SUCCESS(res, res);
      *aInOutNode = newNode;
      *aInOutOffset = aStringToInsert.Length();
    }
  }
  return res;
}


nsresult nsEditor::InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert, 
                                              nsIDOMCharacterData *aTextNode, 
                                              PRInt32 aOffset,
                                              bool aSuppressIME)
{
  nsRefPtr<EditTxn> txn;
  nsresult result = NS_OK;
  bool isIMETransaction = false;
  // aSuppressIME is used when editor must insert text, yet this text is not
  // part of current ime operation.  example: adjusting whitespace around an ime insertion.
  if (mIMETextRangeList && mInIMEMode && !aSuppressIME)
  {
    if (!mIMETextNode)
    {
      mIMETextNode = aTextNode;
      mIMETextOffset = aOffset;
    }
    PRUint16 len ;
    len = mIMETextRangeList->GetLength();
    if (len > 0)
    {
      nsCOMPtr<nsIPrivateTextRange> range;
      for (PRUint16 i = 0; i < len; i++) 
      {
        range = mIMETextRangeList->Item(i);
        if (range)
        {
          PRUint16 type;
          result = range->GetRangeType(&type);
          if (NS_SUCCEEDED(result)) 
          {
            if (type == nsIPrivateTextRange::TEXTRANGE_RAWINPUT) 
            {
              PRUint16 start, end;
              result = range->GetRangeStart(&start);
              if (NS_SUCCEEDED(result)) 
              {
                result = range->GetRangeEnd(&end);
                if (NS_SUCCEEDED(result)) 
                {
                  if (!mPhonetic)
                    mPhonetic = new nsString();
                  if (mPhonetic)
                  {
                    nsAutoString tmp(aStringToInsert);                  
                    tmp.Mid(*mPhonetic, start, end-start);
                  }
                }
              }
            } // if
          }
        } // if
      } // for
    } // if

    nsRefPtr<IMETextTxn> imeTxn;
    result = CreateTxnForIMEText(aStringToInsert, getter_AddRefs(imeTxn));
    txn = imeTxn;
    isIMETransaction = true;
  }
  else
  {
    nsRefPtr<InsertTextTxn> insertTxn;
    result = CreateTxnForInsertText(aStringToInsert, aTextNode, aOffset,
                                    getter_AddRefs(insertTxn));
    txn = insertTxn;
  }
  NS_ENSURE_SUCCESS(result, result);

  // let listeners know what's up
  PRInt32 i;
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillInsertText(aTextNode, aOffset, aStringToInsert);
  
  // XXX we may not need these view batches anymore.  This is handled at a higher level now I believe
  BeginUpdateViewBatch();
  result = DoTransaction(txn);
  EndUpdateViewBatch();

  mRangeUpdater.SelAdjInsertText(aTextNode, aOffset, aStringToInsert);
  
  // let listeners know what happened
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidInsertText(aTextNode, aOffset, aStringToInsert, result);

  // Added some cruft here for bug 43366.  Layout was crashing because we left an 
  // empty text node lying around in the document.  So I delete empty text nodes
  // caused by IME.  I have to mark the IME transaction as "fixed", which means
  // that furure ime txns won't merge with it.  This is because we don't want
  // future ime txns trying to put their text into a node that is no longer in
  // the document.  This does not break undo/redo, because all these txns are 
  // wrapped in a parent PlaceHolder txn, and placeholder txns are already 
  // savvy to having multiple ime txns inside them.
  
  // delete empty ime text node if there is one
  if (isIMETransaction && mIMETextNode)
  {
    PRUint32 len;
    mIMETextNode->GetLength(&len);
    if (!len)
    {
      DeleteNode(mIMETextNode);
      mIMETextNode = nsnull;
      static_cast<IMETextTxn*>(txn.get())->MarkFixed();  // mark the ime txn "fixed"
    }
  }
  
  return result;
}


NS_IMETHODIMP nsEditor::SelectEntireDocument(nsISelection *aSelection)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(GetRoot());
  if (!rootElement) { return NS_ERROR_NOT_INITIALIZED; }

  return aSelection->SelectAllChildren(rootElement);
}


nsINode*
nsEditor::GetFirstEditableNode(nsINode* aRoot)
{
  MOZ_ASSERT(aRoot);

  nsIContent* node = GetLeftmostChild(aRoot);
  if (node && !IsEditable(node)) {
    node = GetNextNode(node, /* aEditableNode = */ true);
  }

  return (node != aRoot) ? node : nsnull;
}


NS_IMETHODIMP
nsEditor::NotifyDocumentListeners(TDocumentListenerNotification aNotificationType)
{
  PRInt32 numListeners = mDocStateListeners.Count();
  if (!numListeners)    // maybe there just aren't any.
    return NS_OK;
 
  nsCOMArray<nsIDocumentStateListener> listeners(mDocStateListeners);
  nsresult rv = NS_OK;
  PRInt32 i;

  switch (aNotificationType)
  {
    case eDocumentCreated:
      for (i = 0; i < numListeners;i++)
      {
        rv = listeners[i]->NotifyDocumentCreated();
        if (NS_FAILED(rv))
          break;
      }
      break;
      
    case eDocumentToBeDestroyed:
      for (i = 0; i < numListeners;i++)
      {
        rv = listeners[i]->NotifyDocumentWillBeDestroyed();
        if (NS_FAILED(rv))
          break;
      }
      break;
  
    case eDocumentStateChanged:
      {
        bool docIsDirty;
        rv = GetDocumentModified(&docIsDirty);
        NS_ENSURE_SUCCESS(rv, rv);
        
        if (docIsDirty == mDocDirtyState)
          return NS_OK;
        
        mDocDirtyState = (PRInt8)docIsDirty;
        
        for (i = 0; i < numListeners;i++)
        {
          rv = listeners[i]->NotifyDocumentStateChanged(mDocDirtyState);
          if (NS_FAILED(rv))
            break;
        }
      }
      break;
    
    default:
      NS_NOTREACHED("Unknown notification");
  }

  return rv;
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertText(const nsAString & aStringToInsert,
                                               nsIDOMCharacterData *aTextNode,
                                               PRInt32 aOffset,
                                               InsertTextTxn ** aTxn)
{
  NS_ENSURE_TRUE(aTextNode && aTxn, NS_ERROR_NULL_POINTER);
  nsresult rv;

  nsRefPtr<InsertTextTxn> txn = new InsertTextTxn();
  rv = txn->Init(aTextNode, aOffset, aStringToInsert, this);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP nsEditor::DeleteText(nsIDOMCharacterData *aElement,
                              PRUint32             aOffset,
                              PRUint32             aLength)
{
  nsRefPtr<DeleteTextTxn> txn;
  nsresult result = CreateTxnForDeleteText(aElement, aOffset, aLength,
                                           getter_AddRefs(txn));
  nsAutoRules beginRulesSniffing(this, kOpDeleteText, nsIEditor::ePrevious);
  if (NS_SUCCEEDED(result))  
  {
    // let listeners know what's up
    PRInt32 i;
    for (i = 0; i < mActionListeners.Count(); i++)
      mActionListeners[i]->WillDeleteText(aElement, aOffset, aLength);
    
    result = DoTransaction(txn); 
    
    // let listeners know what happened
    for (i = 0; i < mActionListeners.Count(); i++)
      mActionListeners[i]->DidDeleteText(aElement, aOffset, aLength, result);
  }
  return result;
}


nsresult
nsEditor::CreateTxnForDeleteText(nsIDOMCharacterData* aElement,
                                 PRUint32             aOffset,
                                 PRUint32             aLength,
                                 DeleteTextTxn**      aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsRefPtr<DeleteTextTxn> txn = new DeleteTextTxn();

  nsresult res = txn->Init(this, aElement, aOffset, aLength, &mRangeUpdater);
  NS_ENSURE_SUCCESS(res, res);

  txn.forget(aTxn);
  return NS_OK;
}




NS_IMETHODIMP nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsRefPtr<SplitElementTxn> txn = new SplitElementTxn();

  nsresult rv = txn->Init(this, aNode, aOffset);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}

NS_IMETHODIMP nsEditor::CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                             nsIDOMNode  *aRightNode,
                                             JoinElementTxn **aTxn)
{
  NS_ENSURE_TRUE(aLeftNode && aRightNode, NS_ERROR_NULL_POINTER);

  nsRefPtr<JoinElementTxn> txn = new JoinElementTxn();

  nsresult rv = txn->Init(this, aLeftNode, aRightNode);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


// END nsEditor core implementation


// BEGIN nsEditor public helper methods

nsresult
nsEditor::SplitNodeImpl(nsIDOMNode * aExistingRightNode,
                        PRInt32      aOffset,
                        nsIDOMNode*  aNewLeftNode,
                        nsIDOMNode*  aParent)
{
#ifdef NS_DEBUG_EDITOR
  if (gNoisy) { printf("SplitNodeImpl: left=%p, right=%p, offset=%d\n", (void*)aNewLeftNode, (void*)aExistingRightNode, aOffset); }
#endif

  NS_ASSERTION(((nsnull!=aExistingRightNode) &&
                (nsnull!=aNewLeftNode) &&
                (nsnull!=aParent)),
                "null arg");
  nsresult result;
  if ((nsnull!=aExistingRightNode) &&
      (nsnull!=aNewLeftNode) &&
      (nsnull!=aParent))
  {
    // get selection
    nsCOMPtr<nsISelection> selection;
    result = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    // remember some selection points
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode;
    PRInt32 selStartOffset, selEndOffset;
    result = GetStartNodeAndOffset(selection, getter_AddRefs(selStartNode), &selStartOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;  // if selection is cleared, remember that
    result = GetEndNodeAndOffset(selection, getter_AddRefs(selEndNode), &selEndOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;  // if selection is cleared, remember that

    nsCOMPtr<nsIDOMNode> resultNode;
    result = aParent->InsertBefore(aNewLeftNode, aExistingRightNode, getter_AddRefs(resultNode));
    //printf("  after insert\n"); content->List();  // DEBUG
    if (NS_SUCCEEDED(result))
    {
      // split the children between the 2 nodes
      // at this point, aExistingRightNode has all the children
      // move all the children whose index is < aOffset to aNewLeftNode
      if (0<=aOffset) // don't bother unless we're going to move at least one child
      {
        // if it's a text node, just shuffle around some text
        nsCOMPtr<nsIDOMCharacterData> rightNodeAsText( do_QueryInterface(aExistingRightNode) );
        nsCOMPtr<nsIDOMCharacterData> leftNodeAsText( do_QueryInterface(aNewLeftNode) );
        if (leftNodeAsText && rightNodeAsText)
        {
          // fix right node
          nsAutoString leftText;
          rightNodeAsText->SubstringData(0, aOffset, leftText);
          rightNodeAsText->DeleteData(0, aOffset);
          // fix left node
          leftNodeAsText->SetData(leftText);
          // moose          
        }
        else
        {  // otherwise it's an interior node, so shuffle around the children
           // go through list backwards so deletes don't interfere with the iteration
          nsCOMPtr<nsIDOMNodeList> childNodes;
          result = aExistingRightNode->GetChildNodes(getter_AddRefs(childNodes));
          if ((NS_SUCCEEDED(result)) && (childNodes))
          {
            PRInt32 i=aOffset-1;
            for ( ; ((NS_SUCCEEDED(result)) && (0<=i)); i--)
            {
              nsCOMPtr<nsIDOMNode> childNode;
              result = childNodes->Item(i, getter_AddRefs(childNode));
              if ((NS_SUCCEEDED(result)) && (childNode))
              {
                result = aExistingRightNode->RemoveChild(childNode, getter_AddRefs(resultNode));
                //printf("  after remove\n"); content->List();  // DEBUG
                if (NS_SUCCEEDED(result))
                {
                  nsCOMPtr<nsIDOMNode> firstChild;
                  aNewLeftNode->GetFirstChild(getter_AddRefs(firstChild));
                  result = aNewLeftNode->InsertBefore(childNode, firstChild, getter_AddRefs(resultNode));
                  //printf("  after append\n"); content->List();  // DEBUG
                }
              }
            }
          }        
        }
        // handle selection
        nsCOMPtr<nsIPresShell> ps = GetPresShell();
        if (ps)
          ps->FlushPendingNotifications(Flush_Frames);

        if (GetShouldTxnSetSelection())
        {
          // editor wants us to set selection at split point
          selection->Collapse(aNewLeftNode, aOffset);
        }
        else if (selStartNode)   
        {
          // else adjust the selection if needed.  if selStartNode is null, then there was no selection.
          // HACK: this is overly simplified - multi-range selections need more work than this
          if (selStartNode.get() == aExistingRightNode)
          {
            if (selStartOffset < aOffset)
            {
              selStartNode = aNewLeftNode;
            }
            else
            {
              selStartOffset -= aOffset;
            }
          }
          if (selEndNode.get() == aExistingRightNode)
          {
            if (selEndOffset < aOffset)
            {
              selEndNode = aNewLeftNode;
            }
            else
            {
              selEndOffset -= aOffset;
            }
          }
          selection->Collapse(selStartNode,selStartOffset);
          selection->Extend(selEndNode,selEndOffset);
        }
      }
    }
  }
  else
    result = NS_ERROR_INVALID_ARG;

  return result;
}

nsresult
nsEditor::JoinNodesImpl(nsIDOMNode * aNodeToKeep,
                        nsIDOMNode * aNodeToJoin,
                        nsIDOMNode * aParent,
                        bool         aNodeToKeepIsFirst)
{
  NS_ASSERTION(aNodeToKeep && aNodeToJoin && aParent, "null arg");
  nsresult result = NS_OK;
  if (aNodeToKeep && aNodeToJoin && aParent)
  {
    // get selection
    nsCOMPtr<nsISelection> selection;
    GetSelection(getter_AddRefs(selection));
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    // remember some selection points
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode;
    PRInt32 selStartOffset, selEndOffset, joinOffset, keepOffset;
    result = GetStartNodeAndOffset(selection, getter_AddRefs(selStartNode), &selStartOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;
    result = GetEndNodeAndOffset(selection, getter_AddRefs(selEndNode), &selEndOffset);
    // Joe or Kin should comment here on why the following line is not a copy/paste error
    if (NS_FAILED(result)) selStartNode = nsnull;

    nsCOMPtr<nsIDOMNode> leftNode;
    if (aNodeToKeepIsFirst)
      leftNode = aNodeToKeep;
    else
      leftNode = aNodeToJoin;

    PRUint32 firstNodeLength;
    result = GetLengthOfDOMNode(leftNode, firstNodeLength);
    NS_ENSURE_SUCCESS(result, result);
    nsCOMPtr<nsIDOMNode> parent = GetNodeLocation(aNodeToJoin, &joinOffset);
    parent = GetNodeLocation(aNodeToKeep, &keepOffset);
    
    // if selection endpoint is between the nodes, remember it as being
    // in the one that is going away instead.  This simplifies later selection
    // adjustment logic at end of this method.
    if (selStartNode)
    {
      if (selStartNode == parent)
      {
        if (aNodeToKeepIsFirst)
        {
          if ((selStartOffset > keepOffset) && (selStartOffset <= joinOffset))
          {
            selStartNode = aNodeToJoin; 
            selStartOffset = 0;
          }
        }
        else
        {
          if ((selStartOffset > joinOffset) && (selStartOffset <= keepOffset))
          {
            selStartNode = aNodeToJoin; 
            selStartOffset = firstNodeLength;
          }
        }
      }
      if (selEndNode == parent)
      {
        if (aNodeToKeepIsFirst)
        {
          if ((selEndOffset > keepOffset) && (selEndOffset <= joinOffset))
          {
            selEndNode = aNodeToJoin; 
            selEndOffset = 0;
          }
        }
        else
        {
          if ((selEndOffset > joinOffset) && (selEndOffset <= keepOffset))
          {
            selEndNode = aNodeToJoin; 
            selEndOffset = firstNodeLength;
          }
        }
      }
    }
    // ok, ready to do join now.
    // if it's a text node, just shuffle around some text
    nsCOMPtr<nsIDOMCharacterData> keepNodeAsText( do_QueryInterface(aNodeToKeep) );
    nsCOMPtr<nsIDOMCharacterData> joinNodeAsText( do_QueryInterface(aNodeToJoin) );
    if (keepNodeAsText && joinNodeAsText)
    {
      nsAutoString rightText;
      nsAutoString leftText;
      if (aNodeToKeepIsFirst)
      {
        keepNodeAsText->GetData(leftText);
        joinNodeAsText->GetData(rightText);
      }
      else
      {
        keepNodeAsText->GetData(rightText);
        joinNodeAsText->GetData(leftText);
      }
      leftText += rightText;
      keepNodeAsText->SetData(leftText);          
    }
    else
    {  // otherwise it's an interior node, so shuffle around the children
      nsCOMPtr<nsIDOMNodeList> childNodes;
      result = aNodeToJoin->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(result)) && (childNodes))
      {
        PRInt32 i;  // must be signed int!
        PRUint32 childCount=0;
        nsCOMPtr<nsIDOMNode> firstNode; //only used if aNodeToKeepIsFirst is false
        childNodes->GetLength(&childCount);
        if (!aNodeToKeepIsFirst)
        { // remember the first child in aNodeToKeep, we'll insert all the children of aNodeToJoin in front of it
          result = aNodeToKeep->GetFirstChild(getter_AddRefs(firstNode));  
          // GetFirstChild returns nsnull firstNode if aNodeToKeep has no children, that's ok.
        }
        nsCOMPtr<nsIDOMNode> resultNode;
        // have to go through the list backwards to keep deletes from interfering with iteration
        nsCOMPtr<nsIDOMNode> previousChild;
        for (i=childCount-1; ((NS_SUCCEEDED(result)) && (0<=i)); i--)
        {
          nsCOMPtr<nsIDOMNode> childNode;
          result = childNodes->Item(i, getter_AddRefs(childNode));
          if ((NS_SUCCEEDED(result)) && (childNode))
          {
            if (aNodeToKeepIsFirst)
            { // append children of aNodeToJoin
              //was result = aNodeToKeep->AppendChild(childNode, getter_AddRefs(resultNode));
              result = aNodeToKeep->InsertBefore(childNode, previousChild, getter_AddRefs(resultNode));
              previousChild = do_QueryInterface(childNode);
            }
            else
            { // prepend children of aNodeToJoin
              result = aNodeToKeep->InsertBefore(childNode, firstNode, getter_AddRefs(resultNode));
              firstNode = do_QueryInterface(childNode);
            }
          }
        }
      }
      else if (!childNodes) {
        result = NS_ERROR_NULL_POINTER;
      }
    }
    if (NS_SUCCEEDED(result))
    { // delete the extra node
      nsCOMPtr<nsIDOMNode> resultNode;
      result = aParent->RemoveChild(aNodeToJoin, getter_AddRefs(resultNode));
      
      if (GetShouldTxnSetSelection())
      {
        // editor wants us to set selection at join point
        selection->Collapse(aNodeToKeep, firstNodeLength);
      }
      else if (selStartNode)
      {
        // and adjust the selection if needed
        // HACK: this is overly simplified - multi-range selections need more work than this
        bool bNeedToAdjust = false;
        
        // check to see if we joined nodes where selection starts
        if (selStartNode.get() == aNodeToJoin)
        {
          bNeedToAdjust = true;
          selStartNode = aNodeToKeep;
          if (aNodeToKeepIsFirst)
          {
            selStartOffset += firstNodeLength;
          }
        }
        else if ((selStartNode.get() == aNodeToKeep) && !aNodeToKeepIsFirst)
        {
          bNeedToAdjust = true;
          selStartOffset += firstNodeLength;
        }
                
        // check to see if we joined nodes where selection ends
        if (selEndNode.get() == aNodeToJoin)
        {
          bNeedToAdjust = true;
          selEndNode = aNodeToKeep;
          if (aNodeToKeepIsFirst)
          {
            selEndOffset += firstNodeLength;
          }
        }
        else if ((selEndNode.get() == aNodeToKeep) && !aNodeToKeepIsFirst)
        {
          bNeedToAdjust = true;
          selEndOffset += firstNodeLength;
        }
        
        // adjust selection if needed
        if (bNeedToAdjust)
        {
          selection->Collapse(selStartNode,selStartOffset);
          selection->Extend(selEndNode,selEndOffset);          
        }
      }
    }
  }
  else
    result = NS_ERROR_INVALID_ARG;

  return result;
}


PRInt32
nsEditor::GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent)
{
  MOZ_ASSERT(aChild && aParent);

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  nsCOMPtr<nsINode> child = do_QueryInterface(aChild);
  MOZ_ASSERT(parent && child);

  PRInt32 idx = parent->IndexOf(child);
  MOZ_ASSERT(idx != -1);
  return idx;
}

// static
already_AddRefed<nsIDOMNode>
nsEditor::GetNodeLocation(nsIDOMNode* aChild, PRInt32* outOffset)
{
  MOZ_ASSERT(aChild && outOffset);
  NS_ENSURE_TRUE(aChild && outOffset, nsnull);
  *outOffset = -1;

  nsCOMPtr<nsIDOMNode> parent;

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    aChild->GetParentNode(getter_AddRefs(parent))));
  if (parent) {
    *outOffset = GetChildOffset(aChild, parent);
  }

  return parent.forget();
}

// returns the number of things inside aNode.  
// If aNode is text, returns number of characters. If not, returns number of children nodes.
nsresult
nsEditor::GetLengthOfDOMNode(nsIDOMNode *aNode, PRUint32 &aCount) 
{
  aCount = 0;
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  aCount = node->Length();
  return NS_OK;
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aParentNode, 
                       PRInt32      aOffset, 
                       bool         aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);
  *aResultNode = nsnull;

  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetPriorNode(parentNode, aOffset,
                                                aEditableNode,
                                                bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetPriorNode(nsINode* aParentNode,
                       PRInt32 aOffset,
                       bool aEditableNode,
                       bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // If we are at the beginning of the node, or it is a text node, then just
  // look before it.
  if (!aOffset || aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    if (aNoBlockCrossing && IsBlockNode(aParentNode)) {
      // If we aren't allowed to cross blocks, don't look before this block.
      return nsnull;
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


nsresult 
nsEditor::GetNextNode(nsIDOMNode   *aParentNode, 
                      PRInt32      aOffset, 
                      bool         aEditableNode, 
                      nsCOMPtr<nsIDOMNode> *aResultNode,
                      bool         bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);
  *aResultNode = nsnull;

  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetNextNode(parentNode, aOffset,
                                               aEditableNode,
                                               bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetNextNode(nsINode* aParentNode,
                      PRInt32 aOffset,
                      bool aEditableNode,
                      bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // if aParentNode is a text node, use its location instead
  if (aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    nsINode* parent = aParentNode->GetNodeParent();
    NS_ENSURE_TRUE(parent, nsnull);
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
      return nsnull;
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
    return NS_OK;
  }

  return GetNextNode(aParentNode, aEditableNode, aNoBlockCrossing);
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aCurrentNode, 
                       bool         aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> currentNode = do_QueryInterface(aCurrentNode);
  NS_ENSURE_TRUE(currentNode, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetPriorNode(currentNode, aEditableNode,
                                                bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetPriorNode(nsINode* aCurrentNode, bool aEditableNode,
                       bool aNoBlockCrossing /* = false */)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nsnull;
  }

  return FindNode(aCurrentNode, false, aEditableNode, aNoBlockCrossing);
}

nsIContent*
nsEditor::FindNextLeafNode(nsINode  *aCurrentNode, 
                           bool      aGoForward,
                           bool      bNoBlockCrossing)
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

    nsINode *parent = cur->GetNodeParent();
    if (!parent) {
      return nsnull;
    }

    NS_ASSERTION(IsDescendantOfEditorRoot(parent),
                 "We started with a proper descendant of root, and should stop "
                 "if we ever hit the root, so we better have a descendant of "
                 "root now!");
    if (IsEditorRoot(parent) ||
        (bNoBlockCrossing && IsBlockNode(parent))) {
      return nsnull;
    }

    cur = parent;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nsnull;
}

nsresult
nsEditor::GetNextNode(nsIDOMNode* aCurrentNode,
                      bool aEditableNode,
                      nsCOMPtr<nsIDOMNode> *aResultNode,
                      bool bNoBlockCrossing)
{
  nsCOMPtr<nsINode> currentNode = do_QueryInterface(aCurrentNode);
  if (!currentNode || !aResultNode) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResultNode = do_QueryInterface(GetNextNode(currentNode, aEditableNode,
                                               bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetNextNode(nsINode* aCurrentNode,
                      bool aEditableNode,
                      bool bNoBlockCrossing)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nsnull;
  }

  return FindNode(aCurrentNode, true, aEditableNode, bNoBlockCrossing);
}

nsIContent*
nsEditor::FindNode(nsINode *aCurrentNode,
                   bool     aGoForward,
                   bool     aEditableNode,
                   bool     bNoBlockCrossing)
{
  if (IsEditorRoot(aCurrentNode)) {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.

    return nsnull;
  }

  nsIContent* candidate =
    FindNextLeafNode(aCurrentNode, aGoForward, bNoBlockCrossing);
  
  if (!candidate) {
    return nsnull;
  }

  if (!aEditableNode || IsEditable(candidate)) {
    return candidate;
  }

  return FindNode(candidate, aGoForward, aEditableNode, bNoBlockCrossing);
}

already_AddRefed<nsIDOMNode>
nsEditor::GetRightmostChild(nsIDOMNode *aCurrentNode, 
                            bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nsnull);
  nsCOMPtr<nsIDOMNode> resultNode, temp = aCurrentNode;
  bool hasChildren;
  aCurrentNode->HasChildNodes(&hasChildren);
  while (hasChildren) {
    temp->GetLastChild(getter_AddRefs(resultNode));
    if (resultNode) {
      if (bNoBlockCrossing && IsBlockNode(resultNode)) {
        return resultNode.forget();
      }
      resultNode->HasChildNodes(&hasChildren);
      temp = resultNode;
    } else {
      hasChildren = false;
    }
  }

  return resultNode.forget();
}

nsIContent*
nsEditor::GetRightmostChild(nsINode *aCurrentNode,
                            bool     bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nsnull);
  nsIContent *cur = aCurrentNode->GetLastChild();
  if (!cur) {
    return nsnull;
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
  return nsnull;
}

nsIContent*
nsEditor::GetLeftmostChild(nsINode *aCurrentNode,
                           bool     bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nsnull);
  nsIContent *cur = aCurrentNode->GetFirstChild();
  if (!cur) {
    return nsnull;
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
  return nsnull;
}

already_AddRefed<nsIDOMNode>
nsEditor::GetLeftmostChild(nsIDOMNode *aCurrentNode,
                           bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nsnull);
  nsCOMPtr<nsIDOMNode> resultNode, temp = aCurrentNode;
  bool hasChildren;
  aCurrentNode->HasChildNodes(&hasChildren);
  while (hasChildren) {
    temp->GetFirstChild(getter_AddRefs(resultNode));
    if (resultNode) {
      if (bNoBlockCrossing && IsBlockNode(resultNode)) {
        return resultNode.forget();
      }
      resultNode->HasChildNodes(&hasChildren);
      temp = resultNode;
    } else {
      hasChildren = false;
    }
  }

  return resultNode.forget();
}

bool
nsEditor::IsBlockNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsBlockNode(node);
}

bool
nsEditor::IsBlockNode(nsINode* aNode)
{
  // stub to be overridden in nsHTMLEditor.
  // screwing around with the class hierarchy here in order
  // to not duplicate the code in GetNextNode/GetPrevNode
  // across both nsEditor/nsHTMLEditor.
  return false;
}

bool
nsEditor::CanContain(nsIDOMNode* aParent, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(parent, false);

  switch (parent->NodeType()) {
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContain(parent->Tag(), aChild);
  }
  return false;
}

bool
nsEditor::CanContainTag(nsIDOMNode* aParent, nsIAtom* aChildTag)
{
  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(parent, false);

  switch (parent->NodeType()) {
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContainTag(parent->Tag(), aChildTag);
  }
  return false;
}

bool 
nsEditor::TagCanContain(nsIAtom* aParentTag, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIContent> child = do_QueryInterface(aChild);
  NS_ENSURE_TRUE(child, false);

  switch (child->NodeType()) {
  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContainTag(aParentTag, child->Tag());
  }
  return false;
}

bool 
nsEditor::TagCanContainTag(nsIAtom* aParentTag, nsIAtom* aChildTag)
{
  return true;
}

bool
nsEditor::IsRoot(nsIDOMNode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetRoot());

  return inNode == rootNode;
}

bool 
nsEditor::IsRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsINode> rootNode = GetRoot();

  return inNode == rootNode;
}

bool
nsEditor::IsEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsINode> rootNode = GetEditorRoot();
  return aNode == rootNode;
}

bool 
nsEditor::IsDescendantOfRoot(nsIDOMNode* inNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  return IsDescendantOfRoot(node);
}

bool
nsEditor::IsDescendantOfRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);
  nsCOMPtr<nsIContent> root = GetRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(inNode, root);
}

bool
nsEditor::IsDescendantOfEditorRoot(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsDescendantOfEditorRoot(node);
}

bool
nsEditor::IsDescendantOfEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsIContent> root = GetEditorRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(aNode, root);
}

bool 
nsEditor::IsContainer(nsIDOMNode *aNode)
{
  return aNode ? true : false;
}

bool
nsEditor::IsTextInDirtyFrameVisible(nsIContent *aNode)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aNode->NodeType() == nsIDOMNode::TEXT_NODE);

  // virtual method
  //
  // If this is a simple non-html editor,
  // the best we can do is to assume it's visible.

  return true;
}

static inline bool
IsElementVisible(dom::Element* aElement)
{
  if (aElement->GetPrimaryFrame()) {
    // It's visible, for our purposes
    return true;
  }

  nsIContent *cur = aElement;
  for (; ;) {
    // Walk up the tree looking for the nearest ancestor with a frame.
    // The state of the child right below it will determine whether
    // we might possibly have a frame or not.
    bool haveLazyBitOnChild = cur->HasFlag(NODE_NEEDS_FRAME);
    cur = cur->GetFlattenedTreeParent();
    if (!cur) {
      if (!haveLazyBitOnChild) {
        // None of our ancestors have lazy bits set, so we shouldn't
        // have a frame
        return false;
      }

      // The root has a lazy frame construction bit.  We need to check
      // our style.
      break;
    }

    if (cur->GetPrimaryFrame()) {
      if (!haveLazyBitOnChild) {
        // Our ancestor directly under |cur| doesn't have lazy bits;
        // that means we won't get a frame
        return false;
      }

      if (cur->GetPrimaryFrame()->IsLeaf()) {
        // Nothing under here will ever get frames
        return false;
      }

      // Otherwise, we might end up with a frame when that lazy bit is
      // processed.  Figure out our actual style.
      break;
    }
  }

  // Now it might be that we have no frame because we're in a
  // display:none subtree, or it might be that we're just dealing with
  // lazy frame construction and it hasn't happened yet.  Check which
  // one it is.
  nsRefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nsnull, nsnull);
  if (styleContext) {
    return styleContext->GetStyleDisplay()->mDisplay != NS_STYLE_DISPLAY_NONE;
  }
  return false;
}

bool 
nsEditor::IsEditable(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return IsEditable(content);
}

bool
nsEditor::IsEditable(nsIContent *aNode)
{
  NS_ENSURE_TRUE(aNode, false);

  if (IsMozEditorBogusNode(aNode) || !IsModifiableNode(aNode)) return false;

  // see if it has a frame.  If so, we'll edit it.
  // special case for textnodes: frame must have width.
  if (aNode->IsElement() && !IsElementVisible(aNode->AsElement())) {
    // If the element has no frame, it's not editable.  Note that we
    // need to check IsElement() here, because some of our tests
    // rely on frameless textnodes being visible.
    return false;
  }
  switch (aNode->NodeType()) {
    case nsIDOMNode::ELEMENT_NODE:
      return true; // not a text node; not invisible
    case nsIDOMNode::TEXT_NODE:
      return IsTextInDirtyFrameVisible(aNode);
    default:
      return false;
  }
}

bool
nsEditor::IsMozEditorBogusNode(nsIContent *element)
{
  return element &&
         element->AttrValueIs(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                              kMOZEditorBogusNodeValue, eCaseMatters);
}

PRUint32
nsEditor::CountEditableChildren(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  PRUint32 count = 0;
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (IsEditable(child)) {
      ++count;
    }
  }
  return count;
}

//END nsEditor static utility methods


NS_IMETHODIMP nsEditor::IncrementModificationCount(PRInt32 inNumMods)
{
  PRUint32 oldModCount = mModCount;

  mModCount += inNumMods;

  if ((oldModCount == 0 && mModCount != 0)
   || (oldModCount != 0 && mModCount == 0))
    NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}


NS_IMETHODIMP nsEditor::GetModificationCount(PRInt32 *outModCount)
{
  NS_ENSURE_ARG_POINTER(outModCount);
  *outModCount = mModCount;
  return NS_OK;
}


NS_IMETHODIMP nsEditor::ResetModificationCount()
{
  bool doNotify = (mModCount != 0);

  mModCount = 0;

  if (doNotify)
    NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}

//END nsEditor Private methods



///////////////////////////////////////////////////////////////////////////
// GetTag: digs out the atom for the tag of this node
//
nsIAtom *
nsEditor::GetTag(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);

  if (!content) 
  {
    NS_ASSERTION(aNode, "null node passed to nsEditor::Tag()");

    return nsnull;
  }
  
  return content->Tag();
}


///////////////////////////////////////////////////////////////////////////
// GetTagString: digs out string for the tag of this node
//                    
nsresult 
nsEditor::GetTagString(nsIDOMNode *aNode, nsAString& outString)
{
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsEditor::GetTag()");
    return NS_ERROR_NULL_POINTER;
  }
  
  nsIAtom *atom = GetTag(aNode);
  if (!atom)
  {
    return NS_ERROR_FAILURE;
  }

  atom->ToString(outString);
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// NodesSameType: do these nodes have the same tag?
//                    
bool 
nsEditor::NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2) {
    NS_NOTREACHED("null node passed to nsEditor::NodesSameType()");
    return false;
  }

  nsCOMPtr<nsIContent> content1 = do_QueryInterface(aNode1);
  NS_ENSURE_TRUE(content1, false);

  nsCOMPtr<nsIContent> content2 = do_QueryInterface(aNode2);
  NS_ENSURE_TRUE(content2, false);

  return AreNodesSameType(content1, content2);
}

/* virtual */
bool
nsEditor::AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2)
{
  MOZ_ASSERT(aNode1);
  MOZ_ASSERT(aNode2);
  return aNode1->Tag() == aNode2->Tag();
}


///////////////////////////////////////////////////////////////////////////
// IsTextNode: true if node of dom type text
//               
bool
nsEditor::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return false;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  return (nodeType == nsIDOMNode::TEXT_NODE);
}

bool
nsEditor::IsTextNode(nsINode *aNode)
{
  return aNode->NodeType() == nsIDOMNode::TEXT_NODE;
}

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
nsCOMPtr<nsIDOMNode> 
nsEditor::GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  
  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);

  NS_ENSURE_TRUE(parent, resultNode);

  resultNode = do_QueryInterface(parent->GetChildAt(aOffset));

  return resultNode;
}

///////////////////////////////////////////////////////////////////////////
// GetNodeAtRangeOffsetPoint: returns the node at this position in a range,
// assuming that aParentOrNode is the node itself if it's a text node, or
// the node's parent otherwise.
//
nsCOMPtr<nsIDOMNode>
nsEditor::GetNodeAtRangeOffsetPoint(nsIDOMNode* aParentOrNode, PRInt32 aOffset)
{
  if (IsTextNode(aParentOrNode)) {
    return aParentOrNode;
  }
  return GetChildAt(aParentOrNode, aOffset);
}


///////////////////////////////////////////////////////////////////////////
// GetStartNodeAndOffset: returns whatever the start parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetStartNodeAndOffset(nsISelection *aSelection,
                                       nsIDOMNode **outStartNode,
                                       PRInt32 *outStartOffset)
{
  NS_ENSURE_TRUE(outStartNode && outStartOffset && aSelection, NS_ERROR_NULL_POINTER);

  *outStartNode = nsnull;
  *outStartOffset = 0;

  nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(aSelection));
  nsCOMPtr<nsIEnumerator> enumerator;
  nsresult result = selPrivate->GetEnumerator(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_FAILURE);

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  NS_ENSURE_SUCCESS(result, result);

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  result = range->GetStartContainer(outStartNode);
  NS_ENSURE_SUCCESS(result, result);

  result = range->GetStartOffset(outStartOffset);
  NS_ENSURE_SUCCESS(result, result);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetEndNodeAndOffset: returns whatever the end parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetEndNodeAndOffset(nsISelection *aSelection,
                                       nsIDOMNode **outEndNode,
                                       PRInt32 *outEndOffset)
{
  NS_ENSURE_TRUE(outEndNode && outEndOffset, NS_ERROR_NULL_POINTER);

  *outEndNode = nsnull;
    
  nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(aSelection));
  nsCOMPtr<nsIEnumerator> enumerator;
  nsresult result = selPrivate->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result) || !enumerator)
    return NS_ERROR_FAILURE;
    
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  if (NS_FAILED(enumerator->CurrentItem(getter_AddRefs(currentItem))))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);
    
  if (NS_FAILED(range->GetEndContainer(outEndNode)))
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetEndOffset(outEndOffset)))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPreformatted: checks the style info for the node for the preformatted
//                 text style.
nsresult 
nsEditor::IsPreformatted(nsIDOMNode *aNode, bool *aResult)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  
  NS_ENSURE_TRUE(aResult && content, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  // Look at the node (and its parent if it's not an element), and grab its style context
  nsRefPtr<nsStyleContext> elementStyle;
  if (!content->IsElement()) {
    content = content->GetParent();
  }
  if (content && content->IsElement()) {
    elementStyle = nsComputedDOMStyle::GetStyleContextForElement(content->AsElement(),
                                                                 nsnull,
                                                                 ps);
  }

  if (!elementStyle)
  {
    // Consider nodes without a style context to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no style context).
    *aResult = false;
    return NS_OK;
  }

  const nsStyleText* styleText = elementStyle->GetStyleText();

  *aResult = styleText->WhiteSpaceIsSignificant();
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// SplitNodeDeep: this splits a node "deeply", splitting children as 
//                appropriate.  The place to split is represented by
//                a dom point at {splitPointParent, splitPointOffset}.
//                That dom point must be inside aNode, which is the node to 
//                split.  outOffset is set to the offset in the parent of aNode where
//                the split terminates - where you would want to insert 
//                a new element, for instance, if that's why you were splitting 
//                the node.
//
nsresult
nsEditor::SplitNodeDeep(nsIDOMNode *aNode, 
                        nsIDOMNode *aSplitPointParent, 
                        PRInt32 aSplitPointOffset,
                        PRInt32 *outOffset,
                        bool    aNoEmptyContainers,
                        nsCOMPtr<nsIDOMNode> *outLeftNode,
                        nsCOMPtr<nsIDOMNode> *outRightNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node && aSplitPointParent && outOffset, NS_ERROR_NULL_POINTER);
  PRInt32 offset = aSplitPointOffset;

  if (outLeftNode)  *outLeftNode  = nsnull;
  if (outRightNode) *outRightNode = nsnull;

  nsCOMPtr<nsINode> nodeToSplit = do_QueryInterface(aSplitPointParent);
  while (nodeToSplit) {
    // need to insert rules code call here to do things like
    // not split a list if you are after the last <li> or before the first, etc.
    // for now we just have some smarts about unneccessarily splitting
    // textnodes, which should be universal enough to put straight in
    // this nsEditor routine.
    
    nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(nodeToSplit);
    PRUint32 len = nodeToSplit->Length();
    bool bDoSplit = false;
    
    if (!(aNoEmptyContainers || nodeAsText) || (offset && (offset != (PRInt32)len)))
    {
      bDoSplit = true;
      nsCOMPtr<nsIDOMNode> tempNode;
      nsresult rv = SplitNode(nodeToSplit->AsDOMNode(), offset,
                              getter_AddRefs(tempNode));
      NS_ENSURE_SUCCESS(rv, rv);

      if (outRightNode) {
        *outRightNode = nodeToSplit->AsDOMNode();
      }
      if (outLeftNode) {
        *outLeftNode = tempNode;
      }
    }

    nsINode* parentNode = nodeToSplit->GetNodeParent();
    NS_ENSURE_TRUE(parentNode, NS_ERROR_FAILURE);

    if (!bDoSplit && offset) {
      // must be "end of text node" case, we didn't split it, just move past it
      offset = parentNode->IndexOf(nodeToSplit) + 1;
      if (outLeftNode) {
        *outLeftNode = nodeToSplit->AsDOMNode();
      }
    } else {
      offset = parentNode->IndexOf(nodeToSplit);
      if (outRightNode) {
        *outRightNode = nodeToSplit->AsDOMNode();
      }
    }

    if (nodeToSplit == node) {
      // we split all the way up to (and including) aNode; we're done
      break;
    }

    nodeToSplit = parentNode;
  }

  if (!nodeToSplit) {
    NS_NOTREACHED("null node obtained in nsEditor::SplitNodeDeep()");
    return NS_ERROR_FAILURE;
  }

  *outOffset = offset;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// JoinNodeDeep:  this joins two like nodes "deeply", joining children as 
//                appropriate.  
nsresult
nsEditor::JoinNodeDeep(nsIDOMNode *aLeftNode, 
                       nsIDOMNode *aRightNode,
                       nsCOMPtr<nsIDOMNode> *aOutJoinNode, 
                       PRInt32 *outOffset)
{
  NS_ENSURE_TRUE(aLeftNode && aRightNode && aOutJoinNode && outOffset, NS_ERROR_NULL_POINTER);

  // while the rightmost children and their descendants of the left node 
  // match the leftmost children and their descendants of the right node
  // join them up.  Can you say that three times fast?
  
  nsCOMPtr<nsIDOMNode> leftNodeToJoin = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsIDOMNode> rightNodeToJoin = do_QueryInterface(aRightNode);
  nsCOMPtr<nsIDOMNode> parentNode,tmp;
  nsresult res = NS_OK;
  
  rightNodeToJoin->GetParentNode(getter_AddRefs(parentNode));
  
  while (leftNodeToJoin && rightNodeToJoin && parentNode &&
          NodesSameType(leftNodeToJoin, rightNodeToJoin))
  {
    // adjust out params
    PRUint32 length;
    res = GetLengthOfDOMNode(leftNodeToJoin, length);
    NS_ENSURE_SUCCESS(res, res);
    
    *aOutJoinNode = rightNodeToJoin;
    *outOffset = length;
    
    // do the join
    res = JoinNodes(leftNodeToJoin, rightNodeToJoin, parentNode);
    NS_ENSURE_SUCCESS(res, res);
    
    if (IsTextNode(parentNode)) // we've joined all the way down to text nodes, we're done!
      return NS_OK;

    else
    {
      // get new left and right nodes, and begin anew
      parentNode = rightNodeToJoin;
      leftNodeToJoin = GetChildAt(parentNode, length-1);
      rightNodeToJoin = GetChildAt(parentNode, length);

      // skip over non-editable nodes
      while (leftNodeToJoin && !IsEditable(leftNodeToJoin))
      {
        leftNodeToJoin->GetPreviousSibling(getter_AddRefs(tmp));
        leftNodeToJoin = tmp;
      }
      if (!leftNodeToJoin) break;
    
      while (rightNodeToJoin && !IsEditable(rightNodeToJoin))
      {
        rightNodeToJoin->GetNextSibling(getter_AddRefs(tmp));
        rightNodeToJoin = tmp;
      }
      if (!rightNodeToJoin) break;
    }
  }
  
  return res;
}

void
nsEditor::BeginUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount >= 0, "bad state");

  if (0 == mUpdateCount)
  {
    // Turn off selection updates and notifications.

    nsCOMPtr<nsISelection> selection;
    GetSelection(getter_AddRefs(selection));

    if (selection) 
    {
      nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
      selPrivate->StartBatchChanges();
    }
  }

  mUpdateCount++;
}


nsresult nsEditor::EndUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount > 0, "bad state");
  
  if (mUpdateCount <= 0)
  {
    mUpdateCount = 0;
    return NS_ERROR_FAILURE;
  }

  mUpdateCount--;

  if (0 == mUpdateCount)
  {
    // Hide the caret with an StCaretHider. By the time it goes out
    // of scope and tries to show the caret, reflow and selection changed
    // notifications should've happened so the caret should have enough info
    // to draw at the correct position.

    nsRefPtr<nsCaret> caret;
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();

    if (presShell)
      caret = presShell->GetCaret();

    StCaretHider caretHider(caret);

    // Turn selection updating and notifications back on.

    nsCOMPtr<nsISelection>selection;
    GetSelection(getter_AddRefs(selection));

    if (selection) {
      nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(selection));
      selPrivate->EndBatchChanges();
    }
  }

  return NS_OK;
}

bool 
nsEditor::GetShouldTxnSetSelection()
{
  return mShouldTxnSetSelection;
}


NS_IMETHODIMP 
nsEditor::DeleteSelectionImpl(EDirection aAction,
                              EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  nsRefPtr<EditAggregateTxn> txn;
  nsCOMPtr<nsINode> deleteNode;
  PRInt32 deleteCharOffset = 0, deleteCharLength = 0;
  res = CreateTxnForDeleteSelection(aAction, getter_AddRefs(txn),
                                    getter_AddRefs(deleteNode),
                                    &deleteCharOffset, &deleteCharLength);
  nsCOMPtr<nsIDOMCharacterData> deleteCharData(do_QueryInterface(deleteNode));

  if (NS_SUCCEEDED(res))  
  {
    nsAutoRules beginRulesSniffing(this, kOpDeleteSelection, aAction);
    PRInt32 i;
    // Notify nsIEditActionListener::WillDelete[Selection|Text|Node]
    if (!deleteNode)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->WillDeleteSelection(selection);
    else if (deleteCharData)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->WillDeleteText(deleteCharData, deleteCharOffset, 1);
    else
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->WillDeleteNode(deleteNode->AsDOMNode());

    // Delete the specified amount
    res = DoTransaction(txn);  

    // Notify nsIEditActionListener::DidDelete[Selection|Text|Node]
    if (!deleteNode)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->DidDeleteSelection(selection);
    else if (deleteCharData)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->DidDeleteText(deleteCharData, deleteCharOffset, 1, res);
    else
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->DidDeleteNode(deleteNode->AsDOMNode(), res);
  }

  return res;
}

// XXX: error handling in this routine needs to be cleaned up!
NS_IMETHODIMP
nsEditor::DeleteSelectionAndCreateNode(const nsAString& aTag,
                                           nsIDOMNode ** aNewNode)
{
  nsresult result = DeleteSelectionAndPrepareToCreateNode();
  NS_ENSURE_SUCCESS(result, result);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  PRInt32 offset = selection->GetAnchorOffset();

  nsCOMPtr<nsIDOMNode> newNode;
  result = CreateNode(aTag, node->AsDOMNode(), offset,
                      getter_AddRefs(newNode));
  // XXX: ERROR_HANDLING  check result, and make sure aNewNode is set correctly
  // in success/failure cases
  *aNewNode = newNode;
  NS_IF_ADDREF(*aNewNode);

  // we want the selection to be just after the new node
  return selection->Collapse(node, offset + 1);
}


/* Non-interface, protected methods */

PRInt32
nsEditor::GetIMEBufferLength()
{
  return mIMEBufferLength;
}

void
nsEditor::SetIsIMEComposing(){  
  // We set mIsIMEComposing according to mIMETextRangeList.
  nsCOMPtr<nsIPrivateTextRange> rangePtr;
  PRUint16 listlen, type;

  mIsIMEComposing = false;
  listlen = mIMETextRangeList->GetLength();

  for (PRUint16 i = 0; i < listlen; i++)
  {
      rangePtr = mIMETextRangeList->Item(i);
      if (!rangePtr) continue;
      nsresult result = rangePtr->GetRangeType(&type);
      if (NS_FAILED(result)) continue;
      if ( type == nsIPrivateTextRange::TEXTRANGE_RAWINPUT ||
           type == nsIPrivateTextRange::TEXTRANGE_CONVERTEDTEXT ||
           type == nsIPrivateTextRange::TEXTRANGE_SELECTEDRAWTEXT ||
           type == nsIPrivateTextRange::TEXTRANGE_SELECTEDCONVERTEDTEXT )
      {
        mIsIMEComposing = true;
#ifdef DEBUG_IME
        printf("nsEditor::mIsIMEComposing = true\n");
#endif
        break;
      }
  }
  return;
}

bool
nsEditor::IsIMEComposing() {
  return mIsIMEComposing;
}

nsresult
nsEditor::DeleteSelectionAndPrepareToCreateNode()
{
  nsresult res;
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  MOZ_ASSERT(selection->GetAnchorFocusRange());

  if (!selection->GetAnchorFocusRange()->Collapsed()) {
    res = DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);

    MOZ_ASSERT(selection->GetAnchorFocusRange() &&
               selection->GetAnchorFocusRange()->Collapsed(),
               "Selection not collapsed after delete");
  }

  // If the selection is a chardata node, split it if necessary and compute
  // where to put the new node
  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  MOZ_ASSERT(node, "Selection has no ranges in it");

  if (node && node->IsNodeOfType(nsINode::eDATA_NODE)) {
    NS_ASSERTION(node->GetNodeParent(),
                 "It's impossible to insert into chardata with no parent -- "
                 "fix the caller");
    NS_ENSURE_STATE(node->GetNodeParent());

    PRInt32 offset = selection->GetAnchorOffset();

    if (offset == 0) {
      res = selection->Collapse(node->GetNodeParent(),
                                node->GetNodeParent()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    } else if (offset == (PRInt32)node->Length()) {
      res = selection->Collapse(node->GetNodeParent(),
                                node->GetNodeParent()->IndexOf(node) + 1);
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    } else {
      nsCOMPtr<nsIDOMNode> tmp;
      res = SplitNode(node->AsDOMNode(), offset, getter_AddRefs(tmp));
      NS_ENSURE_SUCCESS(res, res);
      res = selection->Collapse(node->GetNodeParent(),
                                node->GetNodeParent()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}



void
nsEditor::DoAfterDoTransaction(nsITransaction *aTxn)
{
  bool isTransientTransaction;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    aTxn->GetIsTransient(&isTransientTransaction)));
  
  if (!isTransientTransaction)
  {
    // we need to deal here with the case where the user saved after some
    // edits, then undid one or more times. Then, the undo count is -ve,
    // but we can't let a do take it back to zero. So we flip it up to
    // a +ve number.
    PRInt32 modCount;
    GetModificationCount(&modCount);
    if (modCount < 0)
      modCount = -modCount;
        
    // don't count transient transactions
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      IncrementModificationCount(1)));
  }
}


void
nsEditor::DoAfterUndoTransaction()
{
  // all undoable transactions are non-transient
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    IncrementModificationCount(-1)));
}

void
nsEditor::DoAfterRedoTransaction()
{
  // all redoable transactions are non-transient
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    IncrementModificationCount(1)));
}

NS_IMETHODIMP 
nsEditor::CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                   const nsAString& aAttribute, 
                                   const nsAString& aValue,
                                   ChangeAttributeTxn ** aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsRefPtr<ChangeAttributeTxn> txn = new ChangeAttributeTxn();

  nsresult rv = txn->Init(this, aElement, aAttribute, aValue, false);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                      const nsAString& aAttribute,
                                      ChangeAttributeTxn ** aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsRefPtr<ChangeAttributeTxn> txn = new ChangeAttributeTxn();

  nsresult rv = txn->Init(this, aElement, aAttribute, EmptyString(), true);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP nsEditor::CreateTxnForCreateElement(const nsAString& aTag,
                                                  nsIDOMNode     *aParent,
                                                  PRInt32         aPosition,
                                                  CreateElementTxn ** aTxn)
{
  NS_ENSURE_TRUE(aParent, NS_ERROR_NULL_POINTER);

  nsRefPtr<CreateElementTxn> txn = new CreateElementTxn();

  nsresult rv = txn->Init(this, aTag, aParent, aPosition);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertElement(nsIDOMNode * aNode,
                                                  nsIDOMNode * aParent,
                                                  PRInt32      aPosition,
                                                  InsertElementTxn ** aTxn)
{
  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);

  nsRefPtr<InsertElementTxn> txn = new InsertElementTxn();

  nsresult rv = txn->Init(aNode, aParent, aPosition, this);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}

nsresult
nsEditor::CreateTxnForDeleteNode(nsINode* aNode, DeleteNodeTxn** aTxn)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsRefPtr<DeleteNodeTxn> txn = new DeleteNodeTxn();

  nsresult res = txn->Init(this, aNode, &mRangeUpdater);
  NS_ENSURE_SUCCESS(res, res);

  txn.forget(aTxn);
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::CreateTxnForIMEText(const nsAString& aStringToInsert,
                              IMETextTxn ** aTxn)
{
  NS_ASSERTION(aTxn, "illegal value- null ptr- aTxn");
     
  nsRefPtr<IMETextTxn> txn = new IMETextTxn();

  nsresult rv = txn->Init(mIMETextNode, mIMETextOffset, mIMEBufferLength,
                          mIMETextRangeList, aStringToInsert, this);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForAddStyleSheet(nsCSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn)
{
  nsRefPtr<AddStyleSheetTxn> txn = new AddStyleSheetTxn();

  nsresult rv = txn->Init(this, aSheet);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}



NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveStyleSheet(nsCSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn)
{
  nsRefPtr<RemoveStyleSheetTxn> txn = new RemoveStyleSheetTxn();

  nsresult rv = txn->Init(this, aSheet);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


nsresult
nsEditor::CreateTxnForDeleteSelection(EDirection aAction,
                                      EditAggregateTxn** aTxn,
                                      nsINode** aNode,
                                      PRInt32* aOffset,
                                      PRInt32* aLength)
{
  MOZ_ASSERT(aTxn);
  *aTxn = nsnull;

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  // Check whether the selection is collapsed and we should do nothing:
  if (selection->Collapsed() && aAction == eNone) {
    return NS_OK;
  }

  // allocate the out-param transaction
  nsRefPtr<EditAggregateTxn> aggTxn = new EditAggregateTxn();

  nsSelectionIterator iter(selection);
  for (iter.First(); iter.IsDone() != NS_OK; iter.Next()) {
    nsRefPtr<nsRange> range = iter.CurrentItem();
    NS_ENSURE_STATE(range);

    // Same with range as with selection; if it is collapsed and action
    // is eNone, do nothing.
    if (!range->Collapsed()) {
      nsRefPtr<DeleteRangeTxn> txn = new DeleteRangeTxn();
      txn->Init(this, range, &mRangeUpdater);
      aggTxn->AppendChild(txn);
    } else if (aAction != eNone) {
      // we have an insertion point.  delete the thing in front of it or
      // behind it, depending on aAction
      nsresult res = CreateTxnForDeleteInsertionPoint(range, aAction, aggTxn,
                                                      aNode, aOffset, aLength);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  aggTxn.forget(aTxn);

  return NS_OK;
}

nsresult
nsEditor::CreateTxnForDeleteCharacter(nsIDOMCharacterData* aData,
                                      PRUint32             aOffset,
                                      EDirection           aDirection,
                                      DeleteTextTxn**      aTxn)
{
  NS_ASSERTION(aDirection == eNext || aDirection == ePrevious,
               "invalid direction");
  nsAutoString data;
  aData->GetData(data);
  NS_ASSERTION(data.Length(), "Trying to delete from a zero-length node");
  NS_ENSURE_STATE(data.Length());

  PRUint32 segOffset = aOffset, segLength = 1;
  if (aDirection == eNext) {
    if (segOffset + 1 < data.Length() &&
        NS_IS_HIGH_SURROGATE(data[segOffset]) &&
        NS_IS_LOW_SURROGATE(data[segOffset+1])) {
      // delete both halves of the surrogate pair
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
    return NS_ERROR_FAILURE;
  }
  return CreateTxnForDeleteText(aData, segOffset, segLength, aTxn);
}

//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior
//are not implemented
nsresult
nsEditor::CreateTxnForDeleteInsertionPoint(nsRange*          aRange,
                                           EDirection        aAction,
                                           EditAggregateTxn* aTxn,
                                           nsINode**         aNode,
                                           PRInt32*          aOffset,
                                           PRInt32*          aLength)
{
  MOZ_ASSERT(aAction != eNone);

  nsresult res;

  // get the node and offset of the insertion point
  nsCOMPtr<nsINode> node = aRange->GetStartParent();
  NS_ENSURE_STATE(node);

  PRInt32 offset = aRange->StartOffset();

  // determine if the insertion point is at the beginning, middle, or end of
  // the node
  nsCOMPtr<nsIDOMCharacterData> nodeAsCharData = do_QueryInterface(node);

  PRUint32 count = node->Length();

  bool isFirst = (0 == offset);
  bool isLast  = (count == (PRUint32)offset);

  // XXX: if isFirst && isLast, then we'll need to delete the node
  //      as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if (aAction == ePrevious && isFirst) {
    // we're backspacing from the beginning of the node.  Delete the first
    // thing to our left
    nsCOMPtr<nsIContent> priorNode = GetPriorNode(node, true);
    NS_ENSURE_STATE(priorNode);

    // there is a priorNode, so delete its last child (if chardata, delete the
    // last char). if it has no children, delete it
    nsCOMPtr<nsIDOMCharacterData> priorNodeAsCharData =
      do_QueryInterface(priorNode);
    if (priorNodeAsCharData) {
      PRUint32 length = priorNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      NS_ENSURE_STATE(length);
      nsRefPtr<DeleteTextTxn> txn;
      res = CreateTxnForDeleteCharacter(priorNodeAsCharData, length,
                                        ePrevious, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);

      *aOffset = txn->GetOffset();
      *aLength = txn->GetNumCharsToDelete();
      aTxn->AppendChild(txn);
    } else {
      // priorNode is not chardata, so tell its parent to delete it
      nsRefPtr<DeleteNodeTxn> txn;
      res = CreateTxnForDeleteNode(priorNode, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);

      aTxn->AppendChild(txn);
    }

    NS_ADDREF(*aNode = priorNode);

    return NS_OK;
  }

  if (aAction == eNext && isLast) {
    // we're deleting from the end of the node.  Delete the first thing to our
    // right
    nsCOMPtr<nsIContent> nextNode = GetNextNode(node, true);
    NS_ENSURE_STATE(nextNode);

    // there is a nextNode, so delete its first child (if chardata, delete the
    // first char). if it has no children, delete it
    nsCOMPtr<nsIDOMCharacterData> nextNodeAsCharData =
      do_QueryInterface(nextNode);
    if (nextNodeAsCharData) {
      PRUint32 length = nextNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      NS_ENSURE_STATE(length);
      nsRefPtr<DeleteTextTxn> txn;
      res = CreateTxnForDeleteCharacter(nextNodeAsCharData, 0, eNext,
                                        getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);

      *aOffset = txn->GetOffset();
      *aLength = txn->GetNumCharsToDelete();
      aTxn->AppendChild(txn);
    } else {
      // nextNode is not chardata, so tell its parent to delete it
      nsRefPtr<DeleteNodeTxn> txn;
      res = CreateTxnForDeleteNode(nextNode, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);
      aTxn->AppendChild(txn);
    }

    NS_ADDREF(*aNode = nextNode);

    return NS_OK;
  }

  if (nodeAsCharData) {
    // we have chardata, so delete a char at the proper offset
    nsRefPtr<DeleteTextTxn> txn;
    res = CreateTxnForDeleteCharacter(nodeAsCharData, offset, aAction,
                                      getter_AddRefs(txn));
    NS_ENSURE_SUCCESS(res, res);

    aTxn->AppendChild(txn);
    NS_ADDREF(*aNode = node);
    *aOffset = txn->GetOffset();
    *aLength = txn->GetNumCharsToDelete();
  } else {
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
    NS_ENSURE_STATE(selectedNode);

    nsCOMPtr<nsIDOMCharacterData> selectedNodeAsCharData =
      do_QueryInterface(selectedNode);
    if (selectedNodeAsCharData) {
      // we are deleting from a chardata node, so do a character deletion
      PRUint32 position = 0;
      if (aAction == ePrevious) {
        position = selectedNode->Length();
      }
      nsRefPtr<DeleteTextTxn> delTextTxn;
      res = CreateTxnForDeleteCharacter(selectedNodeAsCharData, position,
                                        aAction, getter_AddRefs(delTextTxn));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(delTextTxn, NS_ERROR_NULL_POINTER);

      aTxn->AppendChild(delTextTxn);
      *aOffset = delTextTxn->GetOffset();
      *aLength = delTextTxn->GetNumCharsToDelete();
    } else {
      nsRefPtr<DeleteNodeTxn> delElementTxn;
      res = CreateTxnForDeleteNode(selectedNode, getter_AddRefs(delElementTxn));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(delElementTxn, NS_ERROR_NULL_POINTER);

      aTxn->AppendChild(delElementTxn);
    }

    NS_ADDREF(*aNode = selectedNode);
  }

  return NS_OK;
}

nsresult 
nsEditor::CreateRange(nsIDOMNode *aStartParent, PRInt32 aStartOffset,
                      nsIDOMNode *aEndParent, PRInt32 aEndOffset,
                      nsIDOMRange **aRange)
{
  return nsRange::CreateRange(aStartParent, aStartOffset, aEndParent,
                              aEndOffset, aRange);
}

nsresult 
nsEditor::AppendNodeToSelectionAsRange(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  if(!selection) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> parentNode;
  res = aNode->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);
  
  PRInt32 offset = GetChildOffset(aNode, parentNode);
  
  nsCOMPtr<nsIDOMRange> range;
  res = CreateRange(parentNode, offset, parentNode, offset+1, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

  return selection->AddRange(range);
}

nsresult nsEditor::ClearSelection()
{
  nsCOMPtr<nsISelection> selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  return selection->RemoveAllRanges();  
}

nsresult
nsEditor::CreateHTMLContent(const nsAString& aTag, dom::Element** aContent)
{
  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // XXX Wallpaper over editor bug (editor tries to create elements with an
  //     empty nodename).
  if (aTag.IsEmpty()) {
    NS_ERROR("Don't pass an empty tag to nsEditor::CreateHTMLContent, "
             "check caller.");
    return NS_ERROR_FAILURE;
  }

  return doc->CreateElem(aTag, nsnull, kNameSpaceID_XHTML,
                         reinterpret_cast<nsIContent**>(aContent));
}

nsresult
nsEditor::SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                   const nsAString & aAttribute,
                                   const nsAString & aValue,
                                   bool aSuppressTransaction)
{
  return SetAttribute(aElement, aAttribute, aValue);
}

nsresult
nsEditor::RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      bool aSuppressTransaction)
{
  return RemoveAttribute(aElement, aAttribute);
}

nsresult
nsEditor::HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent)
{
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/text/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/html/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  nsKeyEvent* nativeKeyEvent = GetNativeKeyEvent(aKeyEvent);
  NS_ENSURE_TRUE(nativeKeyEvent, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(nativeKeyEvent->message == NS_KEY_PRESS,
               "HandleKeyPressEvent gets non-keypress event");

  // if we are readonly or disabled, then do nothing.
  if (IsReadonly() || IsDisabled()) {
    // consume backspace for disabled and readonly textfields, to prevent
    // back in history, which could be confusing to users
    if (nativeKeyEvent->keyCode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE) {
      aKeyEvent->PreventDefault();
    }
    return NS_OK;
  }

  switch (nativeKeyEvent->keyCode) {
    case nsIDOMKeyEvent::DOM_VK_META:
    case nsIDOMKeyEvent::DOM_VK_SHIFT:
    case nsIDOMKeyEvent::DOM_VK_CONTROL:
    case nsIDOMKeyEvent::DOM_VK_ALT:
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
    case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
      if (nativeKeyEvent->IsControl() || nativeKeyEvent->IsAlt() ||
          nativeKeyEvent->IsMeta()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
    case nsIDOMKeyEvent::DOM_VK_DELETE:
      // on certain platforms (such as windows) the shift key
      // modifies what delete does (cmd_cut in this case).
      // bailing here to allow the keybindings to do the cut.
      if (nativeKeyEvent->IsShift() || nativeKeyEvent->IsControl() ||
          nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK; 
  }
  return NS_OK;
}

nsresult
nsEditor::HandleInlineSpellCheck(OperationID action,
                                   nsISelection *aSelection,
                                   nsIDOMNode *previousSelectedNode,
                                   PRInt32 previousSelectedOffset,
                                   nsIDOMNode *aStartNode,
                                   PRInt32 aStartOffset,
                                   nsIDOMNode *aEndNode,
                                   PRInt32 aEndOffset)
{
  return mInlineSpellChecker ? mInlineSpellChecker->SpellCheckAfterEditorChange(action,
                                                       aSelection,
                                                       previousSelectedNode,
                                                       previousSelectedOffset,
                                                       aStartNode,
                                                       aStartOffset,
                                                       aEndNode,
                                                       aEndOffset) : NS_OK;
}

already_AddRefed<nsIContent>
nsEditor::FindSelectionRoot(nsINode *aNode)
{
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  return rootContent.forget();
}

nsresult
nsEditor::InitializeSelection(nsIDOMEventTarget* aFocusEventTarget)
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

  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelectionController> selCon;
  rv = GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionPrivate> selectionPrivate =
    do_QueryInterface(selection);
  NS_ENSURE_TRUE(selectionPrivate, NS_ERROR_UNEXPECTED);

  // Init the caret
  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE(caret, NS_ERROR_UNEXPECTED);
  caret->SetIgnoreUserModify(false);
  caret->SetCaretDOMSelection(selection);
  selCon->SetCaretReadOnly(IsReadonly());
  selCon->SetCaretEnabled(true);

  // Init selection
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  selCon->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);
  selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  // If the computed selection root isn't root content, we should set it
  // as selection ancestor limit.  However, if that is root element, it means
  // there is not limitation of the selection, then, we must set NULL.
  // NOTE: If we set a root element to the ancestor limit, some selection
  // methods don't work fine.
  if (selectionRootContent->GetParent()) {
    selectionPrivate->SetAncestorLimiter(selectionRootContent);
  } else {
    selectionPrivate->SetAncestorLimiter(nsnull);
  }

  // XXX What case needs this?
  if (isTargetDoc) {
    PRInt32 rangeCount;
    selection->GetRangeCount(&rangeCount);
    if (rangeCount == 0) {
      BeginningOfDocument();
    }
  }

  return NS_OK;
}

dom::Element *
nsEditor::GetRoot()
{
  if (!mRootElement)
  {
    nsCOMPtr<nsIDOMElement> root;

    // Let GetRootElement() do the work
    GetRootElement(getter_AddRefs(root));
  }

  return mRootElement;
}

dom::Element*
nsEditor::GetEditorRoot()
{
  return GetRoot();
}

nsresult
nsEditor::DetermineCurrentDirection()
{
  // Get the current root direction from its frame
  dom::Element *rootElement = GetRoot();

  // If we don't have an explicit direction, determine our direction
  // from the content's direction
  if (!(mFlags & (nsIPlaintextEditor::eEditorLeftToRight |
                  nsIPlaintextEditor::eEditorRightToLeft))) {

    nsIFrame* frame = rootElement->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

    // Set the flag here, to enable us to use the same code path below.
    // It will be flipped before returning from the function.
    if (frame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    } else {
      mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SwitchTextDirection()
{
  // Get the current root direction from its frame
  dom::Element *rootElement = GetRoot();
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

  return rv;
}

void
nsEditor::SwitchTextDirectionTo(PRUint32 aDirection)
{
  // Get the current root direction from its frame
  dom::Element *rootElement = GetRoot();
  nsresult rv = DetermineCurrentDirection();
  NS_ENSURE_SUCCESS(rv, );

  // Apply the requested direction
  if (aDirection == nsIPlaintextEditor::eEditorLeftToRight &&
      (mFlags & nsIPlaintextEditor::eEditorRightToLeft)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorLeftToRight),
                 "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), true);
  } else if (aDirection == nsIPlaintextEditor::eEditorRightToLeft &&
             (mFlags & nsIPlaintextEditor::eEditorLeftToRight)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorRightToLeft),
                 "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), true);
  }
}

#if DEBUG_JOE
void
nsEditor::DumpNode(nsIDOMNode *aNode, PRInt32 indent)
{
  PRInt32 i;
  for (i=0; i<indent; i++)
    printf("  ");
  
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMDocumentFragment> docfrag = do_QueryInterface(aNode);
  
  if (element || docfrag)
  { 
    if (element)
    {
      nsAutoString tag;
      element->GetTagName(tag);
      printf("<%s>\n", NS_LossyConvertUTF16toASCII(tag).get());
    }
    else
    {
      printf("<document fragment>\n");
    }
    nsCOMPtr<nsIDOMNodeList> childList;
    aNode->GetChildNodes(getter_AddRefs(childList));
    NS_ENSURE_TRUE(childList, NS_ERROR_NULL_POINTER);
    PRUint32 numChildren;
    childList->GetLength(&numChildren);
    nsCOMPtr<nsIDOMNode> child, tmp;
    aNode->GetFirstChild(getter_AddRefs(child));
    for (i=0; i<numChildren; i++)
    {
      DumpNode(child, indent+1);
      child->GetNextSibling(getter_AddRefs(tmp));
      child = tmp;
    }
  }
  else if (IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aNode);
    nsAutoString str;
    textNode->GetData(str);
    nsCAutoString cstr;
    LossyCopyUTF16toASCII(str, cstr);
    cstr.ReplaceChar('\n', ' ');
    printf("<textnode> %s\n", cstr.get());
  }
}
#endif

bool
nsEditor::IsModifiableNode(nsIDOMNode *aNode)
{
  return true;
}

bool
nsEditor::IsModifiableNode(nsINode *aNode)
{
  return true;
}

nsKeyEvent*
nsEditor::GetNativeKeyEvent(nsIDOMKeyEvent* aDOMKeyEvent)
{
  NS_ENSURE_TRUE(aDOMKeyEvent, nsnull);
  nsEvent* nativeEvent = aDOMKeyEvent->GetInternalNSEvent();
  NS_ENSURE_TRUE(nativeEvent, nsnull);
  NS_ENSURE_TRUE(nativeEvent->eventStructType == NS_KEY_EVENT, nsnull);
  return static_cast<nsKeyEvent*>(nativeEvent);
}

already_AddRefed<nsIContent>
nsEditor::GetFocusedContent()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return nsnull;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nsnull);

  nsCOMPtr<nsIContent> content = fm->GetFocusedContent();
  return SameCOMIdentity(content, piTarget) ? content.forget() : nsnull;
}

bool
nsEditor::IsActiveInDOMWindow()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return false;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  nsPIDOMWindow* ourWindow = doc->GetWindow();
  nsCOMPtr<nsPIDOMWindow> win;
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(ourWindow, false,
                                         getter_AddRefs(win));
  return SameCOMIdentity(content, piTarget);
}

bool
nsEditor::IsAcceptableInputEvent(nsIDOMEvent* aEvent)
{
  // If the event is trusted, the event should always cause input.
  nsCOMPtr<nsIDOMNSEvent> NSEvent = do_QueryInterface(aEvent);
  NS_ENSURE_TRUE(NSEvent, false);

  // If this is mouse event but this editor doesn't have focus, we shouldn't
  // handle it.
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aEvent);
  if (mouseEvent) {
    nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
    if (!focusedContent) {
      return false;
    }
  }

  bool isTrusted;
  nsresult rv = NSEvent->GetIsTrusted(&isTrusted);
  NS_ENSURE_SUCCESS(rv, false);
  if (isTrusted) {
    return true;
  }

  // Ignore untrusted mouse event.
  // XXX Why are we handling other untrusted input events?
  if (mouseEvent) {
    return false;
  }

  // Otherwise, we shouldn't handle any input events when we're not an active
  // element of the DOM window.
  return IsActiveInDOMWindow();
}

void
nsEditor::OnFocus(nsIDOMEventTarget* aFocusEventTarget)
{
  InitializeSelection(aFocusEventTarget);
  if (mInlineSpellChecker) {
    mInlineSpellChecker->UpdateCurrentDictionary();
  }
}

NS_IMETHODIMP
nsEditor::GetSuppressDispatchingInputEvent(bool *aSuppressed)
{
  NS_ENSURE_ARG_POINTER(aSuppressed);
  *aSuppressed = !mDispatchInputEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetSuppressDispatchingInputEvent(bool aSuppress)
{
  mDispatchInputEvent = !aSuppress;
  return NS_OK;
}

nsEditor::HandlingTrustedAction::HandlingTrustedAction(nsEditor* aSelf,
                                                       nsIDOMNSEvent* aEvent)
{
  MOZ_ASSERT(aEvent);

  bool isTrusted = false;
  aEvent->GetIsTrusted(&isTrusted);
  Init(aSelf, isTrusted);
}
