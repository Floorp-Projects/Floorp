/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsIGenericFactory.h"
#include "nsXFormsElementFactory.h"
#include "nsXFormsUtilityService.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsModelElement.h"
#include "nsXFormsUtils.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"
#include "nsIClassInfoImpl.h"
#include "nsXFormsXPathFunctions.h"

// bb0d9c8b-3096-4b66-92a0-6c1ddf80e65f
#define NS_XFORMSUTILITYSERVICE_CID \
{ 0xbb0d9c8b, 0x3096, 0x4b66, { 0x92, 0xa0, 0x6c, 0x1d, 0xdf, 0x80, 0xe6, 0x5f }}

#define NS_XFORMSUTILITYSERVICE_CONTRACTID \
"@mozilla.org/xforms-utility-service;1"

#define XFORMSXPATHFUNCTIONS_CID \
{ 0x8edc8cf1, 0x69a3, 0x11d9, { 0x97, 0x91, 0x00, 0x0a, 0x95, 0xdc, 0x23, 0x4c } }
#define XFORMSXPATHFUNCTIONS_CONTRACTID \
"@mozilla.org/xforms-xpath-functions;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsXFormsElementFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXFormsUtilityService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXFormsXPathFunctions)
NS_DECL_CLASSINFO(nsXFormsXPathFunctions)

static NS_IMETHODIMP
RegisterXFormsModule(nsIComponentManager *aCompMgr,
                     nsIFile *aPath,
                     const char *aRegistryLocation,
                     const char *aComponentType,
                     const nsModuleComponentInfo *aInfo)
{
#ifdef DEBUG
  printf("XFORMS Module: Registering\n");
#endif

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);

  if (!catman)
    return NS_ERROR_FAILURE;

  nsXPIDLCString previous;
  nsresult rv =
    catman->AddCategoryEntry(NS_DOMNS_FEATURE_PREFIX "org.w3c.xforms.dom",
                             "1.0",
                             NS_XTF_ELEMENT_FACTORY_CONTRACTID_PREFIX NS_NAMESPACE_XFORMS,
                             PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv =
    catman->AddCategoryEntry(NS_DOMNS_FEATURE_PREFIX NS_XFORMS_INSTANCE_OWNER,
                             "1.0",
                             NS_XTF_ELEMENT_FACTORY_CONTRACTID_PREFIX NS_NAMESPACE_XFORMS,
                             PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  return catman->AddCategoryEntry("agent-style-sheets",
                                  "xforms stylesheet",
                                  "chrome://xforms/content/xforms.css",
                                  PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_IMETHODIMP
UnregisterXFormsModule(nsIComponentManager *aCompMgr,
                       nsIFile *aPath,
                       const char *aRegistryLocation,
                       const nsModuleComponentInfo *aInfo)
{
#ifdef DEBUG
  printf("XFORMS Module: Unregistering\n");
#endif

  nsXFormsUtils::Shutdown();

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);

  if (!catman)
    return NS_ERROR_FAILURE;

  catman->DeleteCategoryEntry(NS_DOMNS_FEATURE_PREFIX "org.w3c.xforms.dom",
                              "1.0", PR_TRUE);

  catman->DeleteCategoryEntry(NS_DOMNS_FEATURE_PREFIX NS_XFORMS_INSTANCE_OWNER,
                              "1.0", PR_TRUE);

  return catman->DeleteCategoryEntry("agent-style-sheets",
                                     "xforms stylesheet", PR_TRUE);
}

static const nsModuleComponentInfo components[] = {
  { "XForms element factory",
    NS_XFORMSELEMENTFACTORY_CID,
    NS_XTF_ELEMENT_FACTORY_CONTRACTID_PREFIX NS_NAMESPACE_XFORMS,
    nsXFormsElementFactoryConstructor,
    RegisterXFormsModule,
    UnregisterXFormsModule
  },
  { "XForms Utility Service",
    NS_XFORMSUTILITYSERVICE_CID,
    NS_XFORMSUTILITYSERVICE_CONTRACTID,
    nsXFormsUtilityServiceConstructor
  },
  { "XForms XPath extension functions",
    XFORMSXPATHFUNCTIONS_CID,
    XFORMSXPATHFUNCTIONS_CONTRACTID,
    nsXFormsXPathFunctionsConstructor,
    nsnull, nsnull, nsnull,
    NS_CI_INTERFACE_GETTER_NAME(nsXFormsXPathFunctions),
    nsnull,
    &NS_CLASSINFO_NAME(nsXFormsXPathFunctions)
  }
};

PR_STATIC_CALLBACK(nsresult)
XFormsModuleCtor(nsIModule* aSelf)
{
  nsXFormsAtoms::InitAtoms();
  nsXFormsUtils::Init();
  nsXFormsModelElement::Startup();
  return NS_OK;
}

NS_IMPL_NSGETMODULE_WITH_CTOR(xforms, components, XFormsModuleCtor)
