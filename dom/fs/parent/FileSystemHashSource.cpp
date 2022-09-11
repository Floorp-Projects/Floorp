/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemHashSource.h"

#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/data_encoding_ffi_generated.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsComponentManagerUtils.h"
#include "nsICryptoHash.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "nsStringFwd.h"

namespace mozilla::dom::fs::data {

Result<EntryId, QMResult> FileSystemHashSource::GenerateHash(
    const EntryId& aParent, const Name& aName) {
  auto makeHasher = [](nsresult* aRv) {
    return do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, aRv);
  };
  QM_TRY_INSPECT(const auto& hasher,
                 QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_GET_TYPED(
                     nsCOMPtr<nsICryptoHash>, makeHasher)));

  QM_TRY(QM_TO_RESULT(hasher->Init(nsICryptoHash::SHA256)));

  QM_TRY(QM_TO_RESULT(
      hasher->Update(reinterpret_cast<const uint8_t*>(aName.BeginReading()),
                     sizeof(char16_t) * aName.Length())));

  QM_TRY(QM_TO_RESULT(
      hasher->Update(reinterpret_cast<const uint8_t*>(aParent.BeginReading()),
                     aParent.Length())));

  EntryId entryId;
  QM_TRY(QM_TO_RESULT(hasher->Finish(/* aASCII */ false, entryId)));
  MOZ_ASSERT(!entryId.IsEmpty());

  return entryId;
}

Result<Name, QMResult> FileSystemHashSource::EncodeHash(
    const EntryId& aEntryId) {
  MOZ_ASSERT(32u == aEntryId.Length());
  nsCString encoded;
  base32encode(&aEntryId, &encoded);

  // We are stripping last four padding characters because
  // it may not be allowed in some file systems.
  MOZ_ASSERT(56u == encoded.Length() && '=' == encoded[52u] &&
             '=' == encoded[53u] && '=' == encoded[54u] && '=' == encoded[55u]);
  encoded.SetLength(52u);

  Name result;
  QM_TRY(OkIf(result.SetCapacity(encoded.Length(), mozilla::fallible)),
         Err(QMResult(NS_ERROR_OUT_OF_MEMORY)));

  result.AppendASCII(encoded);

  return result;
}

}  // namespace mozilla::dom::fs::data
