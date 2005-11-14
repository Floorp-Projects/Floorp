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
 *  Benjamin Smedberg <bsmedberg@covad.net>
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

#include "nsILocalFile.h"
#include "nsIDOMWindowInternal.h"
#include "nsIProfileMigrator.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"
#include "nsIWindowWatcher.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsArray.h"

#include "nsDirectoryServiceDefs.h"
#include "nsProfileMigrator.h"
#include "nsMailMigrationCID.h"

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

NS_IMPL_ISUPPORTS1(nsProfileMigrator, nsIProfileMigrator)

#define MIGRATION_WIZARD_FE_URL "chrome://messenger/content/migration/migration.xul"
#define MIGRATION_WIZARD_FE_FEATURES "chrome,dialog,modal,centerscreen"

NS_IMETHODIMP
nsProfileMigrator::Migrate(nsIProfileStartup* aStartup)
{
  nsCAutoString key;
  nsCOMPtr<nsIMailProfileMigrator> mailMigrator;
  nsresult rv = GetDefaultMailMigratorKey(key, mailMigrator);
  NS_ENSURE_SUCCESS(rv, rv); // abort migration if we failed to get a mailMigrator (if we were supposed to)

  nsCOMPtr<nsISupportsCString> cstr (do_CreateInstance("@mozilla.org/supports-cstring;1"));
  NS_ENSURE_TRUE(cstr, NS_ERROR_OUT_OF_MEMORY);
  cstr->SetData(key);

  // By opening the Migration FE with a supplied mailMigrator, it will automatically
  // migrate from it. 
  nsCOMPtr<nsIWindowWatcher> ww (do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsISupportsArray> params;
  NS_NewISupportsArray(getter_AddRefs(params));
  if (!ww || !params) return NS_ERROR_FAILURE;

  params->AppendElement(cstr);
  params->AppendElement(mailMigrator);
  params->AppendElement(aStartup);

  nsCOMPtr<nsIDOMWindow> migrateWizard;
  return ww->OpenWindow(nsnull, 
                        MIGRATION_WIZARD_FE_URL,
                        "_blank",
                        MIGRATION_WIZARD_FE_FEATURES,
                        params,
                        getter_AddRefs(migrateWizard));
}

#ifdef XP_WIN
typedef struct {
  WORD wLanguage;
  WORD wCodePage;
} LANGANDCODEPAGE;

#define INTERNAL_NAME_THUNDERBIRD     "Thunderbird"
#define INTERNAL_NAME_SEAMONKEY       "Mozilla"
#define INTERNAL_NAME_DOGBERT         "Netscape Messenger"
#endif

nsresult
nsProfileMigrator::GetDefaultMailMigratorKey(nsACString& aKey, nsCOMPtr<nsIMailProfileMigrator>& mailMigrator)
{
  // look up the value of profile.force.migration in case we are supposed to force migration using a particular
  // migrator....
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString forceMigrationType;
  prefs->GetCharPref("profile.force.migration", getter_Copies(forceMigrationType));

  // if we are being forced to migrate to a particular migration type, then create an instance of that migrator
  // and return it.
  if (forceMigrationType.get())
  {
    PRBool exists = PR_FALSE;
    nsCAutoString migratorID (NS_MAILPROFILEMIGRATOR_CONTRACTID_PREFIX);
    migratorID.Append(forceMigrationType);
    mailMigrator = do_CreateInstance(migratorID.get());
  
    if (mailMigrator)
    {
      mailMigrator->GetSourceExists(&exists);
      if (exists) 
        aKey = forceMigrationType;
      else
        rv = NS_ERROR_FAILURE; // trying to force migration on a source which does not have any profiles
    }
  }
 
  return rv;
}

NS_IMETHODIMP
nsProfileMigrator::Import()
{
  if (ImportRegistryProfiles(NS_LITERAL_CSTRING("Thunderbird")))
    return NS_OK;

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
#elif defined(XP_BEOS)
  rv = dirService->Get(NS_BEOS_SETTINGS_DIR, NS_GET_IID(nsILocalFile),
                       getter_AddRefs(regFile));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  regFile->AppendNative(aAppName);
  regFile->AppendNative(NS_LITERAL_CSTRING("appreg"));
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
#if defined (XP_MACOSX)
    rv = profileFile->SetPersistentDescriptor(nsDependentCString(profilePath));
#else
    NS_ConvertUTF8toUTF16 widePath(profilePath);
    rv = profileFile->InitWithPath(widePath);
#endif
    if (NS_FAILED(rv)) continue;

    nsCOMPtr<nsIToolkitProfile> tprofile;
    profileSvc->CreateProfile(profileFile, nsnull,
                              nsDependentCString(profileName),
                              getter_AddRefs(tprofile));
    migrated = PR_TRUE;
  }

cleanup:
  if (reg)
    NR_RegClose(reg);
  NR_ShutdownRegistry();
  return migrated;
}
