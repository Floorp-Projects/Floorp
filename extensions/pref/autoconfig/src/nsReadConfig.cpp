/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"

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
    rv = bundle->GetStringFromName(MOZ_UTF16("readConfigTitle"), getter_Copies(title));
    if (NS_FAILED(rv))
        return;

    nsXPIDLString err;
    rv = bundle->GetStringFromName(MOZ_UTF16("readConfigMsg"), getter_Copies(err));
    if (NS_FAILED(rv))
        return;

    promptService->Alert(nullptr, title.get(), err.get());
}

// nsISupports Implementation

NS_IMPL_ISUPPORTS(nsReadConfig, nsIReadConfig, nsIObserver)

nsReadConfig::nsReadConfig() :
    mRead(false)
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
        rv = observerService->AddObserver(this, NS_PREFSERVICE_READ_TOPIC_ID, false);
    }
    return(rv);
}

nsReadConfig::~nsReadConfig()
{
    CentralizedAdminPrefManagerFinish();
}

NS_IMETHODIMP nsReadConfig::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *someData)
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
    uint32_t fileNameLen = 0;
    
    nsCOMPtr<nsIPrefBranch> defaultPrefBranch;
    nsCOMPtr<nsIPrefService> prefService = 
        do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = prefService->GetDefaultBranch(nullptr, getter_AddRefs(defaultPrefBranch));
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
        rv = openAndEvaluateJSFile("prefcalls.js", 0, false, false);
        if (NS_FAILED(rv)) 
            return rv;

        // Evaluate platform specific directives
        rv = openAndEvaluateJSFile("platform.js", 0, false, false);
        if (NS_FAILED(rv)) 
            return rv;

        mRead = true;
    }
    // If the lockFileName is nullptr return ok, because no lockFile will be used
  
  
    // Once the config file is read, we should check that the vendor name 
    // is consistent By checking for the vendor name after reading the config 
    // file we allow for the preference to be set (and locked) by the creator 
    // of the cfg file meaning the file can not be renamed (successfully).

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch(nullptr, getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t obscureValue = 0;
    (void) defaultPrefBranch->GetIntPref("general.config.obscure_value", &obscureValue);
    PR_LOG(MCD, PR_LOG_DEBUG, ("evaluating .cfg file %s with obscureValue %d\n", lockFileName.get(), obscureValue));
    rv = openAndEvaluateJSFile(lockFileName.get(), obscureValue, true, true);
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
    // If vendor is not nullptr, do this check
    if (NS_SUCCEEDED(rv)) {

        fileNameLen = strlen(lockFileName);
    
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


nsresult nsReadConfig::openAndEvaluateJSFile(const char *aFileName, int32_t obscureValue,
                                             bool isEncoded,
                                             bool isBinDir)
{
    nsresult rv;

    nsCOMPtr<nsIInputStream> inStr;
    if (isBinDir) {
        nsCOMPtr<nsIFile> jsFile;
        rv = NS_GetSpecialDirectory(NS_GRE_DIR,
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
        nsAutoCString location("resource://gre/defaults/autoconfig/");
        location += aFileName;

        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), location);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIChannel> channel;
        rv = NS_NewChannel(getter_AddRefs(channel),
                           uri,
                           nsContentUtils::GetSystemPrincipal(),
                           nsILoadInfo::SEC_NORMAL,
                           nsIContentPolicy::TYPE_OTHER);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = channel->Open(getter_AddRefs(inStr));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    uint64_t fs64;
    uint32_t amt = 0;
    rv = inStr->Available(&fs64);
    if (NS_FAILED(rv))
        return rv;
    // PR_Malloc dones't support over 4GB
    if (fs64 > UINT32_MAX)
      return NS_ERROR_FILE_TOO_BIG;
    uint32_t fs = (uint32_t)fs64;

    char *buf = (char *)PR_Malloc(fs * sizeof(char));
    if (!buf) 
        return NS_ERROR_OUT_OF_MEMORY;

    rv = inStr->Read(buf, (uint32_t)fs, &amt);
    NS_ASSERTION((amt == fs), "failed to read the entire configuration file!!");
    if (NS_SUCCEEDED(rv)) {
        if (obscureValue > 0) {

            // Unobscure file by subtracting some value from every char. 
            for (uint32_t i = 0; i < amt; i++)
                buf[i] -= obscureValue;
        }
        rv = EvaluateAdminConfigScript(buf, amt, aFileName,
                                       false, true,
                                       isEncoded ? true:false);
    }
    inStr->Close();
    PR_Free(buf);
    
    return rv;
}
