/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIAccessibilityService.h"
#include "nscore.h"

static NS_IMETHODIMP
NS_ConstructAccessibilityService(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    nsIAccessibilityService* accessibility;
    rv = NS_NewAccessibilityService(&accessibility);
    if (NS_FAILED(rv)) {
        NS_ERROR("Unable to construct chrome registry");
        return rv;
    }
    rv = accessibility->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(accessibility);
    return rv;
}

// The list of components we register
static nsModuleComponentInfo components[] = 
{
    { "AccessibilityService", 
      NS_ACCESSIBILITY_SERVICE_CID,
      "@mozilla.org/accessibilityService;1", 
      NS_ConstructAccessibilityService
    },
};

NS_IMPL_NSGETMODULE(nsAccessibilityModule, components);

