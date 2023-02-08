/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XULMenuElement.h"
#include "mozilla/StaticAnalysisFunctions.h"
#include "mozilla/dom/XULButtonElement.h"
#include "mozilla/dom/XULMenuElementBinding.h"
#include "mozilla/dom/XULPopupElement.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "nsXULPopupManager.h"
#include "nsMenuPopupFrame.h"

namespace mozilla::dom {

JSObject* XULMenuElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return XULMenuElement_Binding::Wrap(aCx, this, aGivenProto);
}

Element* XULMenuElement::GetActiveMenuChild() {
  RefPtr popup = GetMenuPopupContent();
  return popup ? popup->GetActiveMenuChild() : nullptr;
}

void XULMenuElement::SetActiveMenuChild(Element* aChild) {
  RefPtr popup = GetMenuPopupContent();
  if (NS_WARN_IF(!popup)) {
    return;
  }

  if (!aChild) {
    popup->SetActiveMenuChild(nullptr);
    return;
  }
  auto* button = XULButtonElement::FromNode(aChild);
  if (NS_WARN_IF(!button) || NS_WARN_IF(!button->IsMenu())) {
    return;
  }
  // KnownLive because it's aChild.
  popup->SetActiveMenuChild(MOZ_KnownLive(button));
}

bool XULButtonElement::HandleKeyPress(KeyboardEvent& keyEvent) {
  RefPtr<nsXULPopupManager> pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    return false;
  }

  // if event has already been handled, bail
  if (keyEvent.DefaultPrevented()) {
    return false;
  }

  if (keyEvent.IsMenuAccessKeyPressed()) {
    return false;
  }

  nsMenuPopupFrame* popupFrame = GetMenuPopup(FlushType::Frames);
  if (NS_WARN_IF(!popupFrame)) {
    return false;
  }

  uint32_t keyCode = keyEvent.KeyCode();
  switch (keyCode) {
    case KeyboardEvent_Binding::DOM_VK_UP:
    case KeyboardEvent_Binding::DOM_VK_DOWN:
    case KeyboardEvent_Binding::DOM_VK_HOME:
    case KeyboardEvent_Binding::DOM_VK_END: {
      nsNavigationDirection theDirection;
      theDirection = NS_DIRECTION_FROM_KEY_CODE(popupFrame, keyCode);
      return pm->HandleKeyboardNavigationInPopup(popupFrame, theDirection);
    }
    default:
      return pm->HandleShortcutNavigation(keyEvent, popupFrame);
  }
}

}  // namespace mozilla::dom
