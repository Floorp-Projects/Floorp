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
 * The Original Code is The Browser Profile Migrator.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
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

#include "nsProfileMigrator.h"

#include "nsIBrowserProfileMigrator.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindow.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIProperties.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIMutableArray.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"
#include "nsIWindowWatcher.h"

#include "nsCOMPtr.h"
#include "nsBrowserCompsCID.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsServiceManagerUtils.h"

#include "nsStringAPI.h"
#include "nsUnicharUtils.h"
#ifdef XP_WIN
#include <windows.h>
#include "nsIWindowsRegKey.h"
#include "nsILocalFileWin.h"
#else
#include <limits.h>
#endif

#include "nsAutoPtr.h"

///////////////////////////////////////////////////////////////////////////////
// nsIProfileMigrator

#define MIGRATION_WIZARD_FE_URL "chrome://browser/content/migration/migration.xul"
#define MIGRATION_WIZARD_FE_FEATURES "chrome,dialog,modal,centerscreen,titlebar"

NS_IMETHODIMP
nsProfileMigrator::Migrate(nsIProfileStartup* aStartup)
{
  nsresult rv;

  nsCAutoString key;
  nsCOMPtr<nsIBrowserProfileMigrator> bpm;

  rv = GetDefaultBrowserMigratorKey(key, bpm);
  if (NS_FAILED(rv)) return rv;

  if (!bpm) {
    nsCAutoString contractID(NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX);
    contractID.Append(key);

    bpm = do_CreateInstance(contractID.get());
    if (!bpm) return NS_ERROR_FAILURE;
  }

  bool sourceExists;
  bpm->GetSourceExists(&sourceExists);
  if (!sourceExists) {
#ifdef XP_WIN
    // The "Default Browser" key in the registry was set to a browser for which
    // no profile data exists. On Windows, this means the Default Browser settings
    // in the registry are bad, and we should just fall back to IE in this case.
    bpm = do_CreateInstance(NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "ie");
#else
    return NS_ERROR_FAILURE;
#endif
  }

  nsCOMPtr<nsISupportsCString> cstr
    (do_CreateInstance("@mozilla.org/supports-cstring;1"));
  if (!cstr) return NS_ERROR_OUT_OF_MEMORY;
  cstr->SetData(key);

  // By opening the Migration FE with a supplied bpm, it will automatically
  // migrate from it. 
  nsCOMPtr<nsIWindowWatcher> ww(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsIMutableArray> params = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!ww || !params) return NS_ERROR_FAILURE;

  params->AppendElement(cstr, false);
  params->AppendElement(bpm, false);
  params->AppendElement(aStartup, false);

  nsCOMPtr<nsIDOMWindow> migrateWizard;
  return ww->OpenWindow(nsnull, 
                        MIGRATION_WIZARD_FE_URL,
                        "_blank",
                        MIGRATION_WIZARD_FE_FEATURES,
                        params,
                        getter_AddRefs(migrateWizard));
}

///////////////////////////////////////////////////////////////////////////////
// nsProfileMigrator

NS_IMPL_ISUPPORTS1(nsProfileMigrator, nsIProfileMigrator)

#ifdef XP_WIN

#define INTERNAL_NAME_IEXPLORE        "iexplore"
#define INTERNAL_NAME_MOZILLA_SUITE   "apprunner"
#define INTERNAL_NAME_CHROME          "chrome"
#define INTERNAL_NAME_FIREFOX         "firefox"
#endif

nsresult
nsProfileMigrator::GetDefaultBrowserMigratorKey(nsACString& aKey,
                                                nsCOMPtr<nsIBrowserProfileMigrator>& bpm)
{
#if XP_WIN

  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey)
    return NS_ERROR_FAILURE;

  NS_NAMED_LITERAL_STRING(kCommandKey,
                          "SOFTWARE\\Classes\\HTTP\\shell\\open\\command");

  if (NS_FAILED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                             kCommandKey, nsIWindowsRegKey::ACCESS_READ)))
    return NS_ERROR_FAILURE;

  nsAutoString value;
  if (NS_FAILED(regKey->ReadStringValue(EmptyString(), value)))
    return NS_ERROR_FAILURE;

  PRInt32 len = value.Find(NS_LITERAL_STRING(".exe"), CaseInsensitiveCompare);
  if (len == -1)
    return NS_ERROR_FAILURE;

  // Move past ".exe"
  len += 4;

  PRUint32 start = 0;
  // skip an opening quotation mark if present
  if (value.get()[1] != ':') {
    start = 1;
    --len;
  }

  const nsDependentSubstring filePath(Substring(value, start, len)); 

  // We want to find out what the default browser is but the path in and of itself
  // isn't enough. Why? Because sometimes on Windows paths get truncated like so:
  // C:\PROGRA~1\MOZILL~2\MOZILL~1.EXE
  // How do we know what product that is? Mozilla or Mozilla Firebird? etc. Mozilla's
  // file objects do nothing to 'normalize' the path so we need to attain an actual
  // product descriptor from the file somehow, and in this case it means getting
  // the "InternalName" field of the file's VERSIONINFO resource. 
  //
  // In the file's resource segment there is a VERSIONINFO section that is laid 
  // out like this:
  //
  // VERSIONINFO
  //   StringFileInfo
  //     <TranslationID>
  //       InternalName           "iexplore"
  //   VarFileInfo
  //     Translation              <TranslationID>
  //
  // By Querying the VERSIONINFO section for its Tranlations, we can find out where
  // the InternalName lives. (A file can have more than one translation of its 
  // VERSIONINFO segment, but we just assume the first one). 

  nsCOMPtr<nsILocalFile> lf;
  NS_NewLocalFile(filePath, true, getter_AddRefs(lf));
  if (!lf)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILocalFileWin> lfw = do_QueryInterface(lf); 
  if (!lfw)
    return NS_ERROR_FAILURE;

  nsAutoString internalName;
  if (NS_FAILED(lfw->GetVersionInfoField("InternalName", internalName)))
    return NS_ERROR_FAILURE;

  if (internalName.LowerCaseEqualsLiteral(INTERNAL_NAME_IEXPLORE)) {
    aKey = "ie";
    return NS_OK;
  }
  else if (internalName.LowerCaseEqualsLiteral(INTERNAL_NAME_CHROME)) {
    aKey = "chrome";
    return NS_OK;
  }
  else if (internalName.LowerCaseEqualsLiteral(INTERNAL_NAME_FIREFOX)) {
    aKey = "firefox";
    return NS_OK;
  }

#else
  bool exists = false;
#define CHECK_MIGRATOR(browser) do {\
  bpm = do_CreateInstance(NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX browser);\
  if (bpm)\
    bpm->GetSourceExists(&exists);\
  if (exists) {\
    aKey = browser;\
    return NS_OK;\
  }} while(0)

#if defined(XP_MACOSX)
  CHECK_MIGRATOR("safari");
#endif
  CHECK_MIGRATOR("chrome");
  CHECK_MIGRATOR("firefox");

#undef CHECK_MIGRATOR
#endif
  return NS_ERROR_FAILURE;
}
