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


//class implementations are in order they are declared in editor.h


Editor::Editor()
{
  //initialize member variables here
};



Editor::~Editor()
{
  //the autopointers will clear themselves up.
};



//BEGIN nsIEditor interface implementations



nsresult
Editor::Init(nsIDOMDocument *aDomInterface)
{
  if (!aDomInterface)
    return NS_ERROR_ILLEGAL_VALUE;

  mDomInterfaceP = aDomInterface;

  nsresult t_result = NS_NewEditorKeyListener(context_AddRefs<nsEditorKeyListener>(mKeyListenerP), this);
  if (NS_OK != t_result)
  {
    assert(FALSE);
    return t_result;
  }
  t_result = NS_NewEditorMouseListener(context_AddRefs<nsEditorMouseListener>(mMouseListenerP), this);
  if (NS_OK != t_result)
  {
    mKeyListener = 0; //dont keep the key listener if the mouse listener fails.
    assert(FALSE);
    return t_result;
  }
  COM_auto_ptr<nsIDOMEventReceiver> erP;
  t_result = mDomInterfaceP->QueryInterface(kIDOMEventReceiverIID, context_AddRefs<nsIDOMEventReceiver>(erP));
  if (NS_OK != t_result) 
  {
    mKeyListener = 0;
    m_MouseListener = 0; //dont need these if we cant register them
    assert(FALSE);
    return t_result;
  }
  er->AddEventListener(mKeyListenerP, kIDOMKeyListenerIID);
  er->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);
}



//END nsIEditorInterfaces


//BEGIN Editor Calls from public
PR_Bool
Editor::KeyDown(int aKeycode)
{
}



PR_Bool
Editor::MouseClick(int aX,int aY)
{
}
//END Editor Calls from public

//BEGIN Editor Private methods
//END Editor Private methods