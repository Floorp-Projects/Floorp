/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMCallback_h_
#define ChromiumCDMCallback_h_

#include "mozilla/CDMProxy.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h" // For MediaKeyStatus
#include "mozilla/dom/MediaKeyMessageEventBinding.h" // For MediaKeyMessageType
#include "mozilla/gmp/GMPTypes.h" // For CDMKeyInformation

class ChromiumCDMCallback {
public:

  virtual ~ChromiumCDMCallback() {}

  virtual void SetSessionId(uint32_t aPromiseId,
                            const nsCString& aSessionId) = 0;

  virtual void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                         bool aSuccessful) = 0;

  virtual void ResolvePromiseWithKeyStatus(uint32_t aPromiseId,
                                           uint32_t aKeyStatus) = 0;

  virtual void ResolvePromise(uint32_t aPromiseId) = 0;

  virtual void RejectPromise(uint32_t aPromiseId,
                             nsresult aError,
                             const nsCString& aErrorMessage) = 0;

  virtual void SessionMessage(const nsACString& aSessionId,
                              uint32_t aMessageType,
                              nsTArray<uint8_t>&& aMessage) = 0;

  virtual void SessionKeysChange(const nsCString& aSessionId,
                                 nsTArray<mozilla::gmp::CDMKeyInformation>&& aKeysInfo) = 0;

  virtual void ExpirationChange(const nsCString& aSessionId,
                                double aSecondsSinceEpoch) = 0;

  virtual void SessionClosed(const nsCString& aSessionId) = 0;

  virtual void Terminated() = 0;

  virtual void Shutdown() = 0;
};

#endif
