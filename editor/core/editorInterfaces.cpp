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
#include "nsString.h"
#include "editor.h"
#include "nsIDOMDocument.h"
 
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
    switch(keyCode) {
    case nsIDOMEvent::VK_BACK:
      break;
    case nsIDOMEvent::VK_RETURN:
      // Need to implement creation of either <P> or <BR> nodes.
      mEditor->Commit(ctrlKey);
      break;
    default:
      // XXX Replace with x-platform NS-virtkeycode transform.
      if (NS_OK == GetCharFromKeyCode(keyCode, isShift, & character)) {
        nsString* key = new nsString();
        *key += character;
        if (!isShift) {
          key->ToLowerCase();
        }
        mEditor->InsertString(key);
        delete key;
      }
      break;
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

