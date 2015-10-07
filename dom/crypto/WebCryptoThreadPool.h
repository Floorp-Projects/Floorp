/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebCryptoThreadPool_h
#define mozilla_dom_WebCryptoThreadPool_h

#include "nsIObserverService.h"
#include "nsIThreadPool.h"

namespace mozilla {
namespace dom {

class WebCryptoThreadPool final : nsIObserver {
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static void
  Initialize();

  static nsresult
  Dispatch(nsIRunnable* aRunnable);

private:
  WebCryptoThreadPool()
    : mMutex("WebCryptoThreadPool::mMutex")
    , mPool(nullptr)
  { }
  virtual ~WebCryptoThreadPool()
  { }

  nsresult
  Init();

  nsresult
  DispatchInternal(nsIRunnable* aRunnable);

  void
  Shutdown();

  NS_IMETHODIMP Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const char16_t* aData) override;

  mozilla::Mutex mMutex;
  nsCOMPtr<nsIThreadPool> mPool;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebCryptoThreadPool_h
