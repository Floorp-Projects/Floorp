/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MenuBoxObject.h"
#include "mozilla/dom/MenuBoxObjectBinding.h"

#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/Element.h"
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

JSObject* MenuBoxObject::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MenuBoxObject_Binding::Wrap(aCx, this, aGivenProto);
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
    RefPtr<Element> el;
    menu->GetActiveChild(getter_AddRefs(el));
    return el.forget();
  }
  return nullptr;
}

void MenuBoxObject::SetActiveChild(Element* arg)
{
  nsMenuFrame* menu = do_QueryFrame(GetFrame(false));
  if (menu) {
    menu->SetActiveChild(arg);
  }
}

bool MenuBoxObject::HandleKeyPress(KeyboardEvent& keyEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    return false;
  }

  // if event has already been handled, bail
  if (keyEvent.DefaultPrevented()) {
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
    case KeyboardEvent_Binding::DOM_VK_UP:
    case KeyboardEvent_Binding::DOM_VK_DOWN:
    case KeyboardEvent_Binding::DOM_VK_HOME:
    case KeyboardEvent_Binding::DOM_VK_END:
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
