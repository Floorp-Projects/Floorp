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
 * The Original Code is the Mozilla Firefox browser.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIDirectoryService.h"

#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsArrayEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsBrowserDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMArray.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIGenericFactory.h"
#include "nsServiceManagerUtils.h"
#include "nsStringAPI.h"
#include "nsXULAppAPI.h"

class nsBrowserDirectoryProvider :
  public nsIDirectoryServiceProvider2
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

  static NS_METHOD Register(nsIComponentManager* aCompMgr,
                            nsIFile* aPath, const char *aLoaderStr,
                            const char *aType,
                            const nsModuleComponentInfo *aInfo);

  static NS_METHOD Unregister(nsIComponentManager* aCompMgr,
                              nsIFile* aPath, const char *aLoaderStr,
                              const nsModuleComponentInfo *aInfo);

private:
  nsresult RestoreBookmarksFromBackup(const nsACString& aLeafName,
				      nsIFile* aParentDir, nsIFile* aTarget);
  void EnsureProfileFile(const nsACString& aLeafName,
			 nsIFile* aParentDir, nsIFile* aTarget);

  class AppendingEnumerator : public nsISimpleEnumerator
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    AppendingEnumerator(nsISimpleEnumerator* aBase,
                        char const *const *aAppendList);

  private:
    nsCOMPtr<nsISimpleEnumerator> mBase;
    char const *const *const      mAppendList;
    nsCOMPtr<nsIFile>             mNext;
  };
};

NS_IMPL_ISUPPORTS2(nsBrowserDirectoryProvider,
                   nsIDirectoryServiceProvider,
                   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
nsBrowserDirectoryProvider::GetFile(const char *aKey, PRBool *aPersist,
                                    nsIFile* *aResult)
{
  nsresult rv;

  *aResult = nsnull;

  // NOTE: This function can be reentrant through the NS_GetSpecialDirectory
  // call, so be careful not to cause infinite recursion.

  nsCOMPtr<nsIFile> file;

  char const* leafName = nsnull;
  PRBool restoreBookmarksBackup = PR_FALSE;
  PRBool ensureFilePermissions = PR_FALSE;

  if (!strcmp(aKey, NS_APP_BOOKMARKS_50_FILE)) {
    ensureFilePermissions = PR_TRUE;
    restoreBookmarksBackup = PR_TRUE;
    leafName = "bookmarks.html";

    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefs) {
      nsCString path;
      rv = prefs->GetCharPref("browser.bookmarks.file", getter_Copies(path));
      if (NS_SUCCEEDED(rv)) {
	NS_NewNativeLocalFile(path, PR_TRUE, (nsILocalFile**)(nsIFile**) getter_AddRefs(file));
      }
    }
  }
  else if (!strcmp(aKey, NS_APP_MICROSUMMARY_DIR)) {
    rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    file->AppendNative(NS_LITERAL_CSTRING("microsummary-generators"));
    file.swap(*aResult);
    return NS_OK;
  }
  else if (!strcmp(aKey, NS_APP_USER_MICROSUMMARY_DIR)) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    file->AppendNative(NS_LITERAL_CSTRING("microsummary-generators"));
    file.swap(*aResult);
    return NS_OK;
  }
  else {
    return NS_ERROR_FAILURE;
  }

  nsDependentCString leafstr(leafName);

  nsCOMPtr<nsIFile> parentDir;
  if (file) {
    rv = file->GetParent(getter_AddRefs(parentDir));
    if (NS_FAILED(rv))
      return rv;
  }
  else {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(parentDir));
    if (NS_FAILED(rv))
      return rv;

    rv = parentDir->Clone(getter_AddRefs(file));
    if (NS_FAILED(rv))
      return rv;

    file->AppendNative(leafstr);
  }

  PRBool exists;
  rv = file->Exists(&exists);

  if (restoreBookmarksBackup && NS_SUCCEEDED(rv) && exists) {
    PRInt64 fileSize;
    file->GetFileSize(&fileSize);
    if (fileSize == 0)
    {
      file->Remove(PR_FALSE);
      exists = PR_FALSE;
    }
  }

  if (NS_SUCCEEDED(rv) && !exists) {
    if (!restoreBookmarksBackup ||
	NS_FAILED(RestoreBookmarksFromBackup(leafstr, parentDir, file)))
      EnsureProfileFile(leafstr, parentDir, file);
  }

  if (ensureFilePermissions) {
    PRBool fileToEnsureExists;
    PRBool isWritable;
    if (NS_SUCCEEDED(file->Exists(&fileToEnsureExists)) && fileToEnsureExists
        && NS_SUCCEEDED(file->IsWritable(&isWritable)) && !isWritable) {
      PRUint32 permissions;
      if (NS_SUCCEEDED(file->GetPermissions(&permissions))) {
        rv = file->SetPermissions(permissions | 0600);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to ensure file permissions");
      }
    }
  }

  *aPersist = PR_TRUE;
  NS_ADDREF(*aResult = file);

  return NS_OK;
}

static void
AppendFileKey(const char *key, nsIProperties* aDirSvc,
              nsCOMArray<nsIFile> &array)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirSvc->Get(key, NS_GET_IID(nsIFile), getter_AddRefs(file));
  if (NS_FAILED(rv))
    return;

  PRBool exists;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return;

  array.AppendObject(file);
}

NS_IMETHODIMP
nsBrowserDirectoryProvider::GetFiles(const char *aKey,
                                     nsISimpleEnumerator* *aResult)
{
  nsresult rv;

  if (!strcmp(aKey, NS_APP_SEARCH_DIR_LIST)) {
    nsCOMPtr<nsIProperties> dirSvc
      (do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    if (!dirSvc)
      return NS_ERROR_FAILURE;

    nsCOMArray<nsIFile> baseFiles;

    AppendFileKey(NS_APP_SEARCH_DIR, dirSvc, baseFiles);
    AppendFileKey(NS_APP_USER_SEARCH_DIR, dirSvc, baseFiles);

    nsCOMPtr<nsISimpleEnumerator> baseEnum;
    rv = NS_NewArrayEnumerator(getter_AddRefs(baseEnum), baseFiles);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsISimpleEnumerator> list;
    rv = dirSvc->Get(XRE_EXTENSIONS_DIR_LIST,
                     NS_GET_IID(nsISimpleEnumerator), getter_AddRefs(list));
    if (NS_FAILED(rv))
      return rv;

    static char const *const kAppendSPlugins[] = {"searchplugins", nsnull};

    nsCOMPtr<nsISimpleEnumerator> extEnum =
      new AppendingEnumerator(list, kAppendSPlugins);
    if (!extEnum)
      return NS_ERROR_OUT_OF_MEMORY;

    return NS_NewUnionEnumerator(aResult, extEnum, baseEnum);
  }

  return NS_ERROR_FAILURE;
}

static char const kContractID[] = "@mozilla.org/browser/directory-provider;1";

// {6DEB193C-F87D-4078-BC78-5E64655B4D62}
#define NS_BROWSERDIRECTORYPROVIDER_CID \
  { 0x6deb193c, 0xf87d, 0x4078, { 0xbc, 0x78, 0x5e, 0x64, 0x65, 0x5b, 0x4d, 0x62 } }

NS_METHOD
nsBrowserDirectoryProvider::Register(nsIComponentManager* aCompMgr,
                                     nsIFile* aPath, const char *aLoaderStr,
                                     const char *aType,
                                     const nsModuleComponentInfo *aInfo)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catMan
    (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catMan)
    return NS_ERROR_FAILURE;

  rv = catMan->AddCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                                "browser-directory-provider",
                                kContractID, PR_TRUE, PR_TRUE, nsnull);
  return rv;
}


NS_METHOD
nsBrowserDirectoryProvider::Unregister(nsIComponentManager* aCompMgr,
                                       nsIFile* aPath, const char *aLoaderStr,
                                       const nsModuleComponentInfo *aInfo)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catMan
    (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catMan)
    return NS_ERROR_FAILURE;

  rv = catMan->DeleteCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                                   "browser-directory-provider", PR_TRUE);
  return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserDirectoryProvider)

static const nsModuleComponentInfo components[] = {
  {
    "nsBrowserDirectoryProvider",
    NS_BROWSERDIRECTORYPROVIDER_CID,
    kContractID,
    nsBrowserDirectoryProviderConstructor,
    nsBrowserDirectoryProvider::Register,
    nsBrowserDirectoryProvider::Unregister
  }
};

NS_IMPL_NSGETMODULE(BrowserDirProvider, components)

nsresult
nsBrowserDirectoryProvider::RestoreBookmarksFromBackup(const nsACString& aLeafName,
						       nsIFile* aParentDir,
						       nsIFile* aTarget)
{
  nsresult rv;

  nsCOMPtr<nsIFile> backupFile;
  rv = aParentDir->Clone(getter_AddRefs(backupFile));
  if (NS_FAILED(rv))
    return rv;

  backupFile->AppendNative(nsDependentCString("bookmarks.bak"));

  PRBool exists;
  rv = backupFile->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return NS_ERROR_FAILURE;

  return backupFile->CopyToNative(aParentDir, aLeafName);
}

void
nsBrowserDirectoryProvider::EnsureProfileFile(const nsACString& aLeafName,
                                              nsIFile* aParentDir,
                                              nsIFile* aTarget)
{
  nsresult rv;

  nsCOMPtr<nsIFile> defaults;
  rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR,
                              getter_AddRefs(defaults));
  if (NS_FAILED(rv))
    return;

  defaults->AppendNative(aLeafName);

  PRBool exists;
  rv = defaults->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return;

  defaults->CopyToNative(aParentDir, aLeafName);
}

NS_IMPL_ISUPPORTS1(nsBrowserDirectoryProvider::AppendingEnumerator,
                   nsISimpleEnumerator)

NS_IMETHODIMP
nsBrowserDirectoryProvider::AppendingEnumerator::HasMoreElements(PRBool *aResult)
{
  *aResult = mNext ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserDirectoryProvider::AppendingEnumerator::GetNext(nsISupports* *aResult)
{
  if (aResult)
    NS_ADDREF(*aResult = mNext);

  mNext = nsnull;

  nsresult rv;

  // Ignore all errors

  PRBool more;
  while (NS_SUCCEEDED(mBase->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> nextbasesupp;
    mBase->GetNext(getter_AddRefs(nextbasesupp));

    nsCOMPtr<nsIFile> nextbase(do_QueryInterface(nextbasesupp));
    if (!nextbase)
      continue;

    nextbase->Clone(getter_AddRefs(mNext));
    if (!mNext)
      continue;

    char const *const * i = mAppendList;
    while (*i) {
      mNext->AppendNative(nsDependentCString(*i));
      ++i;
    }

    PRBool exists;
    rv = mNext->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      break;

    mNext = nsnull;
  }

  return NS_OK;
}

nsBrowserDirectoryProvider::AppendingEnumerator::AppendingEnumerator
    (nsISimpleEnumerator* aBase,
     char const *const *aAppendList) :
  mBase(aBase),
  mAppendList(aAppendList)
{
  // Initialize mNext to begin.
  GetNext(nsnull);
}
