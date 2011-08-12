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
#include "nsAboutProtocolUtils.h"

NS_IMPL_ISUPPORTS1(nsAboutRedirector, nsIAboutModule)

struct RedirEntry {
    const char* id;
    const char* url;
    PRUint32 flags;  // See nsIAboutModule.  The URI_SAFE_FOR_UNTRUSTED_CONTENT
                     // flag does double duty here -- if it's not set, we don't
                     // drop chrome privileges.
};

/*
  Entries which do not have URI_SAFE_FOR_UNTRUSTED_CONTENT will run with chrome
  privileges. This is potentially dangerous. Please use
  URI_SAFE_FOR_UNTRUSTED_CONTENT in the third argument to each map item below
  unless your about: page really needs chrome privileges. Security review is
  required before adding new map entries without
  URI_SAFE_FOR_UNTRUSTED_CONTENT.  Also note, however, that adding
  URI_SAFE_FOR_UNTRUSTED_CONTENT will allow random web sites to link to that
  URI.  Perhaps we should separate the two concepts out...
 */
static RedirEntry kRedirMap[] = {
    { "", "chrome://global/content/about.xhtml",
      nsIAboutModule::ALLOW_SCRIPT },
    { "about", "chrome://global/content/aboutAbout.xhtml", 0 },
    { "credits", "http://www.mozilla.org/credits/",
      nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT },
    { "mozilla", "chrome://global/content/mozilla.xhtml",
      nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT },
    { "plugins", "chrome://global/content/plugins.html", 0 },
    { "config", "chrome://global/content/config.xul", 0 },
#ifdef MOZ_CRASHREPORTER
    { "crashes", "chrome://global/content/crashes.xhtml", 0 },
#endif
    { "logo", "chrome://branding/content/about.png",
      nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT},
    { "buildconfig", "chrome://global/content/buildconfig.html",
      nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT },
    { "license", "chrome://global/content/license.html",
      nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT },
    { "neterror", "chrome://global/content/netError.xhtml",
      nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
      nsIAboutModule::ALLOW_SCRIPT |
      nsIAboutModule::HIDE_FROM_ABOUTABOUT },
    { "memory", "chrome://global/content/aboutMemory.xhtml",
      nsIAboutModule::ALLOW_SCRIPT },
    { "addons", "chrome://mozapps/content/extensions/extensions.xul",
      nsIAboutModule::ALLOW_SCRIPT },
    { "newaddon", "chrome://mozapps/content/extensions/newaddon.xul",
      nsIAboutModule::ALLOW_SCRIPT },
    { "support", "chrome://global/content/aboutSupport.xhtml",
      nsIAboutModule::ALLOW_SCRIPT }
};
static const int kRedirTotal = NS_ARRAY_LENGTH(kRedirMap);

NS_IMETHODIMP
nsAboutRedirector::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ASSERTION(result, "must not be null");

    nsresult rv;

    nsCAutoString path;
    rv = NS_GetAboutModuleName(aURI, path);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    if (NS_FAILED(rv))
        return rv;

    for (int i=0; i<kRedirTotal; i++) 
    {
        if (!strcmp(path.get(), kRedirMap[i].id))
        {
            nsCOMPtr<nsIChannel> tempChannel;
            rv = ioService->NewChannel(nsDependentCString(kRedirMap[i].url),
                                       nsnull, nsnull, getter_AddRefs(tempChannel));
            if (NS_FAILED(rv))
                return rv;

            tempChannel->SetOriginalURI(aURI);

            // Keep the page from getting unnecessary privileges unless it needs them
            if (kRedirMap[i].flags &
                nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT)
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

NS_IMETHODIMP
nsAboutRedirector::GetURIFlags(nsIURI *aURI, PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(aURI);

    nsCAutoString name;
    nsresult rv = NS_GetAboutModuleName(aURI, name);
    NS_ENSURE_SUCCESS(rv, rv);

    for (int i=0; i < kRedirTotal; i++) 
    {
        if (name.EqualsASCII(kRedirMap[i].id))
        {
            *result = kRedirMap[i].flags;
            return NS_OK;
        }
    }

    NS_ERROR("nsAboutRedirector called for unknown case");
    return NS_ERROR_ILLEGAL_VALUE;
}

nsresult
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
