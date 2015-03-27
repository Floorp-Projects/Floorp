/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "AccessibleWrap.h"
#include "nsMai.h"
#include "ProxyAccessible.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"

using namespace mozilla;
using namespace mozilla::a11y;

extern "C" {

static void
getCurrentValueCB(AtkValue *obj, GValue *value)
{
  ProxyAccessible* proxy = nullptr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
  if (!accWrap) {
    proxy = GetProxy(ATK_OBJECT(obj));
    if (!proxy) {
      return;
    }
  }

  memset (value,  0, sizeof (GValue));
  double accValue = accWrap ? accWrap->CurValue() : proxy->CurValue();
  if (IsNaN(accValue))
    return;

  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, accValue);
}

static void
getMaximumValueCB(AtkValue *obj, GValue *value)
{
  ProxyAccessible* proxy = nullptr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
  if (!accWrap) {
    proxy = GetProxy(ATK_OBJECT(obj));
    if (!proxy) {
      return;
    }
  }

  memset(value,  0, sizeof (GValue));
  double accValue = accWrap ? accWrap->MaxValue() : proxy->MaxValue();
  if (IsNaN(accValue))
    return;

  g_value_init(value, G_TYPE_DOUBLE);
  g_value_set_double(value, accValue);
}

static void
getMinimumValueCB(AtkValue *obj, GValue *value)
{
  ProxyAccessible* proxy = nullptr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
  if (!accWrap) {
    proxy = GetProxy(ATK_OBJECT(obj));
    if (!proxy) {
      return;
    }
  }

  memset(value,  0, sizeof (GValue));
  double accValue = accWrap ? accWrap->MinValue() : proxy->MinValue();
  if (IsNaN(accValue))
    return;

  g_value_init(value, G_TYPE_DOUBLE);
  g_value_set_double(value, accValue);
}

static void
getMinimumIncrementCB(AtkValue *obj, GValue *minimumIncrement)
{
  ProxyAccessible* proxy = nullptr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
  if (!accWrap) {
    proxy = GetProxy(ATK_OBJECT(obj));
    if (!proxy) {
      return;
    }
  }

  memset(minimumIncrement,  0, sizeof (GValue));
  double accValue = accWrap ? accWrap->Step() : proxy->Step();
  if (IsNaN(accValue))
    accValue = 0; // zero if the minimum increment is undefined

  g_value_init(minimumIncrement, G_TYPE_DOUBLE);
  g_value_set_double(minimumIncrement, accValue);
}

static gboolean
setCurrentValueCB(AtkValue *obj, const GValue *value)
{
  ProxyAccessible* proxy = nullptr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
  if (!accWrap) {
    proxy = GetProxy(ATK_OBJECT(obj));
    if (!proxy) {
      return FALSE;
    }
  }

  double accValue =g_value_get_double(value);
  return accWrap ? accWrap->SetCurValue(accValue) : proxy->SetCurValue(accValue);
}
}

void
valueInterfaceInitCB(AtkValueIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->get_current_value = getCurrentValueCB;
  aIface->get_maximum_value = getMaximumValueCB;
  aIface->get_minimum_value = getMinimumValueCB;
  aIface->get_minimum_increment = getMinimumIncrementCB;
  aIface->set_current_value = setCurrentValueCB;
}
