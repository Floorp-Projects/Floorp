/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReadConfig.h"
#include "nsJSConfigTriggers.h"

#include "mozilla/Components.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIAppStartup.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIComponentManager.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPromptService.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nspr.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

extern bool sandboxEnabled;

extern mozilla::LazyLogModule MCD;

extern nsresult CentralizedAdminPrefManagerInit(bool aSandboxEnabled);
extern nsresult CentralizedAdminPrefManagerFinish();

static nsresult DisplayError(void) {
  nsresult rv;

  nsCOMPtr<nsIPromptService> promptService =
      do_GetService("@mozilla.org/embedcomp/prompt-service;1");
  if (!promptService) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!bundleService) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(
      "chrome://autoconfig/locale/autoconfig.properties",
      getter_AddRefs(bundle));
  if (!bundle) return NS_ERROR_FAILURE;

  nsAutoString title;
  rv = bundle->GetStringFromName("readConfigTitle", title);
  if (NS_FAILED(rv)) return rv;

  nsAutoString err;
  rv = bundle->GetStringFromName("readConfigMsg", err);
  if (NS_FAILED(rv)) return rv;

  return promptService->Alert(nullptr, title.get(), err.get());
}

// nsISupports Implementation

NS_IMPL_ISUPPORTS(nsReadConfig, nsIObserver)

nsReadConfig::nsReadConfig() : mRead(false) {}

nsresult nsReadConfig::Init() {
  nsresult rv;

  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1", &rv);

  if (observerService) {
    rv =
        observerService->AddObserver(this, NS_PREFSERVICE_READ_TOPIC_ID, false);
  }
  return (rv);
}

nsReadConfig::~nsReadConfig() { CentralizedAdminPrefManagerFinish(); }

NS_IMETHODIMP nsReadConfig::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* someData) {
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, NS_PREFSERVICE_READ_TOPIC_ID)) {
    rv = readConfigFile();
    // Don't show error alerts if the sandbox is enabled, just show
    // sandbox warning.
    if (NS_FAILED(rv)) {
      if (sandboxEnabled) {
        nsContentUtils::ReportToConsoleNonLocalized(
            NS_LITERAL_STRING("Autoconfig is sandboxed by default. See "
                              "https://support.mozilla.org/products/"
                              "firefox-enterprise for more information."),
            nsIScriptError::warningFlag, NS_LITERAL_CSTRING("autoconfig"),
            nullptr);
      } else {
        rv = DisplayError();
        if (NS_FAILED(rv)) {
          nsCOMPtr<nsIAppStartup> appStartup =
              components::AppStartup::Service();
          if (appStartup) appStartup->Quit(nsIAppStartup::eAttemptQuit);
        }
      }
    }
  }
  return rv;
}

/**
 * This is the blocklist for known bad autoconfig files.
 */
static const char* gBlockedConfigs[] = {"dsengine.cfg"};

nsresult nsReadConfig::readConfigFile() {
  nsresult rv = NS_OK;
  nsAutoCString lockFileName;
  nsAutoCString lockVendor;
  uint32_t fileNameLen = 0;

  nsCOMPtr<nsIPrefBranch> defaultPrefBranch;
  nsCOMPtr<nsIPrefService> prefService =
      do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv =
      prefService->GetDefaultBranch(nullptr, getter_AddRefs(defaultPrefBranch));
  if (NS_FAILED(rv)) return rv;

  NS_NAMED_LITERAL_CSTRING(channel, NS_STRINGIFY(MOZ_UPDATE_CHANNEL));

  bool sandboxEnabled =
      channel.EqualsLiteral("beta") || channel.EqualsLiteral("release");

  mozilla::Unused << defaultPrefBranch->GetBoolPref(
      "general.config.sandbox_enabled", &sandboxEnabled);

  rv = defaultPrefBranch->GetCharPref("general.config.filename", lockFileName);

  if (NS_FAILED(rv)) return rv;

  MOZ_LOG(MCD, LogLevel::Debug,
          ("general.config.filename = %s\n", lockFileName.get()));

  for (size_t index = 0, len = mozilla::ArrayLength(gBlockedConfigs);
       index < len; ++index) {
    if (lockFileName == gBlockedConfigs[index]) {
      // This is NS_OK because we don't want to show an error to the user
      return rv;
    }
  }

  // This needs to be read only once.
  //
  if (!mRead) {
    // Initiate the new JS Context for Preference management

    rv = CentralizedAdminPrefManagerInit(sandboxEnabled);
    if (NS_FAILED(rv)) return rv;

    // Open and evaluate function calls to set/lock/unlock prefs
    rv = openAndEvaluateJSFile("prefcalls.js", 0, false, false);
    if (NS_FAILED(rv)) return rv;

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
  (void)defaultPrefBranch->GetIntPref("general.config.obscure_value",
                                      &obscureValue);
  MOZ_LOG(MCD, LogLevel::Debug,
          ("evaluating .cfg file %s with obscureValue %d\n", lockFileName.get(),
           obscureValue));
  rv = openAndEvaluateJSFile(lockFileName.get(), obscureValue, true, true);
  if (NS_FAILED(rv)) {
    MOZ_LOG(MCD, LogLevel::Debug,
            ("error evaluating .cfg file %s %" PRIx32 "\n", lockFileName.get(),
             static_cast<uint32_t>(rv)));
    return rv;
  }

  rv = prefBranch->GetCharPref("general.config.filename", lockFileName);
  if (NS_FAILED(rv))
    // There is NO REASON we should ever get here. This is POST reading
    // of the config file.
    return NS_ERROR_FAILURE;

  rv = prefBranch->GetCharPref("general.config.vendor", lockVendor);
  // If vendor is not nullptr, do this check
  if (NS_SUCCEEDED(rv)) {
    fileNameLen = strlen(lockFileName.get());

    // lockVendor and lockFileName should be the same with the addtion of
    // .cfg to the filename by checking this post reading of the cfg file
    // this value can be set within the cfg file adding a level of security.

    if (PL_strncmp(lockFileName.get(), lockVendor.get(), fileNameLen - 4) != 0)
      return NS_ERROR_FAILURE;
  }

  // get the value of the autoconfig url
  nsAutoCString urlName;
  rv = prefBranch->GetCharPref("autoadmin.global_config_url", urlName);
  if (NS_SUCCEEDED(rv) && !urlName.IsEmpty()) {
    // Instantiating nsAutoConfig object if the pref is present
    mAutoConfig = new nsAutoConfig();

    rv = mAutoConfig->Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mAutoConfig->SetConfigURL(urlName.get());
  }

  return NS_OK;
}  // ReadConfigFile

nsresult nsReadConfig::openAndEvaluateJSFile(const char* aFileName,
                                             int32_t obscureValue,
                                             bool isEncoded, bool isBinDir) {
  nsresult rv;

  nsCOMPtr<nsIInputStream> inStr;
  if (isBinDir) {
    nsCOMPtr<nsIFile> jsFile;
    rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(jsFile));
    if (NS_FAILED(rv)) return rv;

    rv = jsFile->AppendNative(nsDependentCString(aFileName));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), jsFile);
    if (NS_FAILED(rv)) return rv;

  } else {
    nsAutoCString location("resource://gre/defaults/autoconfig/");
    location += aFileName;

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), location);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = channel->Open(getter_AddRefs(inStr));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  uint64_t fs64;
  uint32_t amt = 0;
  rv = inStr->Available(&fs64);
  if (NS_FAILED(rv)) return rv;
  // This used to use PR_Malloc(), which doesn't support over 4GB.
  if (fs64 > UINT32_MAX) return NS_ERROR_FILE_TOO_BIG;
  uint32_t fs = (uint32_t)fs64;

  char* buf = (char*)malloc(fs * sizeof(char));
  if (!buf) return NS_ERROR_OUT_OF_MEMORY;

  rv = inStr->Read(buf, (uint32_t)fs, &amt);
  NS_ASSERTION((amt == fs), "failed to read the entire configuration file!!");
  if (NS_SUCCEEDED(rv)) {
    if (obscureValue > 0) {
      // Unobscure file by subtracting some value from every char.
      for (uint32_t i = 0; i < amt; i++) buf[i] -= obscureValue;
    }
    rv = EvaluateAdminConfigScript(buf, amt, aFileName, false, true, isEncoded,
                                   !isBinDir);
  }
  inStr->Close();
  free(buf);

  return rv;
}
