/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintPreviewUserEventSuppressor.h"

#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"  // for Event
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/Document.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsFocusManager.h"
#include "nsLiteralString.h"

using namespace mozilla::dom;

namespace mozilla {

PrintPreviewUserEventSuppressor::PrintPreviewUserEventSuppressor(
    EventTarget* aTarget)
    : mEventTarget(aTarget) {
  AddListeners();
}

NS_IMPL_ISUPPORTS(PrintPreviewUserEventSuppressor, nsIDOMEventListener)

void PrintPreviewUserEventSuppressor::AddListeners() {
  if (mEventTarget) {
    mEventTarget->AddEventListener(NS_LITERAL_STRING("click"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("contextmenu"), this,
                                   true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("keydown"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("keypress"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("keyup"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mousemove"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mouseout"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mouseover"), this, true);
    mEventTarget->AddEventListener(NS_LITERAL_STRING("mouseup"), this, true);

    mEventTarget->AddSystemEventListener(NS_LITERAL_STRING("keydown"), this,
                                         true);
  }
}

void PrintPreviewUserEventSuppressor::RemoveListeners() {
  if (mEventTarget) {
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("click"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("contextmenu"), this,
                                      true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keypress"), this,
                                      true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keyup"), this, true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this,
                                      true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"), this,
                                      true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseout"), this,
                                      true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseover"), this,
                                      true);
    mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseup"), this, true);

    mEventTarget->RemoveSystemEventListener(NS_LITERAL_STRING("keydown"), this,
                                            true);
  }
}

enum eEventAction {
  eEventAction_Tab,
  eEventAction_ShiftTab,
  eEventAction_Propagate,
  eEventAction_Suppress,
  eEventAction_StopPropagation
};

// Helper function to let certain key events through
static eEventAction GetActionForEvent(Event* aEvent) {
  WidgetKeyboardEvent* keyEvent = aEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (!keyEvent) {
    return eEventAction_Suppress;
  }

  if (keyEvent->mFlags.mInSystemGroup) {
    NS_ASSERTION(keyEvent->mMessage == eKeyDown,
                 "Assuming we're listening only keydown event in system group");
    return eEventAction_StopPropagation;
  }

  if (keyEvent->IsAlt() || keyEvent->IsControl() || keyEvent->IsMeta()) {
    // Don't consume keydown event because following keypress event may be
    // handled as access key or shortcut key.
    return (keyEvent->mMessage == eKeyDown) ? eEventAction_StopPropagation
                                            : eEventAction_Suppress;
  }

  static const uint32_t kOKKeyCodes[] = {NS_VK_PAGE_UP, NS_VK_PAGE_DOWN,
                                         NS_VK_UP,      NS_VK_DOWN,
                                         NS_VK_HOME,    NS_VK_END};

  if (keyEvent->mKeyCode == NS_VK_TAB) {
    return keyEvent->IsShift() ? eEventAction_ShiftTab : eEventAction_Tab;
  }

  if (keyEvent->mCharCode == ' ' || keyEvent->mKeyCode == NS_VK_SPACE) {
    return eEventAction_Propagate;
  }

  if (keyEvent->IsShift()) {
    return eEventAction_Suppress;
  }

  for (uint32_t i = 0; i < ArrayLength(kOKKeyCodes); ++i) {
    if (keyEvent->mKeyCode == kOKKeyCodes[i]) {
      return eEventAction_Propagate;
    }
  }

  return eEventAction_Suppress;
}

NS_IMETHODIMP
PrintPreviewUserEventSuppressor::HandleEvent(Event* aEvent) {
  nsCOMPtr<nsIContent> content = do_QueryInterface(aEvent->GetOriginalTarget());
  if (content && !content->IsXULElement()) {
    eEventAction action = GetActionForEvent(aEvent);
    switch (action) {
      case eEventAction_Tab:
      case eEventAction_ShiftTab: {
        nsAutoString eventString;
        aEvent->GetType(eventString);
        if (eventString.EqualsLiteral("keydown")) {
          // Handle tabbing explicitly here since we don't want focus ending up
          // inside the content document, bug 244128.
          Document* doc = content->GetUncomposedDoc();
          NS_ASSERTION(doc, "no document");

          Document* parentDoc = doc->GetInProcessParentDocument();
          NS_ASSERTION(parentDoc, "no parent document");

          nsCOMPtr<nsPIDOMWindowOuter> win = parentDoc->GetWindow();

          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm && win) {
            Element* fromElement = parentDoc->FindContentForSubDocument(doc);
            bool forward = (action == eEventAction_Tab);
            RefPtr<Element> result;
            fm->MoveFocus(win, fromElement,
                          forward ? nsIFocusManager::MOVEFOCUS_FORWARD
                                  : nsIFocusManager::MOVEFOCUS_BACKWARD,
                          nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
          }
        }
      }
        MOZ_FALLTHROUGH;
      case eEventAction_Suppress:
        aEvent->StopPropagation();
        aEvent->PreventDefault();
        break;
      case eEventAction_StopPropagation:
        aEvent->StopPropagation();
        break;
      case eEventAction_Propagate:
        // intentionally empty
        break;
    }
  }
  return NS_OK;
}

}  // namespace mozilla
