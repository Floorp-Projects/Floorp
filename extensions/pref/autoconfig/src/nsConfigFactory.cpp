/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Mitesh Shah <mitesh@netscape.com>
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

#include "nsIGenericFactory.h"
#include "nsAutoConfig.h"
#include "nsReadConfig.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#if defined(MOZ_LDAP_XPCOM)
#include "nsLDAPSyncQuery.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAutoConfig, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsReadConfig, Init)
#if defined(MOZ_LDAP_XPCOM)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPSyncQuery)
#endif

// Registering nsReadConfig module as part of the app-startup category to get 
// it instantiated.

static NS_METHOD 
RegisterReadConfig(nsIComponentManager *aCompMgr,
                   nsIFile *aPath,
                   const char *registryLocation,
                   const char *componentType,
                   const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = categoryManager->AddCategoryEntry("pref-config-startup", 
                                           "ReadConfig Module",
                                           NS_READCONFIG_CONTRACTID,
                                           PR_TRUE, PR_TRUE, nsnull);
  }
  return rv;
}

static NS_METHOD 
UnRegisterReadConfig(nsIComponentManager *aCompMgr,
                     nsIFile *aPath,
                     const char *registryLocation,
                     const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = categoryManager->DeleteCategoryEntry(APPSTARTUP_CATEGORY,
                                              "ReadConfig Module", PR_TRUE);
  }
  return rv;
}


// The list of components we register
static const nsModuleComponentInfo components[] = 
{
  { 
    NS_AUTOCONFIG_CLASSNAME, 
    NS_AUTOCONFIG_CID, 
    NS_AUTOCONFIG_CONTRACTID, 
    nsAutoConfigConstructor
  },
  { 
    NS_READCONFIG_CLASSNAME,
    NS_READCONFIG_CID,
    NS_READCONFIG_CONTRACTID,
    nsReadConfigConstructor,
    RegisterReadConfig,
    UnRegisterReadConfig
  },
#if defined(MOZ_LDAP_XPCOM)
  { 
    "LDAPSyncQuery module", 
    NS_LDAPSYNCQUERY_CID, 
    "@mozilla.org/ldapsyncquery;1", 
    nsLDAPSyncQueryConstructor
  },
#endif
};

NS_IMPL_NSGETMODULE(nsAutoConfigModule, components)
