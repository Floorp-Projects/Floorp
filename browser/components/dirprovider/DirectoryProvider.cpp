/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDirectoryService.h"
#include "DirectoryProvider.h"

#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsArrayEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMArray.h"
#include "nsDirectoryServiceUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/intl/LocaleService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "nsIPrefLocalizedString.h"

using mozilla::intl::LocaleService;

namespace mozilla {
namespace browser {

NS_IMPL_ISUPPORTS(DirectoryProvider, nsIDirectoryServiceProvider,
                  nsIDirectoryServiceProvider2)

NS_IMETHODIMP
DirectoryProvider::GetFile(const char* aKey, bool* aPersist,
                           nsIFile** aResult) {
  return NS_ERROR_FAILURE;
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

static void AppendDistroSearchDirs(nsIProperties* aDirSvc,
                                   nsCOMArray<nsIFile>& array) {
  nsCOMPtr<nsIFile> searchPlugins;
  nsresult rv = aDirSvc->Get(XRE_APP_DISTRIBUTION_DIR, NS_GET_IID(nsIFile),
                             getter_AddRefs(searchPlugins));
  if (NS_FAILED(rv)) return;
  searchPlugins->AppendNative(NS_LITERAL_CSTRING("searchplugins"));

  bool exists;
  rv = searchPlugins->Exists(&exists);
  if (NS_FAILED(rv) || !exists) return;

  nsCOMPtr<nsIFile> commonPlugins;
  rv = searchPlugins->Clone(getter_AddRefs(commonPlugins));
  if (NS_SUCCEEDED(rv)) {
    commonPlugins->AppendNative(NS_LITERAL_CSTRING("common"));
    rv = commonPlugins->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists) array.AppendObject(commonPlugins);
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    nsCOMPtr<nsIFile> localePlugins;
    rv = searchPlugins->Clone(getter_AddRefs(localePlugins));
    if (NS_FAILED(rv)) return;

    localePlugins->AppendNative(NS_LITERAL_CSTRING("locale"));

    nsAutoCString defLocale;
    rv = prefs->GetCharPref("distribution.searchplugins.defaultLocale",
                            defLocale);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIFile> defLocalePlugins;
      rv = localePlugins->Clone(getter_AddRefs(defLocalePlugins));
      if (NS_SUCCEEDED(rv)) {
        defLocalePlugins->AppendNative(defLocale);
        rv = defLocalePlugins->Exists(&exists);
        if (NS_SUCCEEDED(rv) && exists) {
          array.AppendObject(defLocalePlugins);
          return;  // all done
        }
      }
    }

    // we didn't have a defaultLocale, use the user agent locale
    nsAutoCString locale;
    LocaleService::GetInstance()->GetAppLocaleAsLangTag(locale);

    nsCOMPtr<nsIFile> curLocalePlugins;
    rv = localePlugins->Clone(getter_AddRefs(curLocalePlugins));
    if (NS_SUCCEEDED(rv)) {
      curLocalePlugins->AppendNative(locale);
      rv = curLocalePlugins->Exists(&exists);
      if (NS_SUCCEEDED(rv) && exists) {
        array.AppendObject(curLocalePlugins);
        return;  // all done
      }
    }
  }
}

NS_IMETHODIMP
DirectoryProvider::GetFiles(const char* aKey, nsISimpleEnumerator** aResult) {
  if (!strcmp(aKey, NS_APP_DISTRIBUTION_SEARCH_DIR_LIST)) {
    nsCOMPtr<nsIProperties> dirSvc(
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    if (!dirSvc) return NS_ERROR_FAILURE;

    nsCOMArray<nsIFile> distroFiles;
    AppendDistroSearchDirs(dirSvc, distroFiles);

    return NS_NewArrayEnumerator(aResult, distroFiles, NS_GET_IID(nsIFile));
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DirectoryProvider::AppendingEnumerator::HasMoreElements(bool* aResult) {
  *aResult = mNext ? true : false;
  return NS_OK;
}

NS_IMETHODIMP
DirectoryProvider::AppendingEnumerator::GetNext(nsISupports** aResult) {
  if (aResult) NS_ADDREF(*aResult = mNext);

  mNext = nullptr;

  nsresult rv;

  // Ignore all errors

  bool more;
  while (NS_SUCCEEDED(mBase->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> nextbasesupp;
    mBase->GetNext(getter_AddRefs(nextbasesupp));

    nsCOMPtr<nsIFile> nextbase(do_QueryInterface(nextbasesupp));
    if (!nextbase) continue;

    nextbase->Clone(getter_AddRefs(mNext));
    if (!mNext) continue;

    char const* const* i = mAppendList;
    while (*i) {
      mNext->AppendNative(nsDependentCString(*i));
      ++i;
    }

    bool exists;
    rv = mNext->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists) break;

    mNext = nullptr;
  }

  return NS_OK;
}

DirectoryProvider::AppendingEnumerator::AppendingEnumerator(
    nsISimpleEnumerator* aBase, char const* const* aAppendList)
    : mBase(aBase), mAppendList(aAppendList) {
  // Initialize mNext to begin.
  GetNext(nullptr);
}

}  // namespace browser
}  // namespace mozilla
