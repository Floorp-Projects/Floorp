/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "nsAccessibleWrap.h"
#include "nsMai.h"

extern "C" {

static void
getCurrentValueCB(AtkValue *obj, GValue *value)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleValue> accValue;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleValue),
                            getter_AddRefs(accValue));
    if (!accValue)
        return;

    memset (value,  0, sizeof (GValue));
    double accDouble;
    if (NS_FAILED(accValue->GetCurrentValue(&accDouble)))
        return;
    g_value_init (value, G_TYPE_DOUBLE);
    g_value_set_double (value, accDouble);
}

static void
getMaximumValueCB(AtkValue *obj, GValue *value)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleValue> accValue;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleValue),
                            getter_AddRefs(accValue));
    if (!accValue)
        return;

    memset (value,  0, sizeof (GValue));
    double accDouble;
    if (NS_FAILED(accValue->GetMaximumValue(&accDouble)))
        return;
    g_value_init (value, G_TYPE_DOUBLE);
    g_value_set_double (value, accDouble);
}

static void
getMinimumValueCB(AtkValue *obj, GValue *value)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleValue> accValue;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleValue),
                            getter_AddRefs(accValue));
    if (!accValue)
        return;

    memset (value,  0, sizeof (GValue));
    double accDouble;
    if (NS_FAILED(accValue->GetMinimumValue(&accDouble)))
        return;
    g_value_init (value, G_TYPE_DOUBLE);
    g_value_set_double (value, accDouble);
}

static void
getMinimumIncrementCB(AtkValue *obj, GValue *minimumIncrement)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleValue> accValue;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleValue),
                            getter_AddRefs(accValue));
    if (!accValue)
        return;

    memset (minimumIncrement,  0, sizeof (GValue));
    double accDouble;
    if (NS_FAILED(accValue->GetMinimumIncrement(&accDouble)))
        return;
    g_value_init (minimumIncrement, G_TYPE_DOUBLE);
    g_value_set_double (minimumIncrement, accDouble);
}

static gboolean
setCurrentValueCB(AtkValue *obj, const GValue *value)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleValue> accValue;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleValue),
                            getter_AddRefs(accValue));
    NS_ENSURE_TRUE(accValue, FALSE);

    double accDouble =g_value_get_double (value);
    return !NS_FAILED(accValue->SetCurrentValue(accDouble));
}
}

void
valueInterfaceInitCB(AtkValueIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (NS_UNLIKELY(!aIface))
    return;

  aIface->get_current_value = getCurrentValueCB;
  aIface->get_maximum_value = getMaximumValueCB;
  aIface->get_minimum_value = getMinimumValueCB;
  aIface->get_minimum_increment = getMinimumIncrementCB;
  aIface->set_current_value = setCurrentValueCB;
}
