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
#include "editor.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDocument.h"
#include "nsIServiceManager.h"
#include "nsEditFactory.h"
#include "nsEditorCID.h"
#include "nsTransactionManagerCID.h"
#include "nsITransactionManager.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDOMSelection.h"
#include "nsICollection.h"
#include "nsIEnumerator.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"

// transactions the editor knows how to build
#include "TransactionFactory.h"
#include "EditAggregateTxn.h"
#include "ChangeAttributeTxn.h"
#include "CreateElementTxn.h"
#include "DeleteElementTxn.h"
#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"
#include "DeleteRangeTxn.h"
#include "SplitElementTxn.h"
#include "JoinElementTxn.h"


static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMTextIID,          NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMElementIID,       NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID,          NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMSelectionIID,     NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIDOMRangeIID,         NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,      NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentIID,         NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIFactoryIID,          NS_IFACTORY_IID);
static NS_DEFINE_IID(kIEditFactoryIID,      NS_IEDITORFACTORY_IID);
static NS_DEFINE_IID(kIEditorIID,           NS_IEDITOR_IID);
static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kEditorCID,            NS_EDITOR_CID);
// transaction manager
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);
static NS_DEFINE_CID(kCTransactionManagerFactoryCID, NS_TRANSACTION_MANAGER_FACTORY_CID);
// transactions
static NS_DEFINE_IID(kEditAggregateTxnIID,  EDIT_AGGREGATE_TXN_IID);
static NS_DEFINE_IID(kInsertTextTxnIID,     INSERT_TEXT_TXN_IID);
static NS_DEFINE_IID(kDeleteTextTxnIID,     DELETE_TEXT_TXN_IID);
static NS_DEFINE_IID(kCreateElementTxnIID,  CREATE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteElementTxnIID,  DELETE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteRangeTxnIID,    DELETE_RANGE_TXN_IID);
static NS_DEFINE_IID(kChangeAttributeTxnIID,CHANGE_ATTRIBUTE_TXN_IID);
static NS_DEFINE_IID(kSplitElementTxnIID,   SPLIT_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kJoinElementTxnIID,    JOIN_ELEMENT_TXN_IID);


#ifdef XP_PC
#define TRANSACTION_MANAGER_DLL "txmgr.dll"
#else
#ifdef XP_MAC
#define TRANSACTION_MANAGER_DLL "TRANSACTION_MANAGER_DLL"
#else // XP_UNIX
#define TRANSACTION_MANAGER_DLL "libtxmgr.so"
#endif
#endif


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



/*
we must be good providers of factories ect. this is where to put ALL editor exports
*/
//BEGIN EXPORTS
extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aClass, nsISupports* serviceMgr, nsIFactory
**aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = nsnull;

  if (aClass.Equals(kEditorCID)) {
    return getEditFactory(aFactory);
  }
  return NS_NOINTERFACE;
}



extern "C" NS_EXPORT PRBool
NSCanUnload(void)
{
  return nsEditor::gInstanceCount; //I have no idea. I am copying code here
}



extern "C" NS_EXPORT nsresult 
NSRegisterSelf(const char *path)
{
  return nsRepository::RegisterFactory(kIEditFactoryIID, path, 
                                       PR_TRUE, PR_TRUE); //this will register the factory with the xpcom dll.
}

extern "C" NS_EXPORT nsresult 
NSUnregisterSelf(const char *path)
{
  return nsRepository::UnregisterFactory(kIEditFactoryIID, path);//this will unregister the factory with the xpcom dll.
}

//END EXPORTS


//class implementations are in order they are declared in editor.h

static PRBool needsInit=PR_TRUE;
nsEditor::nsEditor()
{
  //initialize member variables here
  NS_INIT_REFCNT();
  PR_EnterMonitor(getEditorMonitor());
  gInstanceCount++;

  if (PR_TRUE==needsInit)
  {
    needsInit=PR_FALSE;
    nsRepository::RegisterFactory(kCTransactionManagerFactoryCID,
                                TRANSACTION_MANAGER_DLL, PR_FALSE, PR_FALSE);
  }
  mTxnMgr = nsnull;
  PR_ExitMonitor(getEditorMonitor());
}



nsEditor::~nsEditor()
{
  NS_IF_RELEASE(mPresShell);
  NS_IF_RELEASE(mViewManager);
  NS_IF_RELEASE(mTxnMgr);
  
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  nsCOMPtr<nsIDOMEventReceiver> erP;
  nsresult t_result = mDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
  if (NS_SUCCEEDED( t_result )) 
  {
    erP->RemoveEventListener(mKeyListenerP, kIDOMKeyListenerIID);
    //erP->RemoveEventListener(mMouseListenerP, kIDOMMouseListenerIID);
  }
  else
    NS_NOTREACHED("!nsEditor");
}



//BEGIN nsIEditor interface implementations


NS_IMPL_ADDREF(nsEditor)

NS_IMPL_RELEASE(nsEditor)



nsresult
nsEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEditorIID)) {
    *aInstancePtr = (void*)(nsIEditor*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult 
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  *aDoc = nsnull; // init out param
  NS_PRECONDITION(mDoc, "bad state, null mDoc");
  if (!mDoc)
    return NS_ERROR_NULL_POINTER;
  return mDoc->QueryInterface(kIDOMDocumentIID, (void **)aDoc);
}



nsresult
nsEditor::Init(nsIDOMDocument *aDoc, nsIPresShell* aPresShell)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  if ((nsnull==aDoc) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

  mDoc = aDoc;
  mPresShell = aPresShell;
  NS_ADDREF(mPresShell);
  mViewManager = mPresShell->GetViewManager();
  mUpdateCount=0;

  nsresult t_result = NS_NewEditorKeyListener(getter_AddRefs(mKeyListenerP), this);
  if (NS_OK != t_result)
  {
//    NS_NOTREACHED("Init Failed");
    return t_result;
  }
  t_result = NS_NewEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
  if (NS_OK != t_result)
  {
    mKeyListenerP = 0; //dont keep the key listener if the mouse listener fails.
//    NS_NOTREACHED("Mouse Listener");
    return t_result;
  }
  nsCOMPtr<nsIDOMEventReceiver> erP;
  t_result = mDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
  if (NS_OK != t_result) 
  {
    mKeyListenerP = 0;
    mMouseListenerP = 0; //dont need these if we cant register them
//    NS_NOTREACHED("query interface");
    return t_result;
  }
  erP->AddEventListener(mKeyListenerP, kIDOMKeyListenerIID);
  //erP->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);

  /* fire up the transaction manager */

  NS_POSTCONDITION(mDoc && mPresShell, "bad state");
  
  return NS_OK;
}

nsresult
nsEditor::EnableUndo(PRBool aEnable)
{
  nsISupports  *isup = 0;
  nsresult result=NS_OK;

  if (PR_TRUE==aEnable)
  {
    if (!mTxnMgr)
    {
      result = nsServiceManager::GetService(kCTransactionManagerFactoryCID,
                                            kITransactionManagerIID, &isup);
      if (NS_FAILED(result) || !isup) {
        printf("ERROR: Failed to get TransactionManager nsISupports interface.\n");
        return NS_ERROR_NOT_AVAILABLE;
      }
      result = isup->QueryInterface(kITransactionManagerIID, (void **)&mTxnMgr);
      if (NS_FAILED(result) || !mTxnMgr) {
        printf("ERROR: Failed to get TransactionManager interface. (%d)\n", result);
        return result;
      }
    }
  }
  else
  { // disable the transaction manager if it is enabled
    if (mTxnMgr)
    {
      mTxnMgr = nsnull;
    }
  }

  return result;
}

nsresult nsEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  aIsEnabled = ((PRBool)(mTxnMgr));
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

nsresult nsEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  aIsEnabled = ((PRBool)(mTxnMgr));
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

nsresult
nsEditor::SetProperties(Properties aProperties)
{
  return NS_OK;
}



nsresult
nsEditor::GetProperties(Properties **aProperties)
{
  return NS_OK;
}


nsresult 
nsEditor::SetAttribute(nsIDOMElement *aElement, const nsString& aAttribute, const nsString& aValue)
{
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForSetAttribute(aElement, aAttribute, aValue, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}


nsresult 
nsEditor::CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                   const nsString& aAttribute, 
                                   const nsString& aValue,
                                   ChangeAttributeTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(kChangeAttributeTxnIID, (EditTxn **)aTxn);
    if (nsnull!=*aTxn)
    {
      result = (*aTxn)->Init(this, aElement, aAttribute, aValue, PR_FALSE);
    }
  }
  return result;
}

nsresult 
nsEditor::GetAttributeValue(nsIDOMElement *aElement, 
                            const nsString& aAttribute, 
                            nsString& aResultValue, 
                            PRBool&   aResultIsSet)
{
  aResultIsSet=PR_FALSE;
  nsresult result=NS_OK;
  if (nsnull!=aElement)
  {
    nsIDOMAttr* attNode=nsnull;
    result = aElement->GetAttributeNode(aAttribute, &attNode);
    if ((NS_SUCCEEDED(result)) && (nsnull!=attNode))
    {
      attNode->GetSpecified(&aResultIsSet);
      attNode->GetValue(aResultValue);
    }
  }
  return result;
}

nsresult 
nsEditor::RemoveAttribute(nsIDOMElement *aElement, const nsString& aAttribute)
{
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForRemoveAttribute(aElement, aAttribute, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}

nsresult 
nsEditor::CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                      const nsString& aAttribute,
                                      ChangeAttributeTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(kChangeAttributeTxnIID, (EditTxn **)aTxn);
    if (nsnull!=*aTxn)
    {
      nsAutoString value;
      result = (*aTxn)->Init(this, aElement, aAttribute, value, PR_TRUE);
    }
  }
  return result;
}

nsresult
nsEditor::Commit(PRBool aCtrlKey)
{
  if (aCtrlKey)
  {
//    nsSelectionRange range;
    //mSelectionP->GetRange(&range);
  }
  else
  {
  }
  return NS_OK;
}
 

//END nsIEditorInterfaces


//BEGIN nsEditor Private methods

nsresult
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
    node=aStartNode;

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
    answer = aNode;
  }

    // OK, now return the answer, if any
  *aRetNode = answer;
  if (*aRetNode)
    NS_IF_ADDREF(*aRetNode);
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult 
nsEditor::Do(nsITransaction *aTxn)
{
  nsresult result = NS_OK;
  if (nsnull!=aTxn)
  {
    if (nsnull!=mTxnMgr)
    {
      result = mTxnMgr->Do(aTxn);
    }
    else
    {
      result = aTxn->Do();
    }
  }
  return result;
}

nsresult 
nsEditor::Undo(PRUint32 aCount)
{
  nsresult result = NS_OK;
  if (nsnull!=mTxnMgr)
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->Undo();
      if (NS_FAILED(result))
        break;
    }
  }
  return result;
}

nsresult 
nsEditor::Redo(PRUint32 aCount)
{
  nsresult result = NS_OK;
  if (nsnull!=mTxnMgr)
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->Redo();
      if (NS_FAILED(result))
        break;
    }
  }
  return result;
}

nsresult 
nsEditor::BeginUpdate()
{
  NS_PRECONDITION(mUpdateCount>=0, "bad state");
  if (nsnull!=mViewManager)
  {
    if (0==mUpdateCount)
      mViewManager->DisableRefresh();
    mUpdateCount++;
  }
  return NS_OK;
}

nsresult 
nsEditor::EndUpdate()
{
  NS_PRECONDITION(mUpdateCount>0, "bad state");
  if (nsnull!=mViewManager)
  {
    mUpdateCount--;
    if (0==mUpdateCount)
      mViewManager->EnableRefresh();
  }  
  return NS_OK;
}


nsresult nsEditor::CreateElement(const nsString& aTag,
                                 nsIDOMNode *    aParent,
                                 PRInt32         aPosition)
{
  CreateElementTxn *txn;
  nsresult result = CreateTxnForCreateElement(aTag, aParent, aPosition, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}

nsresult nsEditor::CreateTxnForCreateElement(const nsString& aTag,
                                             nsIDOMNode     *aParent,
                                             PRInt32         aPosition,
                                             CreateElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aParent)
  {
    result = TransactionFactory::GetNewTransaction(kCreateElementTxnIID, (EditTxn **)aTxn);
    if (nsnull != *aTxn)
    {
      result = (*aTxn)->Init(mDoc, aTag, aParent, aPosition);
    }
    else
      result = NS_ERROR_OUT_OF_MEMORY;
  }
  return result;
}

nsresult nsEditor::DeleteElement(nsIDOMNode * aParent,
                                 nsIDOMNode * aElement)
{
  DeleteElementTxn *txn;
  nsresult result = CreateTxnForDeleteElement(aParent, aElement, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}

nsresult nsEditor::CreateTxnForDeleteElement(nsIDOMNode * aParent,
                                             nsIDOMNode * aElement,
                                             DeleteElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if ((nsnull != aParent) && (nsnull != aElement))
  {
    result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)aTxn);
    if (nsnull!=*aTxn)
    {
      result = (*aTxn)->Init(aElement, aParent);
    }
    else
      result = NS_ERROR_OUT_OF_MEMORY;
  }

  return result;
}

nsresult 
nsEditor::InsertText(const nsString& aStringToInsert)
{
  nsresult result;
  nsCOMPtr<nsIDOMSelection> selection;
  result = mPresShell->GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    PRBool collapsed;
    result = selection->IsCollapsed(&collapsed);
    if (NS_SUCCEEDED(result) && !collapsed)
      DeleteSelection(eLTR);
  }

  InsertTextTxn *txn;
  result = CreateTxnForInsertText(aStringToInsert, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}

nsresult nsEditor::CreateTxnForInsertText(const nsString & aStringToInsert,
                                          InsertTextTxn ** aTxn)
{
  nsresult result;
  nsCOMPtr<nsIDOMSelection> selection;
  result = mPresShell->GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = selection;
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      result = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(result)) && (nsnull!=currentItem))
      {
        // XXX: we'll want to deleteRange if the selection isn't just an insertion point
        // for now, just insert text after the start of the first node
        nsCOMPtr<nsIDOMRange> range(currentItem);
        if (range)
        {
          nsCOMPtr<nsIDOMNode> node;
          result = range->GetStartParent(getter_AddRefs(node));
          if ((NS_SUCCEEDED(result)) && (node))
          {
            nsCOMPtr<nsIDOMCharacterData> nodeAsText(node);
            if (nodeAsText)
            {
              PRInt32 offset;
              range->GetStartOffset(&offset);
              result = TransactionFactory::GetNewTransaction(kInsertTextTxnIID, (EditTxn **)aTxn);
              if (nsnull!=*aTxn)
              {
                result = (*aTxn)->Init(nodeAsText, offset, aStringToInsert,
                                       mPresShell);
              }
              else
                result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
      }
    }
  }
  else
    result = NS_ERROR_NULL_POINTER;

  return result;
}

nsresult nsEditor::DeleteText(nsIDOMCharacterData *aElement,
                              PRUint32             aOffset,
                              PRUint32             aLength)
{
  DeleteTextTxn *txn;
  nsresult result = CreateTxnForDeleteText(aElement, aOffset, aLength, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  return result;
}


nsresult nsEditor::CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                          PRUint32             aOffset,
                                          PRUint32             aLength,
                                          DeleteTextTxn      **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(kDeleteTextTxnIID, (EditTxn **)aTxn);
    if (nsnull!=*aTxn)
    {
      result = (*aTxn)->Init(aElement, aOffset, aLength);
    }
    else
      result = NS_ERROR_OUT_OF_MEMORY;
  }
  return result;
}

// operates on selection
// deletes the selection (if not collapsed) and splits the current element
// or an ancestor of the current element
/* context-dependent behaviors
   SELECTION       ACTION
   text node       split text node (or a parent)
   block element   insert a <BR>
   H1-H6           if at end, create a <P> following the header
*/
#if 0   // THIS CODE WILL BE REMOVED.  WE ARE GOING TO IMPLEMENT
        // A GENERIC HANDLER SYSTEM.
nsresult nsEditor::CreateTxnToHandleEnterKey(EditAggregateTxn **aTxn)
{
  // allocate the out-param transaction
  nsresult result = TransactionFactory::GetNewTransaction(kEditAggregateTxnIID, (EditTxn **)aTxn);
  if (NS_FAILED(result))
  {
    return result;
  }
  nsCOMPtr<nsIDOMSelection> selection;
  result = mPresShell->GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = selection;
    if (enumerator)
    {
      // XXX: need to handle mutliple ranges here, probably by
      // disallowing the operation
      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsISupports *currentItem=nsnull;
        result = enumerator->CurrentItem(&currentItem);
        if ((NS_SUCCEEDED(result)) && (currentItem))
        {
          nsCOMPtr<nsIDOMRange> range(currentItem);
          PRBool isCollapsed;
          range->GetIsCollapsed(&isCollapsed);
          if(PR_FALSE==isCollapsed)
          {
            EditAggregateTxn *delSelTxn;
            result = CreateTxnForDeleteSelection(nsIEditor::eLTR, &delSelTxn);
            if ((NS_SUCCEEDED(result)) && (delSelTtxn)) {
              (*aTxn)->AppendChild(delSelTtxn); 
            }
          }
          // at this point, we have a collapsed selection
          if (NS_SUCCEEDED(result))
          {
            nsCOMPtr<nsIDOMNode> node;
            result = range->GetStartParent(getter_AddRefs(node));
            if (NS_SUCCEEDED(result))
            {
              nsVoidArray parentFrameList;
              result = GetParentFrameList(node, &parentFrameList);
              if (NS_SUCCEEDED(result))
              {
                EditAggregateTxn *enterKeyTxn;
                result = CreateTxnForEnterKeyAtInsertionPoint(range, &enterKeyTxn));
                if ((NS_SUCCEEDED(result)) && (enterKeyTxn)) {
                  (*aTxn)->AppendChild(enterKeyTxn); 
                }
              }
            }
          }
        }
      }
    }
  }

  // if anything along the way failed, delete the out-param transaction
  if (NS_FAILED(result)) {
    NS_IF_RELEASE(*aTxn);
  }
  return result;
}
#endif

nsresult 
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

nsresult nsEditor::CreateTxnForDeleteSelection(nsIEditor::Direction aDir, 
                                               EditAggregateTxn  ** aTxn)
{
  nsresult result;
  // allocate the out-param transaction
  result = TransactionFactory::GetNewTransaction(kEditAggregateTxnIID, (EditTxn **)aTxn);
  if (NS_FAILED(result))
  {
    return result;
  }
  nsCOMPtr<nsIDOMSelection> selection;
  result = mPresShell->GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = selection;
    if (enumerator)
    {
      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsISupports *currentItem=nsnull;
        result = enumerator->CurrentItem(&currentItem);
        if ((NS_SUCCEEDED(result)) && (currentItem))
        {
          nsCOMPtr<nsIDOMRange> range(currentItem);
          PRBool isCollapsed;
          range->GetIsCollapsed(&isCollapsed);
          if (PR_FALSE==isCollapsed)
          {
            DeleteRangeTxn *txn;
            result = TransactionFactory::GetNewTransaction(kDeleteRangeTxnIID, (EditTxn **)&txn);
            if ((NS_SUCCEEDED(result)) && (nsnull!=txn))
            {
              txn->Init(range);
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
  else
    result = NS_ERROR_NULL_POINTER;

  // if we didn't build the transaction correctly, destroy the out-param transaction so we don't leak it.
  if (NS_FAILED(result))
  {
    NS_IF_RELEASE(*aTxn);
  }

  return result;
}


//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior are not implemented
nsresult
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
  aRange->GetStartOffset(&offset);

  // determine if the insertion point is at the beginning, middle, or end of the node
  nsCOMPtr<nsIDOMCharacterData> nodeAsText(node);
  if (nodeAsText)
  {
    PRUint32 count;
    nodeAsText->GetLength(&count);
    isFirst = PRBool(0==offset);
    isLast  = PRBool(count==offset);
  }
  else
  {
    nsCOMPtr<nsIDOMNode> sibling;
    result = node->GetPreviousSibling(getter_AddRefs(sibling));
    if ((NS_SUCCEEDED(result)) && sibling)
      isFirst = PR_FALSE;
    else
      isFirst = PR_TRUE;
    result = node->GetNextSibling(getter_AddRefs(sibling));
    if ((NS_SUCCEEDED(result)) && sibling)
      isLast = PR_FALSE;
    else
      isLast = PR_TRUE;
  }

  // build a transaction for deleting the appropriate data
  if ((nsIEditor::eRTL==aDir) && (PR_TRUE==isFirst))
  { // we're backspacing from the beginning of the node.  Delete the first thing to our left
    nsCOMPtr<nsIDOMNode> priorNode;
    result = GetPriorNode(node, getter_AddRefs(priorNode));
    if ((NS_SUCCEEDED(result)) && priorNode)
    { // there is a priorNode, so delete it's last child (if text content, delete the last char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> priorNodeAsText(priorNode);
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
        nsCOMPtr<nsIDOMNode> parent;
        result = priorNode->GetParentNode(getter_AddRefs(parent));
        if ((NS_SUCCEEDED(result)) && parent)
        {
          DeleteElementTxn *txn;
          result = CreateTxnForDeleteElement(parent, priorNode, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
          }
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
      nsCOMPtr<nsIDOMCharacterData> nextNodeAsText(nextNode);
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
        nsCOMPtr<nsIDOMNode> parent;
        result = nextNode->GetParentNode(getter_AddRefs(parent));
        if ((NS_SUCCEEDED(result)) && parent)
        {
          DeleteElementTxn *txn;
          result = CreateTxnForDeleteElement(parent, nextNode, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
          }
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
    }
  }
  return result;
}

nsresult 
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

  nsCOMPtr<nsIDOMNode> parent(aCurrentNode);
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

nsresult 
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

  nsCOMPtr<nsIDOMNode> parent(aCurrentNode);
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

nsresult
nsEditor::GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> resultNode(aCurrentNode);
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
  nsCOMPtr<nsIDOMNode> resultNode(aCurrentNode);
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

nsresult nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if (nsnull != aNode)
  {
    result = TransactionFactory::GetNewTransaction(kSplitElementTxnIID, (EditTxn **)aTxn);
    if (nsnull!=*aTxn)
    {
      result = (*aTxn)->Init(this, aNode, aOffset);
    }
    else
      result = NS_ERROR_OUT_OF_MEMORY;
  }
  return result;
}

nsresult nsEditor::CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                        nsIDOMNode  *aRightNode,
                                        JoinElementTxn **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull != aLeftNode) && (nsnull != aRightNode))
  {
    result = TransactionFactory::GetNewTransaction(kJoinElementTxnIID, (EditTxn **)aTxn);
    if (nsnull!=*aTxn)
    {
      result = (*aTxn)->Init(this, aLeftNode, aRightNode);
    }
    else
      result = NS_ERROR_OUT_OF_MEMORY;
  }
  return result;
}

nsresult 
nsEditor::SplitNode(nsIDOMNode * aExistingRightNode,
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
  else
    result = NS_ERROR_INVALID_ARG;

  return result;
}

nsresult
nsEditor::JoinNodes(nsIDOMNode * aNodeToKeep,
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
      if (NS_SUCCEEDED(result))
      { // delete the extra node
        result = aParent->RemoveChild(aNodeToJoin, getter_AddRefs(resultNode));
      }
    }
  }
  else
    result = NS_ERROR_INVALID_ARG;

  return result;
}

//END nsEditor Private methods



