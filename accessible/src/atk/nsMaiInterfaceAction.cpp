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
 * Original Author: Bolian Yin (bolian.yin@sun.com)
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

#include "nsMaiInterfaceAction.h"
#include "nsAccessibleWrap.h"
#include "nsString.h"

G_BEGIN_DECLS

static void interfaceInitCB(AtkActionIface *aIface);
/* action interface callbacks */
static gboolean doActionCB(AtkAction *aAction, gint aActionIndex);
static gint getActionCountCB(AtkAction *aAction);
static const gchar *getDescriptionCB(AtkAction *aAction, gint aActionIndex);
static const gchar *getNameCB(AtkAction *aAction, gint aActionIndex);
static const gchar *getKeyBindingCB(AtkAction *aAction, gint aActionIndex);
static gboolean     setDescriptionCB(AtkAction *aAction, gint aActionIndex,
                                     const gchar *aDesc);
G_END_DECLS

MaiInterfaceAction::MaiInterfaceAction(nsAccessibleWrap *aAccWrap):
    MaiInterface(aAccWrap)
{
}

MaiInterfaceAction::~MaiInterfaceAction()
{
}

MaiInterfaceType
MaiInterfaceAction::GetType()
{
    return MAI_INTERFACE_ACTION;
}

const GInterfaceInfo *
MaiInterfaceAction::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_action_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_action_info;
}

/* static functions */

void
interfaceInitCB(AtkActionIface *aIface)
{
    NS_ASSERTION(aIface, "Invalid aIface");
    if (!aIface)
        return;

    aIface->do_action = doActionCB;
    aIface->get_n_actions = getActionCountCB;
    aIface->get_description = getDescriptionCB;
    aIface->get_keybinding = getKeyBindingCB;
    aIface->get_name = getNameCB;
    aIface->set_description = setDescriptionCB;
}

gboolean
doActionCB(AtkAction *aAction, gint aActionIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    NS_ENSURE_TRUE(accWrap, FALSE);
 
    nsresult rv = accWrap->DoAction(aActionIndex);
    return (NS_FAILED(rv)) ? FALSE : TRUE;
}

gint
getActionCountCB(AtkAction *aAction)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    NS_ENSURE_TRUE(accWrap, 0);

    PRUint8 num = 0;
    nsresult rv = accWrap->GetNumActions(&num);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gint, num);
}

const gchar *
getDescriptionCB(AtkAction *aAction, gint aActionIndex)
{
    // the interface in nsIAccessibleAction is empty
    // use getName as default description
    return getNameCB(aAction, aActionIndex);
}

const gchar *
getNameCB(AtkAction *aAction, gint aActionIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    NS_ENSURE_TRUE(accWrap, nsnull);

    MaiInterfaceAction *action =
        NS_STATIC_CAST(MaiInterfaceAction *,
                       accWrap->GetMaiInterface(MAI_INTERFACE_ACTION));
    NS_ENSURE_TRUE(action, nsnull);

    nsAutoString autoStr;
    nsresult rv = accWrap->GetActionName(aActionIndex, autoStr);
    NS_ENSURE_SUCCESS(rv, nsnull);
    action->SetName(autoStr);
    return action->GetName();
}

const gchar *
getKeyBindingCB(AtkAction *aAction, gint aActionIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    NS_ENSURE_TRUE(accWrap, nsnull);

    MaiInterfaceAction *action =
        NS_STATIC_CAST(MaiInterfaceAction *,
                       accWrap->GetMaiInterface(MAI_INTERFACE_ACTION));
    NS_ENSURE_TRUE(action, nsnull);

    if (action->GetKeyBinding())
        return action->GetKeyBinding();

    //return all KeyBindings including accesskey and shortcut
    
    nsAutoString allKeyBinding;

    //get accesskey
    nsAutoString accessKey;
    nsresult rv = accWrap->GetKeyboardShortcut(accessKey);

    if (NS_SUCCEEDED(rv) && !accessKey.IsEmpty()) {
        nsCOMPtr<nsIAccessible> parentAccessible;
        accWrap->GetParent(getter_AddRefs(parentAccessible));
        if (parentAccessible) {
            PRUint32 role;
            parentAccessible->GetRole(&role);

            if (role == ATK_ROLE_MENU_BAR) {
                //it is topmenu, change from "Alt+f" to "f;<Alt>f"
                nsAutoString rightChar;
                accessKey.Right(rightChar, 1);
                allKeyBinding = rightChar + NS_LITERAL_STRING(";<Alt>") +
                                rightChar;
            }
            else if ((role == ATK_ROLE_MENU) || (role == ATK_ROLE_MENU_ITEM)) {
                //it is submenu, change from "s" to "s;<Alt>fs"
                nsAutoString allKey = accessKey;
                nsCOMPtr<nsIAccessible> grandParentAcc = parentAccessible;

                while ((grandParentAcc) && (role != ATK_ROLE_MENU_BAR)) {
                    nsAutoString grandParentKey;
                    grandParentAcc->GetKeyboardShortcut(grandParentKey);

                    if (!grandParentKey.IsEmpty()) {
                        nsAutoString rightChar;
                        grandParentKey.Right(rightChar, 1);
                        allKey = rightChar + allKey;
                    }

                    nsCOMPtr<nsIAccessible> tempAcc = grandParentAcc;
                    tempAcc->GetParent(getter_AddRefs(grandParentAcc));
                    if (grandParentAcc)
                        grandParentAcc->GetRole(&role);
                }
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
        allKeyBinding = NS_LITERAL_STRING(";");

    //get shortcut
    nsAutoString keyBinding, subShortcut;
    rv = accWrap->GetKeyBinding(keyBinding);

    if (NS_SUCCEEDED(rv) && !keyBinding.IsEmpty()) {
        //change the shortcut from "Ctrl+Shift+L" to "<Control><Shift>L"
        PRInt32 oldPos, curPos=0;
        while ((curPos != -1) && (curPos < (PRInt32)keyBinding.Length())) {
            oldPos = curPos;
            nsAutoString subString;
            curPos = keyBinding.FindChar('+', oldPos);
            if (curPos == -1) {
                keyBinding.Mid(subString, oldPos, keyBinding.Length() - oldPos);
                subShortcut += subString;
            }
            else {
                keyBinding.Mid(subString, oldPos, curPos - oldPos);
      
                //change "Ctrl" to "Control"
                if (subString.EqualsIgnoreCase("ctrl"))
                    subString = NS_LITERAL_STRING("Control");
      
                subShortcut += NS_LITERAL_STRING("<") + subString +
                               NS_LITERAL_STRING(">");
                curPos++;
            }
        }
    }

    allKeyBinding += NS_LITERAL_STRING(";") + subShortcut;
    action->SetKeyBinding(allKeyBinding);
    return action->GetKeyBinding();
}

gboolean
setDescriptionCB(AtkAction *aAction, gint aActionIndex,
                 const gchar *aDesc)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aAction));
    NS_ENSURE_TRUE(accWrap, nsnull);

    /* this is not supported in nsIAccessible yet */
    return FALSE;
}
