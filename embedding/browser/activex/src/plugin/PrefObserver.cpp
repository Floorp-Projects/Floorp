/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "StdAfx.h"

#include <Wininet.h>

#include "npapi.h"
#include "nsIServiceManagerUtils.h"
#include "nsISupportsUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "nsCRT.h"
#include "nsString.h"

#include "XPConnect.h"

// These are the default hosting flags in the absence of a pref.

const PRUint32 kDefaultHostingFlags =
#ifdef XPC_IDISPATCH_SUPPORT
    nsIActiveXSecurityPolicy::HOSTING_FLAGS_HOST_NOTHING;
#else
    nsIActiveXSecurityPolicy::HOSTING_FLAGS_HOST_SAFE_OBJECTS |
    nsIActiveXSecurityPolicy::HOSTING_FLAGS_DOWNLOAD_CONTROLS |
    nsIActiveXSecurityPolicy::HOSTING_FLAGS_SCRIPT_SAFE_OBJECTS;
#endif

class PrefObserver :
    public nsSupportsWeakReference,
    public nsIObserver
{
public:
    PrefObserver();

protected:
    virtual ~PrefObserver();

    void Sync(nsIPrefBranch *aPrefBranch);
    
    PRUint32 mHostingFlags;
    nsCOMPtr<nsIPrefBranchInternal> mPrefBranch;
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    static PrefObserver *sPrefObserver;

    nsresult Subscribe();
    nsresult Unsubscribe();
    PRUint32 GetHostingFlags() const;
};

const char *kActiveXHostingFlags = "security.xpconnect.activex.";
const char *kUserAgentPref = "general.useragent.";
const char *kProxyPref = "network.http.";

PrefObserver *PrefObserver::sPrefObserver = nsnull;

PrefObserver::PrefObserver() :
    mHostingFlags(kDefaultHostingFlags)
{
    nsresult rv = NS_OK;
    mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ASSERTION(mPrefBranch, "where is the pref service?");
}

PrefObserver::~PrefObserver()
{
}

NS_IMPL_ADDREF(PrefObserver)
NS_IMPL_RELEASE(PrefObserver)

NS_INTERFACE_MAP_BEGIN(PrefObserver)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP PrefObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
    if (nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic) != 0)
    {
        return S_OK;
    }

    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject, &rv);
    if (NS_FAILED(rv))
        return rv;

    NS_ConvertUTF16toUTF8 pref(aData);
    if (nsCRT::strcmp(kActiveXHostingFlags, pref.get()) == 0 ||
        nsCRT::strcmp(kUserAgentPref, pref.get()) == 0 ||
        nsCRT::strcmp(kProxyPref, pref.get()) == 0)
    {
        Sync(prefBranch);
    }
    return NS_OK;
}

void PrefObserver::Sync(nsIPrefBranch *aPrefBranch)
{
    NS_ASSERTION(aPrefBranch, "no pref branch");
    if (!aPrefBranch)
    {
        return;
    }

    // TODO
    // const char *userAgent = NPN_UserAgent(mData->pPluginInstance);
	// ::UrlMkSetSessionOption(URLMON_OPTION_USERAGENT, userAgent, strlen(userAgent), 0);	

    // TODO
    // INTERNET_PROXY_INFO ipi;
    // ::UrlMkSetSessionOption(INTERNET_OPTION_PROXY, ....);

    nsCOMPtr<nsIDispatchSupport> dispSupport = do_GetService(NS_IDISPATCH_SUPPORT_CONTRACTID);
    if (!dispSupport)
        mHostingFlags = kDefaultHostingFlags;
    else
        dispSupport->GetHostingFlags(nsnull, &mHostingFlags);
}

nsresult
PrefObserver::Subscribe()
{
    NS_ENSURE_TRUE(mPrefBranch, NS_ERROR_FAILURE);

    mPrefBranch->AddObserver(kProxyPref, this, PR_TRUE);
    mPrefBranch->AddObserver(kUserAgentPref, this, PR_TRUE);
    mPrefBranch->AddObserver(kActiveXHostingFlags, this, PR_TRUE);

    Sync(mPrefBranch);

    return S_OK;
}

nsresult
PrefObserver::Unsubscribe()
{
    NS_ENSURE_TRUE(mPrefBranch, NS_ERROR_FAILURE);

    mPrefBranch->RemoveObserver(kProxyPref, this);
    mPrefBranch->RemoveObserver(kUserAgentPref, this);
    mPrefBranch->RemoveObserver(kActiveXHostingFlags, this);

    return NS_OK;
}

PRUint32 PrefObserver::GetHostingFlags() const
{
    return mHostingFlags;
}

///////////////////////////////////////////////////////////////////////////////

PRUint32 MozAxPlugin::PrefGetHostingFlags()
{
    if (!PrefObserver::sPrefObserver)
    {
        PrefObserver::sPrefObserver = new PrefObserver();
        if (!PrefObserver::sPrefObserver)
        {
            return nsIActiveXSecurityPolicy::HOSTING_FLAGS_HOST_NOTHING;
        }
        PrefObserver::sPrefObserver->AddRef();
        PrefObserver::sPrefObserver->Subscribe();
    }
    return PrefObserver::sPrefObserver->GetHostingFlags();
}

void MozAxPlugin::ReleasePrefObserver()
{
    if (PrefObserver::sPrefObserver)
    {
        PrefObserver::sPrefObserver->Unsubscribe();
        PrefObserver::sPrefObserver->Release();
        PrefObserver::sPrefObserver = nsnull;
    }
}

