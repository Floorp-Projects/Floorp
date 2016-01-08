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
  virtual uint32_t Size() const override;
  virtual void StringAt(uint32_t aIndex,
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

  virtual const uint8_t* KeyId() const override;
  virtual uint32_t KeyIdSize() const override;
  virtual const uint8_t* IV() const override;
  virtual uint32_t IVSize() const override;
  virtual uint32_t NumSubsamples() const override;
  virtual const uint16_t* ClearBytes() const override;
  virtual const uint32_t* CipherBytes() const override;
  virtual const GMPStringList* SessionIds() const override;

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
  virtual uint32_t Id() const {
    return mId;
  }
  virtual uint8_t* Data() {
    return mData.Elements();
  }
  virtual uint32_t Size() const {
    return mData.Length();
  }
  virtual void Resize(uint32_t aSize) {
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
