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
#include "SplitElementTxn.h"

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
        {
          // XXX: for now, just grab the first text node
          nsCOMPtr<nsIDOMNode> currentNode;
          nsCOMPtr<nsIDOMNode> textNode;
          nsCOMPtr<nsIDOMCharacterData> text;
          if (NS_SUCCEEDED(mEditor->GetCurrentNode(getter_AddRefs(currentNode))) && 
              NS_SUCCEEDED(mEditor->GetFirstTextNode(currentNode,getter_AddRefs(textNode))) && 
              NS_SUCCEEDED(textNode->QueryInterface(kIDOMCharacterDataIID, getter_AddRefs(text)))) 
          {
            // XXX: for now, just append the text
            PRUint32 offset;
            text->GetLength(&offset);
            nsresult result = mEditor->DeleteText(text, offset-1, 1);
          }
        }
        break;

      case nsIDOMEvent::VK_DELETE:
        {
          // XXX: for now, just grab the first text node
          nsCOMPtr<nsIDOMNode> currentNode;
          nsCOMPtr<nsIDOMNode> textNode;
          nsCOMPtr<nsIDOMCharacterData> text;
          if (NS_SUCCEEDED(mEditor->GetCurrentNode(getter_AddRefs(currentNode))) && 
              NS_SUCCEEDED(mEditor->GetFirstTextNode(currentNode,getter_AddRefs(textNode))) && 
              NS_SUCCEEDED(textNode->QueryInterface(kIDOMCharacterDataIID, getter_AddRefs(text)))) 
          {
            nsresult result = mEditor->DeleteText(text, 0, 1);
          }
        }
        break;

      case nsIDOMEvent::VK_RETURN:
        // Need to implement creation of either <P> or <BR> nodes.
        mEditor->Commit(ctrlKey);
        break;
      default:
        {
          // XXX Replace with x-platform NS-virtkeycode transform.
          if (NS_OK == GetCharFromKeyCode(keyCode, isShift, & character)) {
            nsString key;
            key += character;
            if (!isShift) {
              key.ToLowerCase();
            }

            // XXX: for now, just grab the first text node
            nsCOMPtr<nsIDOMNode> currentNode;
            nsCOMPtr<nsIDOMNode> textNode;
            nsCOMPtr<nsIDOMCharacterData> text;
            if (NS_SUCCEEDED(mEditor->GetCurrentNode(getter_AddRefs(currentNode))) && 
                NS_SUCCEEDED(mEditor->GetFirstTextNode(currentNode,getter_AddRefs(textNode))) && 
                NS_SUCCEEDED(textNode->QueryInterface(kIDOMCharacterDataIID, getter_AddRefs(text)))) 
            {
              // XXX: for now, just append the text
              PRUint32 offset;
              text->GetLength(&offset);
              mEditor->InsertText(text, offset, key);
            }
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
          if (nsnull!=mEditor)
            mEditor->Undo();
        }
        aProcessed=PR_TRUE;
        break;

      // XXX: hard-coded redo
      case nsIDOMEvent::VK_Y:
        if (PR_TRUE==ctrlKey)
        {
          if (nsnull!=mEditor)
            mEditor->Redo();
        }
        aProcessed=PR_TRUE;
        break;

      // hard-coded split node test:  works on first <P> in the document
      case nsIDOMEvent::VK_S:
        {
          nsString pTag("P");
          nsCOMPtr<nsIDOMNode> currentNode;
          nsCOMPtr<nsIDOMElement> element;
          if (NS_SUCCEEDED(mEditor->GetFirstNodeOfType(nsnull, pTag, getter_AddRefs(currentNode))))
          {
            SplitElementTxn *txn;
            if (PR_FALSE==isShift)   // split the element so there are 0 children in the first half
              txn = new SplitElementTxn(mEditor, currentNode, -1);
            else                    // split the element so there are 2 children in the first half
             txn = new SplitElementTxn(mEditor, currentNode, 1);
            mEditor->ExecuteTransaction(txn);        
          }
        }
        aProcessed=PR_TRUE;
        break;


      //XXX: test for change and remove attribute, hard-coded to be width on first table in doc
      case nsIDOMEvent::VK_TAB:
        {
          //XXX: should be from a factory
          //XXX: should manage the refcount of txn
          nsString attribute("width");
          nsString value("400");

          nsString tableTag("TABLE");
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
          nsString attribute("src");
          nsString value("resource:/res/samples/raptor.jpg");

          nsString imgTag("HR");
          nsString bodyTag("BODY");
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
              mEditor->ExecuteTransaction(txn);        
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
  mEditor->MouseClick(0,0);
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

