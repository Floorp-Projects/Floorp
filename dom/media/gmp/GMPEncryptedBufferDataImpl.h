/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPEncryptedBufferDataImpl_h_
#define GMPEncryptedBufferDataImpl_h_

#include "gmp-decryption.h"
#include "nsTArray.h"
#include "mozilla/gmp/GMPTypes.h"

namespace mozilla {
class CryptoSample;

namespace gmp {

class GMPStringListImpl : public GMPStringList
{
public:
  explicit GMPStringListImpl(const nsTArray<nsCString>& aStrings);
  uint32_t Size() const override;
  void StringAt(uint32_t aIndex,
                const char** aOutString, uint32_t *aOutLength) const override;
  virtual ~GMPStringListImpl() override;
  void RelinquishData(nsTArray<nsCString>& aStrings);

private:
  nsTArray<nsCString> mStrings;
};

class GMPEncryptedBufferDataImpl : public GMPEncryptedBufferMetadata {
public:
  explicit GMPEncryptedBufferDataImpl(const CryptoSample& aCrypto);
  explicit GMPEncryptedBufferDataImpl(const GMPDecryptionData& aData);
  virtual ~GMPEncryptedBufferDataImpl();

  void RelinquishData(GMPDecryptionData& aData);

  const uint8_t* KeyId() const override;
  uint32_t KeyIdSize() const override;
  const uint8_t* IV() const override;
  uint32_t IVSize() const override;
  uint32_t NumSubsamples() const override;
  const uint16_t* ClearBytes() const override;
  const uint32_t* CipherBytes() const override;
  const GMPStringList* SessionIds() const override;

private:
  nsTArray<uint8_t> mKeyId;
  nsTArray<uint8_t> mIV;
  nsTArray<uint16_t> mClearBytes;
  nsTArray<uint32_t> mCipherBytes;

  GMPStringListImpl mSessionIdList;
};

class GMPBufferImpl : public GMPBuffer {
public:
  GMPBufferImpl(uint32_t aId, const nsTArray<uint8_t>& aData)
    : mId(aId)
    , mData(aData)
  {
  }
  uint32_t Id() const override {
    return mId;
  }
  uint8_t* Data() override {
    return mData.Elements();
  }
  uint32_t Size() const override {
    return mData.Length();
  }
  void Resize(uint32_t aSize) override {
    mData.SetLength(aSize);
  }

  // Set metadata object to be freed when this buffer is destroyed.
  void SetMetadata(GMPEncryptedBufferDataImpl* aMetadata) {
    mMetadata = aMetadata;
  }

  uint32_t mId;
  nsTArray<uint8_t> mData;
  nsAutoPtr<GMPEncryptedBufferDataImpl> mMetadata;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPEncryptedBufferDataImpl_h_
