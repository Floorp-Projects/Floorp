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

#include "nsBrowserProfileMigratorUtils.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPhoenixProfileMigrator.h"

///////////////////////////////////////////////////////////////////////////////
// nsPhoenixProfileMigrator

#define FILE_NAME_BOOKMARKS       NS_LITERAL_STRING("bookmarks.html")
#define FILE_NAME_COOKIES         NS_LITERAL_STRING("cookies.txt")
#define FILE_NAME_SITEPERM_OLD    NS_LITERAL_STRING("cookperm.txt")
#define FILE_NAME_SITEPERM_NEW    NS_LITERAL_STRING("hostperm.1")
#define FILE_NAME_CERT8DB         NS_LITERAL_STRING("cert8.db")
#define FILE_NAME_KEY3DB          NS_LITERAL_STRING("key3.db")
#define FILE_NAME_SECMODDB        NS_LITERAL_STRING("secmod.db")
#define FILE_NAME_HISTORY         NS_LITERAL_STRING("history.dat")
#define FILE_NAME_FORMHISTORY     NS_LITERAL_STRING("formhistory.dat")
#define FILE_NAME_LOCALSTORE      NS_LITERAL_STRING("localstore.rdf")
#define FILE_NAME_MIMETYPES       NS_LITERAL_STRING("mimeTypes.rdf")
#define FILE_NAME_DOWNLOADS       NS_LITERAL_STRING("downloads.rdf")
#define FILE_NAME_PREFS           NS_LITERAL_STRING("prefs.js")
#define FILE_NAME_USER_PREFS      NS_LITERAL_STRING("user.js")
#define FILE_NAME_SEARCH          NS_LITERAL_STRING("search.rdf")
#define FILE_NAME_USERCHROME      NS_LITERAL_STRING("userChrome.css")
#define FILE_NAME_USERCONTENT     NS_LITERAL_STRING("userContent.css")
#define DIR_NAME_CHROME           NS_LITERAL_STRING("chrome")

NS_IMPL_ISUPPORTS1(nsPhoenixProfileMigrator, nsIBrowserProfileMigrator)

nsPhoenixProfileMigrator::nsPhoenixProfileMigrator()
{
  mObserverService = do_GetService("@mozilla.org/observer-service;1");
}

nsPhoenixProfileMigrator::~nsPhoenixProfileMigrator()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsIBrowserProfileMigrator

NS_IMETHODIMP
nsPhoenixProfileMigrator::Migrate(PRUint16 aItems, PRBool aReplace, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  // At this time the only reason for this migrator is to get data across from the
  // Phoenix profile directory on initial run, so we don't need to support after-the-fact
  // importing. 
  NS_ASSERTION(aReplace, "Can't migrate from Phoenix/Firebird/Firefox profiles once Firefox is running!");
  if (!aReplace)
    return NS_ERROR_FAILURE;

  if (!mTargetProfile) 
    GetTargetProfile(aProfile, aReplace);
  if (!mSourceProfile)
    GetSourceProfile(aProfile);

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);

  COPY_DATA(CopyPreferences,  aReplace, nsIBrowserProfileMigrator::SETTINGS);
  COPY_DATA(CopyCookies,      aReplace, nsIBrowserProfileMigrator::COOKIES);
  COPY_DATA(CopyHistory,      aReplace, nsIBrowserProfileMigrator::HISTORY);
  COPY_DATA(CopyPasswords,    aReplace, nsIBrowserProfileMigrator::PASSWORDS);
  COPY_DATA(CopyOtherData,    aReplace, nsIBrowserProfileMigrator::OTHERDATA);
  COPY_DATA(CopyBookmarks,    aReplace, nsIBrowserProfileMigrator::BOOKMARKS);

  if (aItems & nsIBrowserProfileMigrator::SETTINGS || 
      aItems & nsIBrowserProfileMigrator::COOKIES || 
      aItems & nsIBrowserProfileMigrator::PASSWORDS ||
      !aItems) {
    // Permissions (Images, Cookies, Popups)
    rv |= CopyFile(FILE_NAME_SITEPERM_NEW, FILE_NAME_SITEPERM_NEW);
    rv |= CopyFile(FILE_NAME_SITEPERM_OLD, FILE_NAME_SITEPERM_OLD);
  }

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

NS_IMETHODIMP
nsPhoenixProfileMigrator::GetMigrateData(const PRUnichar* aProfile, 
                                         PRBool aReplace, 
                                         PRUint16* aResult)
{
  *aResult = 0;
  if (!mSourceProfile) {
    GetSourceProfile(aProfile);
    if (!mSourceProfile)
      return NS_ERROR_FILE_NOT_FOUND;
  }

  MigrationData data[] = { { ToNewUnicode(FILE_NAME_PREFS),
                             nsIBrowserProfileMigrator::SETTINGS,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_USER_PREFS),
                             nsIBrowserProfileMigrator::SETTINGS,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_COOKIES),
                             nsIBrowserProfileMigrator::COOKIES,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_HISTORY),
                             nsIBrowserProfileMigrator::HISTORY,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_BOOKMARKS),
                             nsIBrowserProfileMigrator::BOOKMARKS,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_DOWNLOADS),
                             nsIBrowserProfileMigrator::OTHERDATA,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_MIMETYPES),
                             nsIBrowserProfileMigrator::OTHERDATA,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_USERCHROME),
                             nsIBrowserProfileMigrator::OTHERDATA,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_USERCONTENT),
                             nsIBrowserProfileMigrator::OTHERDATA,
                             PR_TRUE },
                           { ToNewUnicode(FILE_NAME_FORMHISTORY),
                             nsIBrowserProfileMigrator::OTHERDATA,
                             PR_TRUE } };

  // Frees file name strings allocated above.
  GetMigrateDataFromArray(data, sizeof(data)/sizeof(MigrationData), 
                          aReplace, mSourceProfile, aResult);

  // Now locate passwords
  nsXPIDLCString signonsFileName;
  GetSignonFileName(aReplace, getter_Copies(signonsFileName));

  if (!signonsFileName.IsEmpty()) {
    nsAutoString fileName; fileName.AssignWithConversion(signonsFileName);
    nsCOMPtr<nsIFile> sourcePasswordsFile;
    mSourceProfile->Clone(getter_AddRefs(sourcePasswordsFile));
    sourcePasswordsFile->Append(fileName);

    PRBool exists;
    sourcePasswordsFile->Exists(&exists);
    if (exists)
      *aResult |= nsIBrowserProfileMigrator::PASSWORDS;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPhoenixProfileMigrator::GetSourceExists(PRBool* aResult)
{
  nsCOMPtr<nsISupportsArray> profiles;
  GetSourceProfiles(getter_AddRefs(profiles));

  if (profiles) { 
    PRUint32 count;
    profiles->Count(&count);
    *aResult = count > 0;
  }
  else
    *aResult = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsPhoenixProfileMigrator::GetSourceHasMultipleProfiles(PRBool* aResult)
{
  nsCOMPtr<nsISupportsArray> profiles;
  GetSourceProfiles(getter_AddRefs(profiles));

  if (profiles) {
    PRUint32 count;
    profiles->Count(&count);
    *aResult = count > 1;
  }
  else
    *aResult = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsPhoenixProfileMigrator::GetSourceProfiles(nsISupportsArray** aResult)
{
  if (!mProfileNames && !mProfileLocations) {
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(mProfileNames));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mProfileLocations));
    if (NS_FAILED(rv)) return rv;

    // Fills mProfileNames and mProfileLocations
    FillProfileDataFromPhoenixRegistry();
  }
  
  NS_IF_ADDREF(*aResult = mProfileNames);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsPhoenixProfileMigrator

nsresult
nsPhoenixProfileMigrator::GetSourceProfile(const PRUnichar* aProfile)
{
  PRUint32 count;
  mProfileNames->Count(&count);
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsISupportsString> str(do_QueryElementAt(mProfileNames, i));
    nsXPIDLString profileName;
    str->GetData(profileName);
    if (profileName.Equals(aProfile)) {
      mSourceProfile = do_QueryElementAt(mProfileLocations, i);
      break;
    }
  }

  return NS_OK;
}

nsresult
nsPhoenixProfileMigrator::FillProfileDataFromPhoenixRegistry()
{
  // Find the Phoenix Registry
  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
  nsCOMPtr<nsILocalFile> phoenixRegistry;
#ifdef XP_WIN
  fileLocator->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(phoenixRegistry));

  phoenixRegistry->Append(NS_LITERAL_STRING("Phoenix"));
  phoenixRegistry->Append(NS_LITERAL_STRING("registry.dat"));
#elif defined(XP_MACOSX)
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(phoenixRegistry));
  
  phoenixRegistry->Append(NS_LITERAL_STRING("Phoenix"));
  phoenixRegistry->Append(NS_LITERAL_STRING("Application Registry"));
#elif defined(XP_UNIX)
  fileLocator->Get(NS_UNIX_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(phoenixRegistry));
  
  phoenixRegistry->Append(NS_LITERAL_STRING(".phoenix"));
  phoenixRegistry->Append(NS_LITERAL_STRING("appreg"));
#endif


  return GetProfileDataFromRegistry(phoenixRegistry, mProfileNames, mProfileLocations);
}

nsresult
nsPhoenixProfileMigrator::CopyPreferences(PRBool aReplace)
{
  nsresult rv = NS_OK;
  if (!aReplace)
    return rv;

  // Prefs files
  rv |= CopyFile(FILE_NAME_PREFS, FILE_NAME_PREFS);
  rv |= CopyFile(FILE_NAME_USER_PREFS, FILE_NAME_USER_PREFS);

  // Security Stuff
  rv |= CopyFile(FILE_NAME_CERT8DB, FILE_NAME_CERT8DB);
  rv |= CopyFile(FILE_NAME_KEY3DB, FILE_NAME_KEY3DB);
  rv |= CopyFile(FILE_NAME_SECMODDB, FILE_NAME_SECMODDB);

  // User MIME Type overrides
  rv |= CopyFile(FILE_NAME_MIMETYPES, FILE_NAME_MIMETYPES);

  rv |= CopyUserStyleSheets();
  return rv;
}

nsresult 
nsPhoenixProfileMigrator::CopyUserStyleSheets()
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFile> sourceUserContent;
  mSourceProfile->Clone(getter_AddRefs(sourceUserContent));
  sourceUserContent->Append(DIR_NAME_CHROME);
  sourceUserContent->Append(FILE_NAME_USERCONTENT);

  PRBool exists = PR_FALSE;
  sourceUserContent->Exists(&exists);
  if (exists) {
    nsCOMPtr<nsIFile> targetUserContent;
    mTargetProfile->Clone(getter_AddRefs(targetUserContent));
    targetUserContent->Append(DIR_NAME_CHROME);
    nsCOMPtr<nsIFile> targetChromeDir;
    targetUserContent->Clone(getter_AddRefs(targetChromeDir));
    targetUserContent->Append(FILE_NAME_USERCONTENT);

    targetUserContent->Exists(&exists);
    if (exists)
      targetUserContent->Remove(PR_FALSE);

    rv |= sourceUserContent->CopyTo(targetChromeDir, FILE_NAME_USERCONTENT);
  }

  nsCOMPtr<nsIFile> sourceUserChrome;
  mSourceProfile->Clone(getter_AddRefs(sourceUserChrome));
  sourceUserChrome->Append(DIR_NAME_CHROME);
  sourceUserChrome->Append(FILE_NAME_USERCHROME);

  sourceUserChrome->Exists(&exists);
  if (exists) {
    nsCOMPtr<nsIFile> targetUserChrome;
    mTargetProfile->Clone(getter_AddRefs(targetUserChrome));
    targetUserChrome->Append(DIR_NAME_CHROME);
    nsCOMPtr<nsIFile> targetChromeDir;
    targetUserChrome->Clone(getter_AddRefs(targetChromeDir));
    targetUserChrome->Append(FILE_NAME_USERCHROME);

    targetUserChrome->Exists(&exists);
    if (exists)
      targetUserChrome->Remove(PR_FALSE);

    rv |= sourceUserChrome->CopyTo(targetChromeDir, FILE_NAME_USERCHROME);
  }

  return rv;
}

nsresult
nsPhoenixProfileMigrator::CopyCookies(PRBool aReplace)
{
  return aReplace ? CopyFile(FILE_NAME_COOKIES, FILE_NAME_COOKIES) : NS_OK;
}

nsresult
nsPhoenixProfileMigrator::CopyHistory(PRBool aReplace)
{
  return aReplace ? CopyFile(FILE_NAME_HISTORY, FILE_NAME_HISTORY) : NS_OK;
}

nsresult
nsPhoenixProfileMigrator::CopyPasswords(PRBool aReplace)
{
  nsresult rv;

  nsXPIDLCString signonsFileName;
  if (!aReplace)
    return NS_OK;

  // Find out what the signons file was called, this is stored in a pref
  // in Seamonkey.
  nsCOMPtr<nsIPrefService> psvc(do_GetService(NS_PREFSERVICE_CONTRACTID));
  psvc->ResetPrefs();

  nsCOMPtr<nsIFile> seamonkeyPrefsFile;
  mSourceProfile->Clone(getter_AddRefs(seamonkeyPrefsFile));
  seamonkeyPrefsFile->Append(FILE_NAME_PREFS);
  psvc->ReadUserPrefs(seamonkeyPrefsFile);

  nsCOMPtr<nsIPrefBranch> branch(do_QueryInterface(psvc));
  rv = branch->GetCharPref("signon.SignonFileName", getter_Copies(signonsFileName));

  if (signonsFileName.IsEmpty())
    return NS_ERROR_FILE_NOT_FOUND;

  nsAutoString fileName; fileName.AssignWithConversion(signonsFileName);
  return aReplace ? CopyFile(fileName, fileName) : NS_OK;
}

nsresult
nsPhoenixProfileMigrator::CopyBookmarks(PRBool aReplace)
{
  return aReplace ? CopyFile(FILE_NAME_BOOKMARKS, FILE_NAME_BOOKMARKS) : NS_OK;
}

nsresult
nsPhoenixProfileMigrator::CopyOtherData(PRBool aReplace)
{
  if (!aReplace)
    return NS_OK;

  nsresult rv = NS_OK;
  rv |= CopyFile(FILE_NAME_DOWNLOADS, FILE_NAME_DOWNLOADS);
  rv |= CopyFile(FILE_NAME_SEARCH, FILE_NAME_SEARCH);
  rv |= CopyFile(FILE_NAME_LOCALSTORE, FILE_NAME_LOCALSTORE);
  rv |= CopyFile(FILE_NAME_FORMHISTORY, FILE_NAME_FORMHISTORY);

  return rv;
}

