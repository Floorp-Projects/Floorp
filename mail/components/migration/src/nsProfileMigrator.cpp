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

#include "nsIGenericFactory.h"
#include "nsILocalFile.h"
#include "nsIProfileMigrator.h"
#include "nsIServiceManager.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"

#include "nsDirectoryServiceDefs.h"
#include "nsProfileMigrator.h"

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

NS_IMETHODIMP
nsProfileMigrator::Migrate(nsIProfileStartup* aStartup)
{
  // we don't do migration, only import

  return NS_OK;
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
    rv = profileFile->InitWithNativePath(nsDependentCString(profilePath));
#endif
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

// Make this into a component

NS_GENERIC_FACTORY_CONSTRUCTOR(nsProfileMigrator)

static const nsModuleComponentInfo components[] =
{
  { "Profile Importer",
    NS_THUNDERBIRD_PROFILEIMPORT_CID,
    NS_PROFILEMIGRATOR_CONTRACTID,
    nsProfileMigratorConstructor },
};

NS_IMPL_NSGETMODULE(nsMailProfileMigratorModule, components)
