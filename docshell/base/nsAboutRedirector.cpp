/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *      Gagan Saksena (original author)
 */

#include "nsAboutRedirector.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "nsIScriptSecurityManager.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

NS_IMPL_ISUPPORTS1(nsAboutRedirector, nsIAboutModule)

// if you add something here, make sure you update the total
static const char *kRedirMap[][2] = {
    { "credits", "http://www.mozilla.org/credits/" },
    { "mozilla", "chrome://global/content/mozilla.html" },
    { "plugins", "chrome://global/content/plugins.html" }
};
static const int kRedirTotal = 3; // sizeof(kRedirMap)/sizeof(*kRedirMap)

NS_IMETHODIMP
nsAboutRedirector::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    NS_ENSURE_ARG(aURI);
    nsXPIDLCString path;
    (void)aURI->GetPath(getter_Copies(path));
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, ioService, kIOServiceCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    static const char kChromePrefix[] = "chrome:";
    for (int i = 0; i< kRedirTotal; i++) 
    {
        if (!PL_strcasecmp(path, kRedirMap[i][0]))
        {
            nsCOMPtr<nsIChannel> tempChannel;
             rv = ioService->NewChannel(kRedirMap[i][1], nsnull, getter_AddRefs(tempChannel));
             //-- If we're redirecting to a chrome URL, change the owner of the channel
             //   to keep the page from getting unnecessary privileges.
             if (NS_SUCCEEDED(rv) && result &&
                 !PL_strncasecmp(kRedirMap[i][1], kChromePrefix, sizeof(kChromePrefix)-1))
             {
                  NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                  NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
                  if (NS_FAILED(rv))
                      return rv;
            
                  nsCOMPtr<nsIPrincipal> principal;
                  rv = securityManager->GetCodebasePrincipal(aURI, getter_AddRefs(principal));
                  if (NS_FAILED(rv))
                      return rv;
            
                  nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
                  rv = tempChannel->SetOwner(owner);
             }
             *result = tempChannel.get();
             NS_ADDREF(*result);
             return rv;
        }

    }
    NS_ASSERTION(0, "nsAboutRedirector called for unknown case");
    return NS_ERROR_ILLEGAL_VALUE;
}

NS_METHOD
nsAboutRedirector::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsAboutRedirector* about = new nsAboutRedirector();
    if (about == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
