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


#include "nsIDOMDocument.h"
#include "nsEditor.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDocument.h"
#include "nsIServiceManager.h"
#include "nsEditFactory.h"
#include "nsTextEditFactory.h"
#include "nsHTMLEditFactory.h"
#include "nsEditorCID.h"
#include "nsTransactionManagerCID.h"
#include "nsITransactionManager.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDOMSelection.h"
#include "nsIEnumerator.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsICaret.h"
#include "nsISelectionMgr.h"

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


#define HACK_FORCE_REDRAW 1


#ifdef HACK_FORCE_REDRAW
// INCLUDES FOR EVIL HACK TO FOR REDRAW
// BEGIN
#include "nsIViewManager.h"
#include "nsIView.h"
// END

//#define NEW_CLIPBOARD_SUPPORT

#ifdef NEW_CLIPBOARD_SUPPORT

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIDataFlavor.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_IID(kIClipboardIID,    NS_ICLIPBOARD_IID);
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);

static NS_DEFINE_IID(kITransferableIID, NS_ITRANSFERABLE_IID);
static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);
#endif

#endif
static NS_DEFINE_IID(kIContentIID,          NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMTextIID,          NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMElementIID,       NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID,          NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMSelectionIID,     NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIDOMRangeIID,         NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,      NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentIID,         NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIPresShellIID,        NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIFactoryIID,          NS_IFACTORY_IID);
static NS_DEFINE_IID(kIEditFactoryIID,      NS_IEDITORFACTORY_IID);
static NS_DEFINE_IID(kITextEditFactoryIID,  NS_ITEXTEDITORFACTORY_IID);
static NS_DEFINE_IID(kIHTMLEditFactoryIID,  NS_IHTMLEDITORFACTORY_IID);
static NS_DEFINE_IID(kIEditorIID,           NS_IEDITOR_IID);
static NS_DEFINE_IID(kIEditorSupportIID,    NS_IEDITORSUPPORT_IID);
static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kEditorCID,            NS_EDITOR_CID);
static NS_DEFINE_CID(kTextEditorCID,        NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kHTMLEditorCID,        NS_HTMLEDITOR_CID);
static NS_DEFINE_IID(kIContentIteratorIID, NS_ICONTENTITERTOR_IID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
// transaction manager
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);
static NS_DEFINE_CID(kCTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);
// transactions
static NS_DEFINE_IID(kEditAggregateTxnIID,  EDIT_AGGREGATE_TXN_IID);
static NS_DEFINE_IID(kInsertTextTxnIID,     INSERT_TEXT_TXN_IID);
static NS_DEFINE_IID(kDeleteTextTxnIID,     DELETE_TEXT_TXN_IID);
static NS_DEFINE_IID(kCreateElementTxnIID,  CREATE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kInsertElementTxnIID,  INSERT_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteElementTxnIID,  DELETE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteRangeTxnIID,    DELETE_RANGE_TXN_IID);
static NS_DEFINE_IID(kChangeAttributeTxnIID,CHANGE_ATTRIBUTE_TXN_IID);
static NS_DEFINE_IID(kSplitElementTxnIID,   SPLIT_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kJoinElementTxnIID,    JOIN_ELEMENT_TXN_IID);

static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);

#ifdef XP_PC
#define TRANSACTION_MANAGER_DLL "txmgr.dll"
#else
#ifdef XP_MAC
#define TRANSACTION_MANAGER_DLL "TRANSACTION_MANAGER_DLL"
#else // XP_UNIX
#define TRANSACTION_MANAGER_DLL "libtxmgr.so"
#endif
#endif

#define NS_ERROR_EDITOR_NO_SELECTION NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,1)
#define NS_ERROR_EDITOR_NO_TEXTNODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,2)



/* ----- TEST METHODS DECLARATIONS ----- */
// Methods defined here are TEMPORARY
//NS_IMETHODIMP  GetColIndexForCell(nsIPresShell *aPresShell, nsIDOMNode *aCellNode, PRInt32 &aCellIndex);
/* ----- END TEST METHOD DECLARATIONS ----- */


PRInt32 nsEditor::gInstanceCount = 0;

//monitor for the editor



PRMonitor *getEditorMonitor() //if more than one person asks for the monitor at the same time for the FIRST time, we are screwed
{
  static PRMonitor *ns_editlock = nsnull;
  if (nsnull == ns_editlock)
  {
    ns_editlock = (PRMonitor *)1; //how long will the next line take?  lets cut down on the chance of reentrancy
    ns_editlock = PR_NewMonitor();
  }
  else if ((PRMonitor *)1 == ns_editlock)
    return getEditorMonitor();
  return ns_editlock;
}

nsIComponentManager* gCompMgr = NULL;

/*
we must be good providers of factories etc. this is where to put ALL editor exports
*/
//BEGIN EXPORTS
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports * aServMgr, 
                                           const nsCID & aClass, 
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory ** aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = nsnull;

  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  rv = servMgr->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), 
                         (nsISupports**)&gCompMgr);
  if (NS_FAILED(rv)) return rv;

  rv = NS_NOINTERFACE;
  if (aClass.Equals(kEditorCID)) {
    rv = GetEditFactory(aFactory, aClass);
    if (NS_FAILED(rv)) goto done;
  }
  else if (aClass.Equals(kTextEditorCID)) {
    rv = GetTextEditFactory(aFactory, aClass);
    if (NS_FAILED(rv)) goto done;
  }
  else if (aClass.Equals(kHTMLEditorCID)) {
    rv = GetHTMLEditFactory(aFactory, aClass);
    if (NS_FAILED(rv)) goto done;
  }
  
  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, gCompMgr);

  return rv;
}



extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* aServMgr)
{
  return nsEditor::gInstanceCount; //I have no idea. I am copying code here
}



extern "C" NS_EXPORT nsresult 
NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kEditorCID, NULL, NULL, path, 
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->RegisterComponent(kTextEditorCID, NULL, NULL, path, 
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->RegisterComponent(kHTMLEditorCID, NULL, NULL, path, 
                                  PR_TRUE, PR_TRUE);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult 
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterFactory(kIEditFactoryIID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterFactory(kITextEditFactoryIID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterFactory(kIHTMLEditFactoryIID, path);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

//END EXPORTS


//class implementations are in order they are declared in nsEditor.h

nsEditor::nsEditor()
{
  //initialize member variables here
  NS_INIT_REFCNT();
  PR_EnterMonitor(getEditorMonitor());
  gInstanceCount++;
  PR_ExitMonitor(getEditorMonitor());
}



nsEditor::~nsEditor()
{
  NS_IF_RELEASE(mPresShell);
  NS_IF_RELEASE(mViewManager);
}



//BEGIN nsIEditor interface implementations


NS_IMPL_ADDREF(nsEditor)

NS_IMPL_RELEASE(nsEditor)



NS_IMETHODIMP
nsEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIEditor *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEditorIID)) {
    *aInstancePtr = (void*)(nsIEditor*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEditorSupportIID)) {
    *aInstancePtr = (void*)(nsIEditorSupport*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP 
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;
  *aDoc = nsnull; // init out param
  NS_PRECONDITION(mDoc, "bad state, null mDoc");
  if (!mDoc)
    return NS_ERROR_NOT_INITIALIZED;
  return mDoc->QueryInterface(kIDOMDocumentIID, (void **)aDoc);
}

nsresult 
nsEditor::GetPresShell(nsIPresShell **aPS)
{
  if (!aPS)
    return NS_ERROR_NULL_POINTER;
  *aPS = nsnull; // init out param
  NS_PRECONDITION(mPresShell, "bad state, null mPresShell");
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;
  return mPresShell->QueryInterface(kIPresShellIID, (void **)aPS);
}


NS_IMETHODIMP
nsEditor::GetSelection(nsIDOMSelection **aSelection)
{
  if (!aSelection)
    return NS_ERROR_NULL_POINTER;
  *aSelection = nsnull;
  nsresult result = mPresShell->GetSelection(aSelection);  // does an addref
  return result;
}

NS_IMETHODIMP
nsEditor::Init(nsIDOMDocument *aDoc, nsIPresShell* aPresShell)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  if ((nsnull==aDoc) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

  mDoc = do_QueryInterface(aDoc);
  mPresShell = aPresShell;
  NS_ADDREF(mPresShell);
  mPresShell->GetViewManager(&mViewManager);
  mUpdateCount=0;
  InsertTextTxn::ClassInit();

  /* Show the caret */
  nsCOMPtr<nsICaret>	caret;
  if (NS_SUCCEEDED(mPresShell->GetCaret(getter_AddRefs(caret))))
  {
  	caret->SetCaretVisible(PR_TRUE);
  	caret->SetCaretReadOnly(PR_FALSE);
  }

  NS_POSTCONDITION(mDoc && mPresShell, "bad state");

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::EnableUndo(PRBool aEnable)
{
  nsITransactionManager *txnMgr = 0;
  nsresult result=NS_OK;

  if (PR_TRUE==aEnable)
  {
    if (!mTxnMgr)
    {
      result = gCompMgr->CreateInstance(kCTransactionManagerCID,
                                        nsnull,
                                        kITransactionManagerIID, (void **)&txnMgr);
      if (NS_FAILED(result) || !txnMgr) {
        printf("ERROR: Failed to get TransactionManager instance.\n");
        return NS_ERROR_NOT_AVAILABLE;
      }
      mTxnMgr = do_QueryInterface(txnMgr); // CreateInstance refCounted the instance for us, remember it in an nsCOMPtr
    }
  }
  else
  { // disable the transaction manager if it is enabled
    if (mTxnMgr)
    {
      mTxnMgr = do_QueryInterface(nsnull);
    }
  }

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
nsEditor::SetProperties(nsVoidArray *aPropList)
{
  return NS_OK;
}



NS_IMETHODIMP
nsEditor::GetProperties(nsVoidArray *aPropList)
{
  return NS_OK;
}


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
nsEditor::CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                   const nsString& aAttribute, 
                                   const nsString& aValue,
                                   ChangeAttributeTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(kChangeAttributeTxnIID, (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aElement, aAttribute, aValue, PR_FALSE);
    }
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

NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                      const nsString& aAttribute,
                                      ChangeAttributeTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(kChangeAttributeTxnIID, (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  
    {
      nsAutoString value;
      result = (*aTxn)->Init(this, aElement, aAttribute, value, PR_TRUE);
    }
  }
  return result;
}

NS_IMETHODIMP
nsEditor::InsertBreak()
{
  return NS_OK;
}
 

//END nsIEditorInterfaces


//BEGIN nsEditor Private methods

NS_IMETHODIMP
nsEditor::GetFirstNodeOfType(nsIDOMNode *aStartNode, const nsString &aTag, nsIDOMNode **aResult)
{
  nsresult result=NS_OK;

  if (!mDoc)
    return NS_ERROR_NULL_POINTER;
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  /* If no node set, get root node */
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMElement> element;

  if (nsnull==aStartNode)
  {
    mDoc->GetDocumentElement(getter_AddRefs(element));
    result = element->QueryInterface(kIDOMNodeIID,getter_AddRefs(node));
    if (NS_FAILED(result))
      return result;
    if (!node)
      return NS_ERROR_NULL_POINTER;
  }
  else
    node = do_QueryInterface(aStartNode);

  *aResult = nsnull;
  nsCOMPtr<nsIDOMNode> childNode;
  result = node->GetFirstChild(getter_AddRefs(childNode));
  while (childNode)
  {
    result = childNode->QueryInterface(kIDOMNodeIID,getter_AddRefs(element));
    nsAutoString tag;
    if (NS_SUCCEEDED(result) && (element))
    {    
      element->GetTagName(tag);
      if (PR_TRUE==aTag.Equals(tag))
      {
        return (childNode->QueryInterface(kIDOMNodeIID,(void **) aResult)); // does the addref
      }
      else
      {
        nsresult result = GetFirstNodeOfType(childNode, aTag, aResult);
        if (nsnull!=*aResult)
          return result;
      }
    }
    nsCOMPtr<nsIDOMNode> temp = childNode;
    temp->GetNextSibling(getter_AddRefs(childNode));
  }

  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
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

NS_IMETHODIMP 
nsEditor::Do(nsITransaction *aTxn)
{
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
    }
    selection->EndBatchChanges();
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::Undo(PRUint32 aCount)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult selectionResult = GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(selectionResult) && selection) {
    selection->StartBatchChanges();
    if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
    {
      PRUint32 i=0;
      for ( ; i<aCount; i++)
      {
        result = mTxnMgr->Undo();
        if (NS_FAILED(result))
          break;
      }
    }
    selection->EndBatchChanges();
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::Redo(PRUint32 aCount)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult selectionResult = GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(selectionResult) && selection) {
    selection->StartBatchChanges();
    if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
    {
      PRUint32 i=0;
      for ( ; i<aCount; i++)
      {
        result = mTxnMgr->Redo();
        if (NS_FAILED(result))
          break;
      }
    }
    selection->EndBatchChanges();
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::BeginTransaction()
{
  NS_PRECONDITION(mUpdateCount>=0, "bad state");
  if (nsnull!=mViewManager)
  {
    if (0==mUpdateCount)
      mViewManager->DisableRefresh();
    mUpdateCount++;
  }
  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    mTxnMgr->BeginBatch();
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndTransaction()
{
  NS_PRECONDITION(mUpdateCount>0, "bad state");
  if (nsnull!=mViewManager)
  {
    mUpdateCount--;
    if (0==mUpdateCount)
      mViewManager->EnableRefresh();
  }  
  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    mTxnMgr->EndBatch();
  }
  return NS_OK;
}

NS_IMETHODIMP nsEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return NS_OK;
}

// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDoc || !mPresShell) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  nsresult result = mPresShell->GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    nsAutoString bodyTag = "body";
    nsresult result = mDoc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
    if ((NS_SUCCEEDED(result)) && nodeList)
    {
      PRUint32 count;
      nodeList->GetLength(&count);
      NS_ASSERTION(1==count, "there is not exactly 1 body in the document!");
      nsCOMPtr<nsIDOMNode>bodyNode;
      result = nodeList->Item(0, getter_AddRefs(bodyNode));
      if ((NS_SUCCEEDED(result)) && bodyNode)
      {
        selection->Collapse(bodyNode, 0);
        PRInt32 numBodyChildren=0;
        nsCOMPtr<nsIDOMNode>lastChild;
        result = bodyNode->GetLastChild(getter_AddRefs(lastChild));
        if ((NS_SUCCEEDED(result)) && lastChild)
        {
          GetChildOffset(lastChild, bodyNode, numBodyChildren);
          selection->Extend(bodyNode, numBodyChildren+1);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::Cut()
{
  //printf("nsEditor::Cut\n");
  nsresult res = Copy();
  if (NS_SUCCEEDED(res))
    res = DeleteSelection(eLTR);
  return res;
}

extern "C" NS_EXPORT nsISelectionMgr* GetSelectionMgr();

NS_IMETHODIMP nsEditor::Copy()
{
  //printf("nsEditor::Copy\n");

  // Get the nsSelectionMgr:
  // XXX BWEEP BWEEP TEMPORARY!
  // The selection mgr needs to be a service.
  // See http://bugzilla.mozilla.org/show_bug.cgi?id=3509.
  // In the meantime, so I'm not blocked on writing the rest of the code,
  // nsSelectionMgr uses the egregious hack of a global variable:
  nsISelectionMgr* selectionMgr = GetSelectionMgr();
  if (!selectionMgr)
  {
    printf("Can't get selection mgr!\n");
    return NS_ERROR_FAILURE;
  }

  //NS_ADD_REF(theSelectionMgr);
  return mPresShell->DoCopy(selectionMgr);
}

NS_IMETHODIMP nsEditor::Paste()
{
  //printf("nsEditor::Paste\n");
  nsString stuffToPaste;

#ifdef NEW_CLIPBOARD_SUPPORT
  nsIClipboard* clipboard;
  nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                             kIClipboardIID,
                                             (nsISupports **)&clipboard);
  nsITransferable * trans;
  rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, kITransferableIID, (void**) &trans);
  //if (nsnull != trans) {
    //trans->AddDataFlavor("text/xif", "XIF Format");
    //trans->AddDataFlavor(kTextMime, "Text Format");
  //}

  clipboard->GetData(trans);
  trans->GetTransferString(stuffToPaste);

  NS_IF_RELEASE(clipboard);
  NS_IF_RELEASE(trans);


#else 

  // Get the nsSelectionMgr:
  // XXX BWEEP BWEEP TEMPORARY!
  // The selection mgr needs to be a service.
  // See http://bugzilla.mozilla.org/show_bug.cgi?id=3509.
  // In the meantime, so I'm not blocked on writing the rest of the code,
  // nsSelectionMgr uses the egregious hack of a global variable:
  nsISelectionMgr* selectionMgr = GetSelectionMgr();
  if (!selectionMgr)
  {
    printf("Can't get selection mgr!\n");
    return NS_ERROR_FAILURE;
  }
  //NS_ADD_REF(theSelectionMgr);

  // Now we have the selection mgr.  Get its contents as text (for now):
  selectionMgr->PasteTextBlocking(&stuffToPaste);
#endif
  //printf("Trying to insert '%s'\n", stuffToPaste.ToNewCString());

  // Now let InsertText handle the hard stuff:
  return InsertText(stuffToPaste);
}

nsString & nsIEditor::GetTextNodeTag()
{
  static nsString gTextNodeTag("special text node tag");
  return gTextNodeTag;
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

NS_IMETHODIMP nsEditor::CreateTxnForCreateElement(const nsString& aTag,
                                                  nsIDOMNode     *aParent,
                                                  PRInt32         aPosition,
                                                  CreateElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aParent)
  {
    result = TransactionFactory::GetNewTransaction(kCreateElementTxnIID, (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aTag, aParent, aPosition);
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::InsertNode(nsIDOMNode * aNode,
                              nsIDOMNode * aParent,
                              PRInt32      aPosition)
{
  InsertElementTxn *txn;
  nsresult result = CreateTxnForInsertElement(aNode, aParent, aPosition, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
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
    result = TransactionFactory::GetNewTransaction(kInsertElementTxnIID, (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result)) {
      result = (*aTxn)->Init(aNode, aParent, aPosition);
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::DeleteNode(nsIDOMNode * aElement)
{
  DeleteElementTxn *txn;
  nsresult result = CreateTxnForDeleteElement(aElement, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}

NS_IMETHODIMP nsEditor::CreateTxnForDeleteElement(nsIDOMNode * aElement,
                                             DeleteElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)aTxn);
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
    result = TransactionFactory::GetNewTransaction(kEditAggregateTxnIID, (EditTxn**)aAggTxn); 

    if (NS_FAILED(result) || !*aAggTxn) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Set the name for the aggregate transaction  
    (*aAggTxn)->SetName(aTxnName);

    // Get current selection and setup txn to delete it,
    //  but only if selection exists (is not a collapsed "caret" state)
    nsCOMPtr<nsIDOMSelection> selection;
    result = mPresShell->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection)
    {
      PRBool collapsed;
      result = selection->IsCollapsed(&collapsed);
      if (NS_SUCCEEDED(result) && !collapsed) {
        EditAggregateTxn *delSelTxn;
        result = CreateTxnForDeleteSelection(nsIEditor::eLTR, &delSelTxn);
        if (NS_SUCCEEDED(result) && delSelTxn) {
          (*aAggTxn)->AppendChild(delSelTxn);
        }
      }
    }
  }
  return result;
}


NS_IMETHODIMP 
nsEditor::InsertText(const nsString& aStringToInsert)
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
    aggTxn->AppendChild(txn);
    result = Do(aggTxn);  
  }
  else if (NS_ERROR_EDITOR_NO_SELECTION==result)  {
    result = DoInitialInsert(aStringToInsert);
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
      result = selection->GetAnchorNodeAndOffset(getter_AddRefs(selectedNode), &offset);
      if ((NS_SUCCEEDED(result)) && selectedNode)
      {
        nsCOMPtr<nsIDOMNode> newNode;
        result = CreateNode(GetTextNodeTag(), selectedNode, offset+1, 
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
            result = InsertText(aStringToInsert);
          }
        }
      }
    }            
    EndTransaction();
  }
  return result;
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
  }
  else
  {
    nsCOMPtr<nsIDOMSelection> selection;
    result = mPresShell->GetSelection(getter_AddRefs(selection));
    if ((NS_SUCCEEDED(result)) && selection)
    {
      result = NS_ERROR_UNEXPECTED; 
      nsCOMPtr<nsIEnumerator> enumerator;
      enumerator = do_QueryInterface(selection,&result);
      if (enumerator)
      {
        enumerator->First(); 
        nsISupports *currentItem;
        result = enumerator->CurrentItem(&currentItem);
        if ((NS_SUCCEEDED(result)) && (nsnull!=currentItem))
        {
          result = NS_ERROR_UNEXPECTED; 
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
          if (range)
          {
            nsCOMPtr<nsIDOMNode> node;
            result = range->GetStartParent(getter_AddRefs(node));
            if ((NS_SUCCEEDED(result)) && (node))
            {
              result = NS_ERROR_UNEXPECTED; 
              nodeAsText = do_QueryInterface(node,&result);
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
    result = TransactionFactory::GetNewTransaction(kInsertTextTxnIID, (EditTxn **)aTxn);
    if (nsnull!=*aTxn) {
      result = (*aTxn)->Init(nodeAsText, offset, aStringToInsert, mPresShell);
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
  if (!mDoc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsAutoString bodyTag = "body";
  nsresult result = mDoc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
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
      result = CreateTxnForCreateElement(GetTextNodeTag(), node, 0, &txn);
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
    HACKForceRedraw();
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
    result = TransactionFactory::GetNewTransaction(kDeleteTextTxnIID, (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aElement, aOffset, aLength);
    }
  }
  return result;
}


NS_IMETHODIMP nsEditor::DeleteSelectionAndCreateNode(const nsString& aTag, nsIDOMNode ** aNewNode)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection> selection;
  result = GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    PRBool collapsed;
    result = selection->IsCollapsed(&collapsed);
    if (NS_SUCCEEDED(result) && !collapsed) 
    {
      result = DeleteSelection(nsIEditor::eLTR);
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
      PRInt32 testOffset;
      nsresult debugResult = selection->GetAnchorNodeAndOffset(getter_AddRefs(testSelectedNode), &testOffset);
      // no selection is ok.
      // if there is a selection, it must be collapsed
      if (testSelectedNode)
      {
        PRBool testCollapsed;
        debugResult = selection->IsCollapsed(&testCollapsed);
        NS_ASSERTION((NS_SUCCEEDED(result)), "couldn't get a selection after deletion");
        NS_ASSERTION(PR_TRUE==testCollapsed, "selection not reset after deletion");
      }
#endif
    }
    // split the selected node
    nsCOMPtr<nsIDOMNode> parentSelectedNode;
    PRInt32 offsetOfSelectedNode;
    result = selection->GetAnchorNodeAndOffset(getter_AddRefs(parentSelectedNode), &offsetOfSelectedNode);
    if ((NS_SUCCEEDED(result)) && parentSelectedNode)
    {
      PRInt32 offsetOfNewNode;
      nsCOMPtr<nsIDOMNode> selectedNode;
      PRUint32 selectedNodeContentCount=0;
      nsCOMPtr<nsIDOMCharacterData>selectedParentNodeAsText;
      selectedParentNodeAsText = do_QueryInterface(parentSelectedNode);
      /* if the selection is a text node, split the text node if necesary
         and compute where to put the new node
      */
      if (selectedParentNodeAsText) 
      { 
        PRInt32 indexOfTextNodeInParent;
        selectedNode = do_QueryInterface(parentSelectedNode);
        selectedNode->GetParentNode(getter_AddRefs(parentSelectedNode));
        selectedParentNodeAsText->GetLength(&selectedNodeContentCount);
        nsIEditorSupport::GetChildOffset(selectedNode, parentSelectedNode, indexOfTextNodeInParent);

        if ((offsetOfSelectedNode!=0) && (((PRUint32)offsetOfSelectedNode)!=selectedNodeContentCount))
        {
          nsCOMPtr<nsIDOMNode> newSiblingNode;
          result = SplitNode(selectedNode, offsetOfSelectedNode, getter_AddRefs(newSiblingNode));
          // now get the node's offset in it's parent, and insert the new tag there
          if (NS_SUCCEEDED(result)) {
            result = nsIEditorSupport::GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
          }
        }
        else 
        { // determine where to insert the new node
          if (0==offsetOfSelectedNode) {
            offsetOfNewNode = indexOfTextNodeInParent; // insert new node as previous sibling to selection parent
          }
          else {                 // insert new node as last child
            nsIEditorSupport::GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
            offsetOfNewNode++;    // offsets are 0-based, and we need the index of the new node
          }
        }
      }
      /* if the selection is not a text node, split the parent node if necesary
         and compute where to put the new node
      */
      else
      { // it's an interior node
        nsCOMPtr<nsIDOMNodeList>parentChildList;
        parentSelectedNode->GetChildNodes(getter_AddRefs(parentChildList));
        if ((NS_SUCCEEDED(result)) && parentChildList)
        {
          result = parentChildList->Item(offsetOfSelectedNode, getter_AddRefs(selectedNode));
          if ((NS_SUCCEEDED(result)) && selectedNode)
          {
            nsCOMPtr<nsIDOMCharacterData>selectedNodeAsText;
            selectedNodeAsText = do_QueryInterface(selectedNode);
            nsCOMPtr<nsIDOMNodeList>childList;
            selectedNode->GetChildNodes(getter_AddRefs(childList));
            if ((NS_SUCCEEDED(result)) && childList) {
              childList->GetLength(&selectedNodeContentCount);
            }
            else {
              return NS_ERROR_NULL_POINTER;
            }
            if ((offsetOfSelectedNode!=0) && (((PRUint32)offsetOfSelectedNode)!=selectedNodeContentCount))
            {
              nsCOMPtr<nsIDOMNode> newSiblingNode;
              result = SplitNode(selectedNode, offsetOfSelectedNode, getter_AddRefs(newSiblingNode));
              // now get the node's offset in it's parent, and insert the new tag there
              if (NS_SUCCEEDED(result)) {
                result = nsIEditorSupport::GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
              }
            }
            else 
            { // determine where to insert the new node
              if (0==offsetOfSelectedNode) {
                offsetOfNewNode = 0; // insert new node as first child
              }
              else {                 // insert new node as last child
                nsIEditorSupport::GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
                offsetOfNewNode++;    // offsets are 0-based, and we need the index of the new node
              }
            }
          }
        }
      }

      if (NS_SUCCEEDED(result))
      { 
        nsCOMPtr<nsIDOMNode> newNode;
        result = CreateNode(aTag, parentSelectedNode, offsetOfNewNode, getter_AddRefs(newNode));
        // we want the selection to be just after the new node
        selection->Collapse(parentSelectedNode, offsetOfNewNode+1); 
        *aNewNode = newNode;
      }
    }
    else {
      printf("InsertBreak into an empty document is not yet supported\n");
    }
  }
  return result;
}

#define DELETE_SELECTION_DOESNT_GO_THROUGH_RANGE

NS_IMETHODIMP 
nsEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  nsresult result;
#ifdef DELETE_SELECTION_DOESNT_GO_THROUGH_RANGE
  EditAggregateTxn *txn;
  result = CreateTxnForDeleteSelection(aDir, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
#else
  // XXX Warning, this should be moved to a transaction since
  // calling it this way means undo won't work.
  nsCOMPtr<nsIDOMSelection> selection;
  result = mPresShell->GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
    result = selection->DeleteFromDocument();
#endif

  return result;
}

NS_IMETHODIMP nsEditor::CreateTxnForDeleteSelection(nsIEditor::Direction aDir, 
                                                    EditAggregateTxn  ** aTxn)
{
  if (!aTxn)
    return NS_ERROR_NULL_POINTER;
  *aTxn = nsnull;

  nsresult result;
  // allocate the out-param transaction
  result = TransactionFactory::GetNewTransaction(kEditAggregateTxnIID, (EditTxn **)aTxn);
  if (NS_FAILED(result)) {
    return result;
  }
  nsCOMPtr<nsIDOMSelection> selection;
  result = mPresShell->GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection,&result);
    if (enumerator)
    {
      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsISupports *currentItem=nsnull;
        result = enumerator->CurrentItem(&currentItem);
        if ((NS_SUCCEEDED(result)) && (currentItem))
        {
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
          PRBool isCollapsed;
          range->GetIsCollapsed(&isCollapsed);
          if (PR_FALSE==isCollapsed)
          {
            DeleteRangeTxn *txn;
            result = TransactionFactory::GetNewTransaction(kDeleteRangeTxnIID, (EditTxn **)&txn);
            if ((NS_SUCCEEDED(result)) && (nsnull!=txn))
            {
              txn->Init(this, range);
              (*aTxn)->AppendChild(txn);
            }
            else
              result = NS_ERROR_OUT_OF_MEMORY;
          }
          else
          { // we have an insertion point.  delete the thing in front of it or behind it, depending on aDir
            result = CreateTxnForDeleteInsertionPoint(range, aDir, *aTxn);
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
                                           nsIEditor::Direction aDir, 
                                           EditAggregateTxn    *aTxn)
{
  nsCOMPtr<nsIDOMNode> node;
  PRBool isFirst;
  PRBool isLast;
  PRInt32 offset;
  PRInt32 length=1;

  // get the node and offset of the insertion point
  nsresult result = aRange->GetStartParent(getter_AddRefs(node));
  if (NS_FAILED(result))
    return result;
  result = aRange->GetStartOffset(&offset);
  if (NS_FAILED(result))
    return result;

  // determine if the insertion point is at the beginning, middle, or end of the node
  nsCOMPtr<nsIDOMCharacterData> nodeAsText;
  nsCOMPtr<nsIDOMNode> selectedNode;
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
      childList->Item(offset, getter_AddRefs(selectedNode));
    }
    isFirst = PRBool(0==offset);
    isLast  = PRBool((count-1)==(PRUint32)offset);
  }
// XXX: if isFirst && isLast, then we'll need to delete the node 
  //    as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if ((nsIEditor::eRTL==aDir) && (PR_TRUE==isFirst))
  { // we're backspacing from the beginning of the node.  Delete the first thing to our left
    nsCOMPtr<nsIDOMNode> priorNode;
    result = GetPriorNode(node, getter_AddRefs(priorNode));
    if ((NS_SUCCEEDED(result)) && priorNode)
    { // there is a priorNode, so delete it's last child (if text content, delete the last char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> priorNodeAsText;
      priorNodeAsText = do_QueryInterface(priorNode, &result);
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
  else if ((nsIEditor::eLTR==aDir) && (PR_TRUE==isLast))
  { // we're deleting from the end of the node.  Delete the first thing to our right
    nsCOMPtr<nsIDOMNode> nextNode;
    result = GetNextNode(node, getter_AddRefs(nextNode));
    if ((NS_SUCCEEDED(result)) && nextNode)
    { // there is a priorNode, so delete it's last child (if text content, delete the last char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> nextNodeAsText;
      nextNodeAsText = do_QueryInterface(nextNode,&result);
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
      if (nsIEditor::eRTL==aDir) {
        offset --;
      }
      DeleteTextTxn *txn;
      result = CreateTxnForDeleteText(nodeAsText, offset, length, &txn);
      if (NS_SUCCEEDED(result)) {
        aTxn->AppendChild(txn);
      }
    }
    else
    { // we're deleting a node
      DeleteElementTxn *txn;
      result = CreateTxnForDeleteElement(selectedNode, &txn);
      if (NS_SUCCEEDED(result)) {
        aTxn->AppendChild(txn);
      }
    }
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::GetPriorNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result;
  *aResultNode = nsnull;
  // if aCurrentNode has a left sibling, return that sibling's rightmost child (or itself if it has no children)
  result = aCurrentNode->GetPreviousSibling(aResultNode);
  if ((NS_SUCCEEDED(result)) && *aResultNode)
    return GetRightmostChild(*aResultNode, aResultNode);
  
  // otherwise, walk up the parent change until there is a child that comes before 
  // the ancestor of aCurrentNode.  Then return that node's rightmost child

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(aCurrentNode));
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetPreviousSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {

        return GetRightmostChild(node, aResultNode);
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

NS_IMETHODIMP 
nsEditor::GetNextNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result;
  *aResultNode = nsnull;
  // if aCurrentNode has a left sibling, return that sibling's rightmost child (or itself if it has no children)
  result = aCurrentNode->GetNextSibling(aResultNode);
  if ((NS_SUCCEEDED(result)) && *aResultNode)
    return GetLeftmostChild(*aResultNode, aResultNode);
  
  // otherwise, walk up the parent change until there is a child that comes before 
  // the ancestor of aCurrentNode.  Then return that node's rightmost child

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(aCurrentNode));
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetNextSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {
        return GetLeftmostChild(node, aResultNode);
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

NS_IMETHODIMP
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

NS_IMETHODIMP
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

NS_IMETHODIMP 
nsEditor::SplitNode(nsIDOMNode * aNode,
                    PRInt32      aOffset,
                    nsIDOMNode **aNewLeftNode)
{
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
  return result;
}

NS_IMETHODIMP nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if (nsnull != aNode)
  {
    result = TransactionFactory::GetNewTransaction(kSplitElementTxnIID, (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aNode, aOffset);
    }
  }
  return result;
}

NS_IMETHODIMP
nsEditor::JoinNodes(nsIDOMNode * aLeftNode,
                    nsIDOMNode * aRightNode,
                    nsIDOMNode * aParent)
{
  JoinElementTxn *txn;
  nsresult result = CreateTxnForJoinNode(aLeftNode, aRightNode, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
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
    result = TransactionFactory::GetNewTransaction(kJoinElementTxnIID, (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aLeftNode, aRightNode);
    }
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::SplitNodeImpl(nsIDOMNode * aExistingRightNode,
                        PRInt32      aOffset,
                        nsIDOMNode*  aNewLeftNode,
                        nsIDOMNode*  aParent)
{
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
        }
        else
        {  // otherwise it's an interior node, so shuffle around the children
          nsCOMPtr<nsIDOMNodeList> childNodes;
          result = aExistingRightNode->GetChildNodes(getter_AddRefs(childNodes));
          if ((NS_SUCCEEDED(result)) && (childNodes))
          {
            PRInt32 i=0;
            for ( ; ((NS_SUCCEEDED(result)) && (i<aOffset)); i++)
            {
              nsCOMPtr<nsIDOMNode> childNode;
              result = childNodes->Item(i, getter_AddRefs(childNode));
              if ((NS_SUCCEEDED(result)) && (childNode))
              {
                result = aExistingRightNode->RemoveChild(childNode, getter_AddRefs(resultNode));
                if (NS_SUCCEEDED(result))
                {
                  result = aNewLeftNode->AppendChild(childNode, getter_AddRefs(resultNode));
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

NS_IMETHODIMP
nsEditor::JoinNodesImpl(nsIDOMNode * aNodeToKeep,
                        nsIDOMNode * aNodeToJoin,
                        nsIDOMNode * aParent,
                        PRBool       aNodeToKeepIsFirst)
{
  nsresult result;
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
        PRUint32 i;
        PRUint32 childCount=0;
        childNodes->GetLength(&childCount);
        nsCOMPtr<nsIDOMNode> firstNode; //only used if aNodeToKeepIsFirst is false
        if (PR_FALSE==aNodeToKeepIsFirst)
        { // remember the first child in aNodeToKeep, we'll insert all the children of aNodeToJoin in front of it
          result = aNodeToKeep->GetFirstChild(getter_AddRefs(firstNode));  
          // GetFirstChild returns nsnull firstNode if aNodeToKeep has no children, that's ok.
        }
        nsCOMPtr<nsIDOMNode> resultNode;
        for (i=0; ((NS_SUCCEEDED(result)) && (i<childCount)); i++)
        {
          nsCOMPtr<nsIDOMNode> childNode;
          result = childNodes->Item(i, getter_AddRefs(childNode));
          if ((NS_SUCCEEDED(result)) && (childNode))
          {
            if (PR_TRUE==aNodeToKeepIsFirst)
            { // append children of aNodeToJoin
              result = aNodeToKeep->AppendChild(childNode, getter_AddRefs(resultNode)); 
            }
            else
            { // prepend children of aNodeToJoin
              result = aNodeToKeep->InsertBefore(childNode, firstNode, getter_AddRefs(resultNode));
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

nsresult nsIEditorSupport::GetChildOffset(nsIDOMNode *aChild, nsIDOMNode *aParent, PRInt32 &aOffset)
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


//END nsEditor Private methods

void nsEditor::HACKForceRedraw()
{
#ifdef HACK_FORCE_REDRAW
// XXXX: Horrible hack! We are doing this because
// of an error in Gecko which is not rendering the
// document after a change via the DOM - gpk 2/11/99
  // BEGIN HACK!!!
  nsIPresShell* shell = nsnull;
  
 	GetPresShell(&shell);
  if (nsnull != shell) {
    nsIViewManager* viewmgr = nsnull;;
    nsIView* view = nsnull;

    shell->GetViewManager(&viewmgr);
    if (nsnull != viewmgr) {
      viewmgr->GetRootView(view);
      if (nsnull != view) {
        viewmgr->UpdateView(view,nsnull,NS_VMREFRESH_IMMEDIATE);
      }
      NS_RELEASE(viewmgr);
    }
  }
  // END HACK
#endif

}


NS_IMETHODIMP nsEditor::GetLayoutObject(nsIDOMNode *aNode, nsISupports **aLayoutObject)
{
  nsresult result = NS_ERROR_FAILURE;  // we return an error unless we get the index
  if( mPresShell != nsnull )
  {
    if ((nsnull!=aNode))
    { // get the content interface
      nsCOMPtr<nsIContent> nodeAsContent( do_QueryInterface(aNode) );
      if (nodeAsContent)
      { // get the frame from the content interface
        nsISupports *layoutObject=nsnull; // frames are not ref counted, so don't use an nsCOMPtr
        *aLayoutObject = nsnull;
        return (NS_SUCCEEDED(mPresShell->GetLayoutObjectFor(nodeAsContent, &layoutObject)));
      }
    }
    else {
      result = NS_ERROR_NULL_POINTER;
    }
  }
  return result;
}


//END nsEditor Private methods

/* ----- TEST METHODS ----- */
// Methods defined here are TEMPORARY

/* ORIGINAL version by Steve - KEEP FOR REFERENCE
NS_IMETHODIMP GetColIndexForCell(nsIPresShell *aPresShell, nsIDOMNode *aCellNode, PRInt32 &aCellIndex)
{
  aCellIndex=0; // initialize out param
  nsresult result = NS_ERROR_FAILURE;  // we return an error unless we get the index
  if ((nsnull!=aCellNode) && (nsnull!=aPresShell))
  { // get the content interface
    nsCOMPtr<nsIContent> nodeAsContent(aCellNode);
    if (nodeAsContent)
    { // get the frame from the content interface
      nsISupports *layoutObject=nsnull; // frames are not ref counted, so don't use an nsCOMPtr
      result = aPresShell->GetLayoutObjectFor(nodeAsContent, &layoutObject);
      if ((NS_SUCCEEDED(result)) && (nsnull!=layoutObject))
      { // get the table cell interface from the frame
        nsITableCellLayout *cellLayoutObject=nsnull; // again, frames are not ref-counted
        result = layoutObject->QueryInterface(nsITableCellLayout::GetIID(), (void**)(&cellLayoutObject));
        if ((NS_SUCCEEDED(result)) && (nsnull!=cellLayoutObject))
        { // get the index
          result = cellLayoutObject->GetColIndex(aCellIndex);
        }
      }
    }
  }
  else {
    result = NS_ERROR_NULL_POINTER;
  }
  return result;
}
*/

/* ----- END TEST METHODS ----- */





