/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mitesh Shah <mitesh@netscape.com>
 *   Chip Clark  <chipc@netscape.com>
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

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "nsReadConfig.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIAppStartup.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIAutoConfig.h"
#include "nsIComponentManager.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPromptService.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsToolkitCompsCID.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"
#include "prmem.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nspr.h"

extern PRLogModuleInfo *MCD;

extern nsresult EvaluateAdminConfigScript(const char *js_buffer, size_t length,
                                          const char *filename, 
                                          bool bGlobalContext, 
                                          bool bCallbacks, 
                                          bool skipFirstLine);
extern nsresult CentralizedAdminPrefManagerInit();
extern nsresult CentralizedAdminPrefManagerFinish();


static void DisplayError(void)
{
    nsresult rv;

    nsCOMPtr<nsIPromptService> promptService = do_GetService("@mozilla.org/embedcomp/prompt-service;1");
    if (!promptService)
        return;

    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if (!bundleService)
        return;

    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle("chrome://autoconfig/locale/autoconfig.properties",
                                getter_AddRefs(bundle));
    if (!bundle)
        return;

    nsXPIDLString title;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("readConfigTitle").get(), getter_Copies(title));
    if (NS_FAILED(rv))
        return;

    nsXPIDLString err;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("readConfigMsg").get(), getter_Copies(err));
    if (NS_FAILED(rv))
        return;

    promptService->Alert(nsnull, title.get(), err.get());
}

// nsISupports Implementation

NS_IMPL_THREADSAFE_ISUPPORTS2(nsReadConfig, nsIReadConfig, nsIObserver)

nsReadConfig::nsReadConfig() :
    mRead(PR_FALSE)
{
    if (!MCD)
      MCD = PR_NewLogModule("MCD");
}

nsresult nsReadConfig::Init()
{
    nsresult rv;
    
    nsCOMPtr<nsIObserverService> observerService = 
        do_GetService("@mozilla.org/observer-service;1", &rv);

    if (observerService) {
        rv = observerService->AddObserver(this, NS_PREFSERVICE_READ_TOPIC_ID, PR_FALSE);
    }
    return(rv);
}

nsReadConfig::~nsReadConfig()
{
    CentralizedAdminPrefManagerFinish();
}

NS_IMETHODIMP nsReadConfig::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;

    if (!nsCRT::strcmp(aTopic, NS_PREFSERVICE_READ_TOPIC_ID)) {
        rv = readConfigFile();
        if (NS_FAILED(rv)) {
            DisplayError();

            nsCOMPtr<nsIAppStartup> appStartup =
                do_GetService(NS_APPSTARTUP_CONTRACTID);
            if (appStartup)
                appStartup->Quit(nsIAppStartup::eAttemptQuit);
        }
    }
    return rv;
}


nsresult nsReadConfig::readConfigFile()
{
    nsresult rv = NS_OK;
    nsXPIDLCString lockFileName;
    nsXPIDLCString lockVendor;
    PRUint32 fileNameLen = 0;
    
    nsCOMPtr<nsIPrefBranch> defaultPrefBranch;
    nsCOMPtr<nsIPrefService> prefService = 
        do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = prefService->GetDefaultBranch(nsnull, getter_AddRefs(defaultPrefBranch));
    if (NS_FAILED(rv))
        return rv;
        
    // This preference is set in the all.js or all-ns.js (depending whether 
    // running mozilla or netscp6)

    rv = defaultPrefBranch->GetCharPref("general.config.filename", 
                                  getter_Copies(lockFileName));


    PR_LOG(MCD, PR_LOG_DEBUG, ("general.config.filename = %s\n", lockFileName.get()));
    if (NS_FAILED(rv))
        return rv;

    // This needs to be read only once.
    //
    if (!mRead) {
        // Initiate the new JS Context for Preference management
        
        rv = CentralizedAdminPrefManagerInit();
        if (NS_FAILED(rv))
            return rv;
        
        // Open and evaluate function calls to set/lock/unlock prefs
        rv = openAndEvaluateJSFile("prefcalls.js", 0, PR_FALSE, PR_FALSE);
        if (NS_FAILED(rv)) 
            return rv;

        // Evaluate platform specific directives
        rv = openAndEvaluateJSFile("platform.js", 0, PR_FALSE, PR_FALSE);
        if (NS_FAILED(rv)) 
            return rv;

        mRead = PR_TRUE;
    }
    // If the lockFileName is NULL return ok, because no lockFile will be used
  
  
    // Once the config file is read, we should check that the vendor name 
    // is consistent By checking for the vendor name after reading the config 
    // file we allow for the preference to be set (and locked) by the creator 
    // of the cfg file meaning the file can not be renamed (successfully).

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch(nsnull, getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 obscureValue = 0;
    (void) defaultPrefBranch->GetIntPref("general.config.obscure_value", &obscureValue);
    PR_LOG(MCD, PR_LOG_DEBUG, ("evaluating .cfg file %s with obscureValue %d\n", lockFileName.get(), obscureValue));
    rv = openAndEvaluateJSFile(lockFileName.get(), obscureValue, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv))
    {
      PR_LOG(MCD, PR_LOG_DEBUG, ("error evaluating .cfg file %s %x\n", lockFileName.get(), rv));
      return rv;
    }
    
    rv = prefBranch->GetCharPref("general.config.filename", 
                                  getter_Copies(lockFileName));
    if (NS_FAILED(rv))
        // There is NO REASON we should ever get here. This is POST reading 
        // of the config file.
        return NS_ERROR_FAILURE;

  
    rv = prefBranch->GetCharPref("general.config.vendor", 
                                  getter_Copies(lockVendor));
    // If vendor is not NULL, do this check
    if (NS_SUCCEEDED(rv)) {

        fileNameLen = PL_strlen(lockFileName);
    
        // lockVendor and lockFileName should be the same with the addtion of 
        // .cfg to the filename by checking this post reading of the cfg file 
        // this value can be set within the cfg file adding a level of security.
    
        if (PL_strncmp(lockFileName, lockVendor, fileNameLen - 4) != 0)
            return NS_ERROR_FAILURE;
    }
  
    // get the value of the autoconfig url
    nsXPIDLCString urlName;
    rv = prefBranch->GetCharPref("autoadmin.global_config_url",
                                  getter_Copies(urlName));
    if (NS_SUCCEEDED(rv) && !urlName.IsEmpty()) {

        // Instantiating nsAutoConfig object if the pref is present
        mAutoConfig = do_CreateInstance(NS_AUTOCONFIG_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return NS_ERROR_OUT_OF_MEMORY;

        rv = mAutoConfig->SetConfigURL(urlName);
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;

    }
  
    return NS_OK;
} // ReadConfigFile


nsresult nsReadConfig::openAndEvaluateJSFile(const char *aFileName, PRInt32 obscureValue,
                                             bool isEncoded,
                                             bool isBinDir)
{
    nsresult rv;

    nsCOMPtr<nsIInputStream> inStr;
    if (isBinDir) {
        nsCOMPtr<nsIFile> jsFile;
        rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, 
                                    getter_AddRefs(jsFile));
        if (NS_FAILED(rv)) 
            return rv;

        rv = jsFile->AppendNative(nsDependentCString(aFileName));
        if (NS_FAILED(rv)) 
            return rv;

        rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), jsFile);
        if (NS_FAILED(rv)) 
            return rv;

    } else {
        nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
        if (NS_FAILED(rv)) 
            return rv;

        nsCAutoString location("resource://gre/defaults/autoconfig/");
        location += aFileName;

        nsCOMPtr<nsIURI> uri;
        rv = ioService->NewURI(location, nsnull, nsnull, getter_AddRefs(uri));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIChannel> channel;
        rv = ioService->NewChannelFromURI(uri, getter_AddRefs(channel));
        if (NS_FAILED(rv))
            return rv;

        rv = channel->Open(getter_AddRefs(inStr));
        if (NS_FAILED(rv)) 
            return rv;
    }

    PRUint32 fs, amt = 0;
    inStr->Available(&fs);

    char *buf = (char *)PR_Malloc(fs * sizeof(char));
    if (!buf) 
        return NS_ERROR_OUT_OF_MEMORY;

    rv = inStr->Read(buf, fs, &amt);
    NS_ASSERTION((amt == fs), "failed to read the entire configuration file!!");
    if (NS_SUCCEEDED(rv)) {
        if (obscureValue > 0) {

            // Unobscure file by subtracting some value from every char. 
            for (PRUint32 i = 0; i < amt; i++)
                buf[i] -= obscureValue;
        }
        rv = EvaluateAdminConfigScript(buf, amt, aFileName,
                                       PR_FALSE, PR_TRUE,
                                       isEncoded ? PR_TRUE:PR_FALSE);
    }
    inStr->Close();
    PR_Free(buf);
    
    return rv;
}
