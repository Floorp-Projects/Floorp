/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsMenuPopupEntryListener.h"
#include "nsMenuPopupFrame.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIDOMUIEvent.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"

#include "nsIEventStateManager.h"

#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsISupportsArray.h"

/*
 * nsMenuPopupEntryListener implementation
 */

NS_IMPL_ADDREF(nsMenuPopupEntryListener)
NS_IMPL_RELEASE(nsMenuPopupEntryListener)


////////////////////////////////////////////////////////////////////////
nsMenuPopupEntryListener::nsMenuPopupEntryListener(nsMenuPopupFrame* aMenuPopup) 
{
  NS_INIT_REFCNT();
  mMenuPopupFrame = aMenuPopup;
}

////////////////////////////////////////////////////////////////////////
nsMenuPopupEntryListener::~nsMenuPopupEntryListener() 
{
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuPopupEntryListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(nsCOMTypeInfo<nsIDOMEventReceiver>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIDOMMouseMotionListener>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMMouseMotionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMKeyListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuPopupEntryListener::MouseMove(nsIDOMEvent* aMouseEvent)
{
  mMenuPopupFrame->CaptureMouseEvents(PR_TRUE);
  return NS_OK;
}
