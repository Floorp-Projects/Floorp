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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 */

#include "nsIServiceManager.h"
#include "nsICategoryManager.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"

#include "nsContentHTTPStartup.h"
#include "nsIHTTPProtocolHandler.h"
#include "gbdate.h"

#define PRODUCT_NAME NS_LITERAL_STRING("Gecko").get()

NS_IMPL_ISUPPORTS1(nsContentHTTPStartup,nsIObserver)

nsresult
nsContentHTTPStartup::Observe( nsISupports *aSubject,
                              const PRUnichar *aTopic,
                              const PRUnichar *aData)
{
    if (nsCRT::strcmp(aTopic, NS_HTTP_STARTUP_TOPIC) != 0)
        return NS_OK;
    
    nsresult rv = nsnull;

    nsCOMPtr<nsIHTTPProtocolHandler> http(do_QueryInterface(aSubject));
    if (NS_FAILED(rv)) return rv;
    
    rv = http->SetProduct(PRODUCT_NAME);
    if (NS_FAILED(rv)) return rv;
    
    rv = http->SetProductSub(PRODUCT_VERSION);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

nsresult
nsContentHTTPStartup::RegisterHTTPStartup()
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager>
        catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString previousEntry;
    rv = catMan->AddCategoryEntry(NS_HTTP_STARTUP_CATEGORY,
                                  "Content UserAgent Setter",
                                  NS_CONTENTHTTPSTARTUP_CONTRACTID,
                                  PR_TRUE, PR_TRUE,
                                  getter_Copies(previousEntry));
    return rv;
}

nsresult
nsContentHTTPStartup::UnregisterHTTPStartup()
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager>
        catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  
    if (NS_FAILED(rv)) return rv;


    return NS_OK;
}
