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
#include "nsString.h"

const char sSysPrefString[] = "config.use_system_prefs";
union MozPrefValue {
    char *      stringVal;
    PRInt32     intVal;
    PRBool      boolVal;
};

struct SysPrefItem {
    const char *prefName;       // mozilla pref string name
    MozPrefValue defaultValue;  // store the mozilla default value
    PRBool isLocked;  // store the mozilla lock status
    SysPrefItem() {
        prefName = nsnull;
        defaultValue.intVal = 0;
        isLocked = PR_FALSE;
    }
    void SetPrefName(const char *aPrefName) {
        prefName = aPrefName;
    }
};

// all prefs that mozilla need to read from host system if they are available
static const char *sSysPrefList[] = {
    "network.proxy.http",
    "network.proxy.http_port",
    "network.proxy.ftp",
    "network.proxy.ftp_port",
    "config.use_system_prefs.accessibility",
};

PRLogModuleInfo *gSysPrefLog = NULL;

NS_IMPL_ISUPPORTS2(nsSystemPref, nsIObserver, nsISupportsWeakReference)

nsSystemPref::nsSystemPref():
    mSysPrefService(nsnull),
    mEnabled(PR_FALSE),
    mSysPrefs(nsnull)
{
}

nsSystemPref::~nsSystemPref()
{
    mSysPrefService = nsnull;
    mEnabled = PR_FALSE;
    delete [] mSysPrefs;
}

///////////////////////////////////////////////////////////////////////////////
// nsSystemPref::Init
// Setup log and listen on NS_PREFSERVICE_READ_TOPIC_ID from pref service
///////////////////////////////////////////////////////////////////////////////
nsresult
nsSystemPref::Init(void)
{
    nsresult rv;

    if (!gSysPrefLog) {
        gSysPrefLog = PR_NewLogModule("Syspref");
        if (!gSysPrefLog)
            return NS_ERROR_OUT_OF_MEMORY;
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

///////////////////////////////////////////////////////////////////////////////
// nsSystemPref::Observe
// Observe notifications from mozilla pref system and system prefs (if enabled)
///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsSystemPref::Observe(nsISupports *aSubject,
                      const char *aTopic,
                      const PRUnichar *aData)
{
    nsresult rv = NS_OK;

    if (!aTopic)
        return NS_OK;

    // if we are notified by pref service
    // check the system pref settings
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

        rv = prefBranch->GetBoolPref(sSysPrefString, &mEnabled);
        if (NS_FAILED(rv)) {
            SYSPREF_LOG(("...FAil to Get %s\n", sSysPrefString));
            return rv;
        }

        // if there is no system pref service, assume nothing happen to us
        mSysPrefService = do_GetService(NS_SYSTEMPREF_SERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv) || !mSysPrefService) {
            SYSPREF_LOG(("...No System Pref Service\n"));
            return NS_OK;
        }

        // listen on its changes
        nsCOMPtr<nsIPrefBranchInternal>
            prefBranchInternal(do_QueryInterface(prefBranch));
        rv = prefBranchInternal->AddObserver(sSysPrefString, this, PR_TRUE);
        if (NS_FAILED(rv)) {
            SYSPREF_LOG(("...FAil to add observer for %s\n", sSysPrefString));
            return rv;
        }

        if (!mEnabled) {
            SYSPREF_LOG(("%s is disabled\n", sSysPrefString));
            return NS_OK;
        }
        SYSPREF_LOG(("%s is enabled\n", sSysPrefString));
        rv = UseSystemPrefs();

    }
    // sSysPrefString value was changed, update ...
    else if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) &&
             NS_ConvertUTF8toUCS2(sSysPrefString).Equals(aData)) {
        SYSPREF_LOG(("++++++ Notify: topic=%s data=%s\n",
                     aTopic, NS_ConvertUCS2toUTF8(aData).get()));

        nsCOMPtr<nsIPrefBranch> prefBranch;
        nsCOMPtr<nsIPrefService> prefService =
            do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return rv;

        rv = prefService->GetBranch(nsnull, getter_AddRefs(prefBranch));
        if (NS_FAILED(rv))
            return rv;

        PRBool enabled = mEnabled;
        rv = prefBranch->GetBoolPref(sSysPrefString, &mEnabled);
        if (enabled != mEnabled) {
            if (mEnabled)
                //read prefs from system
                rv = UseSystemPrefs();
            else
                //roll back to mozilla prefs
                rv = UseMozillaPrefs();
        }
    }

    // if the system pref notify us that some pref has been changed by user
    // outside mozilla. We need to read it again.
    else if (!nsCRT::strcmp(aTopic, NS_SYSTEMPREF_PREFCHANGE_TOPIC_ID) &&
             aData) {
        NS_ASSERTION(mEnabled == PR_TRUE, "Should not listen when disabled");
        SYSPREF_LOG(("====== System Pref Notify topic=%s data=%s\n",
                     aTopic, (char*)aData));
        rv = ReadSystemPref(NS_LossyConvertUCS2toASCII(aData).get());
        return NS_OK;
    }
    else
        SYSPREF_LOG(("Not needed topic Received %s\n", aTopic));
    return rv;
}

/* private */

////////////////////////////////////////////////////////////////
// nsSystemPref::UseSystemPrefs
// Read all the prefs in the table from system, listen for their
// changes in system pref service.
////////////////////////////////////////////////////////////////
nsresult
nsSystemPref::UseSystemPrefs()
{
    SYSPREF_LOG(("\n====Now Use system prefs==\n"));
    nsresult rv = NS_OK;
    if (!mSysPrefService) {
        return NS_ERROR_FAILURE;
    }

    PRIntn sysPrefCount= sizeof(sSysPrefList) / sizeof(sSysPrefList[0]);

    if (!mSysPrefs) {
        mSysPrefs = new SysPrefItem[sysPrefCount];
        if (!mSysPrefs)
            return NS_ERROR_OUT_OF_MEMORY;
        for (PRIntn index = 0; index < sysPrefCount; ++index)
            mSysPrefs[index].SetPrefName(sSysPrefList[index]);
    }

    nsCOMPtr<nsIPrefBranchInternal>
        sysPrefBranchInternal(do_QueryInterface(mSysPrefService));
    if (!sysPrefBranchInternal)
        return NS_ERROR_FAILURE;

    for (PRIntn index = 0; index < sysPrefCount; ++index) {
        // save mozilla prefs
        SaveMozDefaultPref(mSysPrefs[index].prefName,
                           &mSysPrefs[index].defaultValue,
                           &mSysPrefs[index].isLocked);

        // get the system prefs
        ReadSystemPref(mSysPrefs[index].prefName);
        SYSPREF_LOG(("Add Listener on %s\n", mSysPrefs[index].prefName));
        sysPrefBranchInternal->AddObserver(mSysPrefs[index].prefName,
                                           this, PR_TRUE);
    }
    return rv;
}

//////////////////////////////////////////////////////////////////////
// nsSystemPref::ReadSystemPref
// Read a pref value from system pref service, and lock it in mozilla.
//////////////////////////////////////////////////////////////////////
nsresult
nsSystemPref::ReadSystemPref(const char *aPrefName)
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

    prefBranch->UnlockPref(aPrefName);

    PRInt32 prefType = nsIPrefBranch::PREF_INVALID;
    nsXPIDLCString strValue;
    PRInt32 intValue = 0;
    PRBool boolValue = PR_FALSE;

    rv = prefBranch->GetPrefType(aPrefName, &prefType);
    if (NS_FAILED(rv))
        return rv;
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

//////////////////////////////////////////////////////////////////////
// nsSystemPref::UseMozillaPrefs
// Restore mozilla default prefs, remove system pref listeners
/////////////////////////////////////////////////////////////////////
nsresult
nsSystemPref::UseMozillaPrefs()
{
    nsresult rv = NS_OK;
    SYSPREF_LOG(("\n====Now rollback to Mozilla prefs==\n"));

    // if we did not use system prefs, do nothing
    if (!mSysPrefService)
        return NS_OK;

    nsCOMPtr<nsIPrefBranchInternal>
        sysPrefBranchInternal(do_QueryInterface(mSysPrefService));
    if (!sysPrefBranchInternal)
        return NS_ERROR_FAILURE;

    PRIntn sysPrefCount= sizeof(sSysPrefList) / sizeof(sSysPrefList[0]);
    for (PRIntn index = 0; index < sysPrefCount; ++index) {
        // restore mozilla default value and free string memory if needed
        RestoreMozDefaultPref(mSysPrefs[index].prefName,
                              &mSysPrefs[index].defaultValue,
                              mSysPrefs[index].isLocked);
        SYSPREF_LOG(("stop listening on %s\n", mSysPrefs[index].prefName));
        sysPrefBranchInternal->RemoveObserver(mSysPrefs[index].prefName,
                                              this);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////
// nsSystemPref::RestoreMozDefaultPref
// Save the saved mozilla default value.
// It is also responsible for allocate the string memory when needed, because
// this method know what type of value is stored.
/////////////////////////////////////////////////////////////////////////////
nsresult
nsSystemPref::SaveMozDefaultPref(const char *aPrefName,
                                 MozPrefValue *aPrefValue,
                                 PRBool *aLocked)
{
    NS_ENSURE_ARG_POINTER(aPrefName);
    NS_ENSURE_ARG_POINTER(aPrefValue);
    NS_ENSURE_ARG_POINTER(aLocked);

    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService =
        do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetDefaultBranch(nsnull, getter_AddRefs(prefBranch));
    if (NS_FAILED(rv))
        return rv;

    SYSPREF_LOG(("Save Mozilla value for %s\n", aPrefName));

    PRInt32 prefType = nsIPrefBranch::PREF_INVALID;
    nsXPIDLCString strValue;

    rv = prefBranch->GetPrefType(aPrefName, &prefType);
    if (NS_FAILED(rv))
        return rv;
    switch (prefType) {
    case nsIPrefBranch::PREF_STRING:
        prefBranch->GetCharPref(aPrefName,
                                getter_Copies(strValue));
        SYSPREF_LOG(("Mozilla value is %s", strValue.get()));

        if (aPrefValue->stringVal)
            PL_strfree(aPrefValue->stringVal);
        aPrefValue->stringVal = PL_strdup(strValue.get());
        break;
    case nsIPrefBranch::PREF_INT:
        prefBranch->GetIntPref(aPrefName, &aPrefValue->intVal);
        SYSPREF_LOG(("Mozilla value is %d\n", aPrefValue->intVal));

        break;
    case nsIPrefBranch::PREF_BOOL:
        prefBranch->GetBoolPref(aPrefName, &aPrefValue->boolVal);
        SYSPREF_LOG(("Mozilla value is %s\n",
                     aPrefValue->boolVal ? "TRUE" : "FALSE"));

        break;
    default:
        SYSPREF_LOG(("Fail to Read Mozilla value for it\n"));
        return NS_ERROR_FAILURE;
    }
    rv = prefBranch->PrefIsLocked(aPrefName, aLocked);
    SYSPREF_LOG((" (%s).\n", aLocked ? "Locked" : "NOT Locked"));
    return rv;
}

////////////////////////////////////////////////////////////////////////////
// nsSystemPref::RestoreMozDefaultPref
// Restore the saved mozilla default value to pref service.
// It is also responsible for free the string memory when needed, because
// this method know what type of value is stored.
/////////////////////////////////////////////////////////////////////////////
nsresult
nsSystemPref::RestoreMozDefaultPref(const char *aPrefName,
                                    MozPrefValue *aPrefValue,
                                    PRBool aLocked)
{
    NS_ENSURE_ARG_POINTER(aPrefName);

    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService =
        do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetDefaultBranch(nsnull, getter_AddRefs(prefBranch));
    if (NS_FAILED(rv))
        return rv;

    SYSPREF_LOG(("Restore Mozilla value for %s\n", aPrefName));

    PRInt32 prefType = nsIPrefBranch::PREF_INVALID;
    rv = prefBranch->GetPrefType(aPrefName, &prefType);
    if (NS_FAILED(rv))
        return rv;

    // unlock, if it is locked
    prefBranch->UnlockPref(aPrefName);

    switch (prefType) {
    case nsIPrefBranch::PREF_STRING:
        prefBranch->SetCharPref(aPrefName,
                                aPrefValue->stringVal);
        SYSPREF_LOG(("Mozilla value is %s\n", aPrefValue->stringVal));

        PL_strfree(aPrefValue->stringVal);
        aPrefValue->stringVal = nsnull;

        break;
    case nsIPrefBranch::PREF_INT:
        prefBranch->SetIntPref(aPrefName, aPrefValue->intVal);
        SYSPREF_LOG(("Mozilla value is %d\n", aPrefValue->intVal));

        break;
    case nsIPrefBranch::PREF_BOOL:
        prefBranch->SetBoolPref(aPrefName, aPrefValue->boolVal);
        SYSPREF_LOG(("Mozilla value is %s\n",
                     aPrefValue->boolVal ? "TRUE" : "FALSE"));

        break;
    default:
        SYSPREF_LOG(("Fail to Restore Mozilla value for it\n"));
        return NS_ERROR_FAILURE;
    }

    // restore its old lock status
    if (aLocked)
        prefBranch->LockPref(aPrefName);
    return NS_OK;
}
