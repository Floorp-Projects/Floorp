/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MenuBoxObject.h"
#include "mozilla/dom/MenuBoxObjectBinding.h"

#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMKeyEvent.h"
#include "nsIFrame.h"
#include "nsMenuBarFrame.h"
#include "nsMenuBarListener.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"

namespace mozilla {
namespace dom {

MenuBoxObject::MenuBoxObject()
{
}

MenuBoxObject::~MenuBoxObject()
{
}

JSObject* MenuBoxObject::WrapObject(JSContext* aCx)
{
  return MenuBoxObjectBinding::Wrap(aCx, this);
}

void MenuBoxObject::OpenMenu(bool aOpenFlag)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* frame = GetFrame(false);
    if (frame) {
      if (aOpenFlag) {
        nsCOMPtr<nsIContent> content = mContent;
        pm->ShowMenu(content, false, false);
      }
      else {
        nsMenuFrame* menu = do_QueryFrame(frame);
        if (menu) {
          nsMenuPopupFrame* popupFrame = menu->GetPopup();
          if (popupFrame)
            pm->HidePopup(popupFrame->GetContent(), false, true, false, false);
        }
      }
    }
  }
}

already_AddRefed<Element>
MenuBoxObject::GetActiveChild()
{
  nsMenuFrame* menu = do_QueryFrame(GetFrame(false));
  if (menu) {
    nsCOMPtr<nsIDOMElement> el;
    menu->GetActiveChild(getter_AddRefs(el));
    nsCOMPtr<Element> ret(do_QueryInterface(el));
    return ret.forget();
  }
  return nullptr;
}

void MenuBoxObject::SetActiveChild(Element* arg)
{
  nsMenuFrame* menu = do_QueryFrame(GetFrame(false));
  if (menu) {
    nsCOMPtr<nsIDOMElement> el(do_QueryInterface(arg));
    menu->SetActiveChild(el);
  }
}

bool MenuBoxObject::HandleKeyPress(KeyboardEvent& keyEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    return false;
  }

  // if event has already been handled, bail
  bool eventHandled = false;
  keyEvent.GetDefaultPrevented(&eventHandled);
  if (eventHandled) {
    return false;
  }

  if (nsMenuBarListener::IsAccessKeyPressed(&keyEvent))
    return false;

  nsMenuFrame* menu = do_QueryFrame(GetFrame(false));
  if (!menu) {
    return false;
  }

  nsMenuPopupFrame* popupFrame = menu->GetPopup();
  if (!popupFrame) {
    return false;
  }

  uint32_t keyCode = keyEvent.KeyCode();
  switch (keyCode) {
    case nsIDOMKeyEvent::DOM_VK_UP:
    case nsIDOMKeyEvent::DOM_VK_DOWN:
    case nsIDOMKeyEvent::DOM_VK_HOME:
    case nsIDOMKeyEvent::DOM_VK_END:
    {
      nsNavigationDirection theDirection;
      theDirection = NS_DIRECTION_FROM_KEY_CODE(popupFrame, keyCode);
      return pm->HandleKeyboardNavigationInPopup(popupFrame, theDirection);
    }
    default:
      return pm->HandleShortcutNavigation(&keyEvent, popupFrame);
  }
}

bool MenuBoxObject::OpenedWithKey()
{
  nsMenuFrame* menuframe = do_QueryFrame(GetFrame(false));
  if (!menuframe) {
    return false;
  }

  nsIFrame* frame = menuframe->GetParent();
  while (frame) {
    nsMenuBarFrame* menubar = do_QueryFrame(frame);
    if (menubar) {
      return menubar->IsActiveByKeyboard();
    }
    frame = frame->GetParent();
  }
  return false;
}

} // namespace dom
} // namespace mozilla

// Creation Routine ///////////////////////////////////////////////////////////////////////

using namespace mozilla::dom;

nsresult
NS_NewMenuBoxObject(nsIBoxObject** aResult)
{
  NS_ADDREF(*aResult = new MenuBoxObject());
  return NS_OK;
}
