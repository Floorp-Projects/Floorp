/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_DECRYPTOR_H__
#define FAKE_DECRYPTOR_H__

#include "gmp-decryption.h"
#include "gmp-async-shutdown.h"
#include <string>
#include "mozilla/Attributes.h"

class FakeDecryptor : public GMPDecryptor {
public:

  FakeDecryptor();

  virtual void Init(GMPDecryptorCallback* aCallback) MOZ_OVERRIDE {
    mCallback = aCallback;
  }

  virtual void CreateSession(uint32_t aPromiseId,
                             const char* aInitDataType,
                             uint32_t aInitDataTypeSize,
                             const uint8_t* aInitData,
                             uint32_t aInitDataSize,
                             GMPSessionType aSessionType) MOZ_OVERRIDE
  {
  }

  virtual void LoadSession(uint32_t aPromiseId,
                           const char* aSessionId,
                           uint32_t aSessionIdLength) MOZ_OVERRIDE
  {
  }

  virtual void UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize) MOZ_OVERRIDE;

  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) MOZ_OVERRIDE
  {
  }

  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) MOZ_OVERRIDE
  {
  }

  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) MOZ_OVERRIDE
  {
  }

  virtual void Decrypt(GMPBuffer* aBuffer,
                       GMPEncryptedBufferMetadata* aMetadata) MOZ_OVERRIDE
  {
  }

  virtual void DecryptingComplete() MOZ_OVERRIDE;

  static void Message(const std::string& aMessage);

private:

  virtual ~FakeDecryptor() {}
  static FakeDecryptor* sInstance;

  void TestStorage();

  GMPDecryptorCallback* mCallback;
};

class TestAsyncShutdown : public GMPAsyncShutdown {
public:
  explicit TestAsyncShutdown(GMPAsyncShutdownHost* aHost)
    : mHost(aHost)
  {
  }
  virtual void BeginShutdown() MOZ_OVERRIDE;
private:
  GMPAsyncShutdownHost* mHost;
};

#endif
