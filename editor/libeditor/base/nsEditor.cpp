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
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
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
#include "nsIDocument.h"
#include "nsITransactionManager.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIViewManager.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionController.h"
#include "nsIEnumerator.h"
#include "nsIAtom.h"
#include "nsICaret.h"
#include "nsIEditActionListener.h"
#include "nsIEditorObserver.h"
#include "nsIKBStateControl.h"
#include "nsIWidget.h"
#include "nsIPlaintextEditor.h"
#include "nsGUIEvent.h"  // nsTextEventReply

#include "nsIFrame.h"  // Needed by IME code

#include "nsICSSStyleSheet.h"
#include "nsIDocumentStateListener.h"
#include "nsITextContent.h"

#include "nsIContent.h"
#include "nsIServiceManagerUtils.h"

// transactions the editor knows how to build
#include "TransactionFactory.h"
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

#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsISelectionDisplay.h"
#include "nsINameSpaceManager.h"
#include "nsIHTMLDocument.h"

#define NS_ERROR_EDITOR_NO_SELECTION NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,1)
#define NS_ERROR_EDITOR_NO_TEXTNODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,2)

#ifdef NS_DEBUG_EDITOR
static PRBool gNoisy = PR_FALSE;
#endif


PRInt32 nsEditor::gInstanceCount = 0;

// Value of "ime.password.onFocus.dontCare"
static PRBool gDontCareForIMEOnFocusPassword = PR_FALSE;
// Value of "ime.password.onBlur.dontCare"
static PRBool gDontCareForIMEOnBlurPassword  = PR_FALSE;

//---------------------------------------------------------------------------
//
// nsEditor: base editor class implementation
//
//---------------------------------------------------------------------------

nsIAtom *nsEditor::gTypingTxnName;
nsIAtom *nsEditor::gIMETxnName;
nsIAtom *nsEditor::gDeleteTxnName;

nsEditor::nsEditor()
:  mModCount(0)
,  mPresShellWeak(nsnull)
,  mViewManager(nsnull)
,  mUpdateCount(0)
,  mPlaceHolderTxn(nsnull)
,  mPlaceHolderName(nsnull)
,  mPlaceHolderBatch(0)
,  mSelState(nsnull)
,  mSavedSel()
,  mRangeUpdater()
,  mShouldTxnSetSelection(PR_TRUE)
,  mBodyElement(nsnull)
,  mAction(nsnull)
,  mDirection(eNone)
,  mInIMEMode(PR_FALSE)
,  mIMETextRangeList(nsnull)
,  mIMETextNode(nsnull)
,  mIMETextOffset(0)
,  mIMEBufferLength(0)
,  mIsIMEComposing(PR_FALSE)
,  mNeedRecoverIMEOpenState(PR_FALSE)
,  mActionListeners(nsnull)
,  mEditorObservers(nsnull)
,  mDocDirtyState(-1)
,  mDocWeak(nsnull)
,  mPhonetic(nsnull)
{
  //initialize member variables here

  PR_AtomicIncrement(&gInstanceCount);

  if (!gTypingTxnName)
    gTypingTxnName = NS_NewAtom("Typing");
  else
    NS_ADDREF(gTypingTxnName);
  if (!gIMETxnName)
    gIMETxnName = NS_NewAtom("IME");
  else
    NS_ADDREF(gIMETxnName);
  if (!gDeleteTxnName)
    gDeleteTxnName = NS_NewAtom("Deleting");
  else
    NS_ADDREF(gDeleteTxnName);
}

nsEditor::~nsEditor()
{
  /* first, delete the transaction manager if there is one.
     this will release any remaining transactions.
     this is important because transactions can hold onto the atoms (gTypingTxnName, ...)
     and to make the optimization (holding refcounted statics) work correctly, 
     the editor instance needs to hold the last refcount.
     If you get this wrong, expect to deref a garbage gTypingTxnName pointer if you bring up a second editor.
  */
  if (mTxnMgr) { 
    mTxnMgr = 0;
  }
  nsrefcnt refCount=0;
  if (gTypingTxnName)  // we addref'd in the constructor
  { // want to release it without nulling out the pointer.
    refCount = gTypingTxnName->Release();
    if (0==refCount) {
      gTypingTxnName = nsnull; 
    }
  }

  if (gIMETxnName)  // we addref'd in the constructor
  { // want to release it without nulling out the pointer.
    refCount = gIMETxnName->Release();
    if (0==refCount) {
      gIMETxnName = nsnull;
    }
  }

  if (gDeleteTxnName)  // we addref'd in the constructor
  { // want to release it without nulling out the pointer.
    refCount = gDeleteTxnName->Release();
    if (0==refCount) {
      gDeleteTxnName = nsnull;
    }
  }

  delete mEditorObservers;   // no need to release observers; we didn't addref them
  mEditorObservers = 0;
  
  if (mActionListeners)
  {
    PRInt32 i;
    nsIEditActionListener *listener;

    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      NS_IF_RELEASE(listener);
    }

    delete mActionListeners;
    mActionListeners = 0;
  }

  /* shut down all classes that needed initialization */
  InsertTextTxn::ClassShutdown();
  IMETextTxn::ClassShutdown();

  delete mPhonetic;
 
  PR_AtomicDecrement(&gInstanceCount);

  NS_IF_RELEASE(mViewManager);
}


NS_IMPL_ISUPPORTS4(nsEditor, nsIEditor, nsIEditorIMESupport, nsISupportsWeakReference, nsIPhonetic)

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

  mFlags = aFlags;
  mDocWeak = do_GetWeakReference(aDoc);  // weak reference to doc
  mPresShellWeak = do_GetWeakReference(aPresShell);   // weak reference to pres shell
  mSelConWeak = do_GetWeakReference(aSelCon);   // weak reference to selectioncontroller
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  
  //set up body element if we are passed one.  
  if (aRoot)
    mBodyElement = do_QueryInterface(aRoot);



  // Set up the DTD
  // XXX - in the long run we want to get this from the document, but there
  // is no way to do that right now.  So we leave it null here and set
  // up a nav html dtd in nsHTMLEditor::Init

  mViewManager = ps->GetViewManager();
  if (!mViewManager) {return NS_ERROR_NULL_POINTER;}
  NS_ADDREF(mViewManager);

  mUpdateCount=0;
  InsertTextTxn::ClassInit();

  /* initalize IME stuff */
  IMETextTxn::ClassInit();
  mIMETextNode = do_QueryInterface(nsnull);
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

  nsresult result;
  nsCOMPtr<nsIPrefBranch> prefBranch = 
      do_GetService(NS_PREFSERVICE_CONTRACTID, &result);
  if (NS_SUCCEEDED(result) && prefBranch) {
    PRBool val;
    if (NS_SUCCEEDED(prefBranch->GetBoolPref("ime.password.onFocus.dontCare", &val)))
      gDontCareForIMEOnFocusPassword = val;
    if (NS_SUCCEEDED(prefBranch->GetBoolPref("ime.password.onBlur.dontCare", &val)))
      gDontCareForIMEOnBlurPassword = val;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::PostCreate()
{
  // nuke the modification count, so the doc appears unmodified
  // do this before we notify listeners
  ResetModificationCount();
  
  // update the UI with our state
  NotifyDocumentListeners(eDocumentCreated);
  NotifyDocumentListeners(eDocumentStateChanged);
  
  // Call ResetInputState() for initialization
  ForceCompositionEnd();

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::PreDestroy()
{
  // tell our listeners that the doc is going away
  NotifyDocumentListeners(eDocumentToBeDestroyed);
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
  mFlags = aFlags;
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
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;
  *aDoc = nsnull; // init out param
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  NS_ADDREF(*aDoc = doc);
  return NS_OK;
}


nsresult 
nsEditor::GetPresShell(nsIPresShell **aPS)
{
  if (!aPS)
    return NS_ERROR_NULL_POINTER;
  *aPS = nsnull; // init out param
  NS_PRECONDITION(mPresShellWeak, "bad state, null mPresShellWeak");
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
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
  if (!aSel)
    return NS_ERROR_NULL_POINTER;
  *aSel = nsnull; // init out param
  NS_PRECONDITION(mSelConWeak, "bad state, null mSelConWeak");
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  if (!selCon) return NS_ERROR_NOT_INITIALIZED;
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
  if (!aSelection)
    return NS_ERROR_NULL_POINTER;
  *aSelection = nsnull;
  nsCOMPtr<nsISelectionController> selcon = do_QueryReferent(mSelConWeak);
  if (!selcon) return NS_ERROR_NOT_INITIALIZED;
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
    EditTxn *editTxn;
    result = TransactionFactory::GetNewTransaction(PlaceholderTxn::GetCID(), &editTxn);
    if (NS_FAILED(result)) { return result; }
    if (!editTxn) { return NS_ERROR_NULL_POINTER; }

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
      if (NS_FAILED(result)) return result;
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
    
    // txn mgr will now own this if it's around, and if it isn't we don't care
    NS_IF_RELEASE(editTxn);    
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
    if (NS_FAILED(result)) { return result; }
    if (!selection) { return NS_ERROR_NULL_POINTER; }
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
 
  NS_POSTCONDITION((NS_SUCCEEDED(result)), "transaction did not execute properly\n");

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
  if (!mTxnMgr)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aTxnManager = mTxnMgr);
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
  if (!hasTransaction)
    return result;

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
  if (!aIsEnabled || !aCanUndo)
     return NS_ERROR_NULL_POINTER;
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
  if (!hasTransaction)
    return result;

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
  if (!aIsEnabled || !aCanRedo)
     return NS_ERROR_NULL_POINTER;

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
    if (NS_FAILED(res)) return res;
    mSelState = new nsSelectionState();
    if (!mSelState)
      return NS_ERROR_OUT_OF_MEMORY;

    mSelState->SaveSelection(selection);
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
    nsresult rv = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(rv))
      return rv;

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
  if (!aResult) return NS_ERROR_NULL_POINTER;
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
  nsCOMPtr<nsIDOMElement> rootElement; 
  nsresult res = GetRootElement(getter_AddRefs(rootElement)); 
  if (NS_FAILED(res)) return res; 
  if (!rootElement)   return NS_ERROR_NULL_POINTER; 
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElement);
  nsCOMPtr<nsIDOMNode> firstChild;
  res = rootNode->GetFirstChild(getter_AddRefs(firstChild));

  *aDocumentIsEmpty = NS_SUCCEEDED(res) && firstChild;
  return res;
}


// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  if (!selCon) return NS_ERROR_NOT_INITIALIZED;
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
  if (NS_FAILED(result))
    return result;
  if (!selection)
    return NS_ERROR_NOT_INITIALIZED;
    
  // get the root element 
  nsCOMPtr<nsIDOMElement> rootElement; 
  result = GetRootElement(getter_AddRefs(rootElement)); 
  if (NS_FAILED(result)) return result; 
  if (!rootElement)   return NS_ERROR_NULL_POINTER; 
  
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
      if (NS_FAILED(result)) return result;
      result = selection->Collapse(parentNode, offsetInParent);
    }
  }
  else
  {
    // just the body node, set selection to inside the body
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
  if (NS_FAILED(res)) return res; 
  if (!selection)   return NS_ERROR_NULL_POINTER; 
  
  // get the root element 
  nsCOMPtr<nsIDOMElement> rootElement; 
  res = GetRootElement(getter_AddRefs(rootElement)); 
  if (NS_FAILED(res)) return res; 
  if (!rootElement)   return NS_ERROR_NULL_POINTER; 
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElement); 
  
  // get the length of the rot element 
  PRUint32 len; 
  res = GetLengthOfDOMNode(rootNode, len); 
  if (NS_FAILED(res)) return res; 
  
  // set the selection to after the last child of the root element 
  return selection->Collapse(rootNode, (PRInt32)len); 
} 
  
NS_IMETHODIMP
nsEditor::GetDocumentModified(PRBool *outDocModified)
{
  if (!outDocModified)
    return NS_ERROR_NULL_POINTER;

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

//
// Get an appropriate wrap width for saving this document.
// This class just uses a pref; subclasses are expected to
// override if they know more about the document.
//
NS_IMETHODIMP
nsEditor::GetWrapWidth(PRInt32 *aWrapColumn)
{
  NS_ENSURE_ARG_POINTER(aWrapColumn);
  *aWrapColumn = 72;

  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
    (void) prefBranch->GetIntPref("editor.htmlWrapColumn", aWrapColumn);
  return NS_OK;
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
nsEditor::CanPaste(PRInt32 aSelectionType, PRBool *aCanPaste)
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
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForSetAttribute(aElement, aAttribute, aValue, &txn);
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  return result;
}

NS_IMETHODIMP 
nsEditor::GetAttributeValue(nsIDOMElement *aElement, 
                            const nsAString & aAttribute, 
                            nsAString & aResultValue, 
                            PRBool *aResultIsSet)
{
  if (!aResultIsSet)
    return NS_ERROR_NULL_POINTER;
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
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForRemoveAttribute(aElement, aAttribute, &txn);
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  return result;
}


NS_IMETHODIMP
nsEditor::MarkNodeDirty(nsIDOMNode* aNode)
{  
  //  mark the node dirty.
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(aNode));
  if (element)
    element->SetAttribute(NS_LITERAL_STRING("_moz_dirty"), EmptyString());
  return NS_OK;
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
  nsIEditActionListener *listener;

  nsAutoRules beginRulesSniffing(this, kOpCreateNode, nsIEditor::eNext);
  
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillCreateNode(aTag, aParent, aPosition);
    }
  }

  CreateElementTxn *txn;
  nsresult result = CreateTxnForCreateElement(aTag, aParent, aPosition, &txn);
  if (NS_SUCCEEDED(result)) 
  {
    result = DoTransaction(txn);  
    if (NS_SUCCEEDED(result)) 
    {
      result = txn->GetNewNode(aNewNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "GetNewNode can't fail if txn::DoTransaction succeeded.");
    }
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  
  mRangeUpdater.SelAdjCreateNode(aParent, aPosition);
  
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidCreateNode(aTag, *aNewNode, aParent, aPosition, result);
    }
  }

  return result;
}


NS_IMETHODIMP nsEditor::InsertNode(nsIDOMNode * aNode,
                                   nsIDOMNode * aParent,
                                   PRInt32      aPosition)
{
  PRInt32 i;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillInsertNode(aNode, aParent, aPosition);
    }
  }

  InsertElementTxn *txn;
  nsresult result = CreateTxnForInsertElement(aNode, aParent, aPosition, &txn);
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  mRangeUpdater.SelAdjInsertNode(aParent, aPosition);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidInsertNode(aNode, aParent, aPosition, result);
    }
  }

  return result;
}


NS_IMETHODIMP 
nsEditor::SplitNode(nsIDOMNode * aNode,
                    PRInt32      aOffset,
                    nsIDOMNode **aNewLeftNode)
{
  PRInt32 i;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpSplitNode, nsIEditor::eNext);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillSplitNode(aNode, aOffset);
    }
  }

  SplitElementTxn *txn;
  nsresult result = CreateTxnForSplitNode(aNode, aOffset, &txn);
  if (NS_SUCCEEDED(result))  
  {
    result = DoTransaction(txn);
    if (NS_SUCCEEDED(result))
    {
      result = txn->GetNewNode(aNewLeftNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "result must succeeded for GetNewNode");
    }
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  mRangeUpdater.SelAdjSplitNode(aNode, aOffset, *aNewLeftNode);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
      {
        nsIDOMNode *ptr = (aNewLeftNode) ? *aNewLeftNode : 0;
        listener->DidSplitNode(aNode, aOffset, ptr, result);
      }
    }
  }

  return result;
}



NS_IMETHODIMP
nsEditor::JoinNodes(nsIDOMNode * aLeftNode,
                    nsIDOMNode * aRightNode,
                    nsIDOMNode * aParent)
{
  PRInt32 i, offset;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpJoinNode, nsIEditor::ePrevious);

  // remember some values; later used for saved selection updating.
  // find the offset between the nodes to be joined.
  nsresult result = GetChildOffset(aRightNode, aParent, offset);
  if (NS_FAILED(result)) return result;
  // find the number of children of the lefthand node
  PRUint32 oldLeftNodeLen;
  result = GetLengthOfDOMNode(aLeftNode, oldLeftNodeLen);
  if (NS_FAILED(result)) return result;

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillJoinNodes(aLeftNode, aRightNode, aParent);
    }
  }

  JoinElementTxn *txn;
  result = CreateTxnForJoinNode(aLeftNode, aRightNode, &txn);
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }

  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  mRangeUpdater.SelAdjJoinNodes(aLeftNode, aRightNode, aParent, offset, (PRInt32)oldLeftNodeLen);
  
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidJoinNodes(aLeftNode, aRightNode, aParent, result);
    }
  }

  return result;
}


NS_IMETHODIMP nsEditor::DeleteNode(nsIDOMNode * aElement)
{
  PRInt32 i, offset;
  nsCOMPtr<nsIDOMNode> parent;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpCreateNode, nsIEditor::ePrevious);

  // save node location for selection updating code.
  nsresult result = GetNodeLocation(aElement, address_of(parent), &offset);
  if (NS_FAILED(result)) return result;

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillDeleteNode(aElement);
    }
  }

  DeleteElementTxn *txn;
  result = CreateTxnForDeleteElement(aElement, &txn);
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }

  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidDeleteNode(aElement, result);
    }
  }

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
  if (!inNode || !outNode)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  nsresult res = GetNodeLocation(inNode, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;

  // create new container
  nsCOMPtr<nsIContent> newContent;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(aNodeType, getter_AddRefs(newContent));
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(newContent);
  if (NS_FAILED(res)) return res;
    *outNode = do_QueryInterface(elem);
  
  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty())
  {
    res = elem->SetAttribute(*aAttribute, *aValue);
    if (NS_FAILED(res)) return res;
  }
  if (aCloneAttributes)
  {
    nsCOMPtr<nsIDOMNode>newNode = do_QueryInterface(elem);
    res = CloneAttributes(newNode, inNode);
    if (NS_FAILED(res)) return res;
  }
  
  // notify our internal selection state listener
  // (Note: A nsAutoSelectionReset object must be created
  //  before calling this to initialize mRangeUpdater)
  nsAutoReplaceContainerSelNotify selStateNotify(mRangeUpdater, inNode, *outNode);
  
  nsCOMPtr<nsIDOMNode> child;
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  while (bHasMoreChildren)
  {
    inNode->GetFirstChild(getter_AddRefs(child));
    res = DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = InsertNode(child, *outNode, -1);
    if (NS_FAILED(res)) return res;
    inNode->HasChildNodes(&bHasMoreChildren);
  }

  // insert new container into tree
  res = InsertNode( *outNode, parent, offset);
  if (NS_FAILED(res)) return res;
  
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
  if (!inNode)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  
  nsresult res = GetNodeLocation(inNode, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;
  
  // loop through the child nodes of inNode and promote them
  // into inNode's parent.
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  nsCOMPtr<nsIDOMNodeList> nodeList;
  res = inNode->GetChildNodes(getter_AddRefs(nodeList));
  if (NS_FAILED(res)) return res;
  if (!nodeList) return NS_ERROR_NULL_POINTER;
  PRUint32 nodeOrigLen;
  nodeList->GetLength(&nodeOrigLen);

  // notify our internal selection state listener
  nsAutoRemoveContainerSelNotify selNotify(mRangeUpdater, inNode, parent, offset, nodeOrigLen);
                                   
  nsCOMPtr<nsIDOMNode> child;
  while (bHasMoreChildren)
  {
    inNode->GetLastChild(getter_AddRefs(child));
    res = DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = InsertNode(child, parent, offset);
    if (NS_FAILED(res)) return res;
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
  if (!inNode || !outNode)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  nsresult res = GetNodeLocation(inNode, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;

  // create new container
  nsCOMPtr<nsIContent> newContent;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(aNodeType, getter_AddRefs(newContent));
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(newContent);
  if (NS_FAILED(res)) return res;
  *outNode = do_QueryInterface(elem);
  
  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty())
  {
    res = elem->SetAttribute(*aAttribute, *aValue);
    if (NS_FAILED(res)) return res;
  }
  
  // notify our internal selection state listener
  nsAutoInsertContainerSelNotify selNotify(mRangeUpdater);
  
  // put inNode in new parent, outNode
  res = DeleteNode(inNode);
  if (NS_FAILED(res)) return res;
  res = InsertNode(inNode, *outNode, 0);
  if (NS_FAILED(res)) return res;
  
  // put new parent in doc
  return InsertNode(*outNode, parent, offset);
}

///////////////////////////////////////////////////////////////////////////
// MoveNode:  move aNode to {aParent,aOffset}
nsresult
nsEditor::MoveNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aOffset)
{
  if (!aNode || !aParent)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> oldParent;
  PRInt32 oldOffset;
  nsresult res = GetNodeLocation(aNode, address_of(oldParent), &oldOffset);
  
  if (aOffset == -1)
  {
    PRUint32 unsignedOffset;
    // magic value meaning "move to end of aParent"
    res = GetLengthOfDOMNode(aParent, unsignedOffset);
    if (NS_FAILED(res)) return res;
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
  if (NS_FAILED(res)) return res;
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
  
  if (!aObserver)
    return NS_ERROR_NULL_POINTER;

  if (!mEditorObservers)
  {
    mEditorObservers = new nsVoidArray();

    if (!mEditorObservers)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make sure the listener isn't already on the list
  if (mEditorObservers->IndexOf(aObserver) == -1) 
  {
    if (!mEditorObservers->AppendElement((void *)aObserver))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditorObserver(nsIEditorObserver *aObserver)
{
  if (!aObserver || !mEditorObservers)
    return NS_ERROR_FAILURE;

  if (!mEditorObservers->RemoveElement((void *)aObserver))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

void nsEditor::NotifyEditorObservers(void)
{
  if (mEditorObservers)
  {
    PRInt32 i;
    nsIEditorObserver *observer;
    for (i = 0; i < mEditorObservers->Count(); i++)
    {
      observer = (nsIEditorObserver*)mEditorObservers->ElementAt(i);
      if (observer)
        observer->EditAction();
    }
  }
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  action listener maintainance
#pragma mark -
#endif

NS_IMETHODIMP
nsEditor::AddEditActionListener(nsIEditActionListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  if (!mActionListeners)
  {
    mActionListeners = new nsVoidArray();

    if (!mActionListeners)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make sure the listener isn't already on the list
  if (mActionListeners->IndexOf(aListener) == -1) 
  {
    if (!mActionListeners->AppendElement((void *)aListener))
      return NS_ERROR_FAILURE;
    else
      NS_ADDREF(aListener);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditActionListener(nsIEditActionListener *aListener)
{
  if (!aListener || !mActionListeners)
    return NS_ERROR_FAILURE;

  if (!mActionListeners->RemoveElement((void *)aListener))
    return NS_ERROR_FAILURE;

  NS_IF_RELEASE(aListener);

  if (mActionListeners->Count() < 1)
  {
    delete mActionListeners;
    mActionListeners = 0;
  }

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
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  
  if (!mDocStateListeners)
  {
    rv = NS_NewISupportsArray(getter_AddRefs(mDocStateListeners));
    if (NS_FAILED(rv)) return rv;
  }
  
  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(aListener, &rv);
  if (NS_FAILED(rv)) return rv;
    
  // is it already in the list?
  PRInt32 foundIndex;
  if (NS_SUCCEEDED(mDocStateListeners->GetIndexOf(iSupports, &foundIndex)) && foundIndex != -1)
    return NS_OK;

  return mDocStateListeners->AppendElement(iSupports);
}


NS_IMETHODIMP
nsEditor::RemoveDocumentStateListener(nsIDocumentStateListener *aListener)
{
  if (!aListener || !mDocStateListeners)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;
  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(aListener, &rv);
  if (NS_FAILED(rv)) return rv;

  return mDocStateListeners->RemoveElement(iSupports);
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
  nsCOMPtr<nsIContent> root = do_QueryInterface(mBodyElement);
  if (root)  root->List(stdout);
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugDumpContent()
{
#ifdef DEBUG
  nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;

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
#pragma mark  nsIEditorIMESupport 
#pragma mark -
#endif

//
// The BeingComposition method is called from the Editor Composition event listeners.
//
NS_IMETHODIMP
nsEditor::QueryComposition(nsTextEventReply* aReply)
{
  nsresult result;
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsISelectionController> selcon = do_QueryReferent(mSelConWeak);
  if (selcon)
    selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsICaret> caretP; 
  result = ps->GetCaret(getter_AddRefs(caretP));
  
  if (NS_SUCCEEDED(result) && caretP) {
    if (aReply) {
      caretP->SetCaretDOMSelection(selection);

      // XXX_kin: BEGIN HACK! HACK! HACK!
      // XXX_kin:
      // XXX_kin: This is lame! The IME stuff needs caret coordinates
      // XXX_kin: synchronously, but the editor could be using async
      // XXX_kin: updates (reflows and paints) for performance reasons.
      // XXX_kin: In order to give IME what it needs, we have to temporarily
      // XXX_kin: switch to sync updating during this call so that the
      // XXX_kin: nsAutoUpdateViewBatch can force sync reflows and paints
      // XXX_kin: so that we get back accurate caret coordinates.

      PRUint32 flags = 0;

      if (NS_SUCCEEDED(GetFlags(&flags)) &&
          (flags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask))
      {
        PRBool restoreFlags = PR_FALSE;

        if (NS_SUCCEEDED(SetFlags(flags & (~nsIPlaintextEditor::eEditorUseAsyncUpdatesMask))))
        {
           // Scope the viewBatch within this |if| block so that we
           // force synchronous reflows and paints before restoring
           // our editor flags below.

           nsAutoUpdateViewBatch viewBatch(this);
           restoreFlags = PR_TRUE;
        }

        // Restore the previous set of flags!

        if (restoreFlags)
          SetFlags(flags);
      }


      // XXX_kin: END HACK! HACK! HACK!

      result = caretP->GetCaretCoordinates(nsICaret::eIMECoordinates, selection,
		                      &(aReply->mCursorPosition), &(aReply->mCursorIsCollapsed), nsnull);
    }
  }
  return result;
}
NS_IMETHODIMP
nsEditor::BeginComposition(nsTextEventReply* aReply)
{
#ifdef DEBUG_tague
  printf("nsEditor::StartComposition\n");
#endif
  nsresult ret = QueryComposition(aReply);
  mInIMEMode = PR_TRUE;
  if (mPhonetic)
    mPhonetic->Truncate(0);

  // If user changes the IME open state, don't recover it.
  // Because the user may not want to change the state.
  if (mNeedRecoverIMEOpenState)
    mNeedRecoverIMEOpenState = PR_FALSE;

  return ret;
}

NS_IMETHODIMP
nsEditor::EndComposition(void)
{
  if (!mInIMEMode) return NS_OK; // nothing to do
  
  nsresult result = NS_OK;

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTxnMgr) 
  {
    nsCOMPtr<nsITransaction> txn;
    result = mTxnMgr->PeekUndoStack(getter_AddRefs(txn));  
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryInterface(txn);
    if (plcTxn)
    {
      result = plcTxn->Commit();
    }
  }

  /* reset the data we need to construct a transaction */
  mIMETextNode = do_QueryInterface(nsnull);
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  mInIMEMode = PR_FALSE;
  mIsIMEComposing = PR_FALSE;

  // notify editor observers of action
  NotifyEditorObservers();

  return result;
}

NS_IMETHODIMP
nsEditor::SetCompositionString(const nsAString& aCompositionString, nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
GetEditorContentWindow(nsIPresShell *aPresShell, nsIDOMElement *aRoot, nsIWidget **aResult)
{
  if (!aPresShell || !aRoot || !aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = 0;

  nsresult result;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aRoot);

  if (!content)
    return NS_ERROR_FAILURE;

  nsIFrame *frame = 0; // Not ref counted

  result = aPresShell->GetPrimaryFrameFor(content, &frame);

  if (NS_FAILED(result))
    return result;

  if (!frame)
    return NS_ERROR_FAILURE;

  // Check first to see if this frame contains a view with a native widget.
  nsIView *view = frame->GetViewExternal();

  if (view)
  {
    *aResult = view->GetWidget();

    if (*aResult) {
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  // frame doesn't have a view with a widget, so call GetWindow()
  // which will traverse it's parent hierarchy till it finds a
  // view with a widget.

  *aResult = frame->GetWindow();
  if (!*aResult)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult
nsEditor::GetKBStateControl(nsIKBStateControl **aKBSC)
{
  if (!aKBSC)
    return NS_ERROR_NULL_POINTER;
  *aKBSC = nsnull;
  nsCOMPtr<nsIPresShell> shell;
  nsresult res = GetPresShell(getter_AddRefs(shell));

  if (NS_FAILED(res))
    return res;

  if (!shell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWidget> widget;
  res = GetEditorContentWindow(shell, mBodyElement, getter_AddRefs(widget));
  if (NS_FAILED(res))
    return res;

  nsCOMPtr<nsIKBStateControl> kb = do_QueryInterface(widget);
  if (!kb)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ADDREF(*aKBSC = kb);

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
  if(! mInIMEMode)
    return NS_OK;
#endif

#ifdef XP_UNIX
  if(mFlags & nsIPlaintextEditor::eEditorPasswordMask)
	return NS_OK;
#endif

  nsCOMPtr<nsIKBStateControl> kb;
  nsresult res = GetKBStateControl(getter_AddRefs(kb));
  if (NS_FAILED(res))
    return res;

  if (kb) {
    res = kb->ResetInputState();
    if (NS_FAILED(res)) 
      return res;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::NotifyIMEOnFocus()
{
  mNeedRecoverIMEOpenState = PR_FALSE;

  if(gDontCareForIMEOnFocusPassword
      || !(mFlags & nsIPlaintextEditor::eEditorPasswordMask))
    return NS_OK;

  nsCOMPtr<nsIKBStateControl> kb;
  nsresult res = GetKBStateControl(getter_AddRefs(kb));
  if (NS_FAILED(res))
    return res;

  if (kb) {
    PRBool isOpen;
    res = kb->GetIMEOpenState(&isOpen);
    if (NS_FAILED(res)) 
      return res;

    if (isOpen) {
      res = kb->SetIMEOpenState(PR_FALSE);
      if (NS_FAILED(res)) 
        return res;
    }

    mNeedRecoverIMEOpenState = isOpen;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::NotifyIMEOnBlur()
{
  if (!mNeedRecoverIMEOpenState)
    return NS_OK;
  mNeedRecoverIMEOpenState = PR_FALSE;

  if (gDontCareForIMEOnBlurPassword
      || !(mFlags & nsIPlaintextEditor::eEditorPasswordMask))
    return NS_OK;

  nsCOMPtr<nsIKBStateControl> kb;
  nsresult res = GetKBStateControl(getter_AddRefs(kb));
  if (NS_FAILED(res))
    return res;

  if (kb) {
    res = kb->SetIMEOpenState(PR_TRUE);
    if (NS_FAILED(res)) 
      return res;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetReconversionString(nsReconversionEventReply* aReply)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  public nsEditor methods 
#pragma mark -
#endif
/* Non-interface, public methods */


// This seems like too much work! There should be a "nsDOMDocument::GetBody()"
// Have a look in nsHTMLDocument. Maybe add it to nsIHTMLDocument.
NS_IMETHODIMP 
nsEditor::GetRootElement(nsIDOMElement **aBodyElement)
{
  if (!aBodyElement)
    return NS_ERROR_NULL_POINTER;

  *aBodyElement = 0;

  if (mBodyElement)
  {
    // if we have cached the body element, use that
    *aBodyElement = mBodyElement;
    NS_ADDREF(*aBodyElement);
    return NS_OK;
  }
  
  NS_PRECONDITION(mDocWeak, "bad state, null mDocWeak");
  nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMHTMLElement>bodyElement; 
  nsresult result = doc->GetBody(getter_AddRefs(bodyElement));
  if (NS_FAILED(result))
    return result;
  
  if (!bodyElement)
    return NS_ERROR_NULL_POINTER;

  // Use the first body node in the list:
  mBodyElement = do_QueryInterface(bodyElement);
  *aBodyElement = bodyElement;
  NS_ADDREF(*aBodyElement);

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
  if (!aDestNode || !aSourceNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  if (!destElement || !sourceElement)
    return NS_ERROR_NO_INTERFACE;

  nsAutoString attrValue;
  PRBool isAttrSet;
  nsresult rv = GetAttributeValue(sourceElement,
                                  aAttribute,
                                  attrValue,
                                  &isAttrSet);
  if (NS_FAILED(rv))
    return rv;
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
  if (!aDestNode || !aSourceNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  if (!destElement || !sourceElement)
    return NS_ERROR_NO_INTERFACE;

  nsCOMPtr<nsIDOMNamedNodeMap> sourceAttributes;
  sourceElement->GetAttributes(getter_AddRefs(sourceAttributes));
  nsCOMPtr<nsIDOMNamedNodeMap> destAttributes;
  destElement->GetAttributes(getter_AddRefs(destAttributes));
  if (!sourceAttributes || !destAttributes)
    return NS_ERROR_FAILURE;

  nsAutoEditBatch beginBatching(this);

  // Use transaction system for undo only if destination
  //   is already in the document
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult result = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(result)) return result;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;

  PRBool destInBody = PR_TRUE;
  nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(bodyElement);
  nsCOMPtr<nsIDOMNode> p = aDestNode;
  while (p && p!= bodyNode)
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

      syncScroll = !(flags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask);
    }

    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                    region, syncScroll);
  }

  return NS_OK;
}

/** static helper method */
nsresult nsEditor::GetTextNodeTag(nsAString& aOutString)
{
  aOutString.Truncate();
  static nsString *gTextNodeTag=nsnull;
  if (!gTextNodeTag)
  {
    if ( (gTextNodeTag = new nsString) == 0 )
      return NS_ERROR_OUT_OF_MEMORY;
    gTextNodeTag->AssignLiteral("special text node tag");
  }
  aOutString = *gTextNodeTag;
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
  
  if (!aInOutNode || !*aInOutNode || !aInOutOffset || !aDoc) return NS_ERROR_NULL_POINTER;
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
      if (NS_FAILED(res)) return res;
      if (!nodeAsText) return NS_ERROR_NULL_POINTER;
      nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(nodeAsText);
      // then we insert it into the dom tree
      res = InsertNode(newNode, *aInOutNode, offset);
      if (NS_FAILED(res)) return res;
      offset = 0;
    }
    res = InsertTextIntoTextNodeImpl(aStringToInsert, nodeAsText, offset);
    if (NS_FAILED(res)) return res;
  }
  else
  {
    if (nodeAsText)
    {
      // we are inserting text into an existing text node.
      res = InsertTextIntoTextNodeImpl(aStringToInsert, nodeAsText, offset);
      if (NS_FAILED(res)) return res;
      *aInOutOffset += aStringToInsert.Length();
    }
    else
    {
      // we are inserting text into a non-text node
      // first we have to create a textnode (this also populates it with the text)
      res = aDoc->CreateTextNode(aStringToInsert, getter_AddRefs(nodeAsText));
      if (NS_FAILED(res)) return res;
      if (!nodeAsText) return NS_ERROR_NULL_POINTER;
      nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(nodeAsText);
      // then we insert it into the dom tree
      res = InsertNode(newNode, *aInOutNode, offset);
      if (NS_FAILED(res)) return res;
      *aInOutNode = newNode;
      *aInOutOffset = aStringToInsert.Length();
    }
  }
  return res;
}


NS_IMETHODIMP nsEditor::InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert, 
                                                     nsIDOMCharacterData *aTextNode, 
                                                     PRInt32 aOffset, PRBool suppressIME)
{
  EditTxn *txn;
  nsresult result;
  // suppressIME s used when editor must insert text, yet this text is not
  // part of current ime operation.  example: adjusting whitespace around an ime insertion.
  if (mIMETextRangeList && mInIMEMode && !suppressIME)
  {
    if (!mIMETextNode)
    {
      mIMETextNode = aTextNode;
      mIMETextOffset = aOffset;
    }
    PRUint16 len ;
    result = mIMETextRangeList->GetLength(&len);
    if (NS_SUCCEEDED(result) && len > 0)
    {
      nsCOMPtr<nsIPrivateTextRange> range;
      for (PRUint16 i = 0; i < len; i++) 
      {
        result = mIMETextRangeList->Item(i, getter_AddRefs(range));
        if (NS_SUCCEEDED(result) && range)
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

    result = CreateTxnForIMEText(aStringToInsert, (IMETextTxn**)&txn);
  }
  else
  {
    result = CreateTxnForInsertText(aStringToInsert, aTextNode, aOffset, (InsertTextTxn**)&txn);
  }
  if (NS_FAILED(result)) return result;  // we potentially leak txn here?

  // let listeners know whats up
  PRInt32 i;
  nsIEditActionListener *listener;
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillInsertText(aTextNode, aOffset, aStringToInsert);
    }
  }
  
  // XXX we may not need these view batches anymore.  This is handled at a higher level now I believe
  BeginUpdateViewBatch();
  result = DoTransaction(txn);
  EndUpdateViewBatch();

  mRangeUpdater.SelAdjInsertText(aTextNode, aOffset, aStringToInsert);
  
  // let listeners know what happened
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidInsertText(aTextNode, aOffset, aStringToInsert, result);
    }
  }

  // Added some cruft here for bug 43366.  Layout was crashing because we left an 
  // empty text node lying around in the document.  So I delete empty text nodes
  // caused by IME.  I have to mark the IME transaction as "fixed", which means
  // that furure ime txns won't merge with it.  This is because we don't want
  // future ime txns trying to put their text into a node that is no longer in
  // the document.  This does not break undo/redo, because all these txns are 
  // wrapped in a parent PlaceHolder txn, and placeholder txns are already 
  // savvy to having multiple ime txns inside them.
  
  // delete empty ime text node if there is one
  if (mInIMEMode && mIMETextNode)
  {
    PRUint32 len;
    mIMETextNode->GetLength(&len);
    if (!len)
    {
      DeleteNode(mIMETextNode);
      mIMETextNode = nsnull;
      ((IMETextTxn*)txn)->MarkFixed();  // mark the ime txn "fixed"
    }
  }
  
  // The transaction system (if any) has taken ownwership of txns.
  // aggTxn released at end of routine.
  NS_IF_RELEASE(txn);
  return result;
}


NS_IMETHODIMP nsEditor::SelectEntireDocument(nsISelection *aSelection)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMElement>bodyElement;
  nsresult result = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
    if (bodyNode)
    {
      result = aSelection->SelectAllChildren(bodyNode);
    }
    else {
      return NS_ERROR_NO_INTERFACE;
    }
  }
  return result;
}


nsresult nsEditor::GetFirstEditableNode(nsIDOMNode *aRoot, nsCOMPtr<nsIDOMNode> *outFirstNode)
{
  if (!aRoot || !outFirstNode) return NS_ERROR_NULL_POINTER;
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
  if (!aRoot || !outLastNode) return NS_ERROR_NULL_POINTER;
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
  if (!mDocStateListeners)
    return NS_OK;    // maybe there just aren't any.
 
  PRUint32 numListeners;
  nsresult rv = mDocStateListeners->Count(&numListeners);
  if (NS_FAILED(rv)) return rv;

  PRUint32 i;
  switch (aNotificationType)
  {
    case eDocumentCreated:
      for (i = 0; i < numListeners;i++)
      {
        nsCOMPtr<nsIDocumentStateListener> thisListener =
          do_QueryElementAt(mDocStateListeners, i);
        if (thisListener)
        {
          rv = thisListener->NotifyDocumentCreated();
          if (NS_FAILED(rv))
            break;
        }
      }
      break;
      
    case eDocumentToBeDestroyed:
      for (i = 0; i < numListeners;i++)
      {
        nsCOMPtr<nsIDocumentStateListener> thisListener =
          do_QueryElementAt(mDocStateListeners, i);
        if (thisListener)
        {
          rv = thisListener->NotifyDocumentWillBeDestroyed();
          if (NS_FAILED(rv))
            break;
        }
      }
      break;
  
    case eDocumentStateChanged:
      {
        PRBool docIsDirty;
        rv = GetDocumentModified(&docIsDirty);
        if (NS_FAILED(rv)) return rv;
        
        if (docIsDirty == mDocDirtyState)
          return NS_OK;
        
        mDocDirtyState = (PRInt8)docIsDirty;
        
        for (i = 0; i < numListeners;i++)
        {
          nsCOMPtr<nsIDocumentStateListener> thisListener =
            do_QueryElementAt(mDocStateListeners, i);
          if (thisListener)
          {
            rv = thisListener->NotifyDocumentStateChanged(mDocDirtyState);
            if (NS_FAILED(rv))
              break;
          }
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
  if (!aTextNode || !aTxn) return NS_ERROR_NULL_POINTER;
  nsresult result;

  result = TransactionFactory::GetNewTransaction(InsertTextTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_FAILED(result)) return result;
  if (!*aTxn) return NS_ERROR_OUT_OF_MEMORY;
  result = (*aTxn)->Init(aTextNode, aOffset, aStringToInsert, this);
  return result;
}


NS_IMETHODIMP nsEditor::DeleteText(nsIDOMCharacterData *aElement,
                              PRUint32             aOffset,
                              PRUint32             aLength)
{
  DeleteTextTxn *txn;
  nsresult result = CreateTxnForDeleteText(aElement, aOffset, aLength, &txn);
  nsAutoRules beginRulesSniffing(this, kOpDeleteText, nsIEditor::ePrevious);
  if (NS_SUCCEEDED(result))  
  {
    // let listeners know whats up
    PRInt32 i;
    nsIEditActionListener *listener;
    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->WillDeleteText(aElement, aOffset, aLength);
      }
    }
    
    result = DoTransaction(txn); 
    
    // let listeners know what happened
    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->DidDeleteText(aElement, aOffset, aLength, result);
      }
    }
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  return result;
}


NS_IMETHODIMP nsEditor::CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                               PRUint32             aOffset,
                                               PRUint32             aLength,
                                               DeleteTextTxn      **aTxn)
{
  if (!aElement)
    return NS_ERROR_NULL_POINTER;

  nsresult result = TransactionFactory::GetNewTransaction(DeleteTextTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_SUCCEEDED(result))  {
    result = (*aTxn)->Init(this, aElement, aOffset, aLength, &mRangeUpdater);
  }
  return result;
}




NS_IMETHODIMP nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  nsresult result = TransactionFactory::GetNewTransaction(SplitElementTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_FAILED(result))
    return result;

  return (*aTxn)->Init(this, aNode, aOffset);
}

NS_IMETHODIMP nsEditor::CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                             nsIDOMNode  *aRightNode,
                                             JoinElementTxn **aTxn)
{
  if (!aLeftNode || !aRightNode)
    return NS_ERROR_NULL_POINTER;

  nsresult result = TransactionFactory::GetNewTransaction(JoinElementTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_SUCCEEDED(result))  {
    result = (*aTxn)->Init(this, aLeftNode, aRightNode);
  }
  return result;
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
    if (NS_FAILED(result)) return result;
    if (!selection) return NS_ERROR_NULL_POINTER;

    // remember some selection points
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode;
    PRInt32 selStartOffset, selEndOffset;
    result = GetStartNodeAndOffset(selection, address_of(selStartNode), &selStartOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;  // if selection is cleared, remember that
    result = GetEndNodeAndOffset(selection, address_of(selEndNode), &selEndOffset);
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
    if (!selection) return NS_ERROR_NULL_POINTER;

    // remember some selection points
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode;
    PRInt32 selStartOffset, selEndOffset, joinOffset, keepOffset;
    result = GetStartNodeAndOffset(selection, address_of(selStartNode), &selStartOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;
    result = GetEndNodeAndOffset(selection, address_of(selEndNode), &selEndOffset);
    // Joe or Kin should comment here on why the following line is not a copy/paste error
    if (NS_FAILED(result)) selStartNode = nsnull;

    nsCOMPtr<nsIDOMNode> leftNode;
    if (aNodeToKeepIsFirst)
      leftNode = aNodeToKeep;
    else
      leftNode = aNodeToJoin;

    PRUint32 firstNodeLength;
    result = GetLengthOfDOMNode(leftNode, firstNodeLength);
    if (NS_FAILED(result)) return result;
    nsCOMPtr<nsIDOMNode> parent;
    result = GetNodeLocation(aNodeToJoin, address_of(parent), &joinOffset);
    if (NS_FAILED(result)) return result;
    result = GetNodeLocation(aNodeToKeep, address_of(parent), &keepOffset);
    if (NS_FAILED(result)) return result;
    
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
  if (!cChild || !content)
    return NS_ERROR_NULL_POINTER;

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
  if (NS_FAILED(result)) return result;
  
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
  if (NS_FAILED(result)) return result;
  
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
  if (!aCurrentNode) return nsnull;
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
  if (!aCurrentNode) return nsnull;
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
  // screwing around with the class heirarchy here in order
  // to not duplicate the code in GetNextNode/GetPrevNode
  // across both nsEditor/nsHTMLEditor.  
  return PR_FALSE;
}

PRBool 
nsEditor::CanContainTag(nsIDOMNode* aParent, const nsAString &aChildTag)
{
  nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(aParent);
  if (!parentElement) return PR_FALSE;
  
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
    childStringTag.AssignLiteral("__moz_text");
  }
  else
  {
    nsCOMPtr<nsIDOMElement> childElement = do_QueryInterface(aChild);
    if (!childElement) return PR_FALSE;
    childElement->GetTagName(childStringTag);
  }
  return TagCanContainTag(aParentTag, childStringTag);
}

PRBool 
nsEditor::TagCanContainTag(const nsAString &aParentTag, const nsAString &aChildTag)
{
  // if we don't have a dtd then assume we can insert whatever want
  if (!mDTD) return PR_TRUE;
  
  PRInt32 childTagEnum, parentTagEnum;
  nsAutoString non_const_childTag(aChildTag);
  nsresult res = mDTD->StringTagToIntTag(non_const_childTag,&childTagEnum);
  if (NS_FAILED(res)) return PR_FALSE;
  nsAutoString non_const_parentTag(aParentTag);
  res = mDTD->StringTagToIntTag(non_const_parentTag,&parentTagEnum);
  if (NS_FAILED(res)) return PR_FALSE;

  return mDTD->CanContain(parentTagEnum, childTagEnum);
}

PRBool 
nsEditor::IsRootNode(nsIDOMNode *inNode) 
{
  if (!inNode)
    return PR_FALSE;

  nsCOMPtr<nsIDOMElement> rootElement;

  nsresult result = GetRootElement(getter_AddRefs(rootElement));

  if (NS_FAILED(result) || !rootElement)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElement);
  
  return inNode == rootNode.get();
}

PRBool 
nsEditor::IsDescendantOfBody(nsIDOMNode *inNode) 
{
  if (!inNode) return PR_FALSE;
  nsCOMPtr<nsIDOMElement> junk;
  if (!mBodyElement) GetRootElement(getter_AddRefs(junk));
  if (!mBodyElement) return PR_FALSE;
  nsCOMPtr<nsIDOMNode> root = do_QueryInterface(mBodyElement);
  
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
  if (!aNode) return PR_FALSE;
  nsAutoString stringTag;
  PRInt32 tagEnum;
  nsresult res = aNode->GetNodeName(stringTag);
  if (NS_FAILED(res)) return PR_FALSE;
  res = mDTD->StringTagToIntTag(stringTag,&tagEnum);
  if (NS_FAILED(res)) return PR_FALSE;
  return mDTD->IsContainer(tagEnum);
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
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIPresShell> shell;
  GetPresShell(getter_AddRefs(shell));
  if (!shell)  return PR_FALSE;

  if (IsMozEditorBogusNode(aNode)) return PR_FALSE;
  
  // see if it has a frame.  If so, we'll edit it.
  // special case for textnodes: frame must have width.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content)
  {
    nsIFrame *resultFrame;
    nsresult result = shell->GetPrimaryFrameFor(content, &resultFrame);
    if (NS_FAILED(result) || !resultFrame)   // if it has no frame, it is not editable
      return PR_FALSE;
    nsCOMPtr<nsITextContent> text(do_QueryInterface(content));
    if (!text)
      return PR_TRUE;  // not a text node; has a frame
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
  }
  return PR_FALSE;  // didn't pass any editability test
}

PRBool
nsEditor::IsMozEditorBogusNode(nsIDOMNode *aNode)
{
  if (!aNode)
    return PR_FALSE;

  nsCOMPtr<nsIDOMElement>element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString val;
    (void)element->GetAttribute(kMOZEditorBogusNodeAttr, val);
    if (val.Equals(kMOZEditorBogusNodeValue)) {
      return PR_TRUE;
    }
  }
    
  return PR_FALSE;
}

PRBool
nsEditor::IsEmptyTextContent(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
  nsCOMPtr<nsITextContent> tc(do_QueryInterface(aContent));
  if (tc) {
    result = tc->IsOnlyWhitespace();
  }
  return result;
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
  nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
  nsCOMPtr<nsIContent> cChild = do_QueryInterface(child);
  NS_PRECONDITION(content, "null content in nsEditor::GetIndexOf");
  NS_PRECONDITION(cChild, "null content in nsEditor::GetIndexOf");

  return content->IndexOf(cChild);
}
  

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
nsCOMPtr<nsIDOMNode> 
nsEditor::GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  
  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);

  if (!parent) 
    return resultNode;

  resultNode = do_QueryInterface(parent->GetChildAt(aOffset));

  return resultNode;
}
  


///////////////////////////////////////////////////////////////////////////
// GetStartNodeAndOffset: returns whatever the start parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetStartNodeAndOffset(nsISelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outStartNode,
                                       PRInt32 *outStartOffset)
{
  if (!outStartNode || !outStartOffset || !aSelection) 
    return NS_ERROR_NULL_POINTER;

  // brade:  set outStartNode to null or ?

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
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartContainer(getter_AddRefs(*outStartNode))))
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartOffset(outStartOffset)))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetEndNodeAndOffset: returns whatever the end parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetEndNodeAndOffset(nsISelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outEndNode,
                                       PRInt32 *outEndOffset)
{
  if (!outEndNode || !outEndOffset) 
    return NS_ERROR_NULL_POINTER;
    
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
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetEndContainer(getter_AddRefs(*outEndNode))))
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
  
  if (!aResult || !content) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  
  nsIFrame *frame;
  nsresult result = ps->GetPrimaryFrameFor(content, &frame);
  if (NS_FAILED(result)) return result;

  NS_ASSERTION(frame, "no frame, see bug #188946");
  if (!frame)
  {
    // Consider nodes without a style context to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no style context).
    *aResult = PR_FALSE;
    return NS_OK;
  }

  const nsStyleText* styleText = frame->GetStyleText();

  *aResult = NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace ||
             NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// SplitNodeDeep: this splits a node "deeply", splitting children as 
//                appropriate.  The place to split is represented by
//                a dom point at {splitPointParent, splitPointOffset}.
//                That dom point must be inside aNode, which is the node to 
//                split.  outOffset is set to the offset in the parent of aNode where
//                the split terminates - where you would want to insert 
//                a new element, for instance, if thats why you were splitting 
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
  if (!aNode || !aSplitPointParent || !outOffset) return NS_ERROR_NULL_POINTER;
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
    if (NS_FAILED(res)) return res;
    
    if (!(aNoEmptyContainers || nodeAsText) || (offset && (offset != (PRInt32)len)))
    {
      bDoSplit = PR_TRUE;
      res = SplitNode(nodeToSplit, offset, getter_AddRefs(tempNode));
      if (NS_FAILED(res)) return res;
      if (outRightNode) *outRightNode = nodeToSplit;
      if (outLeftNode)  *outLeftNode  = tempNode;
    }

    res = nodeToSplit->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    if (!parentNode) return NS_ERROR_FAILURE;

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
  if (!aLeftNode || !aRightNode || !aOutJoinNode || !outOffset) return NS_ERROR_NULL_POINTER;

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
      if (NS_FAILED(res)) return res;
    }
    
    *aOutJoinNode = rightNodeToJoin;
    *outOffset = length;
    
    // do the join
    res = JoinNodes(leftNodeToJoin, rightNodeToJoin, parentNode);
    if (NS_FAILED(res)) return res;
    
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

    if (mViewManager)
      mViewManager->BeginUpdateViewBatch();

    // Turn off reflow.

    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));

    if (presShell)
      presShell->BeginReflowBatching();
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

    nsCOMPtr<nsICaret> caret;
    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));

    if (presShell)
      presShell->GetCaret(getter_AddRefs(caret));

    StCaretHider caretHider(caret);
        
    PRUint32 flags = 0;

    GetFlags(&flags);

    // Turn reflow back on.
    //
    // Make sure we enable reflowing before we call
    // mViewManager->EndUpdateViewBatch().  This will make sure that any
    // new updates caused by a reflow, that may happen during the
    // EndReflowBatching(), get included if we force a refresh during
    // the mViewManager->EndUpdateViewBatch() call.

    if (presShell)
    {
      PRBool forceReflow = PR_TRUE;

      if (flags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask)
        forceReflow = PR_FALSE;

      presShell->EndReflowBatching(forceReflow);
    }

    // Turn view updating back on.

    if (mViewManager)
    {
      PRUint32 updateFlag = NS_VMREFRESH_IMMEDIATE;

      if (flags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask)
        updateFlag = NS_VMREFRESH_NO_SYNC;

      mViewManager->EndUpdateViewBatch(updateFlag);
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
  if (NS_FAILED(res)) return res;
  EditAggregateTxn *txn;
  res = CreateTxnForDeleteSelection(aAction, &txn);
  if (NS_FAILED(res)) return res;
  nsAutoRules beginRulesSniffing(this, kOpDeleteSelection, aAction);

  PRInt32 i;
  nsIEditActionListener *listener;
  if (NS_SUCCEEDED(res))  
  {
    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->WillDeleteSelection(selection);
      }
    }

    res = DoTransaction(txn);  

    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->DidDeleteSelection(selection);
      }
    }
  }

  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

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
  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIDOMNode> newNode;
  result = CreateNode(aTag, parentSelectedNode, offsetOfNewNode,
                      getter_AddRefs(newNode));
  // XXX: ERROR_HANDLING  check result, and make sure aNewNode is set correctly in success/failure cases
  *aNewNode = newNode;
  NS_IF_ADDREF(*aNewNode);

  // we want the selection to be just after the new node
  nsCOMPtr<nsISelection> selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;
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
  nsresult result = mIMETextRangeList->GetLength(&listlen);
  if (NS_FAILED(result)) return;

  for (PRUint16 i = 0; i < listlen; i++)
  {
      result = mIMETextRangeList->Item(i, getter_AddRefs(rangePtr));
      if (NS_FAILED(result)) continue;
      result = rangePtr->GetRangeType(&type);
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
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

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
#ifdef NS_DEBUG
    nsCOMPtr<nsIDOMNode>testSelectedNode;
    nsresult debugResult = selection->GetAnchorNode(getter_AddRefs(testSelectedNode));
    // no selection is ok.
    // if there is a selection, it must be collapsed
    if (testSelectedNode)
    {
      PRBool testCollapsed;
      debugResult = selection->GetIsCollapsed(&testCollapsed);
      NS_ASSERTION((NS_SUCCEEDED(result)), "couldn't get a selection after deletion");
      NS_ASSERTION(testCollapsed, "selection not reset after deletion");
    }
#endif
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
    
    /* if the selection is a text node, split the text node if necesary
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
  if (NS_FAILED(rv))
    return rv;
  
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
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(ChangeAttributeTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aElement, aAttribute, aValue, PR_FALSE);
    }
  }
  return result;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                      const nsAString& aAttribute,
                                      ChangeAttributeTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(ChangeAttributeTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  
    {
      nsAutoString value;
      result = (*aTxn)->Init(this, aElement, aAttribute, value, PR_TRUE);
    }
  }
  return result;
}


NS_IMETHODIMP nsEditor::CreateTxnForCreateElement(const nsAString& aTag,
                                                  nsIDOMNode     *aParent,
                                                  PRInt32         aPosition,
                                                  CreateElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aParent)
  {
    result = TransactionFactory::GetNewTransaction(CreateElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aTag, aParent, aPosition);
    }
  }
  return result;
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertElement(nsIDOMNode * aNode,
                                                  nsIDOMNode * aParent,
                                                  PRInt32      aPosition,
                                                  InsertElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aNode && aParent && aTxn)
  {
    result = TransactionFactory::GetNewTransaction(InsertElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result)) {
      result = (*aTxn)->Init(aNode, aParent, aPosition, this);
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::CreateTxnForDeleteElement(nsIDOMNode * aElement,
                                             DeleteElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(DeleteElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result)) {
      result = (*aTxn)->Init(aElement, &mRangeUpdater);
    }
  }
  return result;
}

/*NS_IMETHODIMP nsEditor::CreateAggregateTxnForDeleteSelection(nsIAtom *aTxnName, EditAggregateTxn **aAggTxn) 
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aAggTxn)
  {
    *aAggTxn = nsnull;
    result = TransactionFactory::GetNewTransaction(EditAggregateTxn::GetCID(), (EditTxn**)aAggTxn); 

    if (NS_FAILED(result) || !*aAggTxn) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Set the name for the aggregate transaction  
    (*aAggTxn)->SetName(aTxnName);

    // Get current selection and setup txn to delete it,
    //  but only if selection exists (is not a collapsed "caret" state)
    if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsISelection> selection;
    result = ps->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection)
    {
      PRBool collapsed;
      result = selection->GetIsCollapsed(&collapsed);
      if (NS_SUCCEEDED(result) && !collapsed) {
        EditAggregateTxn *delSelTxn;
        result = CreateTxnForDeleteSelection(eNone, &delSelTxn);
        if (NS_SUCCEEDED(result) && delSelTxn) {
          (*aAggTxn)->AppendChild(delSelTxn);
          NS_RELEASE(delSelTxn);
        }
      }
    }
  }
  return result;
}
*/

NS_IMETHODIMP 
nsEditor::CreateTxnForIMEText(const nsAString& aStringToInsert,
                              IMETextTxn ** aTxn)
{
  NS_ASSERTION(aTxn, "illegal value- null ptr- aTxn");
  if(!aTxn) return NS_ERROR_NULL_POINTER;
     
  nsresult  result;

  result = TransactionFactory::GetNewTransaction(IMETextTxn::GetCID(), (EditTxn **)aTxn);
  if (nsnull!=*aTxn) {
    result = (*aTxn)->Init(mIMETextNode,mIMETextOffset,mIMEBufferLength,mIMETextRangeList,aStringToInsert,mSelConWeak);
  }
  else {
    result = NS_ERROR_OUT_OF_MEMORY;
  }
  return result;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForAddStyleSheet(nsICSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn)
{
  nsresult rv = TransactionFactory::GetNewTransaction(AddStyleSheetTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_FAILED(rv))
    return rv;
    
  if (! *aTxn)
    return NS_ERROR_OUT_OF_MEMORY;

  return (*aTxn)->Init(this, aSheet);
}



NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveStyleSheet(nsICSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn)
{
  nsresult rv = TransactionFactory::GetNewTransaction(RemoveStyleSheetTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_FAILED(rv))
    return rv;
    
  if (! *aTxn)
    return NS_ERROR_OUT_OF_MEMORY;

  return (*aTxn)->Init(this, aSheet);
}


NS_IMETHODIMP
nsEditor::CreateTxnForDeleteSelection(nsIEditor::EDirection aAction,
                                      EditAggregateTxn  ** aTxn)
{
  if (!aTxn)
    return NS_ERROR_NULL_POINTER;
  *aTxn = nsnull;

#ifdef DEBUG_akkana
  NS_ASSERTION(aAction != eNextWord && aAction != ePreviousWord && aAction != eToEndOfLine, "CreateTxnForDeleteSelection: unsupported action!");
#endif

  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  if (!selCon) return NS_ERROR_NOT_INITIALIZED;
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
    result = TransactionFactory::GetNewTransaction(EditAggregateTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_FAILED(result)) {
      return result;
    }

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
            DeleteRangeTxn *txn;
            result = TransactionFactory::GetNewTransaction(DeleteRangeTxn::GetCID(), (EditTxn **)&txn);
            if (NS_SUCCEEDED(result) && txn)
            {
              txn->Init(this, range, &mRangeUpdater);
              (*aTxn)->AppendChild(txn);
              NS_RELEASE(txn);
            }
            else
              result = NS_ERROR_OUT_OF_MEMORY;
          }
          else
          { // we have an insertion point.  delete the thing in front of it or behind it, depending on aAction
            result = CreateTxnForDeleteInsertionPoint(range, aAction, *aTxn);
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


//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior are not implemented
NS_IMETHODIMP
nsEditor::CreateTxnForDeleteInsertionPoint(nsIDOMRange          *aRange, 
                                           nsIEditor::EDirection aAction,
                                           EditAggregateTxn     *aTxn)
{
  // get the node and offset of the insertion point
  nsCOMPtr<nsIDOMNode> node;
  nsresult result = aRange->GetStartContainer(getter_AddRefs(node));
  if (NS_FAILED(result))
    return result;

  PRInt32 offset;
  result = aRange->GetStartOffset(&offset);
  if (NS_FAILED(result))
    return result;

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
          result = CreateTxnForDeleteText(priorNodeAsText, length-1, 1, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
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
          result = CreateTxnForDeleteText(nextNodeAsText, 0, 1, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
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
        }
      }
    }
  }
  else
  {
    if (nodeAsText)
    { // we have text, so delete a char at the proper offset
      if (nsIEditor::ePrevious==aAction) {
        offset --;
      }
      DeleteTextTxn *txn;
      result = CreateTxnForDeleteText(nodeAsText, offset, 1, &txn);
      if (NS_SUCCEEDED(result)) {
        aTxn->AppendChild(txn);
        NS_RELEASE(txn);
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
          PRInt32 begin = 0;    // default for forward delete
          if (ePrevious==aAction)
          {
            PRUint32 length=0;
            selectedNodeAsText->GetLength(&length);
            if (0<length)
              begin = length-1;
          }
          DeleteTextTxn *delTextTxn;
          result = CreateTxnForDeleteText(selectedNodeAsText, begin, 1, &delTextTxn);
          if (NS_FAILED(result))  { return result; }
          if (!delTextTxn) { return NS_ERROR_NULL_POINTER; }
          aTxn->AppendChild(delTextTxn);
          NS_RELEASE(delTextTxn);
        }
        else
        {
          DeleteElementTxn *delElementTxn;
          result = CreateTxnForDeleteElement(selectedNode, &delElementTxn);
          if (NS_FAILED(result))  { return result; }
          if (!delElementTxn) { return NS_ERROR_NULL_POINTER; }
          aTxn->AppendChild(delElementTxn);
          NS_RELEASE(delElementTxn);
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
  if (NS_FAILED(result))
    return result;

  if (!*aRange)
    return NS_ERROR_NULL_POINTER;

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
  if (!aNode) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if(!selection) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> parentNode;
  res = aNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_FAILED(res)) return res;
  if (!parentNode) return NS_ERROR_NULL_POINTER;
  
  PRInt32 offset;
  res = GetChildOffset(aNode, parentNode, offset);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMRange> range;
  res = CreateRange(parentNode, offset, parentNode, offset+1, getter_AddRefs(range));
  if (NS_FAILED(res)) return res;
  if (!range) return NS_ERROR_NULL_POINTER;

  return selection->AddRange(range);
}

nsresult nsEditor::ClearSelection()
{
  nsCOMPtr<nsISelection> selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;
  return selection->RemoveAllRanges();  
}

nsresult
nsEditor::CreateHTMLContent(const nsAString& aTag, nsIContent** aContent)
{
  nsCOMPtr<nsIDOMDocument> tempDoc;
  GetDocument(getter_AddRefs(tempDoc));

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(tempDoc);
  if (!doc)
    return NS_ERROR_FAILURE;

  // XXX Wallpaper over editor bug (editor tries to create elements with an
  //     empty nodename).
  if (aTag.IsEmpty()) {
    NS_ERROR("Don't pass an empty tag to nsEditor::CreateHTMLContent, "
             "check caller.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAtom> tag = do_GetAtom(aTag);
  if (!tag)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(tempDoc);
  if (htmlDoc) {
      return doc->CreateElem(tag, nsnull, doc->GetDefaultNamespaceID(),
                             PR_TRUE, aContent);
  }

  return doc->CreateElem(tag, nsnull, kNameSpaceID_XHTML, PR_FALSE, aContent);
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
      char ctag[40];
      element->GetTagName(tag);
      tag.ToCString(ctag,40);
      ctag[40]=0;
      printf("<%s>\n", ctag);
    }
    else
    {
      printf("<document fragment>\n");
    }
    nsCOMPtr<nsIDOMNodeList> childList;
    aNode->GetChildNodes(getter_AddRefs(childList));
    if (!childList) return NS_ERROR_NULL_POINTER;
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
    char theText[30], *c;
    str.ToCString(theText,30);
    theText[30]=0;
    while (c=strchr(theText,'\n')) (*c) = ' ';
    printf("<textnode> %s\n", theText);
  }
}
#endif
