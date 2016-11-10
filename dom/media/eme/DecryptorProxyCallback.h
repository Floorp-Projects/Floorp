/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecryptorProxyCallback_h_
#define DecryptorProxyCallback_h_

#include "mozilla/dom/MediaKeyStatusMapBinding.h" // For MediaKeyStatus
#include "mozilla/dom/MediaKeyMessageEventBinding.h" // For MediaKeyMessageType
#include "mozilla/CDMProxy.h"

class DecryptorProxyCallback {
public:

  virtual ~DecryptorProxyCallback() {}

  virtual void SetDecryptorId(uint32_t aId) = 0;

  virtual void SetSessionId(uint32_t aCreateSessionId,
                            const nsCString& aSessionId) = 0;

  virtual void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                         bool aSuccess) = 0;

  virtual void ResolvePromise(uint32_t aPromiseId) = 0;

  virtual void RejectPromise(uint32_t aPromiseId,
                             nsresult aException,
                             const nsCString& aSessionId) = 0;

  virtual void SessionMessage(const nsCString& aSessionId,
                              mozilla::dom::MediaKeyMessageType aMessageType,
                              const nsTArray<uint8_t>& aMessage) = 0;

  virtual void ExpirationChange(const nsCString& aSessionId,
                                mozilla::UnixTime aExpiryTime) = 0;

  virtual void SessionClosed(const nsCString& aSessionId) = 0;

  virtual void SessionError(const nsCString& aSessionId,
                            nsresult aException,
                            uint32_t aSystemCode,
                            const nsCString& aMessage) = 0;

  virtual void Decrypted(uint32_t aId,
                         mozilla::DecryptStatus aResult,
                         const nsTArray<uint8_t>& aDecryptedData) = 0;

  virtual void BatchedKeyStatusChanged(const nsCString& aSessionId,
                                       const nsTArray<mozilla::CDMKeyInfo>& aKeyInfos) = 0;
};

#endif
