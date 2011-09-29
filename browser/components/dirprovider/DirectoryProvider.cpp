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
#include "DirectoryProvider.h"

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
#include "mozilla/ModuleUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringAPI.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace browser {

NS_IMPL_ISUPPORTS2(DirectoryProvider,
                   nsIDirectoryServiceProvider,
                   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
DirectoryProvider::GetFile(const char *aKey, bool *aPersist, nsIFile* *aResult)
{
  nsresult rv;

  *aResult = nsnull;

  // NOTE: This function can be reentrant through the NS_GetSpecialDirectory
  // call, so be careful not to cause infinite recursion.

  nsCOMPtr<nsIFile> file;

  char const* leafName = nsnull;

  if (!strcmp(aKey, NS_APP_BOOKMARKS_50_FILE)) {
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
  else if (!strcmp(aKey, NS_APP_EXISTING_PREF_OVERRIDE)) {
    rv = NS_GetSpecialDirectory(NS_APP_DEFAULTS_50_DIR,
                                getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    file->AppendNative(NS_LITERAL_CSTRING("existing-profile-defaults.js"));
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

  bool exists;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return;

  array.AppendObject(file);
}

// Appends the distribution-specific search engine directories to the
// array.  The directory structure is as follows:

// appdir/
// \- distribution/
//    \- searchplugins/
//       |- common/
//       \- locale/
//          |- <locale 1>/
//          ...
//          \- <locale N>/

// common engines are loaded for all locales.  If there is no locale
// directory for the current locale, there is a pref:
// "distribution.searchplugins.defaultLocale"
// which specifies a default locale to use.

static void
AppendDistroSearchDirs(nsIProperties* aDirSvc, nsCOMArray<nsIFile> &array)
{
  nsCOMPtr<nsIFile> searchPlugins;
  nsresult rv = aDirSvc->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                             NS_GET_IID(nsIFile),
                             getter_AddRefs(searchPlugins));
  if (NS_FAILED(rv))
    return;
  searchPlugins->AppendNative(NS_LITERAL_CSTRING("distribution"));
  searchPlugins->AppendNative(NS_LITERAL_CSTRING("searchplugins"));

  bool exists;
  rv = searchPlugins->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return;

  nsCOMPtr<nsIFile> commonPlugins;
  rv = searchPlugins->Clone(getter_AddRefs(commonPlugins));
  if (NS_SUCCEEDED(rv)) {
    commonPlugins->AppendNative(NS_LITERAL_CSTRING("common"));
    rv = commonPlugins->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
        array.AppendObject(commonPlugins);
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {

    nsCOMPtr<nsIFile> localePlugins;
    rv = searchPlugins->Clone(getter_AddRefs(localePlugins));
    if (NS_FAILED(rv))
      return;

    localePlugins->AppendNative(NS_LITERAL_CSTRING("locale"));

    nsCString locale;
    rv = prefs->GetCharPref("general.useragent.locale", getter_Copies(locale));
    if (NS_SUCCEEDED(rv)) {

      nsCOMPtr<nsIFile> curLocalePlugins;
      rv = localePlugins->Clone(getter_AddRefs(curLocalePlugins));
      if (NS_SUCCEEDED(rv)) {

        curLocalePlugins->AppendNative(locale);
        rv = curLocalePlugins->Exists(&exists);
        if (NS_SUCCEEDED(rv) && exists) {
          array.AppendObject(curLocalePlugins);
          return; // all done
        }
      }
    }

    // we didn't append the locale dir - try the default one
    nsCString defLocale;
    rv = prefs->GetCharPref("distribution.searchplugins.defaultLocale",
                            getter_Copies(defLocale));
    if (NS_SUCCEEDED(rv)) {

      nsCOMPtr<nsIFile> defLocalePlugins;
      rv = localePlugins->Clone(getter_AddRefs(defLocalePlugins));
      if (NS_SUCCEEDED(rv)) {

        defLocalePlugins->AppendNative(defLocale);
        rv = defLocalePlugins->Exists(&exists);
        if (NS_SUCCEEDED(rv) && exists)
          array.AppendObject(defLocalePlugins);
      }
    }
  }
}

NS_IMETHODIMP
DirectoryProvider::GetFiles(const char *aKey, nsISimpleEnumerator* *aResult)
{
  nsresult rv;

  if (!strcmp(aKey, NS_APP_SEARCH_DIR_LIST)) {
    nsCOMPtr<nsIProperties> dirSvc
      (do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    if (!dirSvc)
      return NS_ERROR_FAILURE;

    nsCOMArray<nsIFile> baseFiles;

    /**
     * We want to preserve the following order, since the search service loads
     * engines in first-loaded-wins order.
     *   - extension search plugin locations (prepended below using
     *     NS_NewUnionEnumerator)
     *   - distro search plugin locations
     *   - user search plugin locations (profile)
     *   - app search plugin location (shipped engines)
     */
    AppendDistroSearchDirs(dirSvc, baseFiles);
    AppendFileKey(NS_APP_USER_SEARCH_DIR, dirSvc, baseFiles);
    AppendFileKey(NS_APP_SEARCH_DIR, dirSvc, baseFiles);

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

NS_IMPL_ISUPPORTS1(DirectoryProvider::AppendingEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
DirectoryProvider::AppendingEnumerator::HasMoreElements(bool *aResult)
{
  *aResult = mNext ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
DirectoryProvider::AppendingEnumerator::GetNext(nsISupports* *aResult)
{
  if (aResult)
    NS_ADDREF(*aResult = mNext);

  mNext = nsnull;

  nsresult rv;

  // Ignore all errors

  bool more;
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

    bool exists;
    rv = mNext->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      break;

    mNext = nsnull;
  }

  return NS_OK;
}

DirectoryProvider::AppendingEnumerator::AppendingEnumerator
    (nsISimpleEnumerator* aBase,
     char const *const *aAppendList) :
  mBase(aBase),
  mAppendList(aAppendList)
{
  // Initialize mNext to begin.
  GetNext(nsnull);
}

} // namespace browser
} // namespace mozilla
