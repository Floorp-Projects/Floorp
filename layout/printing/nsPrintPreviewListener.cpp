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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "mozilla/dom/Element.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsFocusManager.h"
#include "nsLiteralString.h"

using namespace mozilla;

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
// GetActionForEvent
//
// Helper function to let certain key events through
//
enum eEventAction {
  eEventAction_Tab,       eEventAction_ShiftTab,
  eEventAction_Propagate, eEventAction_Suppress
};

static eEventAction
GetActionForEvent(nsIDOMEvent* aEvent)
{
  static const PRUint32 kOKKeyCodes[] = {
    nsIDOMKeyEvent::DOM_VK_PAGE_UP, nsIDOMKeyEvent::DOM_VK_PAGE_DOWN,
    nsIDOMKeyEvent::DOM_VK_UP,      nsIDOMKeyEvent::DOM_VK_DOWN, 
    nsIDOMKeyEvent::DOM_VK_HOME,    nsIDOMKeyEvent::DOM_VK_END 
  };

  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
  if (keyEvent) {
    bool b;
    keyEvent->GetAltKey(&b);
    if (b) return eEventAction_Suppress;
    keyEvent->GetCtrlKey(&b);
    if (b) return eEventAction_Suppress;

    keyEvent->GetShiftKey(&b);

    PRUint32 keyCode;
    keyEvent->GetKeyCode(&keyCode);
    if (keyCode == nsIDOMKeyEvent::DOM_VK_TAB)
      return b ? eEventAction_ShiftTab : eEventAction_Tab;

    PRUint32 charCode;
    keyEvent->GetCharCode(&charCode);
    if (charCode == ' ' || keyCode == nsIDOMKeyEvent::DOM_VK_SPACE)
      return eEventAction_Propagate;

    if (b) return eEventAction_Suppress;

    for (PRUint32 i = 0; i < sizeof(kOKKeyCodes)/sizeof(kOKKeyCodes[0]); ++i) {
      if (keyCode == kOKKeyCodes[i]) {
        return eEventAction_Propagate;
      }
    }
  }
  return eEventAction_Suppress;
}

NS_IMETHODIMP
nsPrintPreviewListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  nsCOMPtr<nsIDOMNSEvent> nsEvent = do_QueryInterface(aEvent);
  if (nsEvent)
    nsEvent->GetOriginalTarget(getter_AddRefs(target));
  nsCOMPtr<nsIContent> content(do_QueryInterface(target));
  if (content && !content->IsXUL()) {
    eEventAction action = ::GetActionForEvent(aEvent);
    switch (action) {
      case eEventAction_Tab:
      case eEventAction_ShiftTab:
      {
        nsAutoString eventString;
        aEvent->GetType(eventString);
        if (eventString == NS_LITERAL_STRING("keydown")) {
          // Handle tabbing explicitly here since we don't want focus ending up
          // inside the content document, bug 244128.
          nsIDocument* doc = content->GetCurrentDoc();
          NS_ASSERTION(doc, "no document");

          nsIDocument* parentDoc = doc->GetParentDocument();
          NS_ASSERTION(parentDoc, "no parent document");

          nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(parentDoc->GetWindow());

          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm && win) {
            dom::Element* fromElement = parentDoc->FindContentForSubDocument(doc);
            nsCOMPtr<nsIDOMElement> from = do_QueryInterface(fromElement);

            bool forward = (action == eEventAction_Tab);
            nsCOMPtr<nsIDOMElement> result;
            fm->MoveFocus(win, from,
                          forward ? nsIFocusManager::MOVEFOCUS_FORWARD :
                                    nsIFocusManager::MOVEFOCUS_BACKWARD,
                          nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
          }
        }
      }
      // fall-through
      case eEventAction_Suppress:
        aEvent->StopPropagation();
        aEvent->PreventDefault();
        break;
      case eEventAction_Propagate:
        // intentionally empty
        break;
    }
  }
  return NS_OK; 
}
