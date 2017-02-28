/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPDecryptorProxy_h_
#define GMPDecryptorProxy_h_

#include "mozilla/DecryptorProxyCallback.h"
#include "GMPCallbackBase.h"
#include "gmp-decryption.h"
#include "nsString.h"

namespace mozilla {
class CryptoSample;
} // namespace mozilla

class GMPDecryptorProxyCallback : public DecryptorProxyCallback,
                                  public GMPCallbackBase {
public:
  virtual ~GMPDecryptorProxyCallback() {}
};

class GMPDecryptorProxy {
public:
  ~GMPDecryptorProxy() {}

  virtual uint32_t GetPluginId() const = 0;

  virtual nsresult Init(GMPDecryptorProxyCallback* aCallback,
                        bool aDistinctiveIdentifierRequired,
                        bool aPersistentStateRequired) = 0;

  virtual void CreateSession(uint32_t aCreateSessionToken,
                             uint32_t aPromiseId,
                             const nsCString& aInitDataType,
                             const nsTArray<uint8_t>& aInitData,
                             GMPSessionType aSessionType) = 0;

  virtual void LoadSession(uint32_t aPromiseId,
                           const nsCString& aSessionId) = 0;

  virtual void UpdateSession(uint32_t aPromiseId,
                             const nsCString& aSessionId,
                             const nsTArray<uint8_t>& aResponse) = 0;

  virtual void CloseSession(uint32_t aPromiseId,
                            const nsCString& aSessionId) = 0;

  virtual void RemoveSession(uint32_t aPromiseId,
                             const nsCString& aSessionId) = 0;

  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const nsTArray<uint8_t>& aServerCert) = 0;

  virtual void Decrypt(uint32_t aId,
                       const mozilla::CryptoSample& aCrypto,
                       const nsTArray<uint8_t>& aBuffer) = 0;

  virtual void Close() = 0;
};

#endif // GMPDecryptorProxy_h_
