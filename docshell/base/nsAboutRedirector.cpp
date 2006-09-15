/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gagan Saksena (original author)
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

#include "nsAboutRedirector.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIScriptSecurityManager.h"

NS_IMPL_ISUPPORTS1(nsAboutRedirector, nsIAboutModule)

struct RedirEntry {
    const char* id;
    const char* url;
    PRBool dropChromePrivs; // if PR_TRUE, the page will not have chrome privileges
};

/*
  Entries with dropChromePrivs == PR_FALSE will run with chrome
  privileges. This is potentially dangerous. Please use PR_TRUE
  as the third argument to each map item below unless your about:
  page really needs chrome privileges. Security review is required
  before adding new map entries with dropChromePrivs == PR_FALSE.
 */
static RedirEntry kRedirMap[] = {
    { "credits", "http://www.mozilla.org/credits/", PR_TRUE },
    { "mozilla", "chrome://global/content/mozilla.xhtml", PR_TRUE },
    { "plugins", "chrome://communicator/content/plugins.html", PR_FALSE },
    { "config", "chrome://global/content/config.xul", PR_FALSE },
    { "logo", "chrome://global/content/logo.gif", PR_TRUE },
    { "buildconfig", "chrome://global/content/buildconfig.html", PR_TRUE },
    { "about", "chrome://global/content/aboutAbout.html", PR_FALSE }
};
static const int kRedirTotal = NS_ARRAY_LENGTH(kRedirMap);

NS_IMETHODIMP
nsAboutRedirector::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    NS_ASSERTION(aURI, "must not be null");
    NS_ASSERTION(result, "must not be null");

    nsresult rv;

    nsCAutoString path;
    rv = aURI->GetPath(path);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    if (NS_FAILED(rv))
        return rv;

    for (int i=0; i<kRedirTotal; i++) 
    {
        if (!PL_strcasecmp(path.get(), kRedirMap[i].id))
        {
            nsCOMPtr<nsIChannel> tempChannel;
            rv = ioService->NewChannel(nsDependentCString(kRedirMap[i].url),
                                       nsnull, nsnull, getter_AddRefs(tempChannel));
            if (NS_FAILED(rv))
                return rv;

            // Keep the page from getting unnecessary privileges unless it needs them
            if (kRedirMap[i].dropChromePrivs)
            {
                nsCOMPtr<nsIScriptSecurityManager> securityManager = 
                         do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
                if (NS_FAILED(rv))
                    return rv;
            
                nsCOMPtr<nsIPrincipal> principal;
                rv = securityManager->GetCodebasePrincipal(aURI, getter_AddRefs(principal));
                if (NS_FAILED(rv))
                    return rv;
            
                rv = tempChannel->SetOwner(principal);
                if (NS_FAILED(rv))
                    return rv;
            }

            NS_ADDREF(*result = tempChannel);
            return rv;
        }
    }

    NS_ERROR("nsAboutRedirector called for unknown case");
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
