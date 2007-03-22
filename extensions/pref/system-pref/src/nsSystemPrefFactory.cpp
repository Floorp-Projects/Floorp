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
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
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

#include "nsICategoryManager.h"
#include "nsIGenericFactory.h"
#include "nsSystemPref.h"
#include "nsSystemPrefService.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSystemPref, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSystemPrefService, Init)

// Registering nsSystemPref module as part of the app-startup category to get 
// it instantiated.

static NS_METHOD
RegisterSystemPref(nsIComponentManager *aCompMgr,
                   nsIFile *aPath,
                   const char *registryLocation,
                   const char *componentType,
                   const nsModuleComponentInfo *info)
{
    nsresult rv;

    nsCOMPtr<nsICategoryManager>
        categoryManager(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
        rv = categoryManager->AddCategoryEntry(APPSTARTUP_CATEGORY,
                                               "SystemPref Module",
                                               NS_SYSTEMPREF_CONTRACTID,
                                               PR_TRUE, PR_TRUE, nsnull);
    }

    return rv;
}

static NS_METHOD 
UnRegisterSystemPref(nsIComponentManager *aCompMgr,
                     nsIFile *aPath,
                     const char *registryLocation,
                     const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager>
        categoryManager(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
        rv = categoryManager->DeleteCategoryEntry(APPSTARTUP_CATEGORY,
                                                  "SystemPref Module",
                                                  PR_TRUE);
    }
    return rv;
}

static const nsModuleComponentInfo components[] = {
    { NS_SYSTEMPREF_CLASSNAME,
      NS_SYSTEMPREF_CID,
      NS_SYSTEMPREF_CONTRACTID,
      nsSystemPrefConstructor,
      RegisterSystemPref,
      UnRegisterSystemPref,
    },
    { NS_SYSTEMPREF_SERVICE_CLASSNAME,
      NS_SYSTEMPREF_SERVICE_CID,
      NS_SYSTEMPREF_SERVICE_CONTRACTID,
      nsSystemPrefServiceConstructor,
    },
};

NS_IMPL_NSGETMODULE(nsSystemPrefModule, components)
