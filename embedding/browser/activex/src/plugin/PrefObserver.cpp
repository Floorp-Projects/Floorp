/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *   Adam Lock <adamlock@netscape.com>
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

#include "StdAfx.h"

#include <Wininet.h>

#include "npapi.h"
#include "nsIServiceManagerUtils.h"
#include "nsISupportsUtils.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"

class PrefObserver :
    public nsSupportsWeakReference,
    public nsIObserver
{
public:
    PrefObserver();
protected:
    virtual ~PrefObserver();

    static PrefObserver *sPrefObserver;
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    static nsresult Subscribe();
    static nsresult Unsubscribe();
};

PrefObserver *PrefObserver::sPrefObserver = nsnull;

PrefObserver::PrefObserver()
{
    NS_INIT_ISUPPORTS();
}

PrefObserver::~PrefObserver()
{
}

NS_IMPL_ADDREF(PrefObserver);
NS_IMPL_RELEASE(PrefObserver);

NS_INTERFACE_MAP_BEGIN(PrefObserver)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP PrefObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
    // const char *userAgent = NPN_UserAgent(mData->pPluginInstance);
	// ::UrlMkSetSessionOption(URLMON_OPTION_USERAGENT, userAgent, strlen(userAgent), 0);	
    // INTERNET_PROXY_INFO ipi;
    // UrlMkSetSessionOption(INTERNET_OPTION_PROXY, ....);
    return NS_OK;
}

nsresult
PrefObserver::Subscribe()
{
    if (sPrefObserver)
    {
        return NS_OK;
    }

    sPrefObserver = new PrefObserver();
    NS_ENSURE_TRUE(sPrefObserver, NS_ERROR_OUT_OF_MEMORY);
    sPrefObserver->AddRef();

    nsresult rv = NS_OK;
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefService->GetBranch(nsnull, getter_AddRefs(prefBranch));
    NS_ENSURE_TRUE(prefBranch, NS_ERROR_FAILURE);

    nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(prefBranch);
    NS_ENSURE_TRUE(pbi, NS_ERROR_FAILURE);

    pbi->AddObserver("network.http.proxy.", sPrefObserver, PR_TRUE);
    pbi->AddObserver("general.useragent.", sPrefObserver, PR_TRUE);

    return S_OK;
}

nsresult
PrefObserver::Unsubscribe()
{
    if (sPrefObserver)
    {
        sPrefObserver->Release();
        sPrefObserver = nsnull;
    }
    return NS_OK;
}
