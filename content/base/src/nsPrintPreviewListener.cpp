/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPrintPreviewListener.h"
#include "nsIDOMKeyEvent.h"
#include "nsLiteralString.h"

NS_IMPL_ISUPPORTS1(nsPrintPreviewListener, nsIDOMEventListener)


//
// nsPrintPreviewListener ctor
//
nsPrintPreviewListener::nsPrintPreviewListener (nsIDOMEventTarget* aTarget)
  : mEventTarget(aTarget)
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
  if (mEventTarget) {
    mEventTarget->AddEventListener(NS_LITERAL_STRING("click"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("keydown"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("keypress"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("keyup"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mousemove"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mouseout"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mouseover"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mouseup"), this, true);
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
  if (mEventTarget) {
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("click"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keyup"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseout"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseover"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseup"), this, true);
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

NS_IMETHODIMP nsPrintPreviewListener::HandleEvent(nsIDOMEvent* aKeyEvent)
{ 
  if (!IsKeyOK(aKeyEvent)) {
    aKeyEvent->StopPropagation();
    aKeyEvent->PreventDefault(); 
  }
  return NS_OK; 
}
