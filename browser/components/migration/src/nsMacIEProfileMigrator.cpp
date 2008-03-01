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
 *  Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nsDirectoryServiceDefs.h"
#include "nsBrowserProfileMigratorUtils.h"
#include "nsMacIEProfileMigrator.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIProfileMigrator.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsIProperties.h"

#define MACIE_BOOKMARKS_FILE_NAME NS_LITERAL_STRING("Favorites.html")
#define MACIE_PREFERENCES_FOLDER_NAME NS_LITERAL_STRING("Explorer")
#define TEMP_BOOKMARKS_FILE_NAME NS_LITERAL_STRING("bookmarks_tmp.html")

#define MIGRATION_BUNDLE "chrome://browser/locale/migration/migration.properties"

///////////////////////////////////////////////////////////////////////////////
// nsMacIEProfileMigrator

NS_IMPL_ISUPPORTS1(nsMacIEProfileMigrator, nsIBrowserProfileMigrator)

nsMacIEProfileMigrator::nsMacIEProfileMigrator()
{
  mObserverService = do_GetService("@mozilla.org/observer-service;1");
}

nsMacIEProfileMigrator::~nsMacIEProfileMigrator()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsIBrowserProfileMigrator

NS_IMETHODIMP
nsMacIEProfileMigrator::Migrate(PRUint16 aItems, nsIProfileStartup* aStartup, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  PRBool replace = aStartup ? PR_TRUE : PR_FALSE;

  if (!mTargetProfile) { 
    GetProfilePath(aStartup, mTargetProfile);
    if (!mTargetProfile) return NS_ERROR_FAILURE;
  }

  if (!mSourceProfile) {
    nsCOMPtr<nsIProperties> fileLocator =
      do_GetService("@mozilla.org/file/directory_service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    fileLocator->Get(NS_OSX_USER_PREFERENCES_DIR,
                     NS_GET_IID(nsILocalFile),
                     getter_AddRefs(mSourceProfile));
    mSourceProfile->Append(MACIE_PREFERENCES_FOLDER_NAME);
  }

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);

  COPY_DATA(CopyBookmarks, replace, nsIBrowserProfileMigrator::BOOKMARKS);

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

NS_IMETHODIMP
nsMacIEProfileMigrator::GetMigrateData(const PRUnichar* aProfile, 
                                       PRBool aReplace,
                                       PRUint16* aResult)
{
  *aResult = 0;

  if (!mSourceProfile) {
    nsresult rv;
    nsCOMPtr<nsIProperties> fileLocator =
      do_GetService("@mozilla.org/file/directory_service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    fileLocator->Get(NS_OSX_USER_PREFERENCES_DIR,
                     NS_GET_IID(nsILocalFile),
                     getter_AddRefs(mSourceProfile));
    mSourceProfile->Append(MACIE_PREFERENCES_FOLDER_NAME);
  }

  MigrationData data[] = { { ToNewUnicode(MACIE_BOOKMARKS_FILE_NAME),
                             nsIBrowserProfileMigrator::BOOKMARKS,
                             PR_FALSE } };

  // Frees file name strings allocated above.
  GetMigrateDataFromArray(data, sizeof(data)/sizeof(MigrationData), 
                          aReplace, mSourceProfile, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsMacIEProfileMigrator::GetSourceExists(PRBool* aResult)
{
  PRUint16 data;
  GetMigrateData(nsnull, PR_FALSE, &data);
  
  *aResult = data != 0;
  
  return NS_OK;
}

NS_IMETHODIMP
nsMacIEProfileMigrator::GetSourceHasMultipleProfiles(PRBool* aResult)
{
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMacIEProfileMigrator::GetSourceProfiles(nsISupportsArray** aResult)
{
  *aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsMacIEProfileMigrator::GetSourceHomePageURL(nsACString& aResult)
{
  aResult.Truncate();
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsMacIEProfileMigrator

nsresult
nsMacIEProfileMigrator::CopyBookmarks(PRBool aReplace)
{
  nsresult rv;
  nsCOMPtr<nsIFile> sourceFile;
  mSourceProfile->Clone(getter_AddRefs(sourceFile));

  sourceFile->Append(MACIE_BOOKMARKS_FILE_NAME);
  PRBool exists = PR_FALSE;
  sourceFile->Exists(&exists);
  if (!exists)
    return NS_OK;

  // it's an import
  if (!aReplace)
    return ImportBookmarksHTML(sourceFile,
                               PR_FALSE,
                               PR_FALSE,
                               NS_LITERAL_STRING("sourceNameIE").get());

  // Initialize the default bookmarks
  rv = InitializeBookmarks(mTargetProfile);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're blowing away existing content, annotate the Personal Toolbar and
  // then import the file. 
  nsCOMPtr<nsIFile> tempFile;
  mTargetProfile->Clone(getter_AddRefs(tempFile));
  tempFile->Append(TEMP_BOOKMARKS_FILE_NAME);

  // Look for the localized name of the IE Favorites Bar
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(MIGRATION_BUNDLE, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString toolbarFolderNameMacIE;
  bundle->GetStringFromName(NS_LITERAL_STRING("toolbarFolderNameMacIE").get(), 
                            getter_Copies(toolbarFolderNameMacIE));
  nsCAutoString ctoolbarFolderNameMacIE;
  CopyUTF16toUTF8(toolbarFolderNameMacIE, ctoolbarFolderNameMacIE);

  // Now read the 4.x bookmarks file, correcting the Personal Toolbar Folder 
  // line and writing to the temporary file.
  rv = AnnotatePersonalToolbarFolder(sourceFile,
                                     tempFile,
                                     ctoolbarFolderNameMacIE.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // import the temp file
  rv = ImportBookmarksHTML(tempFile,
                           PR_TRUE,
                           PR_FALSE,
                           EmptyString().get());
  NS_ENSURE_SUCCESS(rv, rv);

  // remove the temp file
  return tempFile->Remove(PR_FALSE);
}
