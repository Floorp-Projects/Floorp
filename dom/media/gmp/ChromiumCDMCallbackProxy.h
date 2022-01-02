/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMCallbackProxy_h_
#define ChromiumCDMCallbackProxy_h_

#include "ChromiumCDMCallback.h"
#include "ChromiumCDMProxy.h"
#include "nsThreadUtils.h"

namespace mozilla {

class ChromiumCDMCallbackProxy : public ChromiumCDMCallback {
 public:
  ChromiumCDMCallbackProxy(ChromiumCDMProxy* aProxy,
                           nsIEventTarget* aMainThread)
      : mProxy(aProxy), mMainThread(aMainThread) {}

  void SetSessionId(uint32_t aPromiseId, const nsCString& aSessionId) override;

  void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                 bool aSuccessful) override;

  void ResolvePromiseWithKeyStatus(uint32_t aPromiseId,
                                   uint32_t aKeyStatus) override;

  void ResolvePromise(uint32_t aPromiseId) override;

  void RejectPromise(uint32_t aPromiseId, ErrorResult&& aException,
                     const nsCString& aErrorMessage) override;

  void SessionMessage(const nsACString& aSessionId, uint32_t aMessageType,
                      nsTArray<uint8_t>&& aMessage) override;

  void SessionKeysChange(
      const nsCString& aSessionId,
      nsTArray<mozilla::gmp::CDMKeyInformation>&& aKeysInfo) override;

  void ExpirationChange(const nsCString& aSessionId,
                        double aSecondsSinceEpoch) override;

  void SessionClosed(const nsCString& aSessionId) override;

  void QueryOutputProtectionStatus() override;

  void Terminated() override;

  void Shutdown() override;

 private:
  template <class Func, class... Args>
  void DispatchToMainThread(const char* const aLabel, Func aFunc,
                            Args&&... aArgs);
  // Warning: Weak ref.
  ChromiumCDMProxy* mProxy;
  const nsCOMPtr<nsIEventTarget> mMainThread;
};

}  // namespace mozilla
#endif
