/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "mozilla/URLPreloader.h"
#include "nsIChannel.h"
#include "nsILoadInfo.h"
#include "nsIStreamLoader.h"
#include "nsNetUtil.h"
#include "nsString.h"

namespace mozilla {
namespace intl {

class L10nRegistry {
 public:
  static nsresult Load(const nsACString& aPath,
                       nsIStreamLoaderObserver* aObserver) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aPath);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(uri, NS_ERROR_INVALID_ARG);

    RefPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(
        getter_AddRefs(loader), uri, aObserver,
        nsContentUtils::GetSystemPrincipal(),
        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        nsIContentPolicy::TYPE_OTHER);

    return rv;
  }

  static nsresult LoadSync(const nsACString& aPath, void** aData,
                           uint64_t* aSize) {
    nsCOMPtr<nsIURI> uri;

    nsresult rv = NS_NewURI(getter_AddRefs(uri), aPath);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(uri, NS_ERROR_INVALID_ARG);

    auto result = URLPreloader::ReadURI(uri);
    if (result.isOk()) {
      auto uri = result.unwrap();
      *aData = ToNewCString(uri);
      *aSize = uri.Length();
      return NS_OK;
    }

    auto err = result.unwrapErr();
    if (err != NS_ERROR_INVALID_ARG && err != NS_ERROR_NOT_INITIALIZED) {
      return err;
    }

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(
        getter_AddRefs(channel), uri, nsContentUtils::GetSystemPrincipal(),
        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        nsIContentPolicy::TYPE_OTHER, nullptr, /* nsICookieJarSettings */
        nullptr,                               /* aPerformanceStorage */
        nullptr,                               /* aLoadGroup */
        nullptr,                               /* aCallbacks */
        nsIRequest::LOAD_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInputStream> input;
    rv = channel->Open(getter_AddRefs(input));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

    return NS_ReadInputStreamToBuffer(input, aData, -1, aSize);
  }
};

}  // namespace intl
}  // namespace mozilla

extern "C" {
nsresult L10nRegistryLoad(const nsACString* aPath,
                          const nsIStreamLoaderObserver* aObserver) {
  if (!aPath || !aObserver) {
    return NS_ERROR_INVALID_ARG;
  }

  return mozilla::intl::L10nRegistry::Load(
      *aPath, const_cast<nsIStreamLoaderObserver*>(aObserver));
}

nsresult L10nRegistryLoadSync(const nsACString* aPath, void** aData,
                              uint64_t* aSize) {
  if (!aPath || !aData || !aSize) {
    return NS_ERROR_INVALID_ARG;
  }

  return mozilla::intl::L10nRegistry::LoadSync(*aPath, aData, aSize);
}

}  // extern "C"
