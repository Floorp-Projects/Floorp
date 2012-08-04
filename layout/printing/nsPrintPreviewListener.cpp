/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
