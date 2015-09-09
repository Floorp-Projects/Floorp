/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIEHistoryEnumerator.h"

#include <urlhist.h>
#include <shlguid.h>

#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsIVariant.h"
#include "nsNetUtil.h"
#include "nsStringAPI.h"
#include "nsWindowsMigrationUtils.h"
#include "prtime.h"

////////////////////////////////////////////////////////////////////////////////
//// nsIEHistoryEnumerator

NS_IMPL_ISUPPORTS(nsIEHistoryEnumerator, nsISimpleEnumerator)

nsIEHistoryEnumerator::nsIEHistoryEnumerator()
{
  ::CoInitialize(nullptr);  
}

nsIEHistoryEnumerator::~nsIEHistoryEnumerator()
{
  ::CoUninitialize();
}

void
nsIEHistoryEnumerator::EnsureInitialized()
{
  if (mURLEnumerator)
    return;

  HRESULT hr = ::CoCreateInstance(CLSID_CUrlHistory,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_IUrlHistoryStg2,
                                  getter_AddRefs(mIEHistory));
  if (FAILED(hr))
    return;

  hr = mIEHistory->EnumUrls(getter_AddRefs(mURLEnumerator));
  if (FAILED(hr))
    return;
}

NS_IMETHODIMP
nsIEHistoryEnumerator::HasMoreElements(bool* _retval)
{
  *_retval = false;

  EnsureInitialized();
  MOZ_ASSERT(mURLEnumerator, "Should have instanced an IE History URLEnumerator");
  if (!mURLEnumerator)
    return NS_OK;

  STATURL statURL;
  ULONG fetched;

  // First argument is not implemented, so doesn't matter what we pass.
  HRESULT hr = mURLEnumerator->Next(1, &statURL, &fetched);
  if (FAILED(hr) || fetched != 1UL) {
    // Reached the last entry.
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  if (statURL.pwcsUrl) {
    nsDependentString url(statURL.pwcsUrl);
    nsresult rv = NS_NewURI(getter_AddRefs(uri), url);
    ::CoTaskMemFree(statURL.pwcsUrl);
    if (NS_FAILED(rv)) {
      // Got a corrupt or invalid URI, continue to the next entry.
      return HasMoreElements(_retval);
    }
  }

  nsDependentString title(statURL.pwcsTitle);

  bool lastVisitTimeIsValid;
  PRTime lastVisited = WinMigrationFileTimeToPRTime(&(statURL.ftLastVisited), &lastVisitTimeIsValid);

  mCachedNextEntry = do_CreateInstance("@mozilla.org/hash-property-bag;1");
  MOZ_ASSERT(mCachedNextEntry, "Should have instanced a new property bag");
  if (mCachedNextEntry) {
    mCachedNextEntry->SetPropertyAsInterface(NS_LITERAL_STRING("uri"), uri);
    mCachedNextEntry->SetPropertyAsAString(NS_LITERAL_STRING("title"), title);
    if (lastVisitTimeIsValid) {
      mCachedNextEntry->SetPropertyAsInt64(NS_LITERAL_STRING("time"), lastVisited);
    }

    *_retval = true;
  }

  if (statURL.pwcsTitle)
    ::CoTaskMemFree(statURL.pwcsTitle);

  return NS_OK;
}

NS_IMETHODIMP
nsIEHistoryEnumerator::GetNext(nsISupports** _retval)
{
  *_retval = nullptr;

  if (!mCachedNextEntry)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*_retval = mCachedNextEntry);
  // Release the cached entry, so it can't be returned twice.
  mCachedNextEntry = nullptr;

  return NS_OK;
}
