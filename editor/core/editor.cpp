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



Editor::~Editor()
{
  //the autopointers will clear themselves up.
}



//BEGIN nsIEditor interface implementations



nsresult
Editor::Init(nsIDOMDocument *aDomInterface)
{
  if (!aDomInterface)
    return NS_ERROR_ILLEGAL_VALUE;

  mDomInterfaceP = aDomInterface;

  static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
  static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
  static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);

  nsresult t_result = NS_NewEditorKeyListener(context_AddRefs(mKeyListenerP), this);
  if (NS_OK != t_result)
  {
    assert(0);
    return t_result;
  }
  t_result = NS_NewEditorMouseListener(context_AddRefs(mMouseListenerP), this);
  if (NS_OK != t_result)
  {
    mKeyListenerP = 0; //dont keep the key listener if the mouse listener fails.
    assert(0);
    return t_result;
  }
  COM_auto_ptr<nsIDOMEventReceiver> erP;
  t_result = mDomInterfaceP->QueryInterface(kIDOMEventReceiverIID, context_AddRefs(erP));
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