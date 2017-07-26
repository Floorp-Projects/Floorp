/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineUtils.h"
#include "WidevineDecryptor.h"

#include "gmp-api/gmp-errors.h"
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

namespace mozilla {

namespace detail {
LogModule* GetCDMLog()
{
  static LazyLogModule sLog("CDM");
  return sLog;
}
} // namespace detail

GMPErr
ToGMPErr(cdm::Status aStatus)
{
  switch (aStatus) {
    case cdm::kSuccess: return GMPNoErr;
    case cdm::kNeedMoreData: return GMPGenericErr;
    case cdm::kNoKey: return GMPNoKeyErr;
    case cdm::kInitializationError: return GMPGenericErr;
    case cdm::kDecryptError: return GMPCryptoErr;
    case cdm::kDecodeError: return GMPDecodeErr;
    case cdm::kDeferredInitialization: return GMPGenericErr;
    default: return GMPGenericErr;
  }
}

void InitInputBuffer(const GMPEncryptedBufferMetadata* aCrypto,
                     int64_t aTimestamp,
                     const uint8_t* aData,
                     size_t aDataSize,
                     cdm::InputBuffer &aInputBuffer,
                     nsTArray<cdm::SubsampleEntry> &aSubsamples)
{
  if (aCrypto) {
    aInputBuffer.key_id = aCrypto->KeyId();
    aInputBuffer.key_id_size = aCrypto->KeyIdSize();
    aInputBuffer.iv = aCrypto->IV();
    aInputBuffer.iv_size = aCrypto->IVSize();
    aInputBuffer.num_subsamples = aCrypto->NumSubsamples();
    aSubsamples.SetCapacity(aInputBuffer.num_subsamples);
    const uint16_t* clear = aCrypto->ClearBytes();
    const uint32_t* cipher = aCrypto->CipherBytes();
    for (size_t i = 0; i < aCrypto->NumSubsamples(); i++) {
      aSubsamples.AppendElement(cdm::SubsampleEntry(clear[i], cipher[i]));
    }
  }
  aInputBuffer.data = aData;
  aInputBuffer.data_size = aDataSize;
  aInputBuffer.subsamples = aSubsamples.Elements();
  aInputBuffer.timestamp = aTimestamp;
}

CDMWrapper::CDMWrapper(cdm::ContentDecryptionModule_8* aCDM,
                       WidevineDecryptor* aDecryptor)
  : mCDM(aCDM)
  , mDecryptor(aDecryptor)
{
  MOZ_ASSERT(mCDM);
}

CDMWrapper::~CDMWrapper()
{
  CDM_LOG("CDMWrapper destroying CDM=%p", mCDM);
  mCDM->Destroy();
  mCDM = nullptr;
}

WidevineBuffer::WidevineBuffer(size_t aSize)
{
  CDM_LOG("WidevineBuffer(size=%zu) created", aSize);
  mBuffer.SetLength(aSize);
}

WidevineBuffer::~WidevineBuffer()
{
  CDM_LOG("WidevineBuffer(size=%" PRIu32 ") destroyed", Size());
}

void
WidevineBuffer::Destroy()
{
  delete this;
}

uint32_t
WidevineBuffer::Capacity() const
{
  return mBuffer.Length();
}

uint8_t*
WidevineBuffer::Data()
{
  return mBuffer.Elements();
}

void
WidevineBuffer::SetSize(uint32_t aSize)
{
  mBuffer.SetLength(aSize);
}

uint32_t
WidevineBuffer::Size() const
{
  return mBuffer.Length();
}

nsTArray<uint8_t>
WidevineBuffer::ExtractBuffer() {
  nsTArray<uint8_t> out;
  out.SwapElements(mBuffer);
  return out;
}

WidevineDecryptedBlock::WidevineDecryptedBlock()
  : mBuffer(nullptr)
  , mTimestamp(0)
{
}

WidevineDecryptedBlock::~WidevineDecryptedBlock()
{
  if (mBuffer) {
    mBuffer->Destroy();
    mBuffer = nullptr;
  }
}

void
WidevineDecryptedBlock::SetDecryptedBuffer(cdm::Buffer* aBuffer)
{
  mBuffer = aBuffer;
}

cdm::Buffer*
WidevineDecryptedBlock::DecryptedBuffer()
{
  return mBuffer;
}

void
WidevineDecryptedBlock::SetTimestamp(int64_t aTimestamp)
{
  mTimestamp = aTimestamp;
}

int64_t
WidevineDecryptedBlock::Timestamp() const
{
  return mTimestamp;
}

} // namespace mozilla
