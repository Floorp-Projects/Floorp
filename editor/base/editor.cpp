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


#include "editor.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsRepository.h"
#include "nsEditFactory.h"



static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIEditFactoryIID, NS_IEDITORFACTORY_IID);
static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


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
extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aClass, nsIFactory
**aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = nsnull;

  if (aClass.Equals(kIEditFactoryIID)) {
    return getEditFactory(aFactory);
  }
  return NS_NOINTERFACE;
}



extern "C" NS_EXPORT PRBool
NSCanUnload(void)
{
    return PR_FALSE; //I have no idea. I am copying code here
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


nsEditor::nsEditor()
{
  //initialize member variables here
}



nsEditor::~nsEditor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  COM_auto_ptr<nsIDOMEventReceiver> erP;
  nsresult t_result = mDomInterfaceP->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
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
nsEditor::GetDomInterface(nsIDOMDocument **aDomInterface)
{
  *aDomInterface = mDomInterfaceP; return NS_OK;
}



nsresult
nsEditor::Init(nsIDOMDocument *aDomInterface)
{
  if (!aDomInterface)
    return NS_ERROR_NULL_POINTER;

  mDomInterfaceP = aDomInterface;

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
  COM_auto_ptr<nsIDOMEventReceiver> erP;
  t_result = mDomInterfaceP->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
  if (NS_OK != t_result) 
  {
    mKeyListenerP = 0;
    mMouseListenerP = 0; //dont need these if we cant register them
//    NS_NOTREACHED("query interface");
    return t_result;
  }
  erP->AddEventListener(mKeyListenerP, kIDOMKeyListenerIID);
  //erP->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);

  /*
  now to handle selection
  */
  /*
  COM_auto_ptr<nsIDocument> document;
  if (NS_SUCCEEDED(t_result = mDomInterfaceP->QueryInterface(kIDocumentIID, getter_AddRefs(document))))
  {
    if (!NS_SUCCEEDED(t_result = document->GetSelection(*getter_AddRefs(mSelectionP))))
    {
      NS_NOTREACHED("query interface");
      return t_result;
    }
  }
  else
  {
    NS_NOTREACHED("query interface");
    return t_result;
  }
*/
  
  return NS_OK;
}



nsresult
nsEditor::InsertString(nsString *aString)
{
  return AppendText(aString);
}



nsresult
nsEditor::SetProperties(PROPERTIES aProperty)
{
  return NS_OK;
}



nsresult
nsEditor::GetProperties(PROPERTIES **)
{
  return NS_OK;
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


//BEGIN nsEditor Calls from public
PRBool
nsEditor::KeyDown(int aKeycode)
{
  return PR_TRUE;
}



PRBool
nsEditor::MouseClick(int aX,int aY)
{
  return PR_FALSE;
}



//END nsEditor Calls from public



//BEGIN nsEditor Private methods



nsresult
nsEditor::AppendText(nsString *aStr)
{
  COM_auto_ptr<nsIDOMNode> currentNode;
  COM_auto_ptr<nsIDOMNode> textNode;
  COM_auto_ptr<nsIDOMText> text;
  if (!aStr)
    return NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(GetCurrentNode(getter_AddRefs(currentNode))) && 
      NS_SUCCEEDED(GetFirstTextNode(currentNode,getter_AddRefs(textNode))) && 
      NS_SUCCEEDED(textNode->QueryInterface(kIDOMTextIID, getter_AddRefs(text)))) {
    text->AppendData(*aStr);
  }

  return NS_OK;
}



nsresult
nsEditor::GetCurrentNode(nsIDOMNode ** aNode)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;
  /* If no node set, get first text node */
  COM_auto_ptr<nsIDOMElement> docNode;

  if (NS_SUCCEEDED(mDomInterfaceP->GetDocumentElement(getter_AddRefs(docNode))))
  {
    return docNode->QueryInterface(kIDOMNodeIID,(void **) aNode);
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

  COM_auto_ptr<nsIDOMNode> answer;
  
  aNode->GetNodeType(&mType);

  if (nsIDOMNode::ELEMENT_NODE == mType) {
    if (NS_SUCCEEDED(aNode->HasChildNodes(&mCNodes)) && PR_TRUE == mCNodes) 
    {
      COM_auto_ptr<nsIDOMNode> node1;
      COM_auto_ptr<nsIDOMNode> node2;

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

//END nsEditor Private methods



