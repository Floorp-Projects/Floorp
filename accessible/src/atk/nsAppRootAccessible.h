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
 *   Ginn Chen (ginn.chen@sun.com)
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

#ifndef __NS_APP_ROOT_ACCESSIBLE_H__
#define __NS_APP_ROOT_ACCESSIBLE_H__

#include "nsIMutableArray.h"
#include "nsIAccessibleDocument.h"
#include "nsAccessibilityService.h"
#include "nsAccessibleWrap.h"
#include "nsRootAccessibleWrap.h"

#define MAI_TYPE_APP_ROOT (MAI_TYPE_ATK_OBJECT)

/* nsAppRootAccessible is for the whole application of Mozilla.
 * Only one instance of nsAppRootAccessible exists for one Mozilla instance.
 * And this one should be created when Mozilla Startup (if accessibility
 * feature has been enabled) and destroyed when Mozilla Shutdown.
 *
 * All the accessibility objects for toplevel windows are direct children of
 * the nsAppRootAccessible instance.
 */
class nsAppRootAccessible;
class nsAppRootAccessible: public nsAccessibleWrap
{
public:
    virtual ~nsAppRootAccessible();

    static nsAppRootAccessible *Create();
    static void Load();
    static void Unload();

public:
    nsAppRootAccessible();

    /* virtual function from nsAccessNode */
    NS_IMETHOD Init();

    /* virtual functions from nsAccessible */
    NS_IMETHOD GetName(nsAString & aName);
    NS_IMETHOD GetDescription(nsAString & aDescription);
    NS_IMETHOD GetRole(PRUint32 *aRole);
    NS_IMETHOD GetFinalRole(PRUint32 *aFinalRole);
    NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
    NS_IMETHOD GetParent(nsIAccessible * *aParent);
    NS_IMETHOD GetNextSibling(nsIAccessible * *aNextSibling);
    NS_IMETHOD GetPreviousSibling(nsIAccessible **aPreviousSibling);
    NS_IMETHOD GetChildAt(PRInt32 aChildNum, nsIAccessible **aChild);

    // return the atk object for app root accessible
    NS_IMETHOD GetNativeInterface(void **aOutAccessible);

    nsresult AddRootAccessible(nsIAccessible *aRootAccWrap);
    nsresult RemoveRootAccessible(nsIAccessible *aRootAccWrap);

protected:
    virtual void CacheChildren();

private:
    nsCOMPtr<nsIMutableArray> mChildren;
};

#endif   /* __NS_APP_ROOT_ACCESSIBLE_H__ */
