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

//class implementations are in order they are declared in editor.h


Editor::Editor()
{
  //initialize member variables here
}


static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);

Editor::~Editor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  COM_auto_ptr<nsIDOMEventReceiver> erP;
  nsresult t_result = mDomInterfaceP->QueryInterface(kIDOMEventReceiverIID, func_AddRefs(erP));
  if (NS_SUCCEEDED( t_result )) 
  {
    erP->RemoveEventListener(mKeyListenerP, kIDOMKeyListenerIID);
    erP->RemoveEventListener(mMouseListenerP, kIDOMMouseListenerIID);
  }
  else
    assert(0);
}



//BEGIN nsIEditor interface implementations


NS_IMPL_ADDREF(Editor)

NS_IMPL_RELEASE(Editor)



nsresult
Editor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
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
Editor::Init(nsIDOMDocument *aDomInterface)
{
  if (!aDomInterface)
    return NS_ERROR_ILLEGAL_VALUE;

  mDomInterfaceP = aDomInterface;

  nsresult t_result = NS_NewEditorKeyListener(func_AddRefs(mKeyListenerP), this);
  if (NS_OK != t_result)
  {
    assert(0);
    return t_result;
  }
  t_result = NS_NewEditorMouseListener(func_AddRefs(mMouseListenerP), this);
  if (NS_OK != t_result)
  {
    mKeyListenerP = 0; //dont keep the key listener if the mouse listener fails.
    assert(0);
    return t_result;
  }
  COM_auto_ptr<nsIDOMEventReceiver> erP;
  t_result = mDomInterfaceP->QueryInterface(kIDOMEventReceiverIID, func_AddRefs(erP));
  if (NS_OK != t_result) 
  {
    mKeyListenerP = 0;
    mMouseListenerP = 0; //dont need these if we cant register them
    assert(0);
    return t_result;
  }
  erP->AddEventListener(mKeyListenerP, kIDOMKeyListenerIID);
  erP->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);
  return NS_OK;
}



//END nsIEditorInterfaces


//BEGIN Editor Calls from public
PRBool
Editor::KeyDown(int aKeycode)
{
  return PR_TRUE;
}



PRBool
Editor::MouseClick(int aX,int aY)
{
  return PR_FALSE;
}
//END Editor Calls from public



//BEGIN Editor Private methods
//END Editor Private methods



//BEGIN FACTORY METHODS



nsresult 
NS_InitEditor(nsIEditor ** aInstancePtrResult, nsIDOMDocument *aDomDoc)
{
  Editor* editor = new Editor();
  if (NULL == editor) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult result = editor->Init(aDomDoc);
  if (NS_SUCCEEDED(result))
  {
    static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);

    return editor->QueryInterface(kIEditorIID, (void **) aInstancePtrResult);   
  }
  else
    return result;
}
 


//END FACTORY METHODS
