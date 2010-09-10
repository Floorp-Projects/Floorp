/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Daniel Glazman <glazman@netscape.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Mats Palmgren <matspal@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "pratom.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSEvent.h"
#include "nsPIDOMEventTarget.h"
#include "nsIMEStateManager.h"
#include "nsFocusManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"

#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMEventTarget.h"
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
#include "nsIPrivateDOMEvent.h"
#include "nsGUIEvent.h"

#include "nsIFrame.h"  // Needed by IME code

#include "nsCSSStyleSheet.h"

#include "nsIContent.h"
#include "nsServiceManagerUtils.h"

// transactions the editor knows how to build
#include "EditAggregateTxn.h"
#include "PlaceholderTxn.h"
#include "ChangeAttributeTxn.h"
#include "CreateElementTxn.h"
#include "InsertElementTxn.h"
#include "DeleteElementTxn.h"
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
#include "nsIHTMLDocument.h"
#include "nsIParserService.h"

#include "nsITransferable.h"
#include "nsComputedDOMStyle.h"

#include "mozilla/FunctionTimer.h"

#define NS_ERROR_EDITOR_NO_SELECTION NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,1)
#define NS_ERROR_EDITOR_NO_TEXTNODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,2)

#ifdef NS_DEBUG_EDITOR
static PRBool gNoisy = PR_FALSE;
#endif

#ifdef DEBUG
#include "nsIDOMHTMLDocument.h"
#endif


// Defined in nsEditorRegistration.cpp
extern nsIParserService *sParserService;

//---------------------------------------------------------------------------
//
// nsEditor: base editor class implementation
//
//---------------------------------------------------------------------------

nsEditor::nsEditor()
:  mModCount(0)
,  mFlags(0)
,  mPresShellWeak(nsnull)
,  mUpdateCount(0)
,  mSpellcheckCheckboxState(eTriUnset)
,  mPlaceHolderTxn(nsnull)
,  mPlaceHolderName(nsnull)
,  mPlaceHolderBatch(0)
,  mSelState(nsnull)
,  mSavedSel()
,  mRangeUpdater()
,  mAction(nsnull)
,  mDirection(eNone)
,  mIMETextNode(nsnull)
,  mIMETextOffset(0)
,  mIMEBufferLength(0)
,  mInIMEMode(PR_FALSE)
,  mIsIMEComposing(PR_FALSE)
,  mShouldTxnSetSelection(PR_TRUE)
,  mDidPreDestroy(PR_FALSE)
,  mDocDirtyState(-1)
,  mDocWeak(nsnull)
,  mPhonetic(nsnull)
{
  //initialize member variables here
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
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTxnMgr)
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
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsEditor, nsIEditor)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsEditor, nsIEditor)

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorMethods 
#pragma mark -
#endif


NS_IMETHODIMP
nsEditor::Init(nsIDOMDocument *aDoc, nsIPresShell* aPresShell, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  if ((nsnull==aDoc) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

  // First only set flags, but other stuff shouldn't be initialized now.
  // Don't move this call after initializing mDocWeak and mPresShellWeak.
  // SetFlags() can check whether it's called during initialization or not by
  // them.  Note that SetFlags() will be called by PostCreate().
  nsresult rv = SetFlags(aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetFlags() failed");

  mDocWeak = do_GetWeakReference(aDoc);  // weak reference to doc
  mPresShellWeak = do_GetWeakReference(aPresShell);   // weak reference to pres shell
  mSelConWeak = do_GetWeakReference(aSelCon);   // weak reference to selectioncontroller

  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);
  
  //set up root element if we are passed one.  
  if (aRoot)
    mRootElement = do_QueryInterface(aRoot);

  mUpdateCount=0;

  /* initialize IME stuff */
  mIMETextNode = nsnull;
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  
  /* Show the caret */
  aSelCon->SetCaretReadOnly(PR_FALSE);
  aSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  
  aSelCon->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);//we want to see all the selection reflected to user

#if 1
  // THIS BLOCK CAUSES ASSERTIONS because sometimes we don't yet have
  // a moz-br but we do have a presshell.

  // Set the selection to the beginning:

//hack to get around this for now.
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mSelConWeak);
  if (shell)
    BeginningOfDocument();
#endif

  NS_POSTCONDITION(mDocWeak && mPresShellWeak, "bad state");

  // Make sure that the editor will be destroyed properly
  mDidPreDestroy = PR_FALSE;

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

  // Set up listeners
  rv = CreateEventListeners();
  if (NS_FAILED(rv))
  {
    RemoveEventListeners();

    return rv;
  }

  rv = InstallEventListeners();
  NS_ENSURE_SUCCESS(rv, rv);

  // nuke the modification count, so the doc appears unmodified
  // do this before we notify listeners
  ResetModificationCount();
  
  // update the UI with our state
  NotifyDocumentListeners(eDocumentCreated);
  NotifyDocumentListeners(eDocumentStateChanged);
  
  return NS_OK;
}

nsresult
nsEditor::CreateEventListeners()
{
  // Don't create the handler twice
  if (mEventListener)
    return NS_OK;
  mEventListener = do_QueryInterface(
    static_cast<nsIDOMKeyListener*>(new nsEditorEventListener()));
  NS_ENSURE_TRUE(mEventListener, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

nsresult
nsEditor::InstallEventListeners()
{
  NS_ENSURE_TRUE(mDocWeak && mPresShellWeak && mEventListener,
                 NS_ERROR_NOT_INITIALIZED);

  // Initialize the event target.
  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(GetRoot());
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

PRBool
nsEditor::GetDesiredSpellCheckState()
{
  // Check user override on this element
  if (mSpellcheckCheckboxState != eTriUnset) {
    return (mSpellcheckCheckboxState == eTriTrue);
  }

  // Check user preferences
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  PRInt32 spellcheckLevel = 1;
  if (NS_SUCCEEDED(rv) && prefBranch) {
    prefBranch->GetIntPref("layout.spellcheckDefault", &spellcheckLevel);
  }

  if (spellcheckLevel == 0) {
    return PR_FALSE;                    // Spellchecking forced off globally
  }

  if (!CanEnableSpellCheck()) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIPresShell> presShell;
  rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv)) {
    nsPresContext* context = presShell->GetPresContext();
    if (context && !context->IsDynamic()) {
        return PR_FALSE;
    }
  }

  // Check DOM state
  nsCOMPtr<nsIContent> content = do_QueryInterface(GetRoot());
  if (!content) {
    return PR_FALSE;
  }

  if (content->IsRootOfNativeAnonymousSubtree()) {
    content = content->GetParent();
  }

  nsCOMPtr<nsIDOMNSHTMLElement> element = do_QueryInterface(content);
  if (!element) {
    return PR_FALSE;
  }

  PRBool enable;
  element->GetSpellcheck(&enable);

  return enable;
}

NS_IMETHODIMP
nsEditor::PreDestroy(PRBool aDestroyingFrames)
{
  if (mDidPreDestroy)
    return NS_OK;

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

  mDidPreDestroy = PR_TRUE;
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

  PRBool spellcheckerWasEnabled = CanEnableSpellCheck();
  mFlags = aFlags;

  if (!mDocWeak || !mPresShellWeak) {
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
  if (HasFocus()) {
    // Use "enable" for the default value because if IME is disabled
    // unexpectedly, it makes serious a11y problem.
    PRUint32 newState = nsIContent::IME_STATUS_ENABLE;
    nsresult rv = GetPreferredIMEState(&newState);
    if (NS_SUCCEEDED(rv)) {
      // NOTE: When the enabled state isn't going to be modified, this method
      // is going to do nothing.
      nsIMEStateManager::UpdateIMEState(newState);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsDocumentEditable(PRBool *aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);
  nsCOMPtr<nsIDOMDocument> doc;
  GetDocument(getter_AddRefs(doc));
  *aIsDocumentEditable = doc ? PR_TRUE : PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  NS_ENSURE_TRUE(aDoc, NS_ERROR_NULL_POINTER);
  *aDoc = nsnull; // init out param
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aDoc = doc);
  return NS_OK;
}


nsresult 
nsEditor::GetPresShell(nsIPresShell **aPS)
{
  NS_ENSURE_TRUE(aPS, NS_ERROR_NULL_POINTER);
  *aPS = nsnull; // init out param
  NS_PRECONDITION(mPresShellWeak, "bad state, null mPresShellWeak");
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aPS = ps);
  return NS_OK;
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
  NS_PRECONDITION(mSelConWeak, "bad state, null mSelConWeak");
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aSel = selCon);
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DeleteSelection(EDirection aAction)
{
  return DeleteSelectionImpl(aAction);
}



NS_IMETHODIMP
nsEditor::GetSelection(nsISelection **aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  *aSelection = nsnull;
  nsCOMPtr<nsISelectionController> selcon = do_QueryReferent(mSelConWeak);
  NS_ENSURE_TRUE(selcon, NS_ERROR_NOT_INITIALIZED);
  return selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);  // does an addref
}

NS_IMETHODIMP 
nsEditor::DoTransaction(nsITransaction *aTxn)
{
#ifdef NS_DEBUG_EDITOR
  if (gNoisy) { printf("Editor::DoTransaction ----------\n"); }
#endif

  nsresult result = NS_OK;
  
  if (mPlaceHolderBatch && !mPlaceHolderTxn)
  {
    // it's pretty darn amazing how many different types of pointers
    // this transaction goes through here.  I bet this is a record.
    
    // We start off with an EditTxn since that's what the factory returns.
    nsRefPtr<EditTxn> editTxn = new PlaceholderTxn();
    if (!editTxn) { return NS_ERROR_OUT_OF_MEMORY; }

    // Then we QI to an nsIAbsorbingTransaction to get at placeholder functionality
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn;
    editTxn->QueryInterface(NS_GET_IID(nsIAbsorbingTransaction), getter_AddRefs(plcTxn));
    // have to use line above instead of "plcTxn = do_QueryInterface(editTxn);"
    // due to our broken interface model for transactions.

    // save off weak reference to placeholder txn
    mPlaceHolderTxn = do_GetWeakReference(plcTxn);
    plcTxn->Init(mPlaceHolderName, mSelState, this);
    mSelState = nsnull;  // placeholder txn took ownership of this pointer

    // finally we QI to an nsITransaction since that's what DoTransaction() expects
    nsCOMPtr<nsITransaction> theTxn = do_QueryInterface(plcTxn);
    DoTransaction(theTxn);  // we will recurse, but will not hit this case in the nested call

    if (mTxnMgr)
    {
      nsCOMPtr<nsITransaction> topTxn;
      result = mTxnMgr->PeekUndoStack(getter_AddRefs(topTxn));
      NS_ENSURE_SUCCESS(result, result);
      if (topTxn)
      {
        plcTxn = do_QueryInterface(topTxn);
        if (plcTxn)
        {
          // there is a palceholder transaction on top of the undo stack.  It is 
          // either the one we just created, or an earlier one that we are now merging
          // into.  From here on out remember this placeholder instead of the one
          // we just created.
          mPlaceHolderTxn = do_GetWeakReference(plcTxn);
        }
      }
    }
  }

  if (aTxn)
  {  
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
    nsCOMPtr<nsISelection>selection;
    result = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(selection));

    selPrivate->StartBatchChanges();

    if (mTxnMgr) {
      result = mTxnMgr->DoTransaction(aTxn);
    }
    else {
      result = aTxn->DoTransaction();
    }
    if (NS_SUCCEEDED(result)) {
      result = DoAfterDoTransaction(aTxn);
    }

    selPrivate->EndBatchChanges(); // no need to check result here, don't lose result of operation
  }
 
  NS_POSTCONDITION((NS_SUCCEEDED(result)), "transaction did not execute properly");

  return result;
}


NS_IMETHODIMP
nsEditor::EnableUndo(PRBool aEnable)
{
  nsresult result=NS_OK;

  if (PR_TRUE==aEnable)
  {
    if (!mTxnMgr)
    {
      mTxnMgr = do_CreateInstance(NS_TRANSACTIONMANAGER_CONTRACTID, &result);
      if (NS_FAILED(result) || !mTxnMgr) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    mTxnMgr->SetMaxTransactionCount(-1);
  }
  else
  { // disable the transaction manager if it is enabled
    if (mTxnMgr)
    {
      mTxnMgr->Clear();
      mTxnMgr->SetMaxTransactionCount(0);
    }
  }

  return NS_OK;
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

  mTxnMgr = aTxnManager;
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::Undo(PRUint32 aCount)
{
#ifdef NS_DEBUG_EDITOR
  if (gNoisy) { printf("Editor::Undo ----------\n"); }
#endif

  nsresult result = NS_OK;
  ForceCompositionEnd();

  PRBool hasTxnMgr, hasTransaction = PR_FALSE;
  CanUndo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, result);

  nsAutoRules beginRulesSniffing(this, kOpUndo, nsIEditor::eNone);

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->UndoTransaction();

      if (NS_SUCCEEDED(result))
        result = DoAfterUndoTransaction();
          
      if (NS_FAILED(result))
        break;
    }
  }

  NotifyEditorObservers();  
  return result;
}


NS_IMETHODIMP nsEditor::CanUndo(PRBool *aIsEnabled, PRBool *aCanUndo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanUndo, NS_ERROR_NULL_POINTER);
  *aIsEnabled = ((PRBool)((nsITransactionManager *)0!=mTxnMgr.get()));
  if (*aIsEnabled)
  {
    PRInt32 numTxns=0;
    mTxnMgr->GetNumberOfUndoItems(&numTxns);
    *aCanUndo = ((PRBool)(0!=numTxns));
  }
  else {
    *aCanUndo = PR_FALSE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::Redo(PRUint32 aCount)
{
#ifdef NS_DEBUG_EDITOR
  if (gNoisy) { printf("Editor::Redo ----------\n"); }
#endif

  nsresult result = NS_OK;

  PRBool hasTxnMgr, hasTransaction = PR_FALSE;
  CanRedo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, result);

  nsAutoRules beginRulesSniffing(this, kOpRedo, nsIEditor::eNone);

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->RedoTransaction();

      if (NS_SUCCEEDED(result))
        result = DoAfterRedoTransaction();

      if (NS_FAILED(result))
        break;
    }
  }

  NotifyEditorObservers();  
  return result;
}


NS_IMETHODIMP nsEditor::CanRedo(PRBool *aIsEnabled, PRBool *aCanRedo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanRedo, NS_ERROR_NULL_POINTER);

  *aIsEnabled = ((PRBool)((nsITransactionManager *)0!=mTxnMgr.get()));
  if (*aIsEnabled)
  {
    PRInt32 numTxns=0;
    mTxnMgr->GetNumberOfRedoItems(&numTxns);
    *aCanRedo = ((PRBool)(0!=numTxns));
  }
  else {
    *aCanRedo = PR_FALSE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::BeginTransaction()
{
  BeginUpdateViewBatch();

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    mTxnMgr->BeginBatch();
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndTransaction()
{
  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
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
      if (mSelState) {
        mSelState->SaveSelection(selection);
      }
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
      selPrivate->SetCanCacheFrameOffset(PR_TRUE);
    }
    
    // time to turn off the batch
    EndUpdateViewBatch();
    // make sure selection is in view

    // After ScrollSelectionIntoView(), the pending notifications might be
    // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
    ScrollSelectionIntoView(PR_FALSE);

    // cached for frame offset are Not available now
    if (selPrivate) {
      selPrivate->SetCanCacheFrameOffset(PR_FALSE);
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
      // notify editor observers of action unless it is uncommitted IME
      if (!mInIMEMode) NotifyEditorObservers();
    }
  }
  mPlaceHolderBatch--;
  
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ShouldTxnSetSelection(PRBool *aResult)
{
  NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
  *aResult = mShouldTxnSetSelection;
  return NS_OK;
}

NS_IMETHODIMP  
nsEditor::SetShouldTxnSetSelection(PRBool aShould)
{
  mShouldTxnSetSelection = aShould;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentIsEmpty(PRBool *aDocumentIsEmpty)
{
  *aDocumentIsEmpty = PR_TRUE;

  nsIDOMElement *rootElement = GetRoot(); 
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER); 

  PRBool hasChildNodes;
  nsresult res = rootElement->HasChildNodes(&hasChildNodes);

  *aDocumentIsEmpty = !hasChildNodes;

  return res;
}


// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
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
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection
  nsCOMPtr<nsISelection> selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
    
  // get the root element 
  nsIDOMElement *rootElement = GetRoot(); 
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER); 
  
  // find first editable thingy
  nsCOMPtr<nsIDOMNode> firstNode;
  result = GetFirstEditableNode(rootElement, address_of(firstNode));
  if (firstNode)
  {
    // if firstNode is text, set selection to beginning of the text node
    if (IsTextNode(firstNode)) 
    {
      result = selection->Collapse(firstNode, 0);
    }
    else
    { // otherwise, it's a leaf node and we set the selection just in front of it
      nsCOMPtr<nsIDOMNode> parentNode;
      result = firstNode->GetParentNode(getter_AddRefs(parentNode));
      if (NS_FAILED(result)) { return result; }
      if (!parentNode) { return NS_ERROR_NULL_POINTER; }
      PRInt32 offsetInParent;
      result = nsEditor::GetChildOffset(firstNode, parentNode, offsetInParent);
      NS_ENSURE_SUCCESS(result, result);
      result = selection->Collapse(parentNode, offsetInParent);
    }
  }
  else
  {
    // just the root node, set selection to inside the root
    result = selection->Collapse(rootElement, 0);
  }
  return result;
}

NS_IMETHODIMP
nsEditor::EndOfDocument() 
{ 
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; } 
  nsresult res; 

  // get selection 
  nsCOMPtr<nsISelection> selection; 
  res = GetSelection(getter_AddRefs(selection)); 
  NS_ENSURE_SUCCESS(res, res); 
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER); 
  
  // get the root element 
  nsIDOMElement *rootElement = GetRoot(); 
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER); 

  // get the length of the rot element 
  PRUint32 len; 
  res = GetLengthOfDOMNode(rootElement, len); 
  NS_ENSURE_SUCCESS(res, res);

  // set the selection to after the last child of the root element 
  return selection->Collapse(rootElement, (PRInt32)len); 
} 
  
NS_IMETHODIMP
nsEditor::GetDocumentModified(PRBool *outDocModified)
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
  nsCOMPtr<nsIPresShell> presShell;

  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv))
  {
    nsIDocument *doc = presShell->GetDocument();
    if (doc) {
      characterSet = doc->GetDocumentCharacterSet();
      return NS_OK;
    }
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;

}

NS_IMETHODIMP
nsEditor::SetDocumentCharacterSet(const nsACString& characterSet)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv))
  {
    nsIDocument *doc = presShell->GetDocument();
    if (doc) {
      doc->SetDocumentCharacterSet(characterSet);
      return NS_OK;
    }
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}

NS_IMETHODIMP
nsEditor::Cut()
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanCut(PRBool *aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::Copy()
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanCopy(PRBool *aCanCut)
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
nsEditor::CanPaste(PRInt32 aSelectionType, PRBool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanPasteTransferable(nsITransferable *aTransferable, PRBool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanDrag(nsIDOMEvent *aEvent, PRBool *aCanDrag)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::DoDrag(nsIDOMEvent *aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::InsertFromDrop(nsIDOMEvent *aEvent)
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
                            PRBool *aResultIsSet)
{
  NS_ENSURE_TRUE(aResultIsSet, NS_ERROR_NULL_POINTER);
  *aResultIsSet=PR_FALSE;
  nsresult result=NS_OK;
  if (aElement)
  {
    nsCOMPtr<nsIDOMAttr> attNode;
    result = aElement->GetAttributeNode(aAttribute, getter_AddRefs(attNode));
    if ((NS_SUCCEEDED(result)) && attNode)
    {
      attNode->GetSpecified(aResultIsSet);
      attNode->GetValue(aResultValue);
    }
  }
  return result;
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


NS_IMETHODIMP
nsEditor::MarkNodeDirty(nsIDOMNode* aNode)
{  
  //  mark the node dirty.
  nsCOMPtr<nsIContent> element (do_QueryInterface(aNode));
  if (element)
    element->SetAttr(kNameSpaceID_None, nsEditProperty::mozdirty,
                     EmptyString(), PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsEditor::GetInlineSpellChecker(PRBool autoCreate,
                                              nsIInlineSpellChecker ** aInlineSpellChecker)
{
  NS_ENSURE_ARG_POINTER(aInlineSpellChecker);

  if (mDidPreDestroy) {
    // Don't allow people to get or create the spell checker once the editor
    // is going away.
    *aInlineSpellChecker = nsnull;
    return autoCreate ? NS_ERROR_NOT_AVAILABLE : NS_OK;
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

NS_IMETHODIMP nsEditor::SyncRealTimeSpell()
{
  NS_TIME_FUNCTION;

  PRBool enable = GetDesiredSpellCheckState();

  nsCOMPtr<nsIInlineSpellChecker> spellChecker;
  GetInlineSpellChecker(enable, getter_AddRefs(spellChecker));

  if (spellChecker) {
    spellChecker->SetEnableRealTimeSpell(enable);
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditor::SetSpellcheckUserOverride(PRBool enable)
{
  mSpellcheckCheckboxState = enable ? eTriTrue : eTriFalse;

  return SyncRealTimeSpell();
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  main node manipulation routines 
#pragma mark -
#endif

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



NS_IMETHODIMP
nsEditor::JoinNodes(nsIDOMNode * aLeftNode,
                    nsIDOMNode * aRightNode,
                    nsIDOMNode * aParent)
{
  PRInt32 i, offset;
  nsAutoRules beginRulesSniffing(this, kOpJoinNode, nsIEditor::ePrevious);

  // remember some values; later used for saved selection updating.
  // find the offset between the nodes to be joined.
  nsresult result = GetChildOffset(aRightNode, aParent, offset);
  NS_ENSURE_SUCCESS(result, result);
  // find the number of children of the lefthand node
  PRUint32 oldLeftNodeLen;
  result = GetLengthOfDOMNode(aLeftNode, oldLeftNodeLen);
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


NS_IMETHODIMP nsEditor::DeleteNode(nsIDOMNode * aElement)
{
  PRInt32 i, offset;
  nsCOMPtr<nsIDOMNode> parent;
  nsAutoRules beginRulesSniffing(this, kOpCreateNode, nsIEditor::ePrevious);

  // save node location for selection updating code.
  nsresult result = GetNodeLocation(aElement, address_of(parent), &offset);
  NS_ENSURE_SUCCESS(result, result);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillDeleteNode(aElement);

  nsRefPtr<DeleteElementTxn> txn;
  result = CreateTxnForDeleteElement(aElement, getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidDeleteNode(aElement, result);

  return result;
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
                           PRBool aCloneAttributes)
{
  NS_ENSURE_TRUE(inNode && outNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  nsresult res = GetNodeLocation(inNode, address_of(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);

  // create new container
  nsCOMPtr<nsIContent> newContent;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(aNodeType, getter_AddRefs(newContent));
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(newContent);
  NS_ENSURE_SUCCESS(res, res);
    *outNode = do_QueryInterface(elem);
  
  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty())
  {
    res = elem->SetAttribute(*aAttribute, *aValue);
    NS_ENSURE_SUCCESS(res, res);
  }
  if (aCloneAttributes)
  {
    nsCOMPtr<nsIDOMNode>newNode = do_QueryInterface(elem);
    res = CloneAttributes(newNode, inNode);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // notify our internal selection state listener
  // (Note: A nsAutoSelectionReset object must be created
  //  before calling this to initialize mRangeUpdater)
  nsAutoReplaceContainerSelNotify selStateNotify(mRangeUpdater, inNode, *outNode);
  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    nsCOMPtr<nsIDOMNode> child;
    PRBool bHasMoreChildren;
    inNode->HasChildNodes(&bHasMoreChildren);
    while (bHasMoreChildren)
    {
      inNode->GetFirstChild(getter_AddRefs(child));
      res = DeleteNode(child);
      NS_ENSURE_SUCCESS(res, res);

      res = InsertNode(child, *outNode, -1);
      NS_ENSURE_SUCCESS(res, res);
      inNode->HasChildNodes(&bHasMoreChildren);
    }
  }
  // insert new container into tree
  res = InsertNode( *outNode, parent, offset);
  NS_ENSURE_SUCCESS(res, res);
  
  // delete old container
  return DeleteNode(inNode);
}

///////////////////////////////////////////////////////////////////////////
// RemoveContainer: remove inNode, reparenting its children into their
//                  the parent of inNode
//
nsresult
nsEditor::RemoveContainer(nsIDOMNode *inNode)
{
  NS_ENSURE_TRUE(inNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  
  nsresult res = GetNodeLocation(inNode, address_of(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);
  
  // loop through the child nodes of inNode and promote them
  // into inNode's parent.
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  nsCOMPtr<nsIDOMNodeList> nodeList;
  res = inNode->GetChildNodes(getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(nodeList, NS_ERROR_NULL_POINTER);
  PRUint32 nodeOrigLen;
  nodeList->GetLength(&nodeOrigLen);

  // notify our internal selection state listener
  nsAutoRemoveContainerSelNotify selNotify(mRangeUpdater, inNode, parent, offset, nodeOrigLen);
                                   
  nsCOMPtr<nsIDOMNode> child;
  while (bHasMoreChildren)
  {
    inNode->GetLastChild(getter_AddRefs(child));
    res = DeleteNode(child);
    NS_ENSURE_SUCCESS(res, res);
    res = InsertNode(child, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    inNode->HasChildNodes(&bHasMoreChildren);
  }
  return DeleteNode(inNode);
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
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  nsresult res = GetNodeLocation(inNode, address_of(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);

  // create new container
  nsCOMPtr<nsIContent> newContent;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(aNodeType, getter_AddRefs(newContent));
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(newContent);
  NS_ENSURE_SUCCESS(res, res);
  *outNode = do_QueryInterface(elem);
  
  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty())
  {
    res = elem->SetAttribute(*aAttribute, *aValue);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // notify our internal selection state listener
  nsAutoInsertContainerSelNotify selNotify(mRangeUpdater);
  
  // put inNode in new parent, outNode
  res = DeleteNode(inNode);
  NS_ENSURE_SUCCESS(res, res);

  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    res = InsertNode(inNode, *outNode, 0);
    NS_ENSURE_SUCCESS(res, res);
  }

  // put new parent in doc
  return InsertNode(*outNode, parent, offset);
}

///////////////////////////////////////////////////////////////////////////
// MoveNode:  move aNode to {aParent,aOffset}
nsresult
nsEditor::MoveNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> oldParent;
  PRInt32 oldOffset;
  nsresult res = GetNodeLocation(aNode, address_of(oldParent), &oldOffset);
  
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

  // put aNode in new parent
  res = DeleteNode(aNode);
  NS_ENSURE_SUCCESS(res, res);
  return InsertNode(aNode, aParent, aOffset);
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  editor observer maintainance
#pragma mark -
#endif

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

void nsEditor::NotifyEditorObservers(void)
{
  for (PRInt32 i = 0; i < mEditorObservers.Count(); i++)
    mEditorObservers[i]->EditAction();
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  action listener maintainance
#pragma mark -
#endif

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


#ifdef XP_MAC
#pragma mark -
#pragma mark  docstate listener maintainance
#pragma mark -
#endif


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


#ifdef XP_MAC
#pragma mark -
#pragma mark  misc 
#pragma mark -
#endif

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
  nsCOMPtr<nsIContent> root = do_QueryInterface(mRootElement);
  if (root)  root->List(stdout);
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

#ifdef XP_MAC
#pragma mark -
#pragma mark  support for selection preservation
#pragma mark -
#endif

PRBool   
nsEditor::ArePreservingSelection()
{
  return !(mSavedSel.IsEmpty());
}

nsresult 
nsEditor::PreserveSelectionAcrossActions(nsISelection *aSel)
{
  mSavedSel.SaveSelection(aSel);
  mRangeUpdater.RegisterSelectionState(mSavedSel);
  return NS_OK;
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


#ifdef XP_MAC
#pragma mark -
#pragma mark  IME event handlers 
#pragma mark -
#endif

nsresult
nsEditor::BeginIMEComposition()
{
  mInIMEMode = PR_TRUE;
  if (mPhonetic) {
    mPhonetic->Truncate(0);
  }
  return NS_OK;
}

nsresult
nsEditor::EndIMEComposition()
{
  NS_ENSURE_TRUE(mInIMEMode, NS_OK); // nothing to do

  nsresult rv = NS_OK;

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTxnMgr) {
    nsCOMPtr<nsITransaction> txn;
    rv = mTxnMgr->PeekUndoStack(getter_AddRefs(txn));
    NS_ASSERTION(NS_SUCCEEDED(rv), "PeekUndoStack() failed");
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryInterface(txn);
    if (plcTxn) {
      rv = plcTxn->Commit();
      NS_ASSERTION(NS_SUCCEEDED(rv),
                   "nsIAbsorbingTransaction::Commit() failed");
    }
  }

  /* reset the data we need to construct a transaction */
  mIMETextNode = nsnull;
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  mInIMEMode = PR_FALSE;
  mIsIMEComposing = PR_FALSE;

  // notify editor observers of action
  NotifyEditorObservers();

  return rv;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIPhonetic
#pragma mark -
#endif


NS_IMETHODIMP
nsEditor::GetPhonetic(nsAString& aPhonetic)
{
  if (mPhonetic)
    aPhonetic = *mPhonetic;
  else
    aPhonetic.Truncate(0);

  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorIMESupport 
#pragma mark -
#endif


static nsresult
GetEditorContentWindow(nsIDOMElement *aRoot, nsIWidget **aResult)
{
  NS_ENSURE_TRUE(aRoot && aResult, NS_ERROR_NULL_POINTER);

  *aResult = 0;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aRoot);

  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  // Not ref counted
  nsIFrame *frame = content->GetPrimaryFrame();

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
// We should use nsILookAndFeel to resolve this

#if defined(XP_MAC) || defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_OS2)
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
nsEditor::GetPreferredIMEState(PRUint32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = nsIContent::IME_STATUS_ENABLE;

  if (IsReadonly() || IsDisabled()) {
    *aState = nsIContent::IME_STATUS_DISABLE;
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(GetRoot());
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIFrame* frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  switch (frame->GetStyleUIReset()->mIMEMode) {
    case NS_STYLE_IME_MODE_AUTO:
      if (IsPasswordEditor())
        *aState = nsIContent::IME_STATUS_PASSWORD;
      break;
    case NS_STYLE_IME_MODE_DISABLED:
      // we should use password state for |ime-mode: disabled;|.
      *aState = nsIContent::IME_STATUS_PASSWORD;
      break;
    case NS_STYLE_IME_MODE_ACTIVE:
      *aState |= nsIContent::IME_STATUS_OPEN;
      break;
    case NS_STYLE_IME_MODE_INACTIVE:
      *aState |= nsIContent::IME_STATUS_CLOSE;
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetComposing(PRBool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = IsIMEComposing();
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  public nsEditor methods 
#pragma mark -
#endif
/* Non-interface, public methods */


NS_IMETHODIMP
nsEditor::GetRootElement(nsIDOMElement **aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);
  NS_ENSURE_TRUE(mRootElement, NS_ERROR_NOT_AVAILABLE);
  *aRootElement = mRootElement;
  NS_ADDREF(*aRootElement);
  return NS_OK;
}


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsEditor::StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection)
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
  mAction = nsnull;
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
  PRBool isAttrSet;
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
  nsIDOMElement *rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  PRBool destInBody = PR_TRUE;
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElement);
  nsCOMPtr<nsIDOMNode> p = aDestNode;
  while (p && p != rootNode)
  {
    nsCOMPtr<nsIDOMNode> tmp;
    if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp)
    {
      destInBody = PR_FALSE;
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
              result = SetAttributeOrEquivalent(destElement, sourceAttrName, sourceAttrValue, PR_FALSE);
            }
            else {
              // the element is not inserted in the document yet, we don't want to put a
              // transaction on the UndoStack
              result = SetAttributeOrEquivalent(destElement, sourceAttrName, sourceAttrValue, PR_TRUE);
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

#ifdef XP_MAC
#pragma mark -
#pragma mark  Protected and static methods 
#pragma mark -
#endif

NS_IMETHODIMP nsEditor::ScrollSelectionIntoView(PRBool aScrollToAnchor)
{
  nsCOMPtr<nsISelectionController> selCon;
  if (NS_SUCCEEDED(GetSelectionController(getter_AddRefs(selCon))) && selCon)
  {
    PRInt16 region = nsISelectionController::SELECTION_FOCUS_REGION;

    if (aScrollToAnchor)
      region = nsISelectionController::SELECTION_ANCHOR_REGION;

    PRBool syncScroll = PR_TRUE;
    PRUint32 flags = 0;

    if (NS_SUCCEEDED(GetFlags(&flags)))
    {
      // If the editor is relying on asynchronous reflows, we have
      // to use asynchronous requests to scroll, so that the scrolling happens
      // after reflow requests are processed.
      // XXXbz why not just always do async scroll?
      syncScroll = !(flags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask);
    }

    // After ScrollSelectionIntoView(), the pending notifications might be
    // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                    region, syncScroll);
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
  
  NS_ENSURE_TRUE(aInOutNode && *aInOutNode && aInOutOffset && aDoc, NS_ERROR_NULL_POINTER);
  if (!mInIMEMode && aStringToInsert.IsEmpty()) return NS_OK;
  nsCOMPtr<nsIDOMText> nodeAsText = do_QueryInterface(*aInOutNode);
  PRInt32 offset = *aInOutOffset;
  nsresult res;
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
                                              PRBool aSuppressIME)
{
  nsRefPtr<EditTxn> txn;
  nsresult result = NS_OK;
  PRBool isIMETransaction = PR_FALSE;
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
    isIMETransaction = PR_TRUE;
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
  if (isIMETransaction)
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

  nsIDOMElement *rootElement = GetRoot();
  if (!rootElement) { return NS_ERROR_NOT_INITIALIZED; }

  return aSelection->SelectAllChildren(rootElement);
}


nsresult nsEditor::GetFirstEditableNode(nsIDOMNode *aRoot, nsCOMPtr<nsIDOMNode> *outFirstNode)
{
  NS_ENSURE_TRUE(aRoot && outFirstNode, NS_ERROR_NULL_POINTER);
  nsresult rv = NS_OK;
  *outFirstNode = nsnull;

  nsCOMPtr<nsIDOMNode> node = GetLeftmostChild(aRoot);
  if (node && !IsEditable(node))
  {
    nsCOMPtr<nsIDOMNode> next;
    rv = GetNextNode(node, PR_TRUE, address_of(next));
    node = next;
  }
  
  if (node != aRoot)
    *outFirstNode = node;

  return rv;
}

#ifdef XXX_DEAD_CODE
// jfrancis wants to keep this method around for reference
nsresult nsEditor::GetLastEditableNode(nsIDOMNode *aRoot, nsCOMPtr<nsIDOMNode> *outLastNode)
{
  NS_ENSURE_TRUE(aRoot && outLastNode, NS_ERROR_NULL_POINTER);
  nsresult rv = NS_OK;
  *outLastNode = nsnull;

  nsCOMPtr<nsIDOMNode> node = GetRightmostChild(aRoot);
  if (node && !IsEditable(node))
  {
    nsCOMPtr<nsIDOMNode> next;
    rv = GetPriorNode(node, PR_TRUE, address_of(next));
    node = next;
  }

  if (node != aRoot)
    *outLastNode = node;

  return rv;
}
#endif


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
        PRBool docIsDirty;
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
  nsresult result;

  *aTxn = new InsertTextTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);
  result = (*aTxn)->Init(aTextNode, aOffset, aStringToInsert, this);
  return result;
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


NS_IMETHODIMP nsEditor::CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                               PRUint32             aOffset,
                                               PRUint32             aLength,
                                               DeleteTextTxn      **aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  *aTxn = new DeleteTextTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);
  return (*aTxn)->Init(this, aElement, aOffset, aLength, &mRangeUpdater);
}




NS_IMETHODIMP nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  *aTxn = new SplitElementTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(this, aNode, aOffset);
}

NS_IMETHODIMP nsEditor::CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                             nsIDOMNode  *aRightNode,
                                             JoinElementTxn **aTxn)
{
  NS_ENSURE_TRUE(aLeftNode && aRightNode, NS_ERROR_NULL_POINTER);

  *aTxn = new JoinElementTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(this, aLeftNode, aRightNode);
}


// END nsEditor core implementation

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsEditor public static helper methods 
#pragma mark -
#endif

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
        nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
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
                        PRBool       aNodeToKeepIsFirst)
{
  NS_ASSERTION(aNodeToKeep && aNodeToJoin && aParent, "null arg");
  nsresult result;
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
    nsCOMPtr<nsIDOMNode> parent;
    result = GetNodeLocation(aNodeToJoin, address_of(parent), &joinOffset);
    NS_ENSURE_SUCCESS(result, result);
    result = GetNodeLocation(aNodeToKeep, address_of(parent), &keepOffset);
    NS_ENSURE_SUCCESS(result, result);
    
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
        PRBool bNeedToAdjust = PR_FALSE;
        
        // check to see if we joined nodes where selection starts
        if (selStartNode.get() == aNodeToJoin)
        {
          bNeedToAdjust = PR_TRUE;
          selStartNode = aNodeToKeep;
          if (aNodeToKeepIsFirst)
          {
            selStartOffset += firstNodeLength;
          }
        }
        else if ((selStartNode.get() == aNodeToKeep) && !aNodeToKeepIsFirst)
        {
          bNeedToAdjust = PR_TRUE;
          selStartOffset += firstNodeLength;
        }
                
        // check to see if we joined nodes where selection ends
        if (selEndNode.get() == aNodeToJoin)
        {
          bNeedToAdjust = PR_TRUE;
          selEndNode = aNodeToKeep;
          if (aNodeToKeepIsFirst)
          {
            selEndOffset += firstNodeLength;
          }
        }
        else if ((selEndNode.get() == aNodeToKeep) && !aNodeToKeepIsFirst)
        {
          bNeedToAdjust = PR_TRUE;
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


nsresult 
nsEditor::GetChildOffset(nsIDOMNode *aChild, nsIDOMNode *aParent, PRInt32 &aOffset)
{
  NS_ASSERTION((aChild && aParent), "bad args");

  nsCOMPtr<nsIContent> content = do_QueryInterface(aParent);
  nsCOMPtr<nsIContent> cChild = do_QueryInterface(aChild);
  NS_ENSURE_TRUE(cChild && content, NS_ERROR_NULL_POINTER);

  aOffset = content->IndexOf(cChild);

  return NS_OK;
}

nsresult 
nsEditor::GetNodeLocation(nsIDOMNode *inChild, nsCOMPtr<nsIDOMNode> *outParent, PRInt32 *outOffset)
{
  NS_ASSERTION((inChild && outParent && outOffset), "bad args");
  nsresult result = NS_ERROR_NULL_POINTER;
  if (inChild && outParent && outOffset)
  {
    result = inChild->GetParentNode(getter_AddRefs(*outParent));
    if ((NS_SUCCEEDED(result)) && (*outParent))
    {
      result = GetChildOffset(inChild, *outParent, *outOffset);
    }
  }
  return result;
}

// returns the number of things inside aNode.  
// If aNode is text, returns number of characters. If not, returns number of children nodes.
nsresult
nsEditor::GetLengthOfDOMNode(nsIDOMNode *aNode, PRUint32 &aCount) 
{
  aCount = 0;
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar = do_QueryInterface(aNode);
  if (nodeAsChar) {
    nodeAsChar->GetLength(&aCount);
  }
  else
  {
    PRBool hasChildNodes;
    aNode->HasChildNodes(&hasChildNodes);
    if (hasChildNodes)
    {
      nsCOMPtr<nsIDOMNodeList>nodeList;
      result = aNode->GetChildNodes(getter_AddRefs(nodeList));
      if (NS_SUCCEEDED(result) && nodeList) {
        nodeList->GetLength(&aCount);
      }
    }
  }
  return result;
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aParentNode, 
                       PRInt32      aOffset, 
                       PRBool       aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       PRBool       bNoBlockCrossing)
{
  // just another version of GetPriorNode that takes a {parent, offset}
  // instead of a node
  if (!aParentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  *aResultNode = nsnull;
  
  // if we are at beginning of node, or it is a textnode, then just look before it
  if (!aOffset || IsTextNode(aParentNode))
  {
    if (bNoBlockCrossing && IsBlockNode(aParentNode))
    {
      // if we aren't allowed to cross blocks, don't look before this block
      return NS_OK;
    }
    return GetPriorNode(aParentNode, aEditableNode, aResultNode, bNoBlockCrossing);
  }

  // else look before the child at 'aOffset'
  nsCOMPtr<nsIDOMNode> child = GetChildAt(aParentNode, aOffset);
  if (child)
    return GetPriorNode(child, aEditableNode, aResultNode, bNoBlockCrossing);

  // unless there isn't one, in which case we are at the end of the node
  // and want the deep-right child.
  *aResultNode = GetRightmostChild(aParentNode, bNoBlockCrossing);
  if (!*aResultNode || !aEditableNode || IsEditable(*aResultNode))
    return NS_OK;

  // restart the search from the non-editable node we just found
  nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
  return GetPriorNode(notEditableNode, aEditableNode, aResultNode, bNoBlockCrossing);
}


nsresult 
nsEditor::GetNextNode(nsIDOMNode   *aParentNode, 
                       PRInt32      aOffset, 
                       PRBool       aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       PRBool       bNoBlockCrossing)
{
  // just another version of GetNextNode that takes a {parent, offset}
  // instead of a node
  if (!aParentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  *aResultNode = nsnull;

  // if aParentNode is a text node, use it's location instead
  if (IsTextNode(aParentNode))
  {
    nsCOMPtr<nsIDOMNode> parent;
    nsEditor::GetNodeLocation(aParentNode, address_of(parent), &aOffset);
    aParentNode = parent;
    aOffset++;  // _after_ the text node
  }
  // look at the child at 'aOffset'
  nsCOMPtr<nsIDOMNode> child = GetChildAt(aParentNode, aOffset);
  if (child)
  {
    if (bNoBlockCrossing && IsBlockNode(child))
    {
      *aResultNode = child;  // return this block
      return NS_OK;
    }
    *aResultNode = GetLeftmostChild(child, bNoBlockCrossing);
    if (!*aResultNode) 
    {
      *aResultNode = child;
      return NS_OK;
    }
    if (!IsDescendantOfBody(*aResultNode))
    {
      *aResultNode = nsnull;
      return NS_OK;
    }

    if (!aEditableNode || IsEditable(*aResultNode))
      return NS_OK;

    // restart the search from the non-editable node we just found
    nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
    return GetNextNode(notEditableNode, aEditableNode, aResultNode, bNoBlockCrossing);
  }
    
  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  if (bNoBlockCrossing && IsBlockNode(aParentNode))
  {
    // don't cross out of parent block
    return NS_OK;
  }
  return GetNextNode(aParentNode, aEditableNode, aResultNode, bNoBlockCrossing);
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aCurrentNode, 
                       PRBool       aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       PRBool       bNoBlockCrossing)
{
  nsresult result;
  if (!aCurrentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  *aResultNode = nsnull;  // init out-param

  if (IsRootNode(aCurrentNode))
  {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.

    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> candidate;
  result = GetPriorNodeImpl(aCurrentNode, aEditableNode, address_of(candidate), bNoBlockCrossing);
  NS_ENSURE_SUCCESS(result, result);
  
  if (!candidate)
  {
    // we could not find a prior node.  return null.
    return NS_OK;
  }
  else if (!aEditableNode) *aResultNode = candidate;
  else if (IsEditable(candidate)) *aResultNode = candidate;
  else 
  { // restart the search from the non-editable node we just found
    nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(candidate);
    return GetPriorNode(notEditableNode, aEditableNode, aResultNode, bNoBlockCrossing);
  }
  return result;
}

nsresult 
nsEditor::GetPriorNodeImpl(nsIDOMNode  *aCurrentNode, 
                           PRBool       aEditableNode, 
                           nsCOMPtr<nsIDOMNode> *aResultNode,
                           PRBool       bNoBlockCrossing)
{
  // called only by GetPriorNode so we don't need to check params.

  // if aCurrentNode has a left sibling, return that sibling's rightmost child (or itself if it has no children)
  nsCOMPtr<nsIDOMNode> prevSibling;
  nsresult result = aCurrentNode->GetPreviousSibling(getter_AddRefs(prevSibling));
  if ((NS_SUCCEEDED(result)) && prevSibling)
  {
    if (bNoBlockCrossing && IsBlockNode(prevSibling))
    {
      // don't look inside prevsib, since it is a block
      *aResultNode = prevSibling;
      return NS_OK;
    }
    *aResultNode = GetRightmostChild(prevSibling, bNoBlockCrossing);
    if (!*aResultNode) 
    { 
      *aResultNode = prevSibling;
      return NS_OK;
    }
    if (!IsDescendantOfBody(*aResultNode))
    {
      *aResultNode = nsnull;
      return NS_OK;
    }
  }
  else
  {
    // otherwise, walk up the parent tree until there is a child that comes before 
    // the ancestor of aCurrentNode.  Then return that node's rightmost child
    nsCOMPtr<nsIDOMNode> parent = do_QueryInterface(aCurrentNode);
    nsCOMPtr<nsIDOMNode> node, notEditableNode;
    do {
      node = parent;
      result = node->GetParentNode(getter_AddRefs(parent));
      if ((NS_SUCCEEDED(result)) && parent)
      {
        if (!IsDescendantOfBody(parent))
        {
          *aResultNode = nsnull;
          return NS_OK;
        }
        if ((bNoBlockCrossing && IsBlockNode(parent)) || IsRootNode(parent))
        {
          // we are at front of block or root, do not step out
          *aResultNode = nsnull;
          return NS_OK;
        }
        result = parent->GetPreviousSibling(getter_AddRefs(node));
        if ((NS_SUCCEEDED(result)) && node)
        {
          if (bNoBlockCrossing && IsBlockNode(node))
          {
            // prev sibling is a block, do not step into it
            *aResultNode = node;
            return NS_OK;
          }
          *aResultNode = GetRightmostChild(node, bNoBlockCrossing);
          if (!*aResultNode)  *aResultNode = node;
          return NS_OK;
        }
      }
    } while ((NS_SUCCEEDED(result)) && parent && !*aResultNode);
  }
  return result;
}

nsresult 
nsEditor::GetNextNode(nsIDOMNode  *aCurrentNode, 
                      PRBool       aEditableNode, 
                      nsCOMPtr<nsIDOMNode> *aResultNode,
                      PRBool       bNoBlockCrossing)
{
  if (!aCurrentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  *aResultNode = nsnull;  // init out-param

  if (IsRootNode(aCurrentNode))
  {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.

    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> candidate;
  nsresult result = GetNextNodeImpl(aCurrentNode, aEditableNode,
                                    address_of(candidate), bNoBlockCrossing);
  NS_ENSURE_SUCCESS(result, result);
  
  if (!candidate)
  {
    // we could not find a next node.  return null.
    *aResultNode = nsnull;
    return NS_OK;
  }
  else if (!aEditableNode) *aResultNode = candidate;
  else if (IsEditable(candidate)) *aResultNode = candidate;
  else 
  { // restart the search from the non-editable node we just found
    nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(candidate);
    return GetNextNode(notEditableNode, aEditableNode, aResultNode, bNoBlockCrossing);
  }
  return result;
}


nsresult 
nsEditor::GetNextNodeImpl(nsIDOMNode  *aCurrentNode, 
                          PRBool       aEditableNode, 
                          nsCOMPtr<nsIDOMNode> *aResultNode,
                          PRBool       bNoBlockCrossing)
{
  // called only by GetNextNode so we don't need to check params.

  // if aCurrentNode has a right sibling, return that sibling's leftmost child (or itself if it has no children)
  nsCOMPtr<nsIDOMNode> nextSibling;
  nsresult result = aCurrentNode->GetNextSibling(getter_AddRefs(nextSibling));
  if ((NS_SUCCEEDED(result)) && nextSibling)
  {
    if (bNoBlockCrossing && IsBlockNode(nextSibling))
    {
      // next sibling is a block, do not step into it
      *aResultNode = nextSibling;
      return NS_OK;
    }
    *aResultNode = GetLeftmostChild(nextSibling, bNoBlockCrossing);
    if (!*aResultNode)
    { 
      *aResultNode = nextSibling;
      return NS_OK; 
    }
    if (!IsDescendantOfBody(*aResultNode))
    {
      *aResultNode = nsnull;
      return NS_OK;
    }
  }
  else
  {
    // otherwise, walk up the parent tree until there is a child that comes after 
    // the ancestor of aCurrentNode.  Then return that node's leftmost child
    nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(aCurrentNode));
    nsCOMPtr<nsIDOMNode> node, notEditableNode;
    do {
      node = parent;
      result = node->GetParentNode(getter_AddRefs(parent));
      if ((NS_SUCCEEDED(result)) && parent)
      {
        if (!IsDescendantOfBody(parent))
        {
          *aResultNode = nsnull;
          return NS_OK;
        }
        if ((bNoBlockCrossing && IsBlockNode(parent)) || IsRootNode(parent))
        {
          // we are at end of block or root, do not step out
          *aResultNode = nsnull;
          return NS_OK;
        }
        result = parent->GetNextSibling(getter_AddRefs(node));
        if ((NS_SUCCEEDED(result)) && node)
        {
          if (bNoBlockCrossing && IsBlockNode(node))
          {
            // next sibling is a block, do not step into it
            *aResultNode = node;
            return NS_OK;
          }
          *aResultNode = GetLeftmostChild(node, bNoBlockCrossing);
          if (!*aResultNode) *aResultNode = node;
          return NS_OK; 
        }
      }
    } while ((NS_SUCCEEDED(result)) && parent);
  }
  return result;
}


nsCOMPtr<nsIDOMNode>
nsEditor::GetRightmostChild(nsIDOMNode *aCurrentNode, 
                            PRBool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nsnull);
  nsCOMPtr<nsIDOMNode> resultNode, temp=aCurrentNode;
  PRBool hasChildren;
  aCurrentNode->HasChildNodes(&hasChildren);
  while (hasChildren)
  {
    temp->GetLastChild(getter_AddRefs(resultNode));
    if (resultNode)
    {
      if (bNoBlockCrossing && IsBlockNode(resultNode))
         return resultNode;
      resultNode->HasChildNodes(&hasChildren);
      temp = resultNode;
    }
    else 
      hasChildren = PR_FALSE;
  }

  return resultNode;
}

nsCOMPtr<nsIDOMNode>
nsEditor::GetLeftmostChild(nsIDOMNode *aCurrentNode,
                           PRBool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nsnull);
  nsCOMPtr<nsIDOMNode> resultNode, temp=aCurrentNode;
  PRBool hasChildren;
  aCurrentNode->HasChildNodes(&hasChildren);
  while (hasChildren)
  {
    temp->GetFirstChild(getter_AddRefs(resultNode));
    if (resultNode)
    {
      if (bNoBlockCrossing && IsBlockNode(resultNode))
         return resultNode;
      resultNode->HasChildNodes(&hasChildren);
      temp = resultNode;
    }
    else 
      hasChildren = PR_FALSE;
  }

  return resultNode;
}

PRBool 
nsEditor::IsBlockNode(nsIDOMNode *aNode)
{
  // stub to be overridden in nsHTMLEditor.
  // screwing around with the class hierarchy here in order
  // to not duplicate the code in GetNextNode/GetPrevNode
  // across both nsEditor/nsHTMLEditor.  
  return PR_FALSE;
}

PRBool 
nsEditor::CanContainTag(nsIDOMNode* aParent, const nsAString &aChildTag)
{
  nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(parentElement, PR_FALSE);
  
  nsAutoString parentStringTag;
  parentElement->GetTagName(parentStringTag);
  return TagCanContainTag(parentStringTag, aChildTag);
}

PRBool 
nsEditor::TagCanContain(const nsAString &aParentTag, nsIDOMNode* aChild)
{
  nsAutoString childStringTag;
  
  if (IsTextNode(aChild)) 
  {
    childStringTag.AssignLiteral("#text");
  }
  else
  {
    nsCOMPtr<nsIDOMElement> childElement = do_QueryInterface(aChild);
    NS_ENSURE_TRUE(childElement, PR_FALSE);
    childElement->GetTagName(childStringTag);
  }
  return TagCanContainTag(aParentTag, childStringTag);
}

PRBool 
nsEditor::TagCanContainTag(const nsAString &aParentTag, const nsAString &aChildTag)
{
  return PR_TRUE;
}

PRBool 
nsEditor::IsRootNode(nsIDOMNode *inNode) 
{
  NS_ENSURE_TRUE(inNode, PR_FALSE);

  nsIDOMElement *rootElement = GetRoot();

  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElement);

  return inNode == rootNode;
}

PRBool 
nsEditor::IsDescendantOfBody(nsIDOMNode *inNode) 
{
  NS_ENSURE_TRUE(inNode, PR_FALSE);
  nsIDOMElement *rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, PR_FALSE);
  nsCOMPtr<nsIDOMNode> root = do_QueryInterface(rootElement);

  if (inNode == root.get()) return PR_TRUE;
  
  nsCOMPtr<nsIDOMNode> parent, node = do_QueryInterface(inNode);
  
  do
  {
    node->GetParentNode(getter_AddRefs(parent));
    if (parent == root) return PR_TRUE;
    node = parent;
  } while (parent);
  
  return PR_FALSE;
}

PRBool 
nsEditor::IsContainer(nsIDOMNode *aNode)
{
  return aNode ? PR_TRUE : PR_FALSE;
}

PRBool
nsEditor::IsTextInDirtyFrameVisible(nsIDOMNode *aNode)
{
  // virtual method
  //
  // If this is a simple non-html editor,
  // the best we can do is to assume it's visible.

  return PR_TRUE;
}

PRBool 
nsEditor::IsEditable(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, PR_FALSE);

  if (IsMozEditorBogusNode(aNode) || !IsModifiableNode(aNode)) return PR_FALSE;

  // see if it has a frame.  If so, we'll edit it.
  // special case for textnodes: frame must have width.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content)
  {
    nsIFrame *resultFrame = content->GetPrimaryFrame();
    if (!resultFrame)   // if it has no frame, it is not editable
      return PR_FALSE;
    NS_ASSERTION(content->IsNodeOfType(nsINode::eTEXT) ||
                 content->IsElement(),
                 "frame for non element-or-text?");
    if (!content->IsNodeOfType(nsINode::eTEXT))
      return PR_TRUE;  // not a text node; has a frame

    // test the textframe and all its non-fluid continuations
    while (resultFrame) {
      if (resultFrame->GetStateBits() & NS_FRAME_IS_DIRTY) // we can only trust width data for undirty frames
      {
        // In the past a comment said:
        //   "assume all text nodes with dirty frames are editable"
        // Nowadays we use a virtual function, that assumes TRUE
        // in the simple editor world,
        // and uses enhanced logic to find out in the HTML world.
        return IsTextInDirtyFrameVisible(aNode);
      }
      if (resultFrame->GetSize().width > 0) 
        return PR_TRUE;  // text node has width
      resultFrame = resultFrame->GetNextContinuation();
    }
  }
  return PR_FALSE;  // didn't pass any editability test
}

PRBool
nsEditor::IsMozEditorBogusNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> element = do_QueryInterface(aNode);
  return element &&
         element->AttrValueIs(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                              kMOZEditorBogusNodeValue, eCaseMatters);
}

nsresult
nsEditor::CountEditableChildren(nsIDOMNode *aNode, PRUint32 &outCount) 
{
  outCount = 0;
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  nsresult res=NS_OK;
  PRBool hasChildNodes;
  aNode->HasChildNodes(&hasChildNodes);
  if (hasChildNodes)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    res = aNode->GetChildNodes(getter_AddRefs(nodeList));
    if (NS_SUCCEEDED(res) && nodeList) 
    {
      PRUint32 i;
      PRUint32 len;
      nodeList->GetLength(&len);
      for (i=0 ; i<len; i++)
      {
        nsCOMPtr<nsIDOMNode> child;
        res = nodeList->Item((PRInt32)i, getter_AddRefs(child));
        if ((NS_SUCCEEDED(res)) && (child))
        {
          if (IsEditable(child))
          {
            outCount++;
          }
        }
      }
    }
    else if (!nodeList)
      res = NS_ERROR_NULL_POINTER;
  }
  return res;
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
  PRBool doNotify = (mModCount != 0);

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
PRBool 
nsEditor::NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2) 
  {
    NS_NOTREACHED("null node passed to nsEditor::NodesSameType()");
    return PR_FALSE;
  }
  
  return GetTag(aNode1) == GetTag(aNode2);
}


// IsTextOrElementNode: true if node of dom type element or text
//               
PRBool
nsEditor::IsTextOrElementNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextOrElementNode()");
    return PR_FALSE;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  return ((nodeType == nsIDOMNode::ELEMENT_NODE) || (nodeType == nsIDOMNode::TEXT_NODE));
}



///////////////////////////////////////////////////////////////////////////
// IsTextNode: true if node of dom type text
//               
PRBool
nsEditor::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return PR_FALSE;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  return (nodeType == nsIDOMNode::TEXT_NODE);
}


///////////////////////////////////////////////////////////////////////////
// GetIndexOf: returns the position index of the node in the parent
//
PRInt32 
nsEditor::GetIndexOf(nsIDOMNode *parent, nsIDOMNode *child)
{
  nsCOMPtr<nsINode> parentNode = do_QueryInterface(parent);
  NS_PRECONDITION(parentNode, "null parentNode in nsEditor::GetIndexOf");
  NS_PRECONDITION(parentNode->IsNodeOfType(nsINode::eCONTENT) ||
                    parentNode->IsNodeOfType(nsINode::eDOCUMENT),
                  "The parent node must be an element node or a document node");

  nsCOMPtr<nsIContent> cChild = do_QueryInterface(child);
  NS_PRECONDITION(cChild, "null content in nsEditor::GetIndexOf");

  return parentNode->IndexOf(cChild);
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
// GetStartNodeAndOffset: returns whatever the start parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetStartNodeAndOffset(nsISelection *aSelection,
                                       nsIDOMNode **outStartNode,
                                       PRInt32 *outStartOffset)
{
  NS_ENSURE_TRUE(outStartNode && outStartOffset && aSelection, NS_ERROR_NULL_POINTER);

  *outStartNode = nsnull;

  // brade:  set outStartNode to null or ?

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
nsEditor::IsPreformatted(nsIDOMNode *aNode, PRBool *aResult)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  
  NS_ENSURE_TRUE(aResult && content, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
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
    *aResult = PR_FALSE;
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
                        PRBool  aNoEmptyContainers,
                        nsCOMPtr<nsIDOMNode> *outLeftNode,
                        nsCOMPtr<nsIDOMNode> *outRightNode)
{
  NS_ENSURE_TRUE(aNode && aSplitPointParent && outOffset, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> tempNode, parentNode;  
  PRInt32 offset = aSplitPointOffset;
  nsresult res;
  
  if (outLeftNode)  *outLeftNode  = nsnull;
  if (outRightNode) *outRightNode = nsnull;
  
  nsCOMPtr<nsIDOMNode> nodeToSplit = do_QueryInterface(aSplitPointParent);
  while (nodeToSplit)
  {
    // need to insert rules code call here to do things like
    // not split a list if you are after the last <li> or before the first, etc.
    // for now we just have some smarts about unneccessarily splitting
    // textnodes, which should be universal enough to put straight in
    // this nsEditor routine.
    
    nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(nodeToSplit);
    PRUint32 len;
    PRBool bDoSplit = PR_FALSE;
    res = GetLengthOfDOMNode(nodeToSplit, len);
    NS_ENSURE_SUCCESS(res, res);
    
    if (!(aNoEmptyContainers || nodeAsText) || (offset && (offset != (PRInt32)len)))
    {
      bDoSplit = PR_TRUE;
      res = SplitNode(nodeToSplit, offset, getter_AddRefs(tempNode));
      NS_ENSURE_SUCCESS(res, res);
      if (outRightNode) *outRightNode = nodeToSplit;
      if (outLeftNode)  *outLeftNode  = tempNode;
    }

    res = nodeToSplit->GetParentNode(getter_AddRefs(parentNode));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(parentNode, NS_ERROR_FAILURE);

    if (!bDoSplit && offset)  // must be "end of text node" case, we didn't split it, just move past it
    {
      offset = GetIndexOf(parentNode, nodeToSplit) +1;
      if (outLeftNode)  *outLeftNode  = nodeToSplit;
    }
    else
    {
      offset = GetIndexOf(parentNode, nodeToSplit);
      if (outRightNode) *outRightNode = nodeToSplit;
    }
    
    if (nodeToSplit.get() == aNode)  // we split all the way up to (and including) aNode; we're done
      break;
      
    nodeToSplit = parentNode;
  }
  
  if (!nodeToSplit)
  {
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
    if (IsTextNode(leftNodeToJoin))
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText;
      nodeAsText = do_QueryInterface(leftNodeToJoin);
      nodeAsText->GetLength(&length);
    }
    else
    {
      res = GetLengthOfDOMNode(leftNodeToJoin, length);
      NS_ENSURE_SUCCESS(res, res);
    }
    
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

nsresult nsEditor::BeginUpdateViewBatch()
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

    // Turn off view updating.
    nsCOMPtr<nsIPresShell> ps;
    GetPresShell(getter_AddRefs(ps));
    if (ps) {
      nsCOMPtr<nsIViewManager> viewManager = ps->GetViewManager();
      if (viewManager) {
        mBatch.BeginUpdateViewBatch(viewManager);
      }
    }
  }

  mUpdateCount++;

  return NS_OK;
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
    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));

    if (presShell)
      caret = presShell->GetCaret();

    StCaretHider caretHider(caret);

    PRUint32 flags = 0;

    GetFlags(&flags);

    // Turn view updating back on.
    nsCOMPtr<nsIViewManager> viewManager;
    if (presShell)
      viewManager = presShell->GetViewManager();
    if (viewManager)
    {
      PRUint32 updateFlag = NS_VMREFRESH_IMMEDIATE;

      // If we're doing async updates, use NS_VMREFRESH_DEFERRED here, so that
      // the reflows we caused will get processed before the invalidates.
      if (flags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask) {
        updateFlag = NS_VMREFRESH_DEFERRED;
      } else if (presShell) {
        // Flush out layout.  Need to do this because if we have no invalidates
        // to flush the viewmanager code won't flush our reflow here, and we
        // have selection code that does sync caret scrolling in this case.
        presShell->FlushPendingNotifications(Flush_Layout);
      }
      mBatch.EndUpdateViewBatch(updateFlag);
    }

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

PRBool 
nsEditor::GetShouldTxnSetSelection()
{
  return mShouldTxnSetSelection;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  protected nsEditor methods 
#pragma mark -
#endif


NS_IMETHODIMP 
nsEditor::DeleteSelectionImpl(nsIEditor::EDirection aAction)
{
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  nsRefPtr<EditAggregateTxn> txn;
  nsCOMPtr<nsIDOMNode> deleteNode;
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
        mActionListeners[i]->WillDeleteNode(deleteNode);

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
        mActionListeners[i]->DidDeleteNode(deleteNode, res);
  }

  return res;
}

// XXX: error handling in this routine needs to be cleaned up!
NS_IMETHODIMP
nsEditor::DeleteSelectionAndCreateNode(const nsAString& aTag,
                                           nsIDOMNode ** aNewNode)
{
  nsCOMPtr<nsIDOMNode> parentSelectedNode;
  PRInt32 offsetOfNewNode;
  nsresult result = DeleteSelectionAndPrepareToCreateNode(parentSelectedNode,
                                                          offsetOfNewNode);
  NS_ENSURE_SUCCESS(result, result);

  nsCOMPtr<nsIDOMNode> newNode;
  result = CreateNode(aTag, parentSelectedNode, offsetOfNewNode,
                      getter_AddRefs(newNode));
  // XXX: ERROR_HANDLING  check result, and make sure aNewNode is set correctly in success/failure cases
  *aNewNode = newNode;
  NS_IF_ADDREF(*aNewNode);

  // we want the selection to be just after the new node
  nsCOMPtr<nsISelection> selection;
  result = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  return selection->Collapse(parentSelectedNode, offsetOfNewNode+1);
}


/* Non-interface, protected methods */

nsresult
nsEditor::GetIMEBufferLength(PRInt32* length)
{
  *length = mIMEBufferLength;
  return NS_OK;
}

void
nsEditor::SetIsIMEComposing(){  
  // We set mIsIMEComposing according to mIMETextRangeList.
  nsCOMPtr<nsIPrivateTextRange> rangePtr;
  PRUint16 listlen, type;

  mIsIMEComposing = PR_FALSE;
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
        mIsIMEComposing = PR_TRUE;
#ifdef DEBUG_IME
        printf("nsEditor::mIsIMEComposing = PR_TRUE\n");
#endif
        break;
      }
  }
  return;
}

PRBool
nsEditor::IsIMEComposing() {
  return mIsIMEComposing;
}

NS_IMETHODIMP
nsEditor::DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, PRInt32& offsetOfNewNode)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelection> selection;
  result = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  PRBool collapsed;
  result = selection->GetIsCollapsed(&collapsed);
  if (NS_SUCCEEDED(result) && !collapsed) 
  {
    result = DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(result)) {
      return result;
    }
    // get the new selection
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) {
      return result;
    }

    nsCOMPtr<nsIDOMNode> selectedNode;
    selection->GetAnchorNode(getter_AddRefs(selectedNode));
    // no selection is ok.
    // if there is a selection, it must be collapsed
    if (selectedNode)
    {
      PRBool testCollapsed = PR_FALSE;
      selection->GetIsCollapsed(&testCollapsed);
      if (!testCollapsed) {
        result = selection->CollapseToEnd();
        NS_ENSURE_SUCCESS(result, result);
      }
    }
  }
  // split the selected node
  PRInt32 offsetOfSelectedNode;
  result = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
  if (NS_SUCCEEDED(result) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetOfSelectedNode)) && parentSelectedNode)
  {
    nsCOMPtr<nsIDOMNode> selectedNode;
    PRUint32 selectedNodeContentCount=0;
    nsCOMPtr<nsIDOMCharacterData>selectedParentNodeAsText;
    selectedParentNodeAsText = do_QueryInterface(parentSelectedNode);

    offsetOfNewNode = offsetOfSelectedNode;
    
    /* if the selection is a text node, split the text node if necessary
       and compute where to put the new node
    */
    if (selectedParentNodeAsText) 
    { 
      PRInt32 indexOfTextNodeInParent;
      selectedNode = do_QueryInterface(parentSelectedNode);
      selectedNode->GetParentNode(getter_AddRefs(parentSelectedNode));
      selectedParentNodeAsText->GetLength(&selectedNodeContentCount);
      GetChildOffset(selectedNode, parentSelectedNode, indexOfTextNodeInParent);

      if ((offsetOfSelectedNode!=0) && (((PRUint32)offsetOfSelectedNode)!=selectedNodeContentCount))
      {
        nsCOMPtr<nsIDOMNode> newSiblingNode;
        result = SplitNode(selectedNode, offsetOfSelectedNode, getter_AddRefs(newSiblingNode));
        // now get the node's offset in it's parent, and insert the new tag there
        if (NS_SUCCEEDED(result)) {
          result = GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
        }
      }
      else 
      { // determine where to insert the new node
        if (0==offsetOfSelectedNode) {
          offsetOfNewNode = indexOfTextNodeInParent; // insert new node as previous sibling to selection parent
        }
        else {                 // insert new node as last child
          GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
          offsetOfNewNode++;    // offsets are 0-based, and we need the index of the new node
        }
      }
    }
    // Here's where the new node was inserted
  }
#ifdef DEBUG
  else {
    printf("InsertLineBreak into an empty document is not yet supported\n");
  }
#endif
  return result;
}



NS_IMETHODIMP 
nsEditor::DoAfterDoTransaction(nsITransaction *aTxn)
{
  nsresult rv = NS_OK;
  
  PRBool  isTransientTransaction;
  rv = aTxn->GetIsTransient(&isTransientTransaction);
  NS_ENSURE_SUCCESS(rv, rv);
  
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
        
    rv = IncrementModificationCount(1);    // don't count transient transactions
  }
  
  return rv;
}


NS_IMETHODIMP 
nsEditor::DoAfterUndoTransaction()
{
  nsresult rv = NS_OK;

  rv = IncrementModificationCount(-1);    // all undoable transactions are non-transient

  return rv;
}

NS_IMETHODIMP 
nsEditor::DoAfterRedoTransaction()
{
  return IncrementModificationCount(1);    // all redoable transactions are non-transient
}

NS_IMETHODIMP 
nsEditor::CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                   const nsAString& aAttribute, 
                                   const nsAString& aValue,
                                   ChangeAttributeTxn ** aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  *aTxn = new ChangeAttributeTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);
  return (*aTxn)->Init(this, aElement, aAttribute, aValue, PR_FALSE);
}


NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                      const nsAString& aAttribute,
                                      ChangeAttributeTxn ** aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  *aTxn = new ChangeAttributeTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(this, aElement, aAttribute, EmptyString(), PR_TRUE);
}


NS_IMETHODIMP nsEditor::CreateTxnForCreateElement(const nsAString& aTag,
                                                  nsIDOMNode     *aParent,
                                                  PRInt32         aPosition,
                                                  CreateElementTxn ** aTxn)
{
  NS_ENSURE_TRUE(aParent, NS_ERROR_NULL_POINTER);

  *aTxn = new CreateElementTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(this, aTag, aParent, aPosition);
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertElement(nsIDOMNode * aNode,
                                                  nsIDOMNode * aParent,
                                                  PRInt32      aPosition,
                                                  InsertElementTxn ** aTxn)
{
  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);

  *aTxn = new InsertElementTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(aNode, aParent, aPosition, this);
}

NS_IMETHODIMP nsEditor::CreateTxnForDeleteElement(nsIDOMNode * aElement,
                                             DeleteElementTxn ** aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  *aTxn = new DeleteElementTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(this, aElement, &mRangeUpdater);
}

NS_IMETHODIMP 
nsEditor::CreateTxnForIMEText(const nsAString& aStringToInsert,
                              IMETextTxn ** aTxn)
{
  NS_ASSERTION(aTxn, "illegal value- null ptr- aTxn");
     
  *aTxn = new IMETextTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(mIMETextNode, mIMETextOffset, mIMEBufferLength,
                       mIMETextRangeList, aStringToInsert, mSelConWeak);
}


NS_IMETHODIMP 
nsEditor::CreateTxnForAddStyleSheet(nsCSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn)
{
  *aTxn = new AddStyleSheetTxn();
  NS_ENSURE_TRUE( *aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(this, aSheet);
}



NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveStyleSheet(nsCSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn)
{
  *aTxn = new RemoveStyleSheetTxn();
  NS_ENSURE_TRUE( *aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);

  return (*aTxn)->Init(this, aSheet);
}


NS_IMETHODIMP
nsEditor::CreateTxnForDeleteSelection(nsIEditor::EDirection aAction,
                                      EditAggregateTxn ** aTxn,
                                      nsIDOMNode ** aNode,
                                      PRInt32 *aOffset,
                                      PRInt32 *aLength)
{
  NS_ENSURE_TRUE(aTxn, NS_ERROR_NULL_POINTER);
  *aTxn = nsnull;

  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsISelection> selection;
  nsresult result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                         getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    // Check whether the selection is collapsed and we should do nothing:
    PRBool isCollapsed;
    result = (selection->GetIsCollapsed(&isCollapsed));
    if (NS_SUCCEEDED(result) && isCollapsed && aAction == eNone)
      return NS_OK;

    // allocate the out-param transaction
    *aTxn = new EditAggregateTxn();
    if (!*aTxn) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*aTxn);

    nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(selection));
    nsCOMPtr<nsIEnumerator> enumerator;
    result = selPrivate->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(result) && enumerator)
    {
      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsCOMPtr<nsISupports> currentItem;
        result = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if ((NS_SUCCEEDED(result)) && (currentItem))
        {
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
          range->GetCollapsed(&isCollapsed);
          if (!isCollapsed)
          {
            nsRefPtr<DeleteRangeTxn> txn = new DeleteRangeTxn();
            if (txn)
            {
              txn->Init(this, range, &mRangeUpdater);
              (*aTxn)->AppendChild(txn);
            }
            else
              result = NS_ERROR_OUT_OF_MEMORY;
          }
          // Same with range as with selection; if it is collapsed and action
          // is eNone, do nothing.
          else if (aAction != eNone)
          { // we have an insertion point.  delete the thing in front of it or behind it, depending on aAction
            result = CreateTxnForDeleteInsertionPoint(range, aAction, *aTxn, aNode, aOffset, aLength);
          }
        }
      }
    }
  }

  // if we didn't build the transaction correctly, destroy the out-param transaction so we don't leak it.
  if (NS_FAILED(result))
  {
    NS_IF_RELEASE(*aTxn);
  }

  return result;
}

nsresult
nsEditor::CreateTxnForDeleteCharacter(nsIDOMCharacterData  *aData,
                                      PRUint32              aOffset,
                                      nsIEditor::EDirection aDirection,
                                      DeleteTextTxn       **aTxn)
{
  NS_ASSERTION(aDirection == eNext || aDirection == ePrevious,
               "invalid direction");
  nsAutoString data;
  aData->GetData(data);
  PRUint32 segOffset, segLength = 1;
  if (aDirection == eNext) {
    segOffset = aOffset;
    if (segOffset + 1 < data.Length() &&
        NS_IS_HIGH_SURROGATE(data[segOffset]) &&
        NS_IS_LOW_SURROGATE(data[segOffset+1])) {
      // delete both halves of the surrogate pair
      ++segLength;
    }
  } else {
    segOffset = aOffset - 1;
    if (segOffset > 1 &&
        NS_IS_LOW_SURROGATE(data[segOffset]) &&
        NS_IS_HIGH_SURROGATE(data[segOffset-1])) {
      ++segLength;
      --segOffset;
    }
  }
  return CreateTxnForDeleteText(aData, segOffset, segLength, aTxn);
}

//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior are not implemented
NS_IMETHODIMP
nsEditor::CreateTxnForDeleteInsertionPoint(nsIDOMRange          *aRange, 
                                           nsIEditor::EDirection aAction,
                                           EditAggregateTxn     *aTxn,
                                           nsIDOMNode          **aNode,
                                           PRInt32              *aOffset,
                                           PRInt32              *aLength)
{
  NS_ASSERTION(aAction == eNext || aAction == ePrevious, "invalid action");

  // get the node and offset of the insertion point
  nsCOMPtr<nsIDOMNode> node;
  nsresult result = aRange->GetStartContainer(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(result, result);

  PRInt32 offset;
  result = aRange->GetStartOffset(&offset);
  NS_ENSURE_SUCCESS(result, result);

  // determine if the insertion point is at the beginning, middle, or end of the node
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(node);

  PRUint32 count=0;

  if (nodeAsText)
    nodeAsText->GetLength(&count);
  else
  { 
    // get the child list and count
    nsCOMPtr<nsIDOMNodeList>childList;
    result = node->GetChildNodes(getter_AddRefs(childList));
    if ((NS_SUCCEEDED(result)) && childList)
      childList->GetLength(&count);
  }

  PRBool isFirst = (0 == offset);
  PRBool isLast  = (count == (PRUint32)offset);

  // XXX: if isFirst && isLast, then we'll need to delete the node 
  //      as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if ((ePrevious==aAction) && (PR_TRUE==isFirst))
  { // we're backspacing from the beginning of the node.  Delete the first thing to our left
    nsCOMPtr<nsIDOMNode> priorNode;
    result = GetPriorNode(node, PR_TRUE, address_of(priorNode));
    if ((NS_SUCCEEDED(result)) && priorNode)
    { // there is a priorNode, so delete it's last child (if text content, delete the last char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> priorNodeAsText = do_QueryInterface(priorNode);
      if (priorNodeAsText)
      {
        PRUint32 length=0;
        priorNodeAsText->GetLength(&length);
        if (0<length)
        {
          DeleteTextTxn *txn;
          result = CreateTxnForDeleteCharacter(priorNodeAsText, length,
                                               ePrevious, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
            NS_ADDREF(*aNode = priorNode);
            *aOffset = txn->GetOffset();
            *aLength = txn->GetNumCharsToDelete();
            NS_RELEASE(txn);
          }
        }
        else
        { // XXX: can you have an empty text node?  If so, what do you do?
          printf("ERROR: found a text node with 0 characters\n");
          result = NS_ERROR_UNEXPECTED;
        }
      }
      else
      { // priorNode is not text, so tell it's parent to delete it
        DeleteElementTxn *txn;
        result = CreateTxnForDeleteElement(priorNode, &txn);
        if (NS_SUCCEEDED(result)) {
          aTxn->AppendChild(txn);
          NS_RELEASE(txn);
          NS_ADDREF(*aNode = priorNode);
        }
      }
    }
  }
  else if ((nsIEditor::eNext==aAction) && (PR_TRUE==isLast))
  { // we're deleting from the end of the node.  Delete the first thing to our right
    nsCOMPtr<nsIDOMNode> nextNode;
    result = GetNextNode(node, PR_TRUE, address_of(nextNode));
    if ((NS_SUCCEEDED(result)) && nextNode)
    { // there is a nextNode, so delete it's first child (if text content, delete the first char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> nextNodeAsText = do_QueryInterface(nextNode);
      if (nextNodeAsText)
      {
        PRUint32 length=0;
        nextNodeAsText->GetLength(&length);
        if (0<length)
        {
          DeleteTextTxn *txn;
          result = CreateTxnForDeleteCharacter(nextNodeAsText, 0, eNext, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
            NS_ADDREF(*aNode = nextNode);
            *aOffset = txn->GetOffset();
            *aLength = txn->GetNumCharsToDelete();
            NS_RELEASE(txn);
          }
        }
        else
        { // XXX: can you have an empty text node?  If so, what do you do?
          printf("ERROR: found a text node with 0 characters\n");
          result = NS_ERROR_UNEXPECTED;
        }
      }
      else
      { // nextNode is not text, so tell it's parent to delete it
        DeleteElementTxn *txn;
        result = CreateTxnForDeleteElement(nextNode, &txn);
        if (NS_SUCCEEDED(result)) {
          aTxn->AppendChild(txn);
          NS_RELEASE(txn);
          NS_ADDREF(*aNode = nextNode);
        }
      }
    }
  }
  else
  {
    if (nodeAsText)
    { // we have text, so delete a char at the proper offset
      nsRefPtr<DeleteTextTxn> txn;
      result = CreateTxnForDeleteCharacter(nodeAsText, offset, aAction,
                                           getter_AddRefs(txn));
      if (NS_SUCCEEDED(result)) {
        aTxn->AppendChild(txn);
        NS_ADDREF(*aNode = node);
        *aOffset = txn->GetOffset();
        *aLength = txn->GetNumCharsToDelete();
      }
    }
    else
    { // we're either deleting a node or some text, need to dig into the next/prev node to find out
      nsCOMPtr<nsIDOMNode> selectedNode;
      if (ePrevious==aAction)
      {
        result = GetPriorNode(node, offset, PR_TRUE, address_of(selectedNode));
      }
      else if (eNext==aAction)
      {
        result = GetNextNode(node, offset, PR_TRUE, address_of(selectedNode));
      }
      if (NS_FAILED(result)) { return result; }
      if (selectedNode) 
      {
        nsCOMPtr<nsIDOMCharacterData> selectedNodeAsText =
                                             do_QueryInterface(selectedNode);
        if (selectedNodeAsText)
        { // we are deleting from a text node, so do a text deletion
          PRUint32 position = 0;    // default for forward delete
          if (ePrevious==aAction)
          {
            selectedNodeAsText->GetLength(&position);
          }
          nsRefPtr<DeleteTextTxn> delTextTxn;
          result = CreateTxnForDeleteCharacter(selectedNodeAsText, position,
                                               aAction,
                                               getter_AddRefs(delTextTxn));
          if (NS_FAILED(result))  { return result; }
          if (!delTextTxn) { return NS_ERROR_NULL_POINTER; }
          aTxn->AppendChild(delTextTxn);
          NS_ADDREF(*aNode = selectedNode);
          *aOffset = delTextTxn->GetOffset();
          *aLength = delTextTxn->GetNumCharsToDelete();
        }
        else
        {
          nsRefPtr<DeleteElementTxn> delElementTxn;
          result = CreateTxnForDeleteElement(selectedNode,
                                             getter_AddRefs(delElementTxn));
          if (NS_FAILED(result))  { return result; }
          if (!delElementTxn) { return NS_ERROR_NULL_POINTER; }
          aTxn->AppendChild(delElementTxn);
          NS_ADDREF(*aNode = selectedNode);
        }
      }
    }
  }
  return result;
}

nsresult 
nsEditor::CreateRange(nsIDOMNode *aStartParent, PRInt32 aStartOffset,
                      nsIDOMNode *aEndParent, PRInt32 aEndOffset,
                      nsIDOMRange **aRange)
{
  nsresult result;
  result = CallCreateInstance("@mozilla.org/content/range;1", aRange);
  NS_ENSURE_SUCCESS(result, result);

  NS_ENSURE_TRUE(*aRange, NS_ERROR_NULL_POINTER);

  result = (*aRange)->SetStart(aStartParent, aStartOffset);

  if (NS_SUCCEEDED(result))
    result = (*aRange)->SetEnd(aEndParent, aEndOffset);

  if (NS_FAILED(result))
  {
    NS_RELEASE((*aRange));
    *aRange = 0;
  }
  return result;
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
  
  PRInt32 offset;
  res = GetChildOffset(aNode, parentNode, offset);
  NS_ENSURE_SUCCESS(res, res);
  
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
nsEditor::CreateHTMLContent(const nsAString& aTag, nsIContent** aContent)
{
  nsCOMPtr<nsIDOMDocument> tempDoc;
  GetDocument(getter_AddRefs(tempDoc));

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(tempDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // XXX Wallpaper over editor bug (editor tries to create elements with an
  //     empty nodename).
  if (aTag.IsEmpty()) {
    NS_ERROR("Don't pass an empty tag to nsEditor::CreateHTMLContent, "
             "check caller.");
    return NS_ERROR_FAILURE;
  }

  return doc->CreateElem(aTag, nsnull, kNameSpaceID_XHTML, PR_FALSE, aContent);
}

nsresult
nsEditor::SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                   const nsAString & aAttribute,
                                   const nsAString & aValue,
                                   PRBool aSuppressTransaction)
{
  return SetAttribute(aElement, aAttribute, aValue);
}

nsresult
nsEditor::RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      PRBool aSuppressTransaction)
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
      if (nativeKeyEvent->isControl || nativeKeyEvent->isAlt ||
          nativeKeyEvent->isMeta) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::ePrevious);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
    case nsIDOMKeyEvent::DOM_VK_DELETE:
      // on certain platforms (such as windows) the shift key
      // modifies what delete does (cmd_cut in this case).
      // bailing here to allow the keybindings to do the cut.
      if (nativeKeyEvent->isShift || nativeKeyEvent->isControl ||
          nativeKeyEvent->isAlt || nativeKeyEvent->isMeta) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::eNext);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK; 
  }
  return NS_OK;
}

nsresult
nsEditor::HandleInlineSpellCheck(PRInt32 action,
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
  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(GetRoot());
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

  nsCOMPtr<nsIDocument> targetDoc = do_QueryInterface(aFocusEventTarget);
  PRBool isTargetDoc =
    targetNode->IsNodeOfType(nsINode::eDOCUMENT) &&
    targetNode->HasFlag(NODE_IS_EDITABLE);

  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> presShell;
  rv = GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionController> selCon;
  rv = GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionPrivate> selectionPrivate =
    do_QueryInterface(selection);
  NS_ENSURE_TRUE(selectionPrivate, NS_ERROR_UNEXPECTED);

  // Init the caret
  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE(caret, NS_ERROR_UNEXPECTED);
  caret->SetIgnoreUserModify(PR_FALSE);
  caret->SetCaretDOMSelection(selection);
  selCon->SetCaretReadOnly(IsReadonly());
  selCon->SetCaretEnabled(PR_TRUE);

  // Init selection
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
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

nsIDOMElement *
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

NS_IMETHODIMP
nsEditor::SwitchTextDirection()
{
  // Get the current root direction from its frame
  nsIDOMElement *rootElement = GetRoot();

  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(rootElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame *frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE); 

  // Apply the opposite direction
  if (frame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL)
    rv = rootElement->SetAttribute(NS_LITERAL_STRING("dir"), NS_LITERAL_STRING("ltr"));
  else
    rv = rootElement->SetAttribute(NS_LITERAL_STRING("dir"), NS_LITERAL_STRING("rtl"));

  return rv;
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

PRBool
nsEditor::IsModifiableNode(nsIDOMNode *aNode)
{
  return PR_TRUE;
}

nsKeyEvent*
nsEditor::GetNativeKeyEvent(nsIDOMKeyEvent* aDOMKeyEvent)
{
  nsCOMPtr<nsIPrivateDOMEvent> privDOMEvent = do_QueryInterface(aDOMKeyEvent);
  NS_ENSURE_TRUE(privDOMEvent, nsnull);
  nsEvent* nativeEvent = privDOMEvent->GetInternalNSEvent();
  NS_ENSURE_TRUE(nativeEvent, nsnull);
  NS_ENSURE_TRUE(nativeEvent->eventStructType == NS_KEY_EVENT, nsnull);
  return static_cast<nsKeyEvent*>(nativeEvent);
}

PRBool
nsEditor::HasFocus()
{
  nsCOMPtr<nsPIDOMEventTarget> piTarget = GetPIDOMEventTarget();
  if (!piTarget) {
    return PR_FALSE;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, PR_FALSE);

  nsCOMPtr<nsIContent> content = fm->GetFocusedContent();
  return SameCOMIdentity(content, piTarget);
}

PRBool
nsEditor::IsActiveInDOMWindow()
{
  nsCOMPtr<nsPIDOMEventTarget> piTarget = GetPIDOMEventTarget();
  if (!piTarget) {
    return PR_FALSE;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, PR_FALSE);

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  nsPIDOMWindow* ourWindow = doc->GetWindow();
  nsCOMPtr<nsPIDOMWindow> win;
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(ourWindow, PR_FALSE,
                                         getter_AddRefs(win));
  return SameCOMIdentity(content, piTarget);
}

PRBool
nsEditor::IsAcceptableInputEvent(nsIDOMEvent* aEvent)
{
  // If the event is trusted, the event should always cause input.
  nsCOMPtr<nsIDOMNSEvent> NSEvent = do_QueryInterface(aEvent);
  NS_ENSURE_TRUE(NSEvent, PR_FALSE);

  PRBool isTrusted;
  nsresult rv = NSEvent->GetIsTrusted(&isTrusted);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  if (isTrusted) {
    return PR_TRUE;
  }
  // Otherwise, we shouldn't handle any input events when we're not an active
  // element of the DOM window.
  return IsActiveInDOMWindow();
}
