/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_QuotaOfflineStorage_h
#define mozilla_dom_cache_QuotaOfflineStorage_h

#include "nsISupportsImpl.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/cache/Context.h"
#include "nsIOfflineStorage.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace cache {

class OfflineStorage final : public nsIOfflineStorage
{
public:
  static already_AddRefed<OfflineStorage>
  Register(Context::ThreadsafeHandle* aContext, const QuotaInfo& aQuotaInfo);

  void
  AddDestroyCallback(nsIRunnable* aCallback);

private:
  OfflineStorage(Context::ThreadsafeHandle* aContext,
                 const QuotaInfo& aQuotaInfo,
                 Client* aClient);
  ~OfflineStorage();

  nsRefPtr<Context::ThreadsafeHandle> mContext;
  const QuotaInfo mQuotaInfo;
  nsRefPtr<Client> mClient;
  nsTArray<nsCOMPtr<nsIRunnable>> mDestroyCallbacks;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOFFLINESTORAGE
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_QuotaOfflineStorage_h
