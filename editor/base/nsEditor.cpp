/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsVoidArray.h"

#include "nsIDOMDocument.h"
#include "nsIPref.h"
#include "nsILocale.h"
#include "nsIEditProperty.h"  // to be removed  XXX

#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDocument.h"
#include "nsIDiskDocument.h"
#include "nsIServiceManager.h"
#include "nsTransactionManagerCID.h"
#include "nsITransactionManager.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIDOMSelection.h"
#include "nsIEnumerator.h"
#include "nsIAtom.h"
#include "nsISupportsArray.h"
#include "nsICaret.h"
#include "nsIStyleContext.h"
#include "nsIEditActionListener.h"

#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"
#include "nsIStringStream.h"

#ifdef NECKO
#include "nsNeckoUtil.h"
#else
#include "nsIURL.h"
#endif // NECKO

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsLayoutCID.h"

// transactions the editor knows how to build
#include "TransactionFactory.h"
#include "EditAggregateTxn.h"
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
#include "IMECommitTxn.h"

// #define HACK_FORCE_REDRAW 1

#include "nsEditorCID.h"
#include "nsEditor.h"


#ifdef HACK_FORCE_REDRAW
// INCLUDES FOR EVIL HACK TO FOR REDRAW
// BEGIN
#include "nsIViewManager.h"
#include "nsIView.h"
// END
#endif

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIFileWidget.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"

static NS_DEFINE_CID(kCRangeCID,            NS_RANGE_CID);
static NS_DEFINE_CID(kCContentIteratorCID,  NS_CONTENTITERATOR_CID);

// transaction manager
static NS_DEFINE_CID(kCTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);

#ifdef XP_PC
#define TRANSACTION_MANAGER_DLL "txmgr.dll"
#else
#ifdef XP_MAC
#define TRANSACTION_MANAGER_DLL "TRANSACTION_MANAGER_DLL"
#else // XP_UNIX || XP_BEOS
#define TRANSACTION_MANAGER_DLL "libtxmgr"MOZ_DLL_SUFFIX
#endif
#endif

#define NS_ERROR_EDITOR_NO_SELECTION NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,1)
#define NS_ERROR_EDITOR_NO_TEXTNODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,2)

const char* nsEditor::kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
const char* nsEditor::kMOZEditorBogusNodeValue="TRUE";

#ifdef NS_DEBUG_EDITOR
static PRBool gNoisy = PR_TRUE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif



PRInt32 nsEditor::gInstanceCount = 0;


//class implementations are in order they are declared in nsEditor.h

nsEditor::nsEditor()
:  mPresShellWeak(nsnull)
,  mViewManager(nsnull)
,  mUpdateCount(0)
,  mActionListeners(nsnull)
,  mDocDirtyState(-1)
,  mDocWeak(nsnull)
{
  //initialize member variables here
  NS_INIT_REFCNT();
  PR_EnterMonitor(GetEditorMonitor());
  gInstanceCount++;
  PR_ExitMonitor(GetEditorMonitor());
}

nsEditor::~nsEditor()
{
  // not sure if this needs to be called earlier.
  NotifyDocumentListeners(eDocumentToBeDestroyed);

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
}


// BEGIN nsEditor core implementation

NS_IMPL_ADDREF(nsEditor)
NS_IMPL_RELEASE(nsEditor)


NS_IMETHODIMP
nsEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr)
    return NS_ERROR_NULL_POINTER;
  
  *aInstancePtr = nsnull;
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    nsIEditor *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIEditor::GetIID())) {
    *aInstancePtr = (void*)(nsIEditor*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIEditorIMESupport::GetIID())) {
    *aInstancePtr = (void*)(nsIEditorIMESupport*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  return NS_NOINTERFACE;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark --- nsIEditorMethods ---
#pragma mark -
#endif

NS_IMETHODIMP
nsEditor::Init(nsIDOMDocument *aDoc, nsIPresShell* aPresShell, PRUint32 aFlags)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  if ((nsnull==aDoc) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

  mFlags = aFlags;
  mDocWeak = getter_AddRefs( NS_GetWeakReference(aDoc) );  // weak reference to doc
  mPresShellWeak = getter_AddRefs( NS_GetWeakReference(aPresShell) );   // weak reference to pres shell
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  
  // disable links
  nsCOMPtr<nsIPresContext> context;
  ps->GetPresContext(getter_AddRefs(context));
  if (!context) return NS_ERROR_NULL_POINTER;
  context->SetLinkHandler(0);  

  // Set up the DTD
  // XXX - in the long run we want to get this from the document, but there
  // is no way to do that right now.  So we leave it null here and set
  // up a nav html dtd in nsHTMLEditor::Init

  // Init mEditProperty
  nsresult result = NS_NewEditProperty(getter_AddRefs(mEditProperty));
  if (NS_FAILED(result)) { return result; }
  if (!mEditProperty) {return NS_ERROR_NULL_POINTER;}

  ps->GetViewManager(&mViewManager);
  if (!mViewManager) {return NS_ERROR_NULL_POINTER;}
  mViewManager->Release(); //we want a weak link

  ps->SetDisplayNonTextSelection(PR_TRUE);//we want to see all the selection reflected to user
  mUpdateCount=0;
  InsertTextTxn::ClassInit();

  /* initalize IME stuff */
  IMETextTxn::ClassInit();
  IMECommitTxn::ClassInit();
  mIMETextNode = do_QueryInterface(nsnull);
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  
  /* Show the caret */
  // XXX: I suppose it's legal to fail to get a caret, so don't propogate the
  //      result of GetCaret
  nsCOMPtr<nsICaret>  caret;
  if (NS_SUCCEEDED(ps->GetCaret(getter_AddRefs(caret))) && caret)
  {
    caret->SetCaretVisible(PR_TRUE);
    caret->SetCaretReadOnly(PR_FALSE);
  }

  // Set the selection to the beginning:
  BeginningOfDocument();

  NS_POSTCONDITION(mDocWeak && mPresShellWeak, "bad state");

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::PostCreate()
{
  // update the UI with our state
  NotifyDocumentListeners(eDocumentCreated);
  NotifyDocumentListeners(eDocumentStateChanged);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;
  *aDoc = nsnull; // init out param
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  return doc->QueryInterface(nsIDOMDocument::GetIID(), (void **)aDoc);
}


nsresult 
nsEditor::GetPresShell(nsIPresShell **aPS)
{
  if (!aPS)
    return NS_ERROR_NULL_POINTER;
  *aPS = nsnull; // init out param
  NS_PRECONDITION(mPresShellWeak, "bad state, null mPresShellWeak");
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  return ps->QueryInterface(nsIPresShell::GetIID(), (void **)aPS);
}


NS_IMETHODIMP
nsEditor::GetSelection(nsIDOMSelection **aSelection)
{
  if (!aSelection)
    return NS_ERROR_NULL_POINTER;
  *aSelection = nsnull;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = ps->GetSelection(SELECTION_NORMAL, aSelection);  // does an addref
  return result;
}

NS_IMETHODIMP 
nsEditor::Do(nsITransaction *aTxn)
{
  if (gNoisy) { printf("Editor::Do ----------\n"); }
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult selectionResult = GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(selectionResult) && selection) {
    selection->StartBatchChanges();
    if (aTxn)
    {
      if (mTxnMgr) {
        result = mTxnMgr->Do(aTxn);
      }
      else {
        result = aTxn->Do();
      }
      
      if (NS_SUCCEEDED(result))
        result = DoAfterDoTransaction(aTxn);
    }
    
    selection->EndBatchChanges();
  }

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
      result = nsComponentManager::CreateInstance(kCTransactionManagerCID,
                                        nsnull,
                                        nsITransactionManager::GetIID(), getter_AddRefs(mTxnMgr));
      if (NS_FAILED(result) || !mTxnMgr) {
        printf("ERROR: Failed to get TransactionManager instance.\n");
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

  return result;
}


NS_IMETHODIMP 
nsEditor::Undo(PRUint32 aCount)
{
  if (gNoisy) { printf("Editor::Undo ----------\n"); }
  nsresult result = NS_OK;

  BeginUpdateViewBatch();

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->Undo();

      if (NS_SUCCEEDED(result))
        result = DoAfterUndoTransaction();
          
      if (NS_FAILED(result))
        break;
    }
  }

  EndUpdateViewBatch();

  return result;
}


NS_IMETHODIMP nsEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  aIsEnabled = ((PRBool)((nsITransactionManager *)0!=mTxnMgr.get()));
  if (aIsEnabled)
  {
    PRInt32 numTxns=0;
    mTxnMgr->GetNumberOfUndoItems(&numTxns);
    aCanUndo = ((PRBool)(0==numTxns));
  }
  else {
    aCanUndo = PR_FALSE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::Redo(PRUint32 aCount)
{
  if (gNoisy) { printf("Editor::Redo ----------\n"); }
  nsresult result = NS_OK;

  BeginUpdateViewBatch();

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->Redo();

      if (NS_SUCCEEDED(result))
        result = DoAfterRedoTransaction();

      if (NS_FAILED(result))
        break;
    }
  }

  EndUpdateViewBatch();

  return result;
}


NS_IMETHODIMP nsEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  aIsEnabled = ((PRBool)((nsITransactionManager *)0!=mTxnMgr.get()));
  if (aIsEnabled)
  {
    PRInt32 numTxns=0;
    mTxnMgr->GetNumberOfRedoItems(&numTxns);
    aCanRedo = ((PRBool)(0==numTxns));
  }
  else {
    aCanRedo = PR_FALSE;
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

// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    result = SelectEntireDocument(selection);
  }
  return result;
}

NS_IMETHODIMP nsEditor::BeginningOfDocument()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    nsCOMPtr<nsIDOMNodeList> nodeList;
    nsAutoString bodyTag = "body";
    nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
    if (!doc) return NS_ERROR_NOT_INITIALIZED;
    result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
    if ((NS_SUCCEEDED(result)) && nodeList)
    {
      PRUint32 count;
      nodeList->GetLength(&count);
      NS_VERIFY(1==count, "there is not exactly 1 body in the document!");
      nsCOMPtr<nsIDOMNode> bodyNode;
      result = nodeList->Item(0, getter_AddRefs(bodyNode));
      if ((NS_SUCCEEDED(result)) && bodyNode)
      {
        // Get the first child of the body node:
        nsCOMPtr<nsIDOMNode> firstNode = GetDeepFirstChild(bodyNode);
        if (firstNode)
        {
          result = selection->Collapse(firstNode, 0);
          ScrollIntoView(PR_TRUE);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::EndOfDocument()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    nsCOMPtr<nsIDOMNodeList> nodeList;
    nsAutoString bodyTag = "body";
    nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
    if (!doc) return NS_ERROR_NOT_INITIALIZED;
    result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
    if ((NS_SUCCEEDED(result)) && nodeList)
    {
      PRUint32 count;
      nodeList->GetLength(&count);
      NS_VERIFY(1==count, "there is not exactly 1 body in the document!");
      nsCOMPtr<nsIDOMNode> bodyNode;
      result = nodeList->Item(0, getter_AddRefs(bodyNode));
      if ((NS_SUCCEEDED(result)) && bodyNode)
      {
        nsCOMPtr<nsIDOMNode> lastChild = GetDeepLastChild(bodyNode);
        if ((NS_SUCCEEDED(result)) && lastChild)
        {
          // See if the last child is a text node; if so, set offset:
          PRUint32 offset = 0;
          if (IsTextNode(lastChild))
          {
            nsCOMPtr<nsIDOMText> text (do_QueryInterface(lastChild));
            if (text)
              text->GetLength(&offset);
          }
          result = selection->Collapse(lastChild, offset);
          ScrollIntoView(PR_FALSE);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsEditor::GetDocumentModified(PRBool *outDocModified)
{
  if (!outDocModified)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMDocument>  theDoc;
  nsresult  rv = GetDocument(getter_AddRefs(theDoc));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(theDoc, &rv);
  if (NS_FAILED(rv)) return rv;

  PRInt32  modCount = 0;
  diskDoc->GetModCount(&modCount);

  *outDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentCharacterSet(PRUnichar** characterSet)
{
  nsresult rv;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPresShell> presShell;
  nsString  character_set;

  if (characterSet==nsnull) return NS_ERROR_NULL_POINTER;

  rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv))
  {
    presShell->GetDocument(getter_AddRefs(doc));
    if (doc ) {
    rv = doc->GetDocumentCharacterSet(character_set);
    if (NS_SUCCEEDED(rv)) *characterSet=character_set.ToNewUnicode();
    return rv;
  }
  }

  return rv;

}

NS_IMETHODIMP
nsEditor::SetDocumentCharacterSet(const PRUnichar* characterSet)
{
  nsresult rv;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPresShell> presShell;
  nsString character_set = characterSet;

  if (characterSet==nsnull) return NS_ERROR_NULL_POINTER;

  rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv))
  {
    presShell->GetDocument(getter_AddRefs(doc));
    if (doc ) {
    return doc->SetDocumentCharacterSet(character_set);
  }
  }

  return rv;
}

NS_IMETHODIMP 
nsEditor::SaveFile(nsFileSpec *aFileSpec, PRBool aReplaceExisting, PRBool aSaveCopy, nsIDiskDocument::ESaveFileType aSaveFileType)
{
  if (!aFileSpec)
    return NS_ERROR_NULL_POINTER;

  // get the document
  nsCOMPtr<nsIDOMDocument> doc;
  nsresult res = GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(doc);
  if (!diskDoc)
    return NS_ERROR_NO_INTERFACE;

  nsAutoString useDocCharset("");

  res = diskDoc->SaveFile(aFileSpec, aReplaceExisting, aSaveCopy, 
                          aSaveFileType, useDocCharset);
  if (NS_SUCCEEDED(res))
    DoAfterDocumentSave();

  return res;
}

/*

NS_IMETHODIMP
nsEditor::SetProperties(nsVoidArray * aPropList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsEditor::GetProperties(nsVoidArray *aPropList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
*/

NS_IMETHODIMP 
nsEditor::SetAttribute(nsIDOMElement *aElement, const nsString& aAttribute, const nsString& aValue)
{
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForSetAttribute(aElement, aAttribute, aValue, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}


NS_IMETHODIMP 
nsEditor::GetAttributeValue(nsIDOMElement *aElement, 
                            const nsString& aAttribute, 
                            nsString& aResultValue, 
                            PRBool&   aResultIsSet)
{
  aResultIsSet=PR_FALSE;
  nsresult result=NS_OK;
  if (nsnull!=aElement)
  {
    nsCOMPtr<nsIDOMAttr> attNode;
    result = aElement->GetAttributeNode(aAttribute, getter_AddRefs(attNode));
    if ((NS_SUCCEEDED(result)) && attNode)
    {
      attNode->GetSpecified(&aResultIsSet);
      attNode->GetValue(aResultValue);
    }
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::RemoveAttribute(nsIDOMElement *aElement, const nsString& aAttribute)
{
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForRemoveAttribute(aElement, aAttribute, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}


NS_IMETHODIMP nsEditor::CreateNode(const nsString& aTag,
                                   nsIDOMNode *    aParent,
                                   PRInt32         aPosition,
                                   nsIDOMNode **   aNewNode)
{
  CreateElementTxn *txn;
  nsresult result = CreateTxnForCreateElement(aTag, aParent, aPosition, &txn);
  if (NS_SUCCEEDED(result)) 
  {
    result = Do(txn);  
    if (NS_SUCCEEDED(result)) 
    {
      result = txn->GetNewNode(aNewNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "GetNewNode can't fail if txn::Do succeeded.");
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
    result = Do(txn);  
  }

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
    result = Do(txn);
    if (NS_SUCCEEDED(result))
    {
      result = txn->GetNewNode(aNewLeftNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "result must succeeded for GetNewNode");
    }
  }

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
  PRInt32 i;
  nsIEditActionListener *listener;

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
  nsresult result = CreateTxnForJoinNode(aLeftNode, aRightNode, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }

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
  PRInt32 i;
  nsIEditActionListener *listener;

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
  nsresult result = CreateTxnForDeleteElement(aElement, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }

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


NS_IMETHODIMP nsEditor::OutputToString(nsString& aOutputString,
                                       const nsString& aFormatType,
                                       PRUint32 aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsEditor::OutputToStream(nsIOutputStream* aOutputStream,
                                       const nsString& aFormatType,
                                       const nsString* aCharsetOverride,
                                       PRUint32 aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

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

  if (!mActionListeners->AppendElement((void *)aListener))
    return NS_ERROR_FAILURE;

  NS_ADDREF(aListener);

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


NS_IMETHODIMP
nsEditor::DumpContentTree()
{
  nsCOMPtr<nsIDocument> thedoc;
  nsCOMPtr<nsIPresShell> presShell;
  if (NS_SUCCEEDED(GetPresShell(getter_AddRefs(presShell))))
  {
    presShell->GetDocument(getter_AddRefs(thedoc));
    if (thedoc) {
      nsIContent* root = thedoc->GetRootContent();
      if (nsnull != root) {
        root->List(stdout);
        NS_RELEASE(root);
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugDumpContent() const
{
  nsCOMPtr<nsIContent>content;
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsAutoString bodyTag = "body";
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
  if (nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    NS_ASSERTION(1==count, "there is not exactly 1 body in the document!");
    nsCOMPtr<nsIDOMNode>bodyNode;
    nodeList->Item(0, getter_AddRefs(bodyNode));
    if (bodyNode) {
      content = do_QueryInterface(bodyNode);
    }
  }
  content->List();
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
  NS_NOTREACHED("This should never get called. Overridden by subclasses");
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark --- nsIEditorIMESupport ---
#pragma mark -
#endif

//
// The BeingComposition method is called from the Editor Composition event listeners.
// It caches the current text node and offset which is subsequently used for the 
// created of IMETextTxn's.
//
NS_IMETHODIMP
nsEditor::BeginComposition(nsTextEventReply* aReply)
{
#ifdef DEBUG_tague
  printf("nsEditor::StartComposition\n");
#endif
  nsresult result;
  PRInt32 offset;
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMCharacterData> nodeAsText;
  nsCOMPtr<nsICaret> caretP; 
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  result = ps->GetCaret(getter_AddRefs(caretP));
  if (NS_SUCCEEDED(result) && caretP) {
    if (aReply) {
      caretP->GetWindowRelativeCoordinates(aReply->mCursorPosition,aReply->mCursorIsCollapsed);
    }
  }
  result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    result = NS_ERROR_UNEXPECTED; 
    nsCOMPtr<nsIEnumerator> enumerator;
    result = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(result) && enumerator)
    {
      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      result = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(result)) && (currentItem))
      {
        result = NS_ERROR_UNEXPECTED; 
        nsCOMPtr<nsIDOMRange> range(do_QueryInterface(currentItem));
        if (range)
        {
          nsCOMPtr<nsIDOMNode> node;
          result = range->GetStartParent(getter_AddRefs(node));
          if ((NS_SUCCEEDED(result)) && (node))
          {
            nodeAsText = do_QueryInterface(node);
            range->GetStartOffset(&offset);
            if (!nodeAsText) {
              result = NS_ERROR_EDITOR_NO_TEXTNODE;
            } 
          }    
        }    
      }
      else
      {
        result = NS_ERROR_EDITOR_NO_SELECTION;
      }
    }
  }

  if (NS_SUCCEEDED(result) && nodeAsText)
  {
    //
    // store the information needed to construct IME transactions for this composition
    //
    mIMETextNode = nodeAsText;
    mIMETextOffset = offset;
    mIMEBufferLength = 0;
  }

  return result;
}

NS_IMETHODIMP
nsEditor::EndComposition(void)
{
  nsresult result;
  IMECommitTxn *commitTxn;
  
  //
  // create the commit transaction..we can do it directly from the transaction mgr
  //
  result = TransactionFactory::GetNewTransaction(IMECommitTxn::GetCID(), (EditTxn**)&commitTxn);
  if (NS_SUCCEEDED(result) && commitTxn!=nsnull)
  {
    commitTxn->Init();
    result = Do(commitTxn);
  }

  /* reset the data we need to construct a transaction */
  mIMETextNode = do_QueryInterface(nsnull);
  mIMETextOffset = 0;
  mIMEBufferLength = 0;

  return result;
}

NS_IMETHODIMP
nsEditor::SetCompositionString(const nsString& aCompositionString, nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply)
{
  nsCOMPtr<nsICaret>  caretP;
  nsresult  result = SetInputMethodText(aCompositionString,aTextRangeList);
  mIMEBufferLength = aCompositionString.Length();

  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  ps->GetCaret(getter_AddRefs(caretP));
  caretP->GetWindowRelativeCoordinates(aReply->mCursorPosition,aReply->mCursorIsCollapsed);

  return result;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark --- public nsEditor methods ---
#pragma mark -
#endif
/* Non-interface, public methods */


// This seems like too much work! There should be a "nsDOMDocument::GetBody()"
// Have a look in nsHTMLDocument. Maybe add it to nsIHTMLDocument.
NS_IMETHODIMP 
nsEditor::GetBodyElement(nsIDOMElement **aBodyElement)
{
  nsresult result;

  if (!aBodyElement)
    return NS_ERROR_NULL_POINTER;

  *aBodyElement = 0;
  
  NS_PRECONDITION(mDocWeak, "bad state, null mDocWeak");
  if (!mDocWeak)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMNodeList>nodeList; 
  nsString bodyTag = "body"; 

  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));

  if (NS_FAILED(result))
    return result;
  
  if (!nodeList)
    return NS_ERROR_NULL_POINTER;

  PRUint32 count; 
  nodeList->GetLength(&count);

  NS_ASSERTION(1==count, "More than one body found in document!"); 

  if (count < 1)
    return NS_ERROR_FAILURE;

  // Use the first body node in the list:
  nsCOMPtr<nsIDOMNode> node;
  result = nodeList->Item(0, getter_AddRefs(node)); 
  if (NS_SUCCEEDED(result) && node)
  {
    //return node->QueryInterface(nsIDOMElement::GetIID(), (void **)aBodyElement);
    // Is above equivalent to this:
    nsCOMPtr<nsIDOMElement> bodyElement = do_QueryInterface(node);
    if (bodyElement)
    {
      *aBodyElement = bodyElement;
      // A "getter" method should always addref
      NS_ADDREF(*aBodyElement);
    }
  }
  return result;
}


// Objects must be DOM elements
NS_IMETHODIMP
nsEditor::CloneAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  nsresult result=NS_OK;

  if (!aDestNode || !aSourceNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  if (!destElement || !sourceElement)
    return NS_ERROR_NO_INTERFACE;

  nsAutoString name;
  nsAutoString value;
  nsCOMPtr<nsIDOMNamedNodeMap> sourceAttributes;
  sourceElement->GetAttributes(getter_AddRefs(sourceAttributes));
  nsCOMPtr<nsIDOMNamedNodeMap> destAttributes;
  destElement->GetAttributes(getter_AddRefs(destAttributes));
  if (!sourceAttributes || !destAttributes)
    return NS_ERROR_FAILURE;

  PRUint32 sourceCount;
  sourceAttributes->GetLength(&sourceCount);
  PRUint32 i, destCount;
  destAttributes->GetLength(&destCount);
  nsIDOMNode* attrNode;

  // Clear existing attributes
  for (i = 0; i < destCount; i++)
  {
    if( NS_SUCCEEDED(destAttributes->Item(i, &attrNode)) && attrNode)
    {
      nsCOMPtr<nsIDOMAttr> destAttribute = do_QueryInterface(attrNode);
      if (destAttribute)
      {
        nsCOMPtr<nsIDOMAttr> resultAttribute;
        destElement->RemoveAttributeNode(destAttribute, getter_AddRefs(resultAttribute));
        // Is the resultAttribute deleted automagically?
      }
    }
  }
  // Set just the attributes that the source element has
  for (i = 0; i < sourceCount; i++)
  {
    if( NS_SUCCEEDED(sourceAttributes->Item(i, &attrNode)) && attrNode)
    {
      nsCOMPtr<nsIDOMAttr> sourceAttribute = do_QueryInterface(attrNode);
      if (sourceAttribute)
      {
        nsString sourceAttrName;
        if (NS_SUCCEEDED(sourceAttribute->GetName(sourceAttrName)))
        {
          nsString sourceAttrValue;
          if (NS_SUCCEEDED(sourceAttribute->GetValue(sourceAttrValue)) &&
              sourceAttrValue != "")
          {
            destElement->SetAttribute(sourceAttrName, sourceAttrValue);
          } else {
            // Do we ever get here?
            destElement->RemoveAttribute(sourceAttrName);
#if DEBUG_cmanske
            printf("Attribute in NamedNodeMap has empty value in nsEditor::CloneAttributes()\n");
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
#pragma mark --- Protected and static methods ---
#pragma mark -
#endif

NS_IMETHODIMP nsEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/** static helper method */
nsresult nsEditor::GetTextNodeTag(nsString& aOutString)
{
  aOutString = "";
  static nsString *gTextNodeTag=nsnull;
  if (!gTextNodeTag)
  {
    gTextNodeTag = new nsString("special text node tag");
    if (!gTextNodeTag) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  aOutString = *gTextNodeTag;
  return NS_OK;
}


NS_IMETHODIMP nsEditor::InsertTextImpl(const nsString& aStringToInsert)
{
  EditAggregateTxn *aggTxn = nsnull;
  // Create the "delete current selection" txn
  nsresult result = CreateAggregateTxnForDeleteSelection(InsertTextTxn::gInsertTextTxnName, &aggTxn);
  if ((NS_FAILED(result)) || (nsnull==aggTxn)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InsertTextTxn *txn;
  result = CreateTxnForInsertText(aStringToInsert, nsnull, &txn); // insert at the current selection
  if ((NS_SUCCEEDED(result)) && txn)  {
    BeginUpdateViewBatch();
    aggTxn->AppendChild(txn);
    result = Do(aggTxn);
    EndUpdateViewBatch();
  }
  else if (NS_ERROR_EDITOR_NO_SELECTION==result)  {
    result = DoInitialInsert(aStringToInsert);
  }
  else if (NS_ERROR_EDITOR_NO_TEXTNODE==result) 
  {
    BeginTransaction();
    result = Do(aggTxn);
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsIDOMSelection> selection;
      result = GetSelection(getter_AddRefs(selection));
      if ((NS_SUCCEEDED(result)) && selection)
      {
        nsCOMPtr<nsIDOMNode> selectedNode;
        PRInt32 offset;
        result = selection->GetAnchorNode(getter_AddRefs(selectedNode));
        if (NS_SUCCEEDED(result) && NS_SUCCEEDED(selection->GetAnchorOffset(&offset)) && selectedNode)
        {
          nsCOMPtr<nsIDOMNode> newNode;
          nsAutoString textNodeTag;
          result = GetTextNodeTag(textNodeTag);
          if (NS_FAILED(result)) { return result; }
          result = CreateNode(textNodeTag, selectedNode, offset, 
                              getter_AddRefs(newNode));
          if (NS_SUCCEEDED(result) && newNode)
          {
            nsCOMPtr<nsIDOMCharacterData>newTextNode;
            newTextNode = do_QueryInterface(newNode);
            if (newTextNode)
            {
              nsAutoString placeholderText(" ");
              newTextNode->SetData(placeholderText);
              selection->Collapse(newNode, 0);
              selection->Extend(newNode, 1);
              result = InsertTextImpl(aStringToInsert);    // this really recurses, right?
            }
          }
        }
      }            
    }
    EndTransaction();
  }
  return result;
}

NS_IMETHODIMP nsEditor::SelectEntireDocument(nsIDOMSelection *aSelection)
{
  nsresult result;
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMElement>bodyElement;
  result = GetBodyElement(getter_AddRefs(bodyElement));
  if ((NS_SUCCEEDED(result)) && bodyElement)
  {
    nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
    if (bodyNode)
    {
      result = aSelection->Collapse(bodyNode, 0);
      if (NS_SUCCEEDED(result))
      {
        PRInt32 numBodyChildren=0;
        nsCOMPtr<nsIDOMNode>lastChild;
        result = bodyNode->GetLastChild(getter_AddRefs(lastChild));
        if ((NS_SUCCEEDED(result)) && lastChild)
        {
          GetChildOffset(lastChild, bodyNode, numBodyChildren);
          result = aSelection->Extend(bodyNode, numBodyChildren+1);
        }
      }
    }
    else {
      return NS_ERROR_NO_INTERFACE;
    }
  }
  return result;
}

//
// GetDeepFirst/LastChild borrowed from nsContentIterator
//
nsCOMPtr<nsIDOMNode>
nsEditor::GetDeepFirstChild(nsCOMPtr<nsIDOMNode> aRoot)
{
  nsCOMPtr<nsIContent> deepFirstChild;

  if (aRoot) 
  {  
    nsCOMPtr<nsIContent> cN (do_QueryInterface(aRoot));
    nsCOMPtr<nsIContent> cChild;
    cN->ChildAt(0,*getter_AddRefs(cChild));
    while ( cChild )
    {
      cN = cChild;
      nsCOMPtr<nsIDOMNode> cChildN;
      int i = 0;
      do {
        cN->ChildAt(i++, *getter_AddRefs(cChild));
        cChildN = do_QueryInterface(cChild);
      } while (cChild && cChildN && !IsEditable(cChildN));
    }
    deepFirstChild = cN;
  }

  nsCOMPtr<nsIDOMNode> deepFirstChildN (do_QueryInterface(deepFirstChild));
  return deepFirstChildN;
}


nsCOMPtr<nsIDOMNode>
nsEditor::GetDeepLastChild(nsCOMPtr<nsIDOMNode> aRoot)
{
  nsCOMPtr<nsIContent> deepLastChild;

  if (aRoot) 
  {  
    nsCOMPtr<nsIContent> cN (do_QueryInterface(aRoot));
    nsCOMPtr<nsIContent> cChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);

    while ( numChildren )
    {
      nsCOMPtr<nsIDOMNode> cChildN;
      do {
        cN->ChildAt(--numChildren, *getter_AddRefs(cChild));
        cChildN = (do_QueryInterface(cChild));
      } while (cChild && !IsEditable(cChildN));

      if (cChild)
      {
        cChild->ChildCount(numChildren);
        cN = cChild;
      }
      else
      {
        break;
      }
    }
    deepLastChild = cN;
  }

  nsCOMPtr<nsIDOMNode> deepLastChildN (do_QueryInterface(deepLastChild));
  return deepLastChildN;
}



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
        nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(i));
        nsCOMPtr<nsIDocumentStateListener> thisListener = do_QueryInterface(iSupports);
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
        nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(i));
        nsCOMPtr<nsIDocumentStateListener> thisListener = do_QueryInterface(iSupports);
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
          nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(i));
          nsCOMPtr<nsIDocumentStateListener> thisListener = do_QueryInterface(iSupports);
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


NS_IMETHODIMP nsEditor::CreateTxnForInsertText(const nsString & aStringToInsert,
                                               nsIDOMCharacterData *aTextNode,
                                               InsertTextTxn ** aTxn)
{
  nsresult result;
  PRInt32 offset;
  nsCOMPtr<nsIDOMCharacterData> nodeAsText;

  if (aTextNode) {
    nodeAsText = do_QueryInterface(aTextNode);
    offset = 0;
    result = NS_OK;
  }
  else
  {
    nsCOMPtr<nsIDOMSelection> selection;
    if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;
    result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
    if ((NS_SUCCEEDED(result)) && selection)
    {
#if 0
      // Trap the simple case where the collapsed selection
      //  is pointing to the beginning of a text node.
      //  That's where we should insert new text
      // *** If we don't do this, we will return NS_ERROR_EDITOR_NO_TEXTNODE
      //   from the iterator block below.
      //  This may be OK, now that I fixed InsertText so it inserts 
      //    at the offset of the selection anchor (i.e., the caret offset)
      //    instead of at offset+1
      PRBool collapsed
      result = selection->GetIsCollapsed(&collapsed);
      if (NS_SUCCEEDED(result) && collapsed) 
      {
        // Check if the node at selection offset is a textNode
        //   and use that with offset = 0.
      }
#endif
      result = NS_ERROR_UNEXPECTED; 
      nsCOMPtr<nsIEnumerator> enumerator;
      result = selection->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_SUCCEEDED(result) && enumerator)
      {
        enumerator->First(); 
        nsCOMPtr<nsISupports> currentItem;
        result = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if ((NS_SUCCEEDED(result)) && (currentItem))
        {
          result = NS_ERROR_UNEXPECTED; 
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
          if (range)
          {
            nsCOMPtr<nsIDOMNode> node;
            result = range->GetStartParent(getter_AddRefs(node));
            if ((NS_SUCCEEDED(result)) && (node))
            {
              nodeAsText = do_QueryInterface(node);
              range->GetStartOffset(&offset);
              if (!nodeAsText) {
                result = NS_ERROR_EDITOR_NO_TEXTNODE;
              }
            }
          }
        }
        else
        { 
          result = NS_ERROR_EDITOR_NO_SELECTION;
        }
      }
    }
  }
  if (NS_SUCCEEDED(result) && nodeAsText)
  {
    result = TransactionFactory::GetNewTransaction(InsertTextTxn::GetCID(), (EditTxn **)aTxn);
    if (nsnull!=*aTxn) {
      result = (*aTxn)->Init(nodeAsText, offset, aStringToInsert, mPresShellWeak);
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return result;
}

// we're in the special situation where there is no selection.  Insert the text
// at the beginning of the document.
// XXX: this is all logic that must be moved to the rule system
//      for HTML, we create a text node on the body.  That's what is done below
//      for XML, we would create a text node on the root element.
//      The rule system should be telling us which of these (or any other variant) to do.
/* this method should look something like
   BeginTransaction()
   mRule->GetNodeForInitialInsert(parentNode)
   mRule->CreateInitialDocumentFragment(childNode)
   InsertElement(childNode, parentNode)
   find the first text node in childNode
   insert the text there
*/

NS_IMETHODIMP nsEditor::DoInitialInsert(const nsString & aStringToInsert)
{
  if (!mDocWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsAutoString bodyTag = "body";
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
  if ((NS_SUCCEEDED(result)) && nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    NS_ASSERTION(1==count, "there is not exactly 1 body in the document!");
    nsCOMPtr<nsIDOMNode>node;
    result = nodeList->Item(0, getter_AddRefs(node));
    if ((NS_SUCCEEDED(result)) && node)
    { // now we've got the body tag.
      // create transaction to insert the text node, 
      // and create a transaction to insert the text
      CreateElementTxn *txn;
      nsAutoString textNodeTag;
      result = GetTextNodeTag(textNodeTag);
      if (NS_FAILED(result)) { return result; }
      result = CreateTxnForCreateElement(textNodeTag, node, 0, &txn);
      if ((NS_SUCCEEDED(result)) && txn)
      {
        result = Do(txn);
        if (NS_SUCCEEDED(result))
        {
          nsCOMPtr<nsIDOMNode>newNode;
          txn->GetNewNode(getter_AddRefs(newNode));
          if ((NS_SUCCEEDED(result)) && newNode)
          {
            nsCOMPtr<nsIDOMCharacterData>newTextNode;
            newTextNode = do_QueryInterface(newNode);
            if (newTextNode)
            {
              InsertTextTxn *insertTxn;
              result = CreateTxnForInsertText(aStringToInsert, newTextNode, &insertTxn);
              if (NS_SUCCEEDED(result)) {
                result = Do(insertTxn);
              }
            }
            else {
              result = NS_ERROR_UNEXPECTED;
            }
          }
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::DeleteText(nsIDOMCharacterData *aElement,
                              PRUint32             aOffset,
                              PRUint32             aLength)
{
  DeleteTextTxn *txn;
  nsresult result = CreateTxnForDeleteText(aElement, aOffset, aLength, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
    // HACKForceRedraw();
  }
  return result;
}


NS_IMETHODIMP nsEditor::CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                               PRUint32             aOffset,
                                               PRUint32             aLength,
                                               DeleteTextTxn      **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(DeleteTextTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aElement, aOffset, aLength);
    }
  }
  return result;
}




NS_IMETHODIMP nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if (nsnull != aNode)
  {
    result = TransactionFactory::GetNewTransaction(SplitElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aNode, aOffset);
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                             nsIDOMNode  *aRightNode,
                                             JoinElementTxn **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull != aLeftNode) && (nsnull != aRightNode))
  {
    result = TransactionFactory::GetNewTransaction(JoinElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aLeftNode, aRightNode);
    }
  }
  return result;
}


// END nsEditor core implementation

#ifdef XP_MAC
#pragma mark -
#pragma mark --- nsEditor public static helper methods ---
#pragma mark -
#endif

// BEGIN nsEditor public static helper methods

nsresult
nsEditor::SplitNodeImpl(nsIDOMNode * aExistingRightNode,
                        PRInt32      aOffset,
                        nsIDOMNode*  aNewLeftNode,
                        nsIDOMNode*  aParent)
{

  if (gNoisy) { printf("SplitNodeImpl: left=%p, right=%p, offset=%d\n", aNewLeftNode, aExistingRightNode, aOffset); }
  
  nsresult result;
  NS_ASSERTION(((nsnull!=aExistingRightNode) &&
                (nsnull!=aNewLeftNode) &&
                (nsnull!=aParent)),
                "null arg");
  if ((nsnull!=aExistingRightNode) &&
      (nsnull!=aNewLeftNode) &&
      (nsnull!=aParent))
  {
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
          nsString leftText;
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
  nsresult result = NS_OK;
  NS_ASSERTION(((nsnull!=aNodeToKeep) &&
                (nsnull!=aNodeToJoin) &&
                (nsnull!=aParent)),
                "null arg");
  if ((nsnull!=aNodeToKeep) &&
      (nsnull!=aNodeToJoin) &&
      (nsnull!=aParent))
  {
    // if it's a text node, just shuffle around some text
    nsCOMPtr<nsIDOMCharacterData> keepNodeAsText( do_QueryInterface(aNodeToKeep) );
    nsCOMPtr<nsIDOMCharacterData> joinNodeAsText( do_QueryInterface(aNodeToJoin) );
    if (keepNodeAsText && joinNodeAsText)
    {
      nsString rightText;
      nsString leftText;
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
        childNodes->GetLength(&childCount);
        nsCOMPtr<nsIDOMNode> firstNode; //only used if aNodeToKeepIsFirst is false
        if (PR_FALSE==aNodeToKeepIsFirst)
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
            if (PR_TRUE==aNodeToKeepIsFirst)
            { // append children of aNodeToJoin
              //was result = aNodeToKeep->AppendChild(childNode, getter_AddRefs(resultNode));
              result = aNodeToKeep->InsertBefore(childNode, previousChild, getter_AddRefs(resultNode));
              previousChild = do_QueryInterface(childNode);
            }
            else
            { // prepend children of aNodeToJoin
              //was result = aNodeToKeep->InsertBefore(childNode, firstNode, getter_AddRefs(resultNode));
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
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aChild && aParent)
  {
    nsCOMPtr<nsIDOMNodeList> childNodes;
    result = aParent->GetChildNodes(getter_AddRefs(childNodes));
    if ((NS_SUCCEEDED(result)) && (childNodes))
    {
      PRInt32 i=0;
      for ( ; NS_SUCCEEDED(result); i++)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        result = childNodes->Item(i, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(result)) && (childNode))
        {
          if (childNode.get()==aChild)
          {
            aOffset = i;
            break;
          }
        }
        else if (!childNode)
          result = NS_ERROR_NULL_POINTER;
      }
    }
    else if (!childNodes)
      result = NS_ERROR_NULL_POINTER;
  }
  return result;
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
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(aNode);
  if (nodeAsChar) {
    nodeAsChar->GetLength(&aCount);
  }
  else
  {
    PRBool hasChildNodes;
    aNode->HasChildNodes(&hasChildNodes);
    if (PR_TRUE==hasChildNodes)
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

// Non-static version for the nsIEditor interface and JavaScript
NS_IMETHODIMP 
nsEditor::NodeIsBlock(nsIDOMNode *aNode, PRBool &aIsBlock)
{
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  return IsNodeBlock(aNode, aIsBlock);
}

// The list of block nodes is shorter, so do the real work here...
nsresult
nsEditor::IsNodeBlock(nsIDOMNode *aNode, PRBool &aIsBlock)
{
  // this is a content-based implementation
  if (!aNode) { return NS_ERROR_NULL_POINTER; }

  nsresult result = NS_ERROR_FAILURE;
  aIsBlock = PR_FALSE;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tagName;
    result = element->GetTagName(tagName);
    if (NS_SUCCEEDED(result))
    {
      tagName.ToLowerCase();
      nsIAtom *tagAtom = NS_NewAtom(tagName);
      if (!tagAtom) { return NS_ERROR_NULL_POINTER; }

      if (tagAtom==nsIEditProperty::p          ||
          tagAtom==nsIEditProperty::div        ||
          tagAtom==nsIEditProperty::blockquote ||
          tagAtom==nsIEditProperty::h1         ||
          tagAtom==nsIEditProperty::h2         ||
          tagAtom==nsIEditProperty::h3         ||
          tagAtom==nsIEditProperty::h4         ||
          tagAtom==nsIEditProperty::h5         ||
          tagAtom==nsIEditProperty::h6         ||
          tagAtom==nsIEditProperty::ul         ||
          tagAtom==nsIEditProperty::ol         ||
          tagAtom==nsIEditProperty::dl         ||
          tagAtom==nsIEditProperty::pre        ||
          tagAtom==nsIEditProperty::noscript   ||
          tagAtom==nsIEditProperty::form       ||
          tagAtom==nsIEditProperty::hr         ||
          tagAtom==nsIEditProperty::table      ||
          tagAtom==nsIEditProperty::fieldset   ||
          tagAtom==nsIEditProperty::address    ||
          tagAtom==nsIEditProperty::body       ||
          tagAtom==nsIEditProperty::tr         ||
          tagAtom==nsIEditProperty::td         ||
          tagAtom==nsIEditProperty::th         ||
          tagAtom==nsIEditProperty::caption    ||
          tagAtom==nsIEditProperty::col        ||
          tagAtom==nsIEditProperty::colgroup   ||
          tagAtom==nsIEditProperty::thead      ||
          tagAtom==nsIEditProperty::tfoot      ||
          tagAtom==nsIEditProperty::li         ||
          tagAtom==nsIEditProperty::dt         ||
          tagAtom==nsIEditProperty::dd         ||
          tagAtom==nsIEditProperty::legend     )
      {
        aIsBlock = PR_TRUE;
      }
      result = NS_OK;
    }
  } else {
    // We don't have an element -- probably a text node
    nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(aNode);
    if (nodeAsText)
    {
      aIsBlock = PR_FALSE;
      result = NS_OK;
    }
  }
  return result;
}

// ...and simply assume non-block element is inline
nsresult
nsEditor::IsNodeInline(nsIDOMNode *aNode, PRBool &aIsInline)
{
  // this is a content-based implementation
  if (!aNode) { return NS_ERROR_NULL_POINTER; }

  nsresult result;
  aIsInline = PR_FALSE;
  PRBool IsBlock = PR_FALSE;
  result = IsNodeBlock(aNode, IsBlock);
  aIsInline = !IsBlock;
  return result;
}

nsresult
nsEditor::GetBlockParent(nsIDOMNode *aNode, nsIDOMElement **aBlockParent) 
{
  nsresult result = NS_OK;
  if (!aBlockParent) {return NS_ERROR_NULL_POINTER;}
  *aBlockParent = nsnull;
  nsCOMPtr<nsIDOMNode>parent;
  nsCOMPtr<nsIDOMNode>temp;
  result = aNode->GetParentNode(getter_AddRefs(parent));
  while (NS_SUCCEEDED(result) && parent)
  {
    PRBool isInline;
    result = IsNodeInline(parent, isInline);
    if (PR_FALSE==isInline)
    {
      parent->QueryInterface(nsIDOMElement::GetIID(), (void**)aBlockParent);
      break;
    }
    result = parent->GetParentNode(getter_AddRefs(temp));
    parent = do_QueryInterface(temp);
  }
  if (gNoisy) { 
    printf("GetBlockParent for %p returning parent %p\n", aNode, *aBlockParent); 
  }
  return result;
}

nsresult
nsEditor::GetBlockSection(nsIDOMNode *aChild,
                          nsIDOMNode **aLeftNode, 
                          nsIDOMNode **aRightNode) 
{
  nsresult result = NS_OK;
  if (!aChild || !aLeftNode || !aRightNode) {return NS_ERROR_NULL_POINTER;}
  *aLeftNode = aChild;
  *aRightNode = aChild;

  nsCOMPtr<nsIDOMNode>sibling;
  result = aChild->GetPreviousSibling(getter_AddRefs(sibling));
  while ((NS_SUCCEEDED(result)) && sibling)
  {
    PRBool isInline;
    IsNodeInline(sibling, isInline);
    if (PR_FALSE==isInline) 
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(sibling);
      if (!nodeAsText) {
        break;
      }
      // XXX: needs some logic to work for other leaf nodes besides text!
    }
    *aLeftNode = sibling;
    result = (*aLeftNode)->GetPreviousSibling(getter_AddRefs(sibling)); 
  }
  NS_ADDREF((*aLeftNode));
  // now do the right side
  result = aChild->GetNextSibling(getter_AddRefs(sibling));
  while ((NS_SUCCEEDED(result)) && sibling)
  {
    PRBool isInline;
    IsNodeInline(sibling, isInline);
    if (PR_FALSE==isInline) 
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(sibling);
      if (!nodeAsText) {
        break;
      }
    }
    *aRightNode = sibling;
    result = (*aRightNode)->GetNextSibling(getter_AddRefs(sibling)); 
  }
  NS_ADDREF((*aRightNode));
  if (gNoisy) { printf("GetBlockSection returning %p %p\n", 
                      (*aLeftNode), (*aRightNode)); }

  return result;
}

nsresult
nsEditor::GetBlockSectionsForRange(nsIDOMRange *aRange, nsISupportsArray *aSections) 
{
  if (!aRange || !aSections) {return NS_ERROR_NULL_POINTER;}

  nsresult result;
  nsCOMPtr<nsIContentIterator>iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIDOMRange> lastRange;
    iter->Init(aRange);
    nsCOMPtr<nsIContent> currentContent;
    iter->CurrentNode(getter_AddRefs(currentContent));
    while (NS_COMFALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode>currentNode = do_QueryInterface(currentContent);
      if (currentNode)
      {
        nsCOMPtr<nsIAtom> currentContentTag;
        currentContent->GetTag(*getter_AddRefs(currentContentTag));
        // <BR> divides block content ranges.  We can achieve this by nulling out lastRange
        if (nsIEditProperty::br==currentContentTag.get())
        {
          lastRange = do_QueryInterface(nsnull);
        }
        else
        {
          PRBool isInlineOrText;
          result = IsNodeInline(currentNode, isInlineOrText);
          if (PR_FALSE==isInlineOrText)
          {
            PRUint16 nodeType;
            currentNode->GetNodeType(&nodeType);
            if (nsIDOMNode::TEXT_NODE == nodeType) {
              isInlineOrText = PR_TRUE;
            }
          }
          if (PR_TRUE==isInlineOrText)
          {
            nsCOMPtr<nsIDOMNode>leftNode;
            nsCOMPtr<nsIDOMNode>rightNode;
            result = GetBlockSection(currentNode,
                                     getter_AddRefs(leftNode),
                                     getter_AddRefs(rightNode));
            if (gNoisy) {printf("currentNode %p has block content (%p,%p)\n", currentNode.get(), leftNode.get(), rightNode.get());}
            if ((NS_SUCCEEDED(result)) && leftNode && rightNode)
            {
              // add range to the list if it doesn't overlap with the previous range
              PRBool addRange=PR_TRUE;
              if (lastRange)
              {
                nsCOMPtr<nsIDOMNode> lastStartNode;
                nsCOMPtr<nsIDOMElement> blockParentOfLastStartNode;
                lastRange->GetStartParent(getter_AddRefs(lastStartNode));
                result = GetBlockParent(lastStartNode, getter_AddRefs(blockParentOfLastStartNode));
                if ((NS_SUCCEEDED(result)) && blockParentOfLastStartNode)
                {
                  if (gNoisy) {printf("lastStartNode %p has block parent %p\n", lastStartNode.get(), blockParentOfLastStartNode.get());}
                  nsCOMPtr<nsIDOMElement> blockParentOfLeftNode;
                  result = GetBlockParent(leftNode, getter_AddRefs(blockParentOfLeftNode));
                  if ((NS_SUCCEEDED(result)) && blockParentOfLeftNode)
                  {
                    if (gNoisy) {printf("leftNode %p has block parent %p\n", leftNode.get(), blockParentOfLeftNode.get());}
                    if (blockParentOfLastStartNode==blockParentOfLeftNode) {
                      addRange = PR_FALSE;
                    }
                  }
                }
              }
              if (PR_TRUE==addRange) 
              {
                if (gNoisy) {printf("adding range, setting lastRange with start node %p\n", leftNode.get());}
                nsCOMPtr<nsIDOMRange> range;
                result = nsComponentManager::CreateInstance(kCRangeCID, nsnull, 
                                                            nsIDOMRange::GetIID(), getter_AddRefs(range));
                if ((NS_SUCCEEDED(result)) && range)
                { // initialize the range
                  range->SetStart(leftNode, 0);
                  range->SetEnd(rightNode, 0);
                  aSections->AppendElement(range);
                  lastRange = do_QueryInterface(range);
                }
              }        
            }
          }
        }
      }
      /* do not check result here, and especially do not return the result code.
       * we rely on iter->IsDone to tell us when the iteration is complete
       */
      iter->Next();
      iter->CurrentNode(getter_AddRefs(currentContent));
    }
  }
  return result;
}

nsresult
nsEditor::IntermediateNodesAreInline(nsIDOMRange *aRange,
                                     nsIDOMNode  *aStartNode, 
                                     PRInt32      aStartOffset, 
                                     nsIDOMNode  *aEndNode,
                                     PRInt32      aEndOffset,
                                     PRBool      &aResult) 
{
  aResult = PR_TRUE; // init out param.  we assume the condition is true unless we find a node that violates it
  if (!aStartNode || !aEndNode || !aRange) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIContentIterator>iter;
  nsresult result;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), getter_AddRefs(iter));
  //XXX: maybe CreateInstance is expensive, and I should keep around a static iter?  
  //     as long as this method can't be called recursively or re-entrantly!

  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIContent>startContent;
    startContent = do_QueryInterface(aStartNode);
    nsCOMPtr<nsIContent>endContent;
    endContent = do_QueryInterface(aEndNode);
    if (startContent && endContent)
    {
      iter->Init(aRange);
      nsCOMPtr<nsIContent> content;
      iter->CurrentNode(getter_AddRefs(content));
      while (NS_COMFALSE == iter->IsDone())
      {
        if ((content.get() != startContent.get()) &&
            (content.get() != endContent.get()))
        {
          nsCOMPtr<nsIDOMNode>currentNode;
          currentNode = do_QueryInterface(content);
          PRBool isInline=PR_FALSE;
          IsNodeInline(currentNode, isInline);
          if (PR_FALSE==isInline)
          {
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(currentNode);
            if (!nodeAsText)  // text nodes don't count in this check, so ignore them
            {
              aResult = PR_FALSE;
              break;
            }
          }
        }
        /* do not check result here, and especially do not return the result code.
         * we rely on iter->IsDone to tell us when the iteration is complete
         */
        iter->Next();
        iter->CurrentNode(getter_AddRefs(content));
      }
    }
  }

  return result;
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aParentNode, 
                       PRInt32      aOffset, 
                       PRBool       aEditableNode, 
                       nsIDOMNode **aResultNode)
{
  // just another version of GetPriorNode that takes a {parent, offset}
  // instead of a node
  nsresult result = NS_OK;
  if (!aParentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  // if we are at beginning of node than just look before it
  if (!aOffset)
  {
    return GetPriorNode(aParentNode, aEditableNode, aResultNode);
  }
  else
  {
    // else look before the child at 'aOffset'
    nsCOMPtr<nsIDOMNode> child = GetChildAt(aParentNode, aOffset);
    if (child)
      return GetPriorNode(child, aEditableNode, aResultNode);
    // unless there isn't one, in which case we are at the end of the node
    // and want the deep-right child.
    else
    {
      result = GetRightmostChild(*aResultNode, aResultNode);
      if (NS_FAILED(result)) return result;
      if (!aEditableNode) return result;
      if (IsEditable(*aResultNode))  return result;
      else 
      { 
        // restart the search from the non-editable node we just found
        nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
        return GetPriorNode(notEditableNode, aEditableNode, aResultNode);
      }
    }
  }
}



nsresult 
nsEditor::GetNextNode(nsIDOMNode   *aParentNode, 
                       PRInt32      aOffset, 
                       PRBool       aEditableNode, 
                       nsIDOMNode **aResultNode)
{
  // just another version of GetNextNode that takes a {parent, offset}
  // instead of a node
  nsresult result = NS_OK;
  if (!aParentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  // look at the child at 'aOffset'
  nsCOMPtr<nsIDOMNode> child = GetChildAt(aParentNode, aOffset);
  if (child)
  {
    result = GetLeftmostChild(*aResultNode, aResultNode);
    if (NS_FAILED(result)) return result;
    if (!aEditableNode) return result;
    if (IsEditable(*aResultNode))  return result;
    else 
    { 
      // restart the search from the non-editable node we just found
      nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
      return GetNextNode(notEditableNode, aEditableNode, aResultNode);
    }
  }
    
  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  else
  {
    return GetNextNode(aParentNode, aEditableNode, aResultNode);
  }
}



nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aCurrentNode, 
                       PRBool       aEditableNode, 
                       nsIDOMNode **aResultNode)
{
  nsresult result;
  if (!aCurrentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  *aResultNode = nsnull;  // init out-param

  // if aCurrentNode has a left sibling, return that sibling's rightmost child (or itself if it has no children)
  result = aCurrentNode->GetPreviousSibling(aResultNode);
  if ((NS_SUCCEEDED(result)) && *aResultNode)
  {
    result = GetRightmostChild(*aResultNode, aResultNode);
    if (NS_FAILED(result)) { return result; }
    if (PR_FALSE==aEditableNode) {
      return result;
    }
    if (PR_TRUE==IsEditable(*aResultNode)) {
      return result;
    }
    else 
    { // restart the search from the non-editable node we just found
      nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
      return GetPriorNode(notEditableNode, aEditableNode, aResultNode);
    }
  }
  
  // otherwise, walk up the parent change until there is a child that comes before 
  // the ancestor of aCurrentNode.  Then return that node's rightmost child
  nsCOMPtr<nsIDOMNode> parent = do_QueryInterface(aCurrentNode);
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetPreviousSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {
        result = GetRightmostChild(node, aResultNode);
        if (NS_FAILED(result)) { return result; }
        if (PR_FALSE==aEditableNode) {
          return result;
        }
        if (PR_TRUE==IsEditable(*aResultNode)) {
          return result;
        }
        else 
        { // restart the search from the non-editable node we just found
          nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
          return GetPriorNode(notEditableNode, aEditableNode, aResultNode);
        }
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

nsresult 
nsEditor::GetNextNode(nsIDOMNode  *aCurrentNode, 
                      PRBool       aEditableNode, 
                      nsIDOMNode **aResultNode)
{
  nsresult result;
  *aResultNode = nsnull;
  // if aCurrentNode has a right sibling, return that sibling's leftmost child (or itself if it has no children)
  result = aCurrentNode->GetNextSibling(aResultNode);
  if ((NS_SUCCEEDED(result)) && *aResultNode)
  {
    result = GetLeftmostChild(*aResultNode, aResultNode);
    if (NS_FAILED(result)) { return result; }
    if (PR_FALSE==aEditableNode) {
      return result;
    }
    if (PR_TRUE==IsEditable(*aResultNode)) {
      return result;
    }
    else 
    { // restart the search from the non-editable node we just found
      nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
      return GetNextNode(notEditableNode, aEditableNode, aResultNode);
    }
  }
  
  // otherwise, walk up the parent change until there is a child that comes after 
  // the ancestor of aCurrentNode.  Then return that node's leftmost child

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(aCurrentNode));
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetNextSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {
        result = GetLeftmostChild(node, aResultNode);
        if (NS_FAILED(result)) { return result; }
        if (PR_FALSE==aEditableNode) {
          return result;
        }
        if (PR_TRUE==IsEditable(*aResultNode)) {
          return result;
        }
        else 
        { // restart the search from the non-editable node we just found
          nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
          return GetNextNode(notEditableNode, aEditableNode, aResultNode);
        }
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

nsresult
nsEditor::GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> resultNode(do_QueryInterface(aCurrentNode));
  PRBool hasChildren;
  resultNode->HasChildNodes(&hasChildren);
  while ((NS_SUCCEEDED(result)) && (PR_TRUE==hasChildren))
  {
    nsCOMPtr<nsIDOMNode> temp(resultNode);
    temp->GetLastChild(getter_AddRefs(resultNode));
    resultNode->HasChildNodes(&hasChildren);
  }

  if (NS_SUCCEEDED(result)) {
    *aResultNode = resultNode;
    NS_ADDREF(*aResultNode);
  }
  return result;
}

nsresult
nsEditor::GetLeftmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> resultNode(do_QueryInterface(aCurrentNode));
  PRBool hasChildren;
  resultNode->HasChildNodes(&hasChildren);
  while ((NS_SUCCEEDED(result)) && (PR_TRUE==hasChildren))
  {
    nsCOMPtr<nsIDOMNode> temp(resultNode);
    temp->GetFirstChild(getter_AddRefs(resultNode));
    resultNode->HasChildNodes(&hasChildren);
  }

  if (NS_SUCCEEDED(result)) {
    *aResultNode = resultNode;
    NS_ADDREF(*aResultNode);
  }
  return result;
}

PRBool 
nsEditor::NodeIsType(nsIDOMNode *aNode, nsIAtom *aTag)
{
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tag;
    element->GetTagName(tag);
    PRUnichar *unicodeString;
    aTag->GetUnicode(&unicodeString);
    if (tag.EqualsIgnoreCase(unicodeString))
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool 
nsEditor::CanContainTag(nsIDOMNode* aParent, const nsString &aTag)
{
  // if we don't have a dtd then assume we can insert whatever want
  if (!mDTD) return PR_TRUE;
  
  PRInt32 childTagEnum, parentTagEnum;
  nsString parentStringTag;
  nsString non_const_aTag(aTag);
  nsresult res = mDTD->StringTagToIntTag(non_const_aTag,&childTagEnum);
  if (NS_FAILED(res)) return PR_FALSE;

  nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(aParent);
  if (!parentElement) return PR_FALSE;
  
  parentElement->GetTagName(parentStringTag);
  res = mDTD->StringTagToIntTag(parentStringTag,&parentTagEnum);
  if (NS_FAILED(res)) return PR_FALSE;

  return mDTD->CanContain(parentTagEnum, childTagEnum);
}


PRBool 
nsEditor::IsEditable(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIPresShell> shell;
  GetPresShell(getter_AddRefs(shell));
  if (!shell)  return PR_FALSE;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString att(kMOZEditorBogusNodeAttr);
    nsAutoString val;
    (void)element->GetAttribute(att, val);
    if (val.Equals(kMOZEditorBogusNodeValue)) {
      return PR_FALSE;
    }
  }
  // it's not the bogus node, so see if it is an irrelevant text node
  if (PR_TRUE==IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMCharacterData>text;
    text = do_QueryInterface(aNode);
    if (text)
    {
      nsAutoString data;
      text->GetData(data);
      PRUint32 length = data.Length();
      if (0==length) {
        return PR_FALSE;
      }
      // if the node contains only newlines, it's not editable
      PRUint32 i;
      for (i=0; i<length; i++)
      {
        if ('\n'!=data.CharAt(i)) {
          return PR_TRUE;
        }
      }
      return PR_FALSE;
    }
  }
  
  // we got this far, so see if it has a frame.  If so, we'll edit it.
  nsIFrame *resultFrame;
  nsCOMPtr<nsIContent>content;
  content = do_QueryInterface(aNode);
  if (content)
  {
    nsresult result = shell->GetPrimaryFrameFor(content, &resultFrame);
    if (NS_FAILED(result) || !resultFrame) {  // if it has no frame, it is not editable
      return PR_FALSE;
    }
    else {                                    // it has a frame, so it is editable
      return PR_TRUE;
    }
  }
  return PR_FALSE;  // it's not a content object (???) so it's not editable
}

nsresult
nsEditor::CountEditableChildren(nsIDOMNode *aNode, PRUint32 &outCount) 
{
  outCount = 0;
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  nsresult res=NS_OK;
  PRBool hasChildNodes;
  aNode->HasChildNodes(&hasChildNodes);
  if (PR_TRUE==hasChildNodes)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    PRUint32 len;
    PRUint32 i;
    res = aNode->GetChildNodes(getter_AddRefs(nodeList));
    if (NS_SUCCEEDED(res) && nodeList) 
    {
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


NS_IMETHODIMP nsEditor::IncDocModCount(PRInt32 inNumMods)
{
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDiskDocument>  diskDoc = do_QueryInterface(doc);
  if (diskDoc)
    diskDoc->IncrementModCount(inNumMods);

  NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}


NS_IMETHODIMP nsEditor::GetDocModCount(PRInt32 &outModCount)
{
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDiskDocument>  diskDoc = do_QueryInterface(doc);
  if (diskDoc)
    diskDoc->GetModCount(&outModCount);

  return NS_OK;
}


NS_IMETHODIMP nsEditor::ResetDocModCount()
{
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDiskDocument>  diskDoc = do_QueryInterface(doc);
  if (diskDoc)
    diskDoc->ResetModCount();

  NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}


void nsEditor::HACKForceRedraw()
{
#ifdef HACK_FORCE_REDRAW
// XXXX: Horrible hack! We are doing this because
// of an error in Gecko which is not rendering the
// document after a change via the DOM - gpk 2/11/99
  // BEGIN HACK!!!
  nsCOMPtr<nsIPresShell> shell;
  
   GetPresShell(getter_AddRefs(shell));
  if (shell) {
    nsCOMPtr<nsIViewManager> viewmgr;

    shell->GetViewManager(getter_AddRefs(viewmgr));
    if (viewmgr) {
      nsIView* view;
      viewmgr->GetRootView(view);      // views are not refCounted
      if (view) {
        viewmgr->UpdateView(view,nsnull,NS_VMREFRESH_IMMEDIATE);
      }
    }
  }
  // END HACK
#endif
}

nsresult
nsEditor::GetFirstNodeOfType(nsIDOMNode     *aStartNode, 
                             const nsString &aTag, 
                             nsIDOMNode    **aResult)
{
  nsresult result=NS_OK;

  if (!aStartNode)
    return NS_ERROR_NULL_POINTER;
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> element;
  *aResult = nsnull;
  nsCOMPtr<nsIDOMNode> childNode;
  result = aStartNode->GetFirstChild(getter_AddRefs(childNode));
  while (childNode)
  {
    result = childNode->QueryInterface(nsIDOMNode::GetIID(),getter_AddRefs(element));
    nsAutoString tag;
    if (NS_SUCCEEDED(result) && (element))
    {    
      element->GetTagName(tag);
      if (PR_TRUE==aTag.EqualsIgnoreCase(tag))
      {
        return (childNode->QueryInterface(nsIDOMNode::GetIID(),(void **) aResult)); // does the addref
      }
      else
      {
        result = GetFirstNodeOfType(childNode, aTag, aResult);
        if (nsnull!=*aResult)
          return result;
      }
    }
    nsCOMPtr<nsIDOMNode> temp = childNode;
    temp->GetNextSibling(getter_AddRefs(childNode));
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsEditor::GetFirstTextNode(nsIDOMNode *aNode, nsIDOMNode **aRetNode)
{
  if (!aNode || !aRetNode)
  {
    NS_NOTREACHED("GetFirstTextNode Failed");
    return NS_ERROR_NULL_POINTER;
  }

  PRUint16 mType;
  PRBool mCNodes;

  nsCOMPtr<nsIDOMNode> answer;
  
  aNode->GetNodeType(&mType);

  if (nsIDOMNode::ELEMENT_NODE == mType) {
    if (NS_SUCCEEDED(aNode->HasChildNodes(&mCNodes)) && PR_TRUE == mCNodes) 
    {
      nsCOMPtr<nsIDOMNode> node1;
      nsCOMPtr<nsIDOMNode> node2;

      if (!NS_SUCCEEDED(aNode->GetFirstChild(getter_AddRefs(node1))))
      {
        NS_NOTREACHED("GetFirstTextNode Failed");
      }
      while(!answer && node1) 
      {
        GetFirstTextNode(node1, getter_AddRefs(answer));
        node1->GetNextSibling(getter_AddRefs(node2));
        node1 = node2;
      }
    }
  }
  else if (nsIDOMNode::TEXT_NODE == mType) {
    answer = do_QueryInterface(aNode);
  }

    // OK, now return the answer, if any
  *aRetNode = answer;
  if (*aRetNode)
    NS_IF_ADDREF(*aRetNode);
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}


//END nsEditor Private methods

NS_IMETHODIMP 
nsEditor::SetInputMethodText(const nsString& aStringToInsert, nsIPrivateTextRangeList *aTextRangeList)
{
  IMETextTxn      *txn;
  
  nsresult result = CreateTxnForIMEText(aStringToInsert,aTextRangeList,&txn); // insert at the current selection
  if ((NS_SUCCEEDED(result)) && txn)  {
    BeginUpdateViewBatch();
    result = Do(txn);
    EndUpdateViewBatch();
  }
  else if (NS_ERROR_EDITOR_NO_SELECTION==result)  {
    result = DoInitialInputMethodInsert(aStringToInsert,aTextRangeList);
  }
  else if (NS_ERROR_EDITOR_NO_TEXTNODE==result) 
  {
    BeginTransaction();
    nsCOMPtr<nsIDOMSelection> selection;
    result = GetSelection(getter_AddRefs(selection));
    if ((NS_SUCCEEDED(result)) && selection)
    {
      nsCOMPtr<nsIDOMNode> selectedNode;
      PRInt32 offset;
      result = selection->GetAnchorNode(getter_AddRefs(selectedNode));
      if (NS_SUCCEEDED(result) && NS_SUCCEEDED(selection->GetAnchorOffset(&offset)) && selectedNode)
      {
        nsCOMPtr<nsIDOMNode> newNode;
        nsAutoString textNodeTag;
        result = GetTextNodeTag(textNodeTag);
        if (NS_FAILED(result)) { return result; }
        result = CreateNode(textNodeTag, selectedNode, offset+1,getter_AddRefs(newNode));
        if (NS_SUCCEEDED(result) && newNode)
        {
          nsCOMPtr<nsIDOMCharacterData>newTextNode;
          newTextNode = do_QueryInterface(newNode);
          if (newTextNode)
          {
            nsAutoString placeholderText(" ");
            newTextNode->SetData(placeholderText);
            selection->Collapse(newNode, 0);
            selection->Extend(newNode, 1);
            result = SetInputMethodText(aStringToInsert,aTextRangeList);
          }
        }
      }
    }
    
    EndTransaction();
  }

  return result;
}



NS_IMETHODIMP nsEditor::DoInitialInputMethodInsert(const nsString & aStringToInsert, nsIPrivateTextRangeList* aTextRangeList)
{
  if (!mDocWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsAutoString bodyTag = "body";
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
  if ((NS_SUCCEEDED(result)) && nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    NS_ASSERTION(1==count, "there is not exactly 1 body in the document!");
    nsCOMPtr<nsIDOMNode>node;
    result = nodeList->Item(0, getter_AddRefs(node));
    if ((NS_SUCCEEDED(result)) && node)
    {  // now we've got the body tag.
      // create transaction to insert the text node, 
      // and create a transaction to insert the text
      CreateElementTxn *txn;
      nsAutoString textNodeTag;
      result = GetTextNodeTag(textNodeTag);
      if (NS_FAILED(result)) { return result; }
      result = CreateTxnForCreateElement(textNodeTag, node, 0, &txn);
      if ((NS_SUCCEEDED(result)) && txn)
      {
        result = Do(txn);
        if (NS_SUCCEEDED(result))
        {
          nsCOMPtr<nsIDOMNode>newNode;
          txn->GetNewNode(getter_AddRefs(newNode));
          if ((NS_SUCCEEDED(result)) && newNode)
          {
            nsCOMPtr<nsIDOMCharacterData>newTextNode;
            newTextNode = do_QueryInterface(newNode);
            if (newTextNode)
            {
              mIMETextNode = newTextNode;
              mIMETextOffset = 0;
              mIMEBufferLength = 0;

              IMETextTxn *IMETxn;
              result = CreateTxnForIMEText(aStringToInsert,aTextRangeList,&IMETxn);
              if (NS_SUCCEEDED(result)) {
                result = Do(IMETxn);
              }
            }
            else {
              result = NS_ERROR_UNEXPECTED;
            }
          }
        }
      }
    }
  }

  return result;
}
///////////////////////////////////////////////////////////////////////////
// GetTag: digs out the atom for the tag of this node
//                    
nsCOMPtr<nsIAtom> 
nsEditor::GetTag(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIAtom> atom;
  
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsEditor::GetTag()");
    return atom;
  }
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content)
    content->GetTag(*getter_AddRefs(atom));

  return atom;
}


///////////////////////////////////////////////////////////////////////////
// GetTagString: digs out string for the tag of this node
//                    
nsresult 
nsEditor::GetTagString(nsIDOMNode *aNode, nsString& outString)
{
  nsCOMPtr<nsIAtom> atom;
  
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsEditor::GetTag()");
    return NS_ERROR_NULL_POINTER;
  }
  
  atom = GetTag(aNode);
  if (atom)
  {
    atom->ToString(outString);
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
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
  
  nsCOMPtr<nsIAtom> atom1 = GetTag(aNode1);
  nsCOMPtr<nsIAtom> atom2 = GetTag(aNode2);
  
  if (atom1.get() == atom2.get())
    return PR_TRUE;

  return PR_FALSE;
}



///////////////////////////////////////////////////////////////////////////
// IsBlockNode: true if this node is an html block node
//                    
PRBool
nsEditor::IsBlockNode(nsIDOMNode *aNode)
{
  return !IsInlineNode(aNode);
}


///////////////////////////////////////////////////////////////////////////
// IsInlineNode: true if this node is an html inline node
//                    
PRBool
nsEditor::IsInlineNode(nsIDOMNode *aNode)
{
  PRBool retVal = PR_FALSE;
  IsNodeInline(aNode, retVal);
  return retVal;
}


///////////////////////////////////////////////////////////////////////////
// GetBlockNodeParent: returns enclosing block level ancestor, if any
//
nsCOMPtr<nsIDOMNode>
nsEditor::GetBlockNodeParent(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> tmp;
  nsCOMPtr<nsIDOMNode> p;

  if (NS_FAILED(aNode->GetParentNode(getter_AddRefs(p))))  // no parent, ran off top of tree
    return tmp;

  while (p && !IsBlockNode(p))
  {
    if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp)))) // no parent, ran off top of tree
      return p;

    p = tmp;
  }
  return p;
}


///////////////////////////////////////////////////////////////////////////
// HasSameBlockNodeParent: true if nodes have same block level ancestor
//               
PRBool
nsEditor::HasSameBlockNodeParent(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2)
  {
    NS_NOTREACHED("null node passed to HasSameBlockNodeParent()");
    return PR_FALSE;
  }
  
  if (aNode1 == aNode2)
    return PR_TRUE;
    
  nsCOMPtr<nsIDOMNode> p1 = GetBlockNodeParent(aNode1);
  nsCOMPtr<nsIDOMNode> p2 = GetBlockNodeParent(aNode2);

  return (p1 == p2);
}


///////////////////////////////////////////////////////////////////////////
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
  if ((nodeType == nsIDOMNode::ELEMENT_NODE) || (nodeType == nsIDOMNode::TEXT_NODE))
    return PR_TRUE;
    
  return PR_FALSE;
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
  if (nodeType == nsIDOMNode::TEXT_NODE)
    return PR_TRUE;
    
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// GetIndexOf: returns the position index of the node in the parent
//
PRInt32 
nsEditor::GetIndexOf(nsIDOMNode *parent, nsIDOMNode *child)
{
  PRInt32 idx = 0;
  
  NS_PRECONDITION(parent, "null parent passed to nsEditor::GetIndexOf");
  NS_PRECONDITION(parent, "null child passed to nsEditor::GetIndexOf");
  nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
  nsCOMPtr<nsIContent> cChild = do_QueryInterface(child);
  NS_PRECONDITION(content, "null content in nsEditor::GetIndexOf");
  NS_PRECONDITION(cChild, "null content in nsEditor::GetIndexOf");
  
  nsresult res = content->IndexOf(cChild, idx);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("could not find child in parent - nsEditor::GetIndexOf");
  }
  return idx;
}
  

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
nsCOMPtr<nsIDOMNode> 
nsEditor::GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  
  if (!aParent) 
    return resultNode;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aParent);
  nsCOMPtr<nsIContent> cChild;
  NS_PRECONDITION(content, "null content in nsEditor::GetChildAt");
  
  if (NS_FAILED(content->ChildAt(aOffset, *getter_AddRefs(cChild))))
    return resultNode;
  
  resultNode = do_QueryInterface(cChild);
  return resultNode;
}
  


///////////////////////////////////////////////////////////////////////////
// NextNodeInBlock: gets the next/prev node in the block, if any.  Next node
//                  must be an element or text node, others are ignored
nsCOMPtr<nsIDOMNode>
nsEditor::NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir)
{
  nsCOMPtr<nsIDOMNode> nullNode;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> blockContent;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (!aNode)  return nullNode;

  nsCOMPtr<nsIContentIterator> iter;
  if (NS_FAILED(nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter))))
    return nullNode;

  // much gnashing of teeth as we twit back and forth between content and domnode types
  content = do_QueryInterface(aNode);
  if (IsBlockNode(aNode))
  {
    blockParent = do_QueryInterface(aNode);
  }
  else
  {
    blockParent = GetBlockNodeParent(aNode);
  }
  if (!blockParent) return nullNode;
  blockContent = do_QueryInterface(blockParent);
  if (!blockContent) return nullNode;
  
  if (NS_FAILED(iter->Init(blockContent)))  return nullNode;
  if (NS_FAILED(iter->PositionAt(content)))  return nullNode;
  
  while (NS_COMFALSE == iter->IsDone())
  {
    if (NS_FAILED(iter->CurrentNode(getter_AddRefs(content)))) return nullNode;
    // ignore nodes that aren't elements or text, or that are the block parent 
    node = do_QueryInterface(content);
    if (node && IsTextOrElementNode(node) && (node != blockParent) && (node.get() != aNode))
      return node;
    
    if (aDir == kIterForward)
      iter->Next();
    else
      iter->Prev();
  }
  
  return nullNode;
}


///////////////////////////////////////////////////////////////////////////
// GetStartNodeAndOffset: returns whatever the start parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetStartNodeAndOffset(nsIDOMSelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outStartNode,
                                       PRInt32 *outStartOffset)
{
  if (!outStartNode || !outStartOffset || !aSelection) 
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIEnumerator> enumerator;
  nsresult result;
  result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result) || !enumerator)
    return NS_ERROR_FAILURE;
    
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  if ((NS_FAILED(enumerator->CurrentItem(getter_AddRefs(currentItem)))) || !currentItem)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartParent(getter_AddRefs(*outStartNode))))
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartOffset(outStartOffset)))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetEndNodeAndOffset: returns whatever the end parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetEndNodeAndOffset(nsIDOMSelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outEndNode,
                                       PRInt32 *outEndOffset)
{
  if (!outEndNode || !outEndOffset) 
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIEnumerator> enumerator;
  nsresult result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result) || !enumerator)
    return NS_ERROR_FAILURE;
    
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  if ((NS_FAILED(enumerator->CurrentItem(getter_AddRefs(currentItem)))) || !currentItem)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetEndParent(getter_AddRefs(*outEndNode))))
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
  nsresult result;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  nsIFrame *frame;
  nsCOMPtr<nsIStyleContext> styleContext;
  const nsStyleText* styleText;
  PRBool bPreformatted;
  
  if (!aResult || !content) return NS_ERROR_NULL_POINTER;
  
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  
  result = ps->GetPrimaryFrameFor(content, &frame);
  if (NS_FAILED(result)) return result;
  
  result = ps->GetStyleContextFor(frame, getter_AddRefs(styleContext));
  if (NS_FAILED(result)) return result;

  styleText = (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);

  bPreformatted = (NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace) ||
    (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace);

  *aResult = bPreformatted;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsNextCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace
nsresult 
nsEditor::IsNextCharWhitespace(nsIDOMNode *aParentNode, 
                                      PRInt32 aOffset,
                                      PRBool *aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = PR_FALSE;
  
  nsString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    textNode->GetLength(&strLength);
    if ((PRUint32)aOffset < strLength)
    {
      // easy case: next char is in same node
      textNode->SubstringData(aOffset,aOffset+1,tempString);
      *aResult = nsString::IsSpace(tempString.First());
      return NS_OK;
    }
  }
  
  // harder case: next char in next node.
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterForward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    if (!IsInlineNode(node))  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          textNode->SubstringData(0,1,tempString);
          *aResult = nsString::IsSpace(tempString.First());
          return NS_OK;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    tmp = node;
    node = NextNodeInBlock(tmp, kIterForward);
  }
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPrevCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace
nsresult 
nsEditor::IsPrevCharWhitespace(nsIDOMNode *aParentNode, 
                                      PRInt32 aOffset,
                                      PRBool *aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = PR_FALSE;
  
  nsString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    if (aOffset > 0)
    {
      // easy case: prev char is in same node
      textNode->SubstringData(aOffset-1,aOffset,tempString);
      *aResult = nsString::IsSpace(tempString.First());
      return NS_OK;
    }
  }
  
  // harder case: prev char in next node
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterBackward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    if (!IsInlineNode(node))  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          textNode->SubstringData(strLength-1,strLength,tempString);
          *aResult = nsString::IsSpace(tempString.First());
          return NS_OK;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    // otherwise we found a node we want to skip, keep going
    tmp = node;
    node = NextNodeInBlock(tmp, kIterBackward);
  }
  
  return NS_OK;
  
}


///////////////////////////////////////////////////////////////////////////
// SplitNodeDeep: this splits a node "deeply", splitting children as 
//                appropriate.  The place to split is represented by
//                a dom point at {splitPointParent, splitPointOffset}.
//                That dom point must be inside aNode, which is the node to 
//                split.  outOffset is set to the offset in aNode where
//                the split terminates - where you would want to insert 
//                a new element, for instance, if thats why you were splitting 
//                the node.
//
nsresult
nsEditor::SplitNodeDeep(nsIDOMNode *aNode, 
                        nsIDOMNode *aSplitPointParent, 
                        PRInt32 aSplitPointOffset,
                        PRInt32 *outOffset)
{
  if (!aNode || !aSplitPointParent || !outOffset) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> nodeToSplit = do_QueryInterface(aSplitPointParent);
  nsCOMPtr<nsIDOMNode> tempNode, parentNode;  
  PRInt32 offset = aSplitPointOffset;
  nsresult res;
  
  while (nodeToSplit)
  {
    // need to insert rules code call here to do things like
    // not split a list if you are after the last <li> or before the first, etc.
    // for now we just have some smarts about unneccessarily splitting
    // textnodes, which should be universal enough to put straight in
    // this nsEditor routine.
    
    nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(nodeToSplit);
    PRUint32 textLen=0;
    if (nodeAsText)
      nodeAsText->GetLength(&textLen);
    PRBool bDoSplit = PR_FALSE;
    
    if (!nodeAsText || (offset && (offset != (PRInt32)textLen)))
    {
      bDoSplit = PR_TRUE;
      res = SplitNode(nodeToSplit, offset, getter_AddRefs(tempNode));
      if (NS_FAILED(res)) return res;
    }

    res = nodeToSplit->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    if (!parentNode) return NS_ERROR_FAILURE;
    
    if (!bDoSplit && offset)  // must be "end of text node" case, we didn't split it, just move past it
      offset = GetIndexOf(parentNode, nodeToSplit) +1;
    else
      offset = GetIndexOf(parentNode, nodeToSplit);
    
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
  NS_PRECONDITION(mUpdateCount>=0, "bad state");

  nsCOMPtr<nsIDOMSelection>selection;
  nsresult selectionResult = GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(selectionResult) && selection) {
    selection->StartBatchChanges();
  }

  if (nsnull!=mViewManager)
  {
    if (0==mUpdateCount)
    {
#ifdef HACK_FORCE_REDRAW
      mViewManager->DisableRefresh();
#else
      mViewManager->BeginUpdateViewBatch();
#endif
    }
    mUpdateCount++;
  }

  return NS_OK;
}


nsresult nsEditor::EndUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount>0, "bad state");

  if (nsnull!=mViewManager)
  {
    mUpdateCount--;
    if (0==mUpdateCount)
    {
#ifdef HACK_FORCE_REDRAW
      mViewManager->EnableRefresh();
      HACKForceRedraw();
#else
      mViewManager->EndUpdateViewBatch();
#endif
    }
  }  

  nsCOMPtr<nsIDOMSelection>selection;
  nsresult selectionResult = GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(selectionResult) && selection) {
    selection->EndBatchChanges();
  }

  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark --- protected nsEditor methods ---
#pragma mark -
#endif


NS_IMETHODIMP 
nsEditor::DeleteSelectionImpl(ESelectionCollapseDirection aAction)
{
  nsresult result;

  EditAggregateTxn *txn;
  result = CreateTxnForDeleteSelection(aAction, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }

  return result;
}


/* Non-interface, protected methods */

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
    GetDocModCount(modCount);
    if (modCount < 0)
      modCount = -modCount;
        
    rv = IncDocModCount(1);    // don't count transient transactions
  }
  
  return rv;
}


NS_IMETHODIMP 
nsEditor::DoAfterUndoTransaction()
{
  nsresult rv = NS_OK;

  rv = IncDocModCount(-1);    // all undoable transactions are non-transient

  return rv;
}

NS_IMETHODIMP 
nsEditor::DoAfterRedoTransaction()
{
  nsresult rv = NS_OK;

  rv = IncDocModCount(1);    // all redoable transactions are non-transient

  return rv;
}

NS_IMETHODIMP 
nsEditor::DoAfterDocumentSave()
{
  // the mod count is reset by nsIDiskDocument.
  NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                   const nsString& aAttribute, 
                                   const nsString& aValue,
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
                                      const nsString& aAttribute,
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


NS_IMETHODIMP nsEditor::CreateTxnForCreateElement(const nsString& aTag,
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
      result = (*aTxn)->Init(aElement);
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::CreateAggregateTxnForDeleteSelection(nsIAtom *aTxnName, EditAggregateTxn **aAggTxn) 
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
    nsCOMPtr<nsIDOMSelection> selection;
    if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;
    result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection)
    {
      PRBool collapsed;
      result = selection->GetIsCollapsed(&collapsed);
      if (NS_SUCCEEDED(result) && !collapsed) {
        EditAggregateTxn *delSelTxn;
        result = CreateTxnForDeleteSelection(eDoNothing, &delSelTxn);
        if (NS_SUCCEEDED(result) && delSelTxn) {
          (*aAggTxn)->AppendChild(delSelTxn);
        }
      }
    }
  }
  return result;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForIMEText(const nsString & aStringToInsert,
                              nsIPrivateTextRangeList*  aTextRangeList,
                              IMETextTxn ** aTxn)
{
  nsresult  result;

  if (mIMETextNode==nsnull) 
    BeginComposition(nsnull);

  result = TransactionFactory::GetNewTransaction(IMETextTxn::GetCID(), (EditTxn **)aTxn);
  if (nsnull!=*aTxn) {
    result = (*aTxn)->Init(mIMETextNode,mIMETextOffset,mIMEBufferLength,aTextRangeList,aStringToInsert,mPresShellWeak);
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
nsEditor::CreateTxnForDeleteSelection(nsIEditor::ESelectionCollapseDirection aAction,
                                                    EditAggregateTxn  ** aTxn)
{
  if (!aTxn)
    return NS_ERROR_NULL_POINTER;
  *aTxn = nsnull;

  nsresult result;
  nsCOMPtr<nsIDOMSelection> selection;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    // Check whether the selection is collapsed and we should do nothing:
    PRBool isCollapsed;
    result = (selection->GetIsCollapsed(&isCollapsed));
    if (NS_SUCCEEDED(result) && isCollapsed && aAction == eDoNothing)
      return NS_OK;

    // allocate the out-param transaction
    result = TransactionFactory::GetNewTransaction(EditAggregateTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_FAILED(result)) {
      return result;
    }

    nsCOMPtr<nsIEnumerator> enumerator;
    result = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(result) && enumerator)
    {
      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsCOMPtr<nsISupports> currentItem;
        result = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if ((NS_SUCCEEDED(result)) && (currentItem))
        {
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
          range->GetIsCollapsed(&isCollapsed);
          if (PR_FALSE==isCollapsed)
          {
            DeleteRangeTxn *txn;
            result = TransactionFactory::GetNewTransaction(DeleteRangeTxn::GetCID(), (EditTxn **)&txn);
            if ((NS_SUCCEEDED(result)) && (nsnull!=txn))
            {
              txn->Init(this, range);
              (*aTxn)->AppendChild(txn);
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
nsEditor::CreateTxnForDeleteInsertionPoint(nsIDOMRange         *aRange, 
                                           nsIEditor::ESelectionCollapseDirection
                                             aAction,
                                           EditAggregateTxn    *aTxn)
{
  nsCOMPtr<nsIDOMNode> node;
  PRBool isFirst;
  PRBool isLast;
  PRInt32 offset;
  //PRInt32 length=1;

  // get the node and offset of the insertion point
  nsresult result = aRange->GetStartParent(getter_AddRefs(node));
  if (NS_FAILED(result))
    return result;
  result = aRange->GetStartOffset(&offset);
  if (NS_FAILED(result))
    return result;

  // determine if the insertion point is at the beginning, middle, or end of the node
  nsCOMPtr<nsIDOMCharacterData> nodeAsText;
  nodeAsText = do_QueryInterface(node);

  if (nodeAsText)
  {
    PRUint32 count;
    nodeAsText->GetLength(&count);
    isFirst = PRBool(0==offset);
    isLast  = PRBool(count==(PRUint32)offset);
  }
  else
  { 
    // get the child list and count
    nsCOMPtr<nsIDOMNodeList>childList;
    PRUint32 count=0;
    result = node->GetChildNodes(getter_AddRefs(childList));
    if ((NS_SUCCEEDED(result)) && childList)
    {
      childList->GetLength(&count);
    }
    isFirst = PRBool(0==offset);
    isLast  = PRBool((count-1)==(PRUint32)offset);
  }
// XXX: if isFirst && isLast, then we'll need to delete the node 
  //    as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if ((eDeletePrevious==aAction) && (PR_TRUE==isFirst))
  { // we're backspacing from the beginning of the node.  Delete the first thing to our left
    nsCOMPtr<nsIDOMNode> priorNode;
    result = GetPriorNode(node, PR_TRUE, getter_AddRefs(priorNode));
    if ((NS_SUCCEEDED(result)) && priorNode)
    { // there is a priorNode, so delete it's last child (if text content, delete the last char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> priorNodeAsText;
      priorNodeAsText = do_QueryInterface(priorNode);
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
        }
      }
    }
  }
  else if ((nsIEditor::eDeleteNext==aAction) && (PR_TRUE==isLast))
  { // we're deleting from the end of the node.  Delete the first thing to our right
    nsCOMPtr<nsIDOMNode> nextNode;
    result = GetNextNode(node, PR_TRUE, getter_AddRefs(nextNode));
    if ((NS_SUCCEEDED(result)) && nextNode)
    { // there is a priorNode, so delete it's last child (if text content, delete the last char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> nextNodeAsText;
      nextNodeAsText = do_QueryInterface(nextNode);
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
        }
      }
    }
  }
  else
  {
    if (nodeAsText)
    { // we have text, so delete a char at the proper offset
      if (nsIEditor::eDeletePrevious==aAction) {
        offset --;
      }
      DeleteTextTxn *txn;
      result = CreateTxnForDeleteText(nodeAsText, offset, 1, &txn);
      if (NS_SUCCEEDED(result)) {
        aTxn->AppendChild(txn);
      }
    }
    else
    { // we're deleting a node
      DeleteElementTxn *txn;
      nsCOMPtr<nsIDOMNode> selectedNode;
      if (eDeletePrevious==aAction)
      {
        result = GetPriorNode(node, offset, PR_TRUE, getter_AddRefs(selectedNode));
      }
      else if (eDeleteNext==aAction)
      {
        result = GetNextNode(node, offset, PR_TRUE, getter_AddRefs(selectedNode));
      }
      if (NS_SUCCEEDED(result) && selectedNode) 
      {
        result = CreateTxnForDeleteElement(selectedNode, &txn);
        if (NS_SUCCEEDED(result)) 
        {
          aTxn->AppendChild(txn);
        }
      }
    }
  }
  return result;
}

