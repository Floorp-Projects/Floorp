/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s):
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

#include "nsSystemPref.h"
#include "nsIObserverService.h"

#include "nsSystemPrefLog.h"
#include "nsSystemPrefService.h"

// all prefs that mozilla need to read from host system if they are available
static const char* sSysPrefList[] = {
    "network.proxy.http",
    "network.proxy.http_port",
};

PRLogModuleInfo *gSysPrefLog = NULL;

NS_IMPL_ISUPPORTS2(nsSystemPref, nsIObserver, nsISupportsWeakReference)

nsSystemPref::nsSystemPref()
{
    mSysPrefService = nsnull;
}

nsSystemPref::~nsSystemPref()
{
    mSysPrefService = nsnull;
}

nsresult nsSystemPref::Init(void)
{
    nsresult rv;

    if (!gSysPrefLog) {
        gSysPrefLog = PR_NewLogModule("Syspref");
        PR_ASSERT(gSysPrefLog);
    }

    nsCOMPtr<nsIObserverService> observerService = 
        do_GetService("@mozilla.org/observer-service;1", &rv);

    if (observerService) {
        rv = observerService->AddObserver(this, NS_PREFSERVICE_READ_TOPIC_ID,
                                          PR_FALSE);
        SYSPREF_LOG(("Add Observer for %s\n", NS_PREFSERVICE_READ_TOPIC_ID));
    }
    return(rv);
}

NS_IMETHODIMP nsSystemPref::Observe(nsISupports *aSubject,
                                    const char *aTopic,
                                    const PRUnichar *aData)
{
    nsresult rv = NS_OK;

    if (!aTopic)
        return NS_OK;

    //////////////////////////////////////////////////////////////////////
    // if we are notified by pref service
    // check the system pref settings, and read prefs from system
    //////////////////////////////////////////////////////////////////////
    if (!nsCRT::strcmp(aTopic, NS_PREFSERVICE_READ_TOPIC_ID)) {
        SYSPREF_LOG(("Observed: %s\n", aTopic));

        nsCOMPtr<nsIPrefBranch> prefBranch;
        nsCOMPtr<nsIPrefService> prefService = 
            do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return rv;

        rv = prefService->GetBranch(nsnull, getter_AddRefs(prefBranch));
        if (NS_FAILED(rv))
            return rv;

        PRBool enabled = PR_FALSE;
        rv = prefBranch->GetBoolPref("config.system-pref", &enabled);
        if (NS_FAILED(rv)) {
            SYSPREF_LOG(("...FAil to Get config.system-pref\n"));
            return rv;
        }

        if (!enabled) {
            SYSPREF_LOG(("...Config config.system-pref is disabled\n"));
            return NS_OK;
        }

        mSysPrefService = do_GetService(NS_SYSTEMPREF_SERVICE_CONTRACTID, &rv);
        if (mSysPrefService)
            rv = ReadSystemPrefs();
    }
    //////////////////////////////////////////////////////////////////////
    // if the system pref notify us that some pref has been changed by user
    // outside mozilla. We need to read it again.
    //////////////////////////////////////////////////////////////////////
    else if (!nsCRT::strcmp(aTopic, NS_SYSTEMPREF_PREFCHANGE_TOPIC_ID) &&
             aData) {
        SYSPREF_LOG(("======in Notify topic=%s data=%s\n", aTopic, aData));
        ReadSystemPref(NS_ConvertUCS2toUTF8(aData).get());
        return NS_OK;
    }
    else
        SYSPREF_LOG(("Not needed topic Received %s\n", aTopic));
    return rv;
}

/* private */

//////////////////////////////////////////////////////////////////////
// Read all the prefs in the table, listen for their changes in
// system pref service.
/////////////////////////////////////////////////////////////////////
nsresult nsSystemPref::ReadSystemPrefs()
{
    nsresult rv = NS_OK;
    if (!mSysPrefService)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrefBranchInternal>
        sysPrefBranchInternal(do_QueryInterface(mSysPrefService));
    if (!sysPrefBranchInternal)
        return NS_ERROR_FAILURE;

    PRInt16 sysPrefLen= sizeof(sSysPrefList) / sizeof(sSysPrefList[0]);
    for (PRInt16 index = 0; index < sysPrefLen; ++index) {
        ReadSystemPref(sSysPrefList[index]);
        SYSPREF_LOG(("Add Listener on %s\n", sSysPrefList[index]));
        sysPrefBranchInternal->AddObserver(sSysPrefList[index],
                                           this, PR_TRUE);
    }
    return rv;
}

//////////////////////////////////////////////////////////////////////
// Read a pref value from system pref service, and lock it in mozilla.
//////////////////////////////////////////////////////////////////////
nsresult nsSystemPref::ReadSystemPref(const char *aPrefName)
{
    if (!mSysPrefService)
        return NS_ERROR_FAILURE;
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService =
        do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetDefaultBranch(nsnull, getter_AddRefs(prefBranch));
    if (NS_FAILED(rv))
        return rv;

    SYSPREF_LOG(("about to read aPrefName %s\n", aPrefName));

    PRBool isLocked;
    prefBranch->PrefIsLocked(aPrefName, &isLocked);
    if (isLocked)
        prefBranch->UnlockPref(aPrefName);

    nsXPIDLCString strValue;
    PRInt32 intValue = 0;
    PRInt32 prefType = nsIPrefBranch::PREF_INVALID;
    PRBool boolValue = PR_FALSE;

    rv = prefBranch->GetPrefType(aPrefName, &prefType);
    switch (prefType) {
    case nsIPrefBranch::PREF_STRING:
        mSysPrefService->GetCharPref(aPrefName, getter_Copies(strValue));
        SYSPREF_LOG(("system value is %s\n", strValue.get()));

        prefBranch->SetCharPref(aPrefName, strValue.get());
        break;
    case nsIPrefBranch::PREF_INT:
        mSysPrefService->GetIntPref(aPrefName, &intValue);
        SYSPREF_LOG(("system value is %d\n", intValue));

        prefBranch->SetIntPref(aPrefName, intValue);
        break;
    case nsIPrefBranch::PREF_BOOL:
        mSysPrefService->GetBoolPref(aPrefName, &boolValue);
        SYSPREF_LOG(("system value is %s\n", boolValue ? "TRUE" : "FALSE"));

        prefBranch->SetBoolPref(aPrefName, boolValue);
        break;
    default:
        SYSPREF_LOG(("Fail to system value for it\n"));
        return NS_ERROR_FAILURE;
    }
    prefBranch->LockPref(aPrefName);
    return NS_OK;
}
