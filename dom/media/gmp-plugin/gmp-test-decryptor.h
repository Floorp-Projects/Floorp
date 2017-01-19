/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_DECRYPTOR_H__
#define FAKE_DECRYPTOR_H__

#include "gmp-decryption.h"
#include <string>
#include "mozilla/Attributes.h"

class FakeDecryptor : public GMPDecryptor {
public:

  explicit FakeDecryptor(GMPDecryptorHost* aHost);

  void Init(GMPDecryptorCallback* aCallback,
            bool aDistinctiveIdentifierRequired,
            bool aPersistentStateRequired) override
  {
    mCallback = aCallback;
  }

  void CreateSession(uint32_t aCreateSessionToken,
                     uint32_t aPromiseId,
                     const char* aInitDataType,
                     uint32_t aInitDataTypeSize,
                     const uint8_t* aInitData,
                     uint32_t aInitDataSize,
                     GMPSessionType aSessionType) override
  {
  }

  void LoadSession(uint32_t aPromiseId,
                   const char* aSessionId,
                   uint32_t aSessionIdLength) override
  {
  }

  void UpdateSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength,
                     const uint8_t* aResponse,
                     uint32_t aResponseSize) override;

  void CloseSession(uint32_t aPromiseId,
                    const char* aSessionId,
                    uint32_t aSessionIdLength) override
  {
  }

  void RemoveSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength) override
  {
  }

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCert,
                            uint32_t aServerCertSize) override
  {
  }

  void Decrypt(GMPBuffer* aBuffer,
               GMPEncryptedBufferMetadata* aMetadata) override
  {
  }

  void DecryptingComplete() override;

  static void Message(const std::string& aMessage);

  void ProcessRecordNames(GMPRecordIterator* aRecordIterator,
                          GMPErr aStatus);

  static void SetNodeId(const char* aNodeId, uint32_t aLength) {
    sNodeId = std::string(aNodeId, aNodeId + aLength);
  }

private:

  virtual ~FakeDecryptor() {}
  static FakeDecryptor* sInstance;
  static std::string sNodeId;

  void TestStorage();

  GMPDecryptorCallback* mCallback;
  GMPDecryptorHost* mHost;
};

#endif
