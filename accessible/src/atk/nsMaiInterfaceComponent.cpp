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

#include "nsMaiInterfaceComponent.h"
#include "nsAccessibleWrap.h"

G_BEGIN_DECLS

/* component interface callbacks */
static void interfaceInitCB(AtkComponentIface *aIface);
static AtkObject *refAccessibleAtPointCB(AtkComponent *aComponent,
                                         gint aAccX, gint aAccY,
                                         AtkCoordType aCoordType);
static void getExtentsCB(AtkComponent *aComponent,
                         gint *aAccX, gint *aAccY,
                         gint *aAccWidth, gint *aAccHeight,
                         AtkCoordType aCoordType);
/* the "contains", "get_position", "get_size" can take advantage of
 * "get_extents", there is no need to implement them now.
 */
static gboolean grabFocusCB(AtkComponent *aComponent);

/* what are missing now for atk component */

/* ==================================================
 * add_focus_handler
 * remove_focus_handler
 * set_extents
 * set_position
 * set_size
 * get_layer
 * get_mdi_zorder
 * ==================================================
 */
G_END_DECLS

MaiInterfaceComponent::MaiInterfaceComponent(nsAccessibleWrap *aAccWrap):
    MaiInterface(aAccWrap)
{
}

MaiInterfaceComponent::~MaiInterfaceComponent()
{
}

MaiInterfaceType
MaiInterfaceComponent::GetType()
{
    return MAI_INTERFACE_COMPONENT;
}

const GInterfaceInfo *
MaiInterfaceComponent::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_component_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_component_info;
}

/* static functions */

void
interfaceInitCB(AtkComponentIface *aIface)
{
    NS_ASSERTION(aIface, "Invalid Interface");
    if(!aIface)
        return;

    /*
     * Use default implementation in atk for contains, get_position,
     * and get_size
     */
    aIface->ref_accessible_at_point = refAccessibleAtPointCB;
    aIface->get_extents = getExtentsCB;
    aIface->grab_focus = grabFocusCB;
}

AtkObject *
refAccessibleAtPointCB(AtkComponent *aComponent,
                       gint aAccX, gint aAccY,
                       AtkCoordType aCoordType)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aComponent));
    NS_ENSURE_TRUE(accWrap, nsnull);

    // or ATK_XY_SCREEN  what is definition this in nsIAccessible ???
    if (aCoordType == ATK_XY_WINDOW) {
        /* deal with the coord type */
    }

    nsCOMPtr<nsIAccessible> pointAcc;
    nsresult rv = accWrap->GetChildAtPoint(aAccX, aAccY, getter_AddRefs(pointAcc));
    if (NS_FAILED(rv))
        return nsnull;

    nsIAccessible *tmpAcc = pointAcc;
    nsAccessibleWrap *tmpAccWrap =
        NS_STATIC_CAST(nsAccessibleWrap *, tmpAcc);
    AtkObject *atkObj = tmpAccWrap->GetAtkObject();
    if (!atkObj)
        return nsnull;
    g_object_ref(atkObj);
    return atkObj;
}

void
getExtentsCB(AtkComponent *aComponent,
             gint *aAccX,
             gint *aAccY,
             gint *aAccWidth,
             gint *aAccHeight,
             AtkCoordType aCoordType)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aComponent));
    if (!accWrap)
        return;

    PRInt32 nsAccX, nsAccY, nsAccWidth, nsAccHeight;
    nsresult rv = accWrap->GetBounds(&nsAccX, &nsAccY,
                                           &nsAccWidth, &nsAccHeight);
    if (NS_FAILED(rv))
        return;
    // or ATK_XY_SCREEN  what is definition this in nsIAccessible?
    if (aCoordType == ATK_XY_WINDOW) {
        /* deal with the coord type */
    }

    *aAccX = nsAccX;
    *aAccY = nsAccY;
    *aAccWidth = nsAccWidth;
    *aAccHeight = nsAccHeight;
}

gboolean
grabFocusCB(AtkComponent *aComponent)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aComponent));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsresult rv = accWrap->TakeFocus();
    return (NS_FAILED(rv)) ? FALSE : TRUE;
}
