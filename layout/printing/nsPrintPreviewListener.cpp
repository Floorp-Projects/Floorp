/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   
 */

#include "nsPrintPreviewListener.h"
#include "nsIDOMKeyEvent.h"

NS_IMPL_ADDREF(nsPrintPreviewListener)
NS_IMPL_RELEASE(nsPrintPreviewListener)

NS_INTERFACE_MAP_BEGIN(nsPrintPreviewListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMContextMenuListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMContextMenuListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMContextMenuListener)
NS_INTERFACE_MAP_END


//
// nsPrintPreviewListener ctor
//
nsPrintPreviewListener::nsPrintPreviewListener (nsIDOMEventReceiver* aEVRec) 
  : mEventReceiver(aEVRec),
    mRegFlags(REG_NONE_LISTENER)
{
  NS_ADDREF_THIS();
} // ctor


//-------------------------------------------------------
//
// AddListeners
//
// Subscribe to the events that will allow us to track various events. 
//
nsresult
nsPrintPreviewListener::AddListeners()
{
  if (mRegFlags != REG_NONE_LISTENER) return NS_ERROR_FAILURE;

  if (mEventReceiver) {
    nsIDOMContextMenuListener *pListener = NS_STATIC_CAST(nsIDOMContextMenuListener *, this);
    NS_ASSERTION(pListener, "Cast can't fail!");

    nsresult rv = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMContextMenuListener));
    NS_ENSURE_SUCCESS(rv, rv);
    mRegFlags |= REG_CONTEXTMENU_LISTENER;

    rv = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMKeyListener));
    NS_ENSURE_SUCCESS(rv, rv);
    mRegFlags |= REG_KEY_LISTENER;

    rv = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseListener));
    NS_ENSURE_SUCCESS(rv, rv);
    mRegFlags |= REG_MOUSE_LISTENER;

    rv = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseMotionListener));
    NS_ENSURE_SUCCESS(rv, rv);
    mRegFlags |= REG_MOUSEMOTION_LISTENER;
  }

  return NS_OK;
}


//-------------------------------------------------------
//
// RemoveListeners
//
// Unsubscribe from all the various events that we were listening to. 
//
nsresult 
nsPrintPreviewListener::RemoveListeners()
{
  if (mEventReceiver && mRegFlags != REG_NONE_LISTENER) {
    nsIDOMContextMenuListener *pListener = NS_STATIC_CAST(nsIDOMContextMenuListener *, this);
    NS_ASSERTION(pListener, "Cast can't fail!");

    // ignore return values, so we can try to unregister the other listeners
    if (mRegFlags & REG_CONTEXTMENU_LISTENER) {
      mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMContextMenuListener));
    }
    if (mRegFlags & REG_KEY_LISTENER) {
      mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMKeyListener));
    }
    if (mRegFlags & REG_MOUSE_LISTENER) {
      mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseListener));
    }
    if (mRegFlags & REG_MOUSEMOTION_LISTENER) {
      mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseMotionListener));
    }
  }

  return NS_OK;
}

//-------------------------------------------------------
//
// IsKeyOK
//
// Helper function to let certain key events thru
//
static PRBool IsKeyOK(nsIDOMEvent* aEvent)
{
  const PRUint32 kOKKeyCodes[] = {nsIDOMKeyEvent::DOM_VK_PAGE_UP, nsIDOMKeyEvent::DOM_VK_PAGE_DOWN,
                                  nsIDOMKeyEvent::DOM_VK_UP, nsIDOMKeyEvent::DOM_VK_DOWN, 
                                  nsIDOMKeyEvent::DOM_VK_HOME, nsIDOMKeyEvent::DOM_VK_END, 
                                  nsIDOMKeyEvent::DOM_VK_TAB, 0};

  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
  if (keyEvent) {
    PRBool b;
    keyEvent->GetAltKey(&b);
    if (b) return PR_FALSE;
    keyEvent->GetCtrlKey(&b);
    if (b) return PR_FALSE;
    keyEvent->GetShiftKey(&b);
    if (b) return PR_FALSE;

    PRUint32 keyCode;
    keyEvent->GetKeyCode(&keyCode);
    PRInt32 i = 0;
    while (kOKKeyCodes[i] != 0) {
      if (keyCode == kOKKeyCodes[i]) {
        return PR_TRUE;
      }
      i++;
    }
  }
  return PR_FALSE;
}

//-------------------------------------------------------
//
// KeyDown
//
NS_IMETHODIMP nsPrintPreviewListener::KeyDown(nsIDOMEvent* aKeyEvent)         
{ 
  if (!IsKeyOK(aKeyEvent)) {
    aKeyEvent->PreventDefault(); 
  }
  return NS_OK; 
}

//-------------------------------------------------------
//
// KeyUp
//
NS_IMETHODIMP nsPrintPreviewListener::KeyUp(nsIDOMEvent* aKeyEvent)           
{ 
  if (!IsKeyOK(aKeyEvent)) {
    aKeyEvent->PreventDefault(); 
  }
  return NS_OK; 
}

//-------------------------------------------------------
//
// KeyPress
//
NS_IMETHODIMP nsPrintPreviewListener::KeyPress(nsIDOMEvent* aKeyEvent)        
{ 
  if (!IsKeyOK(aKeyEvent)) {
    aKeyEvent->PreventDefault(); 
  }
  return NS_OK; 
}

