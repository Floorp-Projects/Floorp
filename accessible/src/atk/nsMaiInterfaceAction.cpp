/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaiInterfaceAction.h"

#include "nsAccUtils.h"
#include "nsRoleMap.h"
#include "nsString.h"

#include "nsIDOMDOMStringList.h"

void
actionInterfaceInitCB(AtkActionIface *aIface)
{
    NS_ASSERTION(aIface, "Invalid aIface");
    if (!aIface)
        return;

    aIface->do_action = doActionCB;
    aIface->get_n_actions = getActionCountCB;
    aIface->get_description = getActionDescriptionCB;
    aIface->get_keybinding = getKeyBindingCB;
    aIface->get_name = getActionNameCB;
}

gboolean
doActionCB(AtkAction *aAction, gint aActionIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    if (!accWrap)
        return FALSE;
 
    nsresult rv = accWrap->DoAction(aActionIndex);
    return (NS_FAILED(rv)) ? FALSE : TRUE;
}

gint
getActionCountCB(AtkAction *aAction)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    if (!accWrap)
        return 0;

    PRUint8 num = 0;
    nsresult rv = accWrap->GetNumActions(&num);
    return (NS_FAILED(rv)) ? 0 : static_cast<gint>(num);
}

const gchar *
getActionDescriptionCB(AtkAction *aAction, gint aActionIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    if (!accWrap)
        return nsnull;

    nsAutoString description;
    nsresult rv = accWrap->GetActionDescription(aActionIndex, description);
    NS_ENSURE_SUCCESS(rv, nsnull);
    return nsAccessibleWrap::ReturnString(description);
}

const gchar *
getActionNameCB(AtkAction *aAction, gint aActionIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    if (!accWrap)
        return nsnull;

    nsAutoString autoStr;
    nsresult rv = accWrap->GetActionName(aActionIndex, autoStr);
    NS_ENSURE_SUCCESS(rv, nsnull);
    return nsAccessibleWrap::ReturnString(autoStr);
}

const gchar *
getKeyBindingCB(AtkAction *aAction, gint aActionIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    if (!accWrap)
        return nsnull;

    //return all KeyBindings including accesskey and shortcut
    nsAutoString allKeyBinding;

    //get accesskey
    nsAutoString accessKey;
    nsresult rv = accWrap->GetKeyboardShortcut(accessKey);

    if (NS_SUCCEEDED(rv) && !accessKey.IsEmpty()) {
        nsAccessible* parent = accWrap->GetParent();
        if (parent) {
          PRUint32 atkRole = atkRoleMap[parent->NativeRole()];

            if (atkRole == ATK_ROLE_MENU_BAR) {
                //it is topmenu, change from "Alt+f" to "f;<Alt>f"
                nsAutoString rightChar;
                accessKey.Right(rightChar, 1);
                allKeyBinding = rightChar + NS_LITERAL_STRING(";<Alt>") +
                                rightChar;
            }
            else if ((atkRole == ATK_ROLE_MENU) || (atkRole == ATK_ROLE_MENU_ITEM)) {
                //it is submenu, change from "s" to "s;<Alt>f:s"
                nsAutoString allKey = accessKey;
                nsAccessible* grandParent = parent;

                do {
                    nsAutoString grandParentKey;
                    grandParent->GetKeyboardShortcut(grandParentKey);

                    if (!grandParentKey.IsEmpty()) {
                        nsAutoString rightChar;
                        grandParentKey.Right(rightChar, 1);
                        allKey = rightChar + NS_LITERAL_STRING(":") + allKey;
                    }

                } while ((grandParent = grandParent->GetParent()) &&
                         atkRoleMap[grandParent->NativeRole()] != ATK_ROLE_MENU_BAR);

                allKeyBinding = accessKey + NS_LITERAL_STRING(";<Alt>") +
                                allKey;
            }
        }
        else {
            //default process, rarely happens.
            nsAutoString rightChar;
            accessKey.Right(rightChar, 1);
            allKeyBinding = rightChar + NS_LITERAL_STRING(";<Alt>") + rightChar;
        }
    }
    else  //don't have accesskey
        allKeyBinding.AssignLiteral(";");

    //get shortcut
    nsAutoString subShortcut;
    nsCOMPtr<nsIDOMDOMStringList> keyBindings;
    rv = accWrap->GetKeyBindings(aActionIndex, getter_AddRefs(keyBindings));

    if (NS_SUCCEEDED(rv) && keyBindings) {
        PRUint32 length = 0;
        keyBindings->GetLength(&length);
        for (PRUint32 i = 0; i < length; i++) {
            nsAutoString keyBinding;
            keyBindings->Item(i, keyBinding);

            //change the shortcut from "Ctrl+Shift+L" to "<Control><Shift>L"
            PRInt32 oldPos, curPos=0;
            while ((curPos != -1) && (curPos < (PRInt32)keyBinding.Length())) {
                oldPos = curPos;
                nsAutoString subString;
                curPos = keyBinding.FindChar('+', oldPos);
                if (curPos == -1) {
                    keyBinding.Mid(subString, oldPos, keyBinding.Length() - oldPos);
                    subShortcut += subString;
                } else {
                    keyBinding.Mid(subString, oldPos, curPos - oldPos);

                    //change "Ctrl" to "Control"
                    if (subString.LowerCaseEqualsLiteral("ctrl"))
                        subString.AssignLiteral("Control");

                    subShortcut += NS_LITERAL_STRING("<") + subString +
                                   NS_LITERAL_STRING(">");
                    curPos++;
                }
            }
        }
    }

    allKeyBinding += NS_LITERAL_STRING(";") + subShortcut;
    return nsAccessibleWrap::ReturnString(allKeyBinding);
}
