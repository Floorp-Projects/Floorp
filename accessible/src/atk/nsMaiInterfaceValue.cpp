/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Silvia Zhao (silvia.zhao@sun.com)
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaiInterfaceValue.h"

G_BEGIN_DECLS
    
/*value interface callbacks*/

static void interfaceInitCB(AtkValueIface *aIface);
static void getCurrentValueCB(AtkValue *obj,
                              GValue *value);
static void getMaximumValueCB(AtkValue *obj,
                              GValue *value);
static void getMinimumValueCB(AtkValue *obj,
                              GValue *value);
static gboolean setCurrentValueCB(AtkValue *obj,
                                  const GValue *value);
G_END_DECLS

MaiInterfaceValue::MaiInterfaceValue(nsAccessibleWrap *aAccWrap):
    MaiInterface(aAccWrap)
{
}

MaiInterfaceValue::~MaiInterfaceValue()
{
}

MaiInterfaceType
MaiInterfaceValue::GetType()
{
    return MAI_INTERFACE_VALUE;
}

const GInterfaceInfo *
MaiInterfaceValue::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_value_info = {
        (GInterfaceInitFunc) interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_value_info;
}

/* static functions */

void
interfaceInitCB(AtkValueIface *aIface)
{
    NS_ASSERTION(aIface, "Invalid aIface");
    if (!aIface)
        return;

    aIface->get_current_value = getCurrentValueCB;
    aIface->get_maximum_value = getMaximumValueCB;
    aIface->get_minimum_value = getMinimumValueCB;
    aIface->set_current_value = setCurrentValueCB;
}

void
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

void
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

void
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

gboolean
setCurrentValueCB(AtkValue *obj, const GValue *value)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(obj));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleValue> accValue;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleValue),
                            getter_AddRefs(accValue));
    NS_ENSURE_TRUE(accValue, FALSE);

    PRBool aBool;
    double accDouble;
    accDouble = g_value_get_double (value);
    accValue->SetCurrentValue(accDouble, &aBool);
    return aBool;
}
