/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPCDMCallbackProxy_h_
#define GMPCDMCallbackProxy_h_

#include "mozilla/CDMProxy.h"
#include "gmp-decryption.h"
#include "GMPDecryptorProxy.h"

namespace mozilla {

// Proxies call backs from the CDM on the GMP thread back to the MediaKeys
// object on the main thread.
class GMPCDMCallbackProxy : public GMPDecryptorProxyCallback {
public:

  void SetDecryptorId(uint32_t aId) override;

  void SetSessionId(uint32_t aCreateSessionToken,
                    const nsCString& aSessionId) override;

  void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                 bool aSuccess) override;

  void ResolvePromise(uint32_t aPromiseId) override;

  void RejectPromise(uint32_t aPromiseId,
                     nsresult aException,
                     const nsCString& aSessionId) override;

  void SessionMessage(const nsCString& aSessionId,
                      dom::MediaKeyMessageType aMessageType,
                      const nsTArray<uint8_t>& aMessage) override;

  void ExpirationChange(const nsCString& aSessionId,
                        UnixTime aExpiryTime) override;

  void SessionClosed(const nsCString& aSessionId) override;

  void SessionError(const nsCString& aSessionId,
                    nsresult aException,
                    uint32_t aSystemCode,
                    const nsCString& aMessage) override;

  void Decrypted(uint32_t aId,
                 DecryptStatus aResult,
                 const nsTArray<uint8_t>& aDecryptedData) override;

  void BatchedKeyStatusChanged(const nsCString& aSessionId,
                               const nsTArray<CDMKeyInfo>& aKeyInfos) override;

  void Terminated() override;

  ~GMPCDMCallbackProxy() {}

private:
  friend class GMPCDMProxy;
  explicit GMPCDMCallbackProxy(CDMProxy* aProxy);

  void BatchedKeyStatusChangedInternal(const nsCString& aSessionId,
                                       const nsTArray<CDMKeyInfo>& aKeyInfos);
  // Warning: Weak ref.
  CDMProxy* mProxy;
};

} // namespace mozilla

#endif // GMPCDMCallbackProxy_h_
