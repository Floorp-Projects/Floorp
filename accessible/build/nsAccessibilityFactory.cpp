/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessibleRetrieval.h"
#include "nscore.h"

static nsresult
NS_ConstructAccessibilityService(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nullptr, "no aggregation");
    nsIAccessibilityService* accessibility;
    rv = NS_GetAccessibilityService(&accessibility);
    if (NS_FAILED(rv)) {
        NS_ERROR("Unable to construct accessibility service");
        return rv;
    }
    rv = accessibility->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(accessibility);
    return rv;
}

NS_DEFINE_NAMED_CID(NS_ACCESSIBILITY_SERVICE_CID);

static const mozilla::Module::CIDEntry kA11yCIDs[] = {
    { &kNS_ACCESSIBILITY_SERVICE_CID, false, nullptr, NS_ConstructAccessibilityService },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kA11yContracts[] = {
    { "@mozilla.org/accessibilityService;1", &kNS_ACCESSIBILITY_SERVICE_CID },
    { "@mozilla.org/accessibleRetrieval;1", &kNS_ACCESSIBILITY_SERVICE_CID },
    { nullptr }
};

static const mozilla::Module kA11yModule = {
    mozilla::Module::kVersion,
    kA11yCIDs,
    kA11yContracts
};

NSMODULE_DEFN(nsAccessibilityModule) = &kA11yModule;


