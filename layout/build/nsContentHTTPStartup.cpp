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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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

#include "nsIServiceManager.h"
#include "nsICategoryManager.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"

#include "nsContentHTTPStartup.h"
#include "nsIHttpProtocolHandler.h"
#include "gbdate.h"

#define PRODUCT_NAME "Gecko"

NS_IMPL_ISUPPORTS1(nsContentHTTPStartup,nsIObserver)

nsresult
nsContentHTTPStartup::Observe( nsISupports *aSubject,
                              const PRUnichar *aTopic,
                              const PRUnichar *aData)
{
    if (nsCRT::strcmp(aTopic, NS_HTTP_STARTUP_TOPIC) != 0)
        return NS_OK;
    
    nsresult rv = nsnull;

    nsCOMPtr<nsIHttpProtocolHandler> http(do_QueryInterface(aSubject));
    if (NS_FAILED(rv)) return rv;
    
    rv = http->SetProduct(PRODUCT_NAME);
    if (NS_FAILED(rv)) return rv;
    
    rv = http->SetProductSub((char*) PRODUCT_VERSION);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

nsresult
nsContentHTTPStartup::RegisterHTTPStartup(nsIComponentManager*         aCompMgr,
                                          nsIFile*                     aPath,
                                          const char*                  aRegistryLocation,
                                          const char*                  aComponentType,
                                          const nsModuleComponentInfo* aInfo)
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
nsContentHTTPStartup::UnregisterHTTPStartup(nsIComponentManager*         aCompMgr,
                                            nsIFile*                     aPath,
                                            const char*                  aRegistryLocation,
                                            const nsModuleComponentInfo* aInfo)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager>
        catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  
    if (NS_FAILED(rv)) return rv;


    return NS_OK;
}
