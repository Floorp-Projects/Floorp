/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecryptorProxyCallback_h_
#define DecryptorProxyCallback_h_

class DecryptorProxyCallback {
public:
  virtual ~DecryptorProxyCallback() {}

  virtual void SetSessionId(uint32_t aCreateSessionId,
                            const nsCString& aSessionId) = 0;

  virtual void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                         bool aSuccess) = 0;

  virtual void ResolvePromise(uint32_t aPromiseId) = 0;

  virtual void RejectPromise(uint32_t aPromiseId,
                             nsresult aException,
                             const nsCString& aSessionId) = 0;

  virtual void SessionMessage(const nsCString& aSessionId,
                              GMPSessionMessageType aMessageType,
                              const nsTArray<uint8_t>& aMessage) = 0;

  virtual void ExpirationChange(const nsCString& aSessionId,
                                GMPTimestamp aExpiryTime) = 0;

  virtual void SessionClosed(const nsCString& aSessionId) = 0;

  virtual void SessionError(const nsCString& aSessionId,
                            nsresult aException,
                            uint32_t aSystemCode,
                            const nsCString& aMessage) = 0;

  virtual void KeyStatusChanged(const nsCString& aSessionId,
                                const nsTArray<uint8_t>& aKeyId,
                                GMPMediaKeyStatus aStatus) = 0;

  virtual void Decrypted(uint32_t aId,
                         GMPErr aResult,
                         const nsTArray<uint8_t>& aDecryptedData) = 0;
};

#endif