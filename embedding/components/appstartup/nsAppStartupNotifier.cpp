/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsAppStartupNotifier.h"
#include "nsISimpleEnumerator.h"

NS_IMPL_ISUPPORTS(nsAppStartupNotifier, nsIObserver)

nsAppStartupNotifier::nsAppStartupNotifier()
{
}

nsAppStartupNotifier::~nsAppStartupNotifier()
{
}

NS_IMETHODIMP nsAppStartupNotifier::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *someData)
{
    NS_ENSURE_ARG(aTopic);
    nsresult rv;

    // now initialize all startup listeners
    nsCOMPtr<nsICategoryManager> categoryManager =
                    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = categoryManager->EnumerateCategory(aTopic,
                               getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> entry;
    while (NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(entry)))) {
        nsCOMPtr<nsISupportsCString> category = do_QueryInterface(entry, &rv);

        if (NS_SUCCEEDED(rv)) {
            nsAutoCString categoryEntry;
            rv = category->GetData(categoryEntry);

            nsXPIDLCString contractId;
            categoryManager->GetCategoryEntry(aTopic, 
                                              categoryEntry.get(),
                                              getter_Copies(contractId));

            if (NS_SUCCEEDED(rv)) {

                // If we see the word "service," in the beginning
                // of the contractId then we create it as a service
                // if not we do a createInstance

                nsCOMPtr<nsISupports> startupInstance;
                if (Substring(contractId, 0, 8).EqualsLiteral("service,"))
                    startupInstance = do_GetService(contractId.get() + 8, &rv);
                else
                    startupInstance = do_CreateInstance(contractId, &rv);

                if (NS_SUCCEEDED(rv)) {
                    // Try to QI to nsIObserver
                    nsCOMPtr<nsIObserver> startupObserver =
                        do_QueryInterface(startupInstance, &rv);
                    if (NS_SUCCEEDED(rv)) {
                        rv = startupObserver->Observe(nullptr, aTopic, nullptr);
     
                        // mainly for debugging if you want to know if your observer worked.
                        NS_ASSERTION(NS_SUCCEEDED(rv), "Startup Observer failed!\n");
                    }
                }
                else {
                  #ifdef DEBUG
                    nsAutoCString warnStr("Cannot create startup observer : ");
                    warnStr += contractId.get();
                    NS_WARNING(warnStr.get());
                  #endif
                }

            }
        }
    }

    return NS_OK;
}
