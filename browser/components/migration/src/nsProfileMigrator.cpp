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

#include "nsIBookmarksService.h"
#include "nsIBrowserProfileMigrator.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsArray.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"
#include "nsIWindowWatcher.h"

#include "nsCOMPtr.h"
#include "nsBrowserCompsCID.h"
#include "nsDirectoryServiceDefs.h"

#include "nsCRT.h"
#include "NSReg.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#ifdef XP_WIN
#include <windows.h>
#endif

#ifndef MAXPATHLEN
#ifdef _MAX_PATH
#define MAXPATHLEN _MAX_PATH
#elif defined(CCHMAXPATH)
#define MAXPATHLEN CCHMAXPATH
#else
#define MAXPATHLEN 1024
#endif
#endif

///////////////////////////////////////////////////////////////////////////////
// nsIProfileMigrator

#define MIGRATION_WIZARD_FE_URL "chrome://browser/content/migration/migration.xul"
#define MIGRATION_WIZARD_FE_FEATURES "chrome,dialog,modal,centerscreen"

NS_IMETHODIMP
nsProfileMigrator::Migrate(nsIProfileStartup* aStartup)
{
  nsresult rv;

  nsCAutoString key;
  nsCOMPtr<nsIBrowserProfileMigrator> bpm;

  rv = GetDefaultBrowserMigratorKey(key, bpm);
  if (NS_FAILED(rv)) return rv;

  if (!bpm) {
    nsCAutoString contractID =
      NS_LITERAL_CSTRING(NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX) + key;

    bpm = do_CreateInstance(contractID.get());
    if (!bpm) return NS_ERROR_FAILURE;
  }

  PRBool sourceExists;
  rv = bpm->GetSourceExists(&sourceExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!sourceExists) return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupportsCString> cstr
    (do_CreateInstance("@mozilla.org/supports-cstring;1"));
  if (!cstr) return NS_ERROR_OUT_OF_MEMORY;
  cstr->SetData(key);

  // By opening the Migration FE with a supplied bpm, it will automatically
  // migrate from it. 
  nsCOMPtr<nsIWindowWatcher> ww(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsISupportsArray> params;
  NS_NewISupportsArray(getter_AddRefs(params));
  if (!ww || !params) return NS_ERROR_FAILURE;

  params->AppendElement(cstr);
  params->AppendElement(bpm);
  params->AppendElement(aStartup);

  nsCOMPtr<nsIDOMWindow> migrateWizard;
  return ww->OpenWindow(nsnull, 
                        MIGRATION_WIZARD_FE_URL,
                        "_blank",
                        MIGRATION_WIZARD_FE_FEATURES,
                        params,
                        getter_AddRefs(migrateWizard));
}

NS_IMETHODIMP
nsProfileMigrator::Import()
{
  if (ImportRegistryProfiles(NS_LITERAL_CSTRING("Firefox")))
    return NS_OK;

  return NS_ERROR_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
// nsProfileMigrator

NS_IMPL_ISUPPORTS1(nsProfileMigrator, nsIProfileMigrator)

#ifdef XP_WIN
typedef struct {
  WORD wLanguage;
  WORD wCodePage;
} LANGANDCODEPAGE;

#define INTERNAL_NAME_FIREBIRD        "Firebird"
#define INTERNAL_NAME_FIREFOX         "Firefox"
#define INTERNAL_NAME_PHOENIX         "Phoenix"
#define INTERNAL_NAME_IEXPLORE        "iexplore"
#define INTERNAL_NAME_SEAMONKEY       "apprunner"
#define INTERNAL_NAME_DOGBERT         "NETSCAPE"
#define INTERNAL_NAME_OPERA           "Opera"
#endif

nsresult
nsProfileMigrator::GetDefaultBrowserMigratorKey(nsACString& aKey,
                                                nsCOMPtr<nsIBrowserProfileMigrator>& bpm)
{
#if XP_WIN
  HKEY hkey;

  const char* kCommandKey = "SOFTWARE\\Classes\\HTTP\\shell\\open\\command";
  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, kCommandKey, 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
    DWORD type;
    DWORD length = MAX_PATH;
    unsigned char value[MAX_PATH];
    if (::RegQueryValueEx(hkey, NULL, 0, &type, value, &length) == ERROR_SUCCESS) {
      nsCAutoString str; str.Assign((char*)value);

      PRInt32 lastIndex = str.Find(".exe", PR_TRUE);

      nsCAutoString filePath;
      if (str.CharAt(1) == ':') 
        filePath = Substring(str, 0, lastIndex + 4);
      else
        filePath = Substring(str, 1, lastIndex + 3);        

      char* strCpy = ToNewCString(filePath);

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
      DWORD dummy;
      DWORD size = ::GetFileVersionInfoSize(strCpy, &dummy);
      if (size) {
        void* ver = malloc(size);  
        memset(ver, 0, size);

        if (::GetFileVersionInfo(strCpy, 0, size, ver)) {
          LANGANDCODEPAGE* translate;
          UINT pageCount;
          ::VerQueryValue(ver, TEXT("\\VarFileInfo\\Translation"), (void**)&translate, &pageCount);

          if (pageCount > 0) {
            TCHAR subBlock[MAX_PATH];
            wsprintf(subBlock, TEXT("\\StringFileInfo\\%04x%04x\\InternalName"), translate[0].wLanguage, translate[0].wCodePage);

            LPVOID internalName = NULL;
            UINT size;
            ::VerQueryValue(ver, subBlock, (void**)&internalName, &size);

            if (!nsCRT::strcasecmp((char*)internalName, INTERNAL_NAME_IEXPLORE)) {
              aKey = "ie";
              return NS_OK;
            }
            if (!nsCRT::strcasecmp((char*)internalName, INTERNAL_NAME_SEAMONKEY)) {
              aKey = "seamonkey";
              return NS_OK;
            }
            if (!nsCRT::strcasecmp((char*)internalName, INTERNAL_NAME_DOGBERT)) {
              aKey = "dogbert";
              return NS_OK;
            }
            if (!nsCRT::strcasecmp((char*)internalName, INTERNAL_NAME_OPERA)) {
              aKey = "opera";
              return NS_OK;
            }
            // Migrate data from any existing Application Data\Phoenix\* installations.
            if (!nsCRT::strcasecmp((char*)internalName, INTERNAL_NAME_FIREBIRD) || 
                !nsCRT::strcasecmp((char*)internalName, INTERNAL_NAME_FIREFOX) ||
                !nsCRT::strcasecmp((char*)internalName, INTERNAL_NAME_PHOENIX)) {
              aKey = "phoenix";
              return NS_OK;
            }
          }
        }
      }
    }
  }
#else
  // XXXben - until we figure out what to do here with default browsers on MacOS and
  // GNOME, simply copy data from a previous Seamonkey install. 
  PRBool exists = PR_FALSE;
  bpm = do_CreateInstance(NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "phoenix");
  if (bpm)
    bpm->GetSourceExists(&exists);
  if (exists) {
    aKey = "phoenix";
    return NS_OK;
  }

  bpm = do_CreateInstance(NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "seamonkey");
  if (bpm)
    bpm->GetSourceExists(&exists);
  if (exists) {
    aKey = "seamonkey";
    return NS_OK;
  }
#endif

  return NS_ERROR_FAILURE;
}

PRBool
nsProfileMigrator::ImportRegistryProfiles(const nsACString& aAppName)
{
  nsresult rv;

  nsCOMPtr<nsIToolkitProfileService> profileSvc
    (do_GetService(NS_PROFILESERVICE_CONTRACTID));
  NS_ENSURE_TRUE(profileSvc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIProperties> dirService
    (do_GetService("@mozilla.org/file/directory_service;1"));
  NS_ENSURE_TRUE(dirService, NS_ERROR_FAILURE);

  nsCOMPtr<nsILocalFile> regFile;
#ifdef XP_WIN
  rv = dirService->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile),
                       getter_AddRefs(regFile));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  regFile->AppendNative(aAppName);
  regFile->AppendNative(NS_LITERAL_CSTRING("registry.dat"));
#elif defined(XP_MACOSX)
  rv = dirService->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile),
                       getter_AddRefs(regFile));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  regFile->AppendNative(aAppName);
  regFile->AppendNative(NS_LITERAL_CSTRING("Application Registry"));
#elif defined(XP_OS2)
  rv = dirService->Get(NS_OS2_HOME_DIR, NS_GET_IID(nsILocalFile),
                       getter_AddRefs(regFile));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  regFile->AppendNative(aAppName);
  regFile->AppendNative(NS_LITERAL_CSTRING("registry.dat"));
#else
  rv = dirService->Get(NS_UNIX_HOME_DIR, NS_GET_IID(nsILocalFile),
                       getter_AddRefs(regFile));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  nsCAutoString dotAppName;
  ToLowerCase(aAppName, dotAppName);
  dotAppName.Insert('.', 0);
  
  regFile->AppendNative(dotAppName);
  regFile->AppendNative(NS_LITERAL_CSTRING("appreg"));
#endif

  nsCAutoString path;
  rv = regFile->GetNativePath(path);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  if (NR_StartupRegistry())
    return PR_FALSE;

  PRBool migrated = PR_FALSE;
  HREG reg = nsnull;
  RKEY profiles = 0;
  REGENUM enumstate = 0;
  char profileName[MAXREGNAMELEN];

  if (NR_RegOpen(path.get(), &reg))
    goto cleanup;

  if (NR_RegGetKey(reg, ROOTKEY_COMMON, "Profiles", &profiles))
    goto cleanup;

  while (!NR_RegEnumSubkeys(reg, profiles, &enumstate,
                            profileName, MAXREGNAMELEN, REGENUM_CHILDREN)) {
#ifdef DEBUG_bsmedberg
    printf("Found profile %s.\n", profileName);
#endif

    RKEY profile = 0;
    if (NR_RegGetKey(reg, profiles, profileName, &profile)) {
      NS_ERROR("Could not get the key that was enumerated.");
      continue;
    }

    char profilePath[MAXPATHLEN];
    if (NR_RegGetEntryString(reg, profile, "directory",
                             profilePath, MAXPATHLEN))
      continue;

    nsCOMPtr<nsILocalFile> profileFile
      (do_CreateInstance("@mozilla.org/file/local;1"));
    if (!profileFile)
      continue;

    rv = profileFile->InitWithNativePath(nsDependentCString(profilePath));
    if (NS_FAILED(rv)) continue;

    nsCOMPtr<nsIToolkitProfile> tprofile;
    profileSvc->CreateProfile(profileFile, nsDependentCString(profileName),
                              getter_AddRefs(tprofile));
    migrated = PR_TRUE;
  }

cleanup:
  if (reg)
    NR_RegClose(reg);
  NR_ShutdownRegistry();
  return migrated;
}
