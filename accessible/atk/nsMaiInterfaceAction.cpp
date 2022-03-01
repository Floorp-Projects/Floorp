/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "LocalAccessible-inl.h"
#include "nsMai.h"
#include "Role.h"
#include "mozilla/Likely.h"
#include "RemoteAccessible.h"
#include "nsString.h"

using namespace mozilla::a11y;

extern "C" {

static gboolean doActionCB(AtkAction* aAction, gint aActionIndex) {
  AtkObject* atkObject = ATK_OBJECT(aAction);
  if (Accessible* acc = GetInternalObj(atkObject)) {
    return acc->DoAction(aActionIndex);
  }

  return false;
}

static gint getActionCountCB(AtkAction* aAction) {
  AtkObject* atkObject = ATK_OBJECT(aAction);
  if (Accessible* acc = GetInternalObj(atkObject)) {
    return acc->ActionCount();
  }

  return 0;
}

static const gchar* getActionDescriptionCB(AtkAction* aAction,
                                           gint aActionIndex) {
  AtkObject* atkObject = ATK_OBJECT(aAction);
  nsAutoString description;
  if (Accessible* acc = GetInternalObj(atkObject)) {
    acc->ActionDescriptionAt(aActionIndex, description);
    return AccessibleWrap::ReturnString(description);
  }

  return nullptr;
}

static const gchar* getActionNameCB(AtkAction* aAction, gint aActionIndex) {
  AtkObject* atkObject = ATK_OBJECT(aAction);
  nsAutoString autoStr;
  if (Accessible* acc = GetInternalObj(atkObject)) {
    acc->ActionNameAt(aActionIndex, autoStr);
    return AccessibleWrap::ReturnString(autoStr);
  }

  return nullptr;
}

static const gchar* getKeyBindingCB(AtkAction* aAction, gint aActionIndex) {
  nsAutoString keyBindingsStr;
  AccessibleWrap* acc = GetAccessibleWrap(ATK_OBJECT(aAction));
  if (acc) {
    AccessibleWrap::GetKeyBinding(acc, keyBindingsStr);
  } else if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aAction))) {
    proxy->AtkKeyBinding(keyBindingsStr);
  } else {
    return nullptr;
  }

  return AccessibleWrap::ReturnString(keyBindingsStr);
}
}

void actionInterfaceInitCB(AtkActionIface* aIface) {
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface)) return;

  aIface->do_action = doActionCB;
  aIface->get_n_actions = getActionCountCB;
  aIface->get_description = getActionDescriptionCB;
  aIface->get_keybinding = getKeyBindingCB;
  aIface->get_name = getActionNameCB;
}
