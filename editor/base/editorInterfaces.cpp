/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "editorInterfaces.h"
#include "editor.h"

#include "CreateElementTxn.h"

#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"

#include "nsString.h"

static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMCharacterDataIID, NS_IDOMCHARACTERDATA_IID);

 
/*
 * nsEditorKeyListener implementation
 */


NS_IMPL_ADDREF(nsEditorKeyListener)

NS_IMPL_RELEASE(nsEditorKeyListener)


nsEditorKeyListener::nsEditorKeyListener() 
{
  NS_INIT_REFCNT();
}



nsEditorKeyListener::~nsEditorKeyListener() 
{
}



nsresult
nsEditorKeyListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMKeyListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult
nsEditorKeyListener::ProcessEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}



nsresult
nsEditorKeyListener::GetCharFromKeyCode(PRUint32 aKeyCode, PRBool aIsShift, char *aChar)
{
  /* This is completely temporary to get this working while I check out Unicode conversion code. */
#ifdef XP_MAC
  if (aChar) {
    *aChar = (char)aKeyCode;
    return NS_OK;
    }
#else
  if (aKeyCode >= 0x41 && aKeyCode <= 0x5A) {
    if (aIsShift) {
      *aChar = (char)aKeyCode;
    }
    else {
      *aChar = (char)(aKeyCode + 0x20);
    }
    return NS_OK;
  }
  else if ((aKeyCode >= 0x30 && aKeyCode <= 0x39) || aKeyCode == 0x20) {
      *aChar = (char)aKeyCode;
      return NS_OK;
  }
#endif
  return NS_ERROR_FAILURE;
}



nsresult
nsEditorKeyListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRUint32 keyCode;
  PRBool isShift;
  PRBool ctrlKey;
  char character;
  
  if (NS_SUCCEEDED(aKeyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(aKeyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(aKeyEvent->GetCtrlKey(&ctrlKey))
      ) {
    PRBool keyProcessed;
    ProcessShortCutKeys(aKeyEvent, keyProcessed);
    if (PR_FALSE==keyProcessed)
    {
      switch(keyCode) {
      case nsIDOMEvent::VK_BACK:
        mEditor->DeleteSelection(nsIEditor::eRTL);
        break;

      case nsIDOMEvent::VK_DELETE:
        mEditor->DeleteSelection(nsIEditor::eLTR);
        break;

      case nsIDOMEvent::VK_RETURN:
        // Need to implement creation of either <P> or <BR> nodes.
        mEditor->Commit(ctrlKey);
        break;
      default:
        {
          // XXX Replace with x-platform NS-virtkeycode transform.
          if (NS_OK == GetCharFromKeyCode(keyCode, isShift, & character)) {
            nsAutoString key;
            key += character;
            if (!isShift) {
              key.ToLowerCase();
            }
            mEditor->InsertText(key);
          }
        }
        break;
      }
    }
  }
  
  return NS_ERROR_BASE;
}



nsresult
nsEditorKeyListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}



nsresult
nsEditorKeyListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

/* these includes are for debug only.  this module should never instantiate it's own transactions */
#include "SplitElementTxn.h"
#include "TransactionFactory.h"
static NS_DEFINE_IID(kSplitElementTxnIID,   SPLIT_ELEMENT_TXN_IID);
nsresult
nsEditorKeyListener::ProcessShortCutKeys(nsIDOMEvent* aKeyEvent, PRBool& aProcessed)
{
  aProcessed=PR_FALSE;
  PRUint32 keyCode;
  PRBool isShift;
  PRBool ctrlKey;
  
  if (NS_SUCCEEDED(aKeyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(aKeyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(aKeyEvent->GetCtrlKey(&ctrlKey))
      ) 
  {
    // XXX: please please please get these mappings from an external source!
    switch (keyCode)
    {
      // XXX: hard-coded undo
      case nsIDOMEvent::VK_Z:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (nsnull!=mEditor)
            mEditor->Undo(1);
        }
        break;

      // XXX: hard-coded redo
      case nsIDOMEvent::VK_Y:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (nsnull!=mEditor)
            mEditor->Redo(1);
        }
        break;

      // hard-coded split node test:  works on first <P> in the document
      case nsIDOMEvent::VK_S:
        if (PR_TRUE==ctrlKey)
        {
          nsAutoString pTag("P");
          nsCOMPtr<nsIDOMNode> currentNode;
          nsCOMPtr<nsIDOMElement> element;
          if (NS_SUCCEEDED(mEditor->GetFirstNodeOfType(nsnull, pTag, getter_AddRefs(currentNode))))
          {
            nsresult result;
            SplitElementTxn *txn;
            if (PR_FALSE==isShift)   // split the element so there are 0 children in the first half
            {
              result = TransactionFactory::GetNewTransaction(kSplitElementTxnIID, (EditTxn **)&txn);
              if (txn)
                txn->Init(mEditor, currentNode, -1);
            }
            else                    // split the element so there are 2 children in the first half
            {
              result = TransactionFactory::GetNewTransaction(kSplitElementTxnIID, (EditTxn **)&txn);
              if (txn)
               txn->Init(mEditor, currentNode, 1);
            }
            if (txn)
              mEditor->Do(txn);        
          }
          aProcessed=PR_TRUE;
        }
        break;


      //XXX: test for change and remove attribute, hard-coded to be width on first table in doc
      case nsIDOMEvent::VK_TAB:
        {
          //XXX: should be from a factory
          //XXX: should manage the refcount of txn
          nsAutoString attribute("width");
          nsAutoString value("400");

          nsAutoString tableTag("TABLE");
          nsCOMPtr<nsIDOMNode> currentNode;
          nsCOMPtr<nsIDOMElement> element;
          if (NS_SUCCEEDED(mEditor->GetFirstNodeOfType(nsnull, tableTag, getter_AddRefs(currentNode))))
          {
            if (NS_SUCCEEDED(currentNode->QueryInterface(kIDOMElementIID, getter_AddRefs(element)))) 
            {
              nsresult result;
              if (PR_TRUE==ctrlKey)   // remove the attribute
                result = mEditor->RemoveAttribute(element, attribute);
              else                    // change the attribute
                result = mEditor->SetAttribute(element, attribute, value);
            }
          }
        }
        aProcessed=PR_TRUE;
        break;

      case nsIDOMEvent::VK_INSERT:
        {
          nsresult result;
          //XXX: should be from a factory
          //XXX: should manage the refcount of txn
          nsAutoString attribute("src");
          nsAutoString value("resource:/res/samples/raptor.jpg");

          nsAutoString imgTag("HR");
          nsAutoString bodyTag("BODY");
          nsCOMPtr<nsIDOMNode> currentNode;
          result = mEditor->GetFirstNodeOfType(nsnull, bodyTag, getter_AddRefs(currentNode));
          if (NS_SUCCEEDED(result))
          {
            PRInt32 position;
            if (PR_TRUE==ctrlKey)
              position=CreateElementTxn::eAppend;
            else
              position=0;
            result = mEditor->CreateElement(imgTag, currentNode, position);
          }
/*
          //for building a composite transaction... 
          nsCOMPtr<nsIDOMElement> element;
          if (NS_SUCCEEDED(mEditor->GetFirstNodeOfType(nsnull, imgTag, getter_AddRefs(currentNode))))
          {
            if (NS_SUCCEEDED(currentNode->QueryInterface(kIDOMElementIID, getter_AddRefs(element)))) 
            {
              ChangeAttributeTxn *txn;
              txn = new ChangeAttributeTxn(mEditor, element, attribute, value, PR_FALSE);
              mEditor->Do(txn);        
            }
          }
*/
        }
        aProcessed=PR_TRUE;
        break;

    }
  }
  return NS_OK;
}



/*
 * nsEditorMouseListener implementation
 */



NS_IMPL_ADDREF(nsEditorMouseListener)

NS_IMPL_RELEASE(nsEditorMouseListener)


nsEditorMouseListener::nsEditorMouseListener() 
{
  NS_INIT_REFCNT();
}



nsEditorMouseListener::~nsEditorMouseListener() 
{
}



nsresult
nsEditorMouseListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult
nsEditorMouseListener::ProcessEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}



nsresult
nsEditorMouseListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMNode> target;
  if (NS_OK == aMouseEvent->GetTarget(getter_AddRefs(target))) {
//    nsSetCurrentNode(aTarget);
  }
  //Should not be error.  Need a new way to do return values
  return NS_ERROR_BASE;
}



nsresult
nsEditorMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsEditorMouseListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsEditorMouseListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsEditorMouseListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsEditorMouseListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



/*
 * Factory functions
 */



nsresult 
NS_NewEditorKeyListener(nsIDOMEventListener ** aInstancePtrResult, nsEditor *aEditor)
{
  nsEditorKeyListener* it = new nsEditorKeyListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}



nsresult
NS_NewEditorMouseListener(nsIDOMEventListener ** aInstancePtrResult, nsEditor *aEditor)
{
  nsEditorMouseListener* it = new nsEditorMouseListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}


