/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#include "nsEmbedChromeRegistry.h"
#include "nsString.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS1(nsEmbedChromeRegistry, nsIChromeRegistry)

nsEmbedChromeRegistry::nsEmbedChromeRegistry()
{
    NS_INIT_ISUPPORTS();
}

nsresult
nsEmbedChromeRegistry::Init()
{
    nsresult rv;
    
    rv = NS_NewISupportsArray(getter_AddRefs(mEmptyArray));
    if (NS_FAILED(rv)) return rv;


    return NS_OK;
}

NS_IMETHODIMP
nsEmbedChromeRegistry::CheckForNewChrome()
{
    return NS_OK;
}

NS_IMETHODIMP
nsEmbedChromeRegistry::Canonify(nsIURI* aChromeURI)
{
#if 0
  // Canonicalize 'chrome:' URLs. We'll take any 'chrome:' URL
  // without a filename, and change it to a URL -with- a filename;
  // e.g., "chrome://navigator/content" to
  // "chrome://navigator/content/navigator.xul".
  if (! aChromeURI)
      return NS_ERROR_NULL_POINTER;

  PRBool modified = PR_TRUE; // default is we do canonification
  nsCAutoString package, provider, file;
  nsresult rv;
  rv = SplitURL(aChromeURI, package, provider, file, &modified);
  if (NS_FAILED(rv))
    return rv;

  if (!modified)
    return NS_OK;

  nsCAutoString canonical( kChromePrefix );
  canonical += package;
  canonical += "/";
  canonical += provider;
  canonical += "/";
  canonical += file;

  return aChromeURI->SetSpec(canonical);
#else
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsEmbedChromeRegistry::ConvertChromeURL(nsIURI* aChromeURL, char** aResult)
{
    nsresult rv;
    
    nsCAutoString finalURL;
    rv = aChromeURL->GetSpec(finalURL);
    if (NS_FAILED(rv)) return rv;
    
    *aResult = ToNewCString(finalURL);
    if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;
    
    return NS_OK;
}

NS_IMETHODIMP
nsEmbedChromeRegistry::GetStyleSheets(nsIURI* aChromeURL, nsISupportsArray** aResult)
{
    *aResult = mEmptyArray;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsEmbedChromeRegistry::GetAgentSheets(nsIDocShell* aDocShell,
                                      nsISupportsArray** aResult)
{
    *aResult = mEmptyArray;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsEmbedChromeRegistry::GetUserSheets(PRBool aUseChromeSheets,
                                     nsISupportsArray** aResult)
{
    *aResult = mEmptyArray;
    NS_ADDREF(*aResult);
    return NS_OK;
}
