/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecryptJob.h"
#include "mozilla/Atomics.h"

namespace mozilla {

static Atomic<uint32_t> sDecryptJobInstanceCount(0u);

DecryptJob::DecryptJob(MediaRawData* aSample)
  : mId(++sDecryptJobInstanceCount )
  , mSample(aSample)
{
}

RefPtr<DecryptPromise>
DecryptJob::Ensure()
{
  return mPromise.Ensure(__func__);
}

void
DecryptJob::PostResult(DecryptStatus aResult)
{
  nsTArray<uint8_t> empty;
  PostResult(aResult, empty);
}

void
DecryptJob::PostResult(DecryptStatus aResult,
                       Span<const uint8_t> aDecryptedData)
{
  if (aDecryptedData.Length() != mSample->Size()) {
    NS_WARNING("CDM returned incorrect number of decrypted bytes");
  }
  if (aResult == eme::Ok) {
    UniquePtr<MediaRawDataWriter> writer(mSample->CreateWriter());
    PodCopy(writer->Data(),
            aDecryptedData.Elements(),
            std::min<size_t>(aDecryptedData.Length(), mSample->Size()));
  } else if (aResult == eme::NoKeyErr) {
    NS_WARNING("CDM returned NoKeyErr");
    // We still have the encrypted sample, so we can re-enqueue it to be
    // decrypted again once the key is usable again.
  } else {
    nsAutoCString str("CDM returned decode failure DecryptStatus=");
    str.AppendInt(aResult);
    NS_WARNING(str.get());
  }
  mPromise.Resolve(DecryptResult(aResult, mSample), __func__);
}

} // namespace mozilla
