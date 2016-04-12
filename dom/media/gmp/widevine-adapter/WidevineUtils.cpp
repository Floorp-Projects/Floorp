/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineUtils.h"

#include "gmp-api/gmp-errors.h"
#include <stdarg.h>
#include <stdio.h>

namespace mozilla {

#ifdef ENABLE_WIDEVINE_LOG
void
Log(const char* aFormat, ...)
{
  va_list ap;
  va_start(ap, aFormat);
  const size_t len = 1024;
  char buf[len];
  vsnprintf(buf, len, aFormat, ap);
  va_end(ap);
  if (getenv("GMP_LOG_FILE")) {
    FILE* f = fopen(getenv("GMP_LOG_FILE"), "a");
    if (f) {
      fprintf(f, "%s\n", buf);
      fflush(f);
      fclose(f);
      f = nullptr;
    }
  } else {
    printf("LOG: %s\n", buf);
  }
}
#endif // ENABLE_WIDEVINE_LOG

GMPErr
ToGMPErr(cdm::Status aStatus)
{
  switch (aStatus) {
    case cdm::kSuccess: return GMPNoErr;
    case cdm::kNeedMoreData: return GMPGenericErr;
    case cdm::kNoKey: return GMPNoKeyErr;
    case cdm::kSessionError: return GMPGenericErr;
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

} // namespace mozilla
