/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "nsMai.h"
#include "Role.h"
#include "mozilla/Likely.h"
#include "ProxyAccessible.h"
#include "nsString.h"

using namespace mozilla::a11y;

extern "C" {

static gboolean
doActionCB(AtkAction *aAction, gint aActionIndex)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
  if (accWrap) {
    return accWrap->DoAction(aActionIndex);
  }

  ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aAction));
  return proxy && proxy->DoAction(aActionIndex);
}

static gint
getActionCountCB(AtkAction *aAction)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
  if (accWrap) {
    return accWrap->ActionCount();
  }

  ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aAction));
  return proxy ? proxy->ActionCount() : 0;
}

static const gchar*
getActionDescriptionCB(AtkAction *aAction, gint aActionIndex)
{
  nsAutoString description;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
  if (accWrap) {
    accWrap->ActionDescriptionAt(aActionIndex, description);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aAction))) {
    proxy->ActionDescriptionAt(aActionIndex, description);
  } else {
    return nullptr;
  }

  return AccessibleWrap::ReturnString(description);
}

static const gchar*
getActionNameCB(AtkAction *aAction, gint aActionIndex)
{
  nsAutoString autoStr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
  if (accWrap) {
    accWrap->ActionNameAt(aActionIndex, autoStr);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aAction))) {
    proxy->ActionNameAt(aActionIndex, autoStr);
  } else {
    return nullptr;
  }

  return AccessibleWrap::ReturnString(autoStr);
}

static const gchar*
getKeyBindingCB(AtkAction *aAction, gint aActionIndex)
{
  nsAutoString keyBindingsStr;
  AccessibleWrap* acc = GetAccessibleWrap(ATK_OBJECT(aAction));
  if (acc) {
    AccessibleWrap::GetKeyBinding(acc, keyBindingsStr);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aAction))) {
    proxy->AtkKeyBinding(keyBindingsStr);
  } else {
    return nullptr;
  }

  return AccessibleWrap::ReturnString(keyBindingsStr);
}
}

void
actionInterfaceInitCB(AtkActionIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->do_action = doActionCB;
  aIface->get_n_actions = getActionCountCB;
  aIface->get_description = getActionDescriptionCB;
  aIface->get_keybinding = getKeyBindingCB;
  aIface->get_name = getActionNameCB;
}
