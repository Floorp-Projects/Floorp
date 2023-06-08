/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemHashStorageFunction.h"

#include "FileSystemHashSource.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/storage/Variant.h"
#include "nsString.h"
#include "nsStringFwd.h"

namespace mozilla::dom::fs::data {

NS_IMPL_ISUPPORTS(FileSystemHashStorageFunction, mozIStorageFunction)

NS_IMETHODIMP
FileSystemHashStorageFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  MOZ_ASSERT(aFunctionArguments);
  MOZ_ASSERT(aResult);

  const int32_t parentIndex = 0;
  const int32_t childIndex = 1;

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aFunctionArguments->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 2u);

    int32_t parentType = mozIStorageValueArray::VALUE_TYPE_INTEGER;
    MOZ_ALWAYS_SUCCEEDS(
        aFunctionArguments->GetTypeOfIndex(parentIndex, &parentType));
    MOZ_ASSERT(parentType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    int32_t childType = mozIStorageValueArray::VALUE_TYPE_INTEGER;
    MOZ_ALWAYS_SUCCEEDS(
        aFunctionArguments->GetTypeOfIndex(childIndex, &childType));
    MOZ_ASSERT(childType == mozIStorageValueArray::VALUE_TYPE_BLOB);
  }
#endif

  QM_TRY_INSPECT(
      const EntryId& parentHash,
      MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsCString, aFunctionArguments,
                                        GetBlobAsUTF8String, parentIndex));

  QM_TRY_INSPECT(const Name& childName, MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                                            nsString, aFunctionArguments,
                                            GetBlobAsString, childIndex));

  QM_TRY_INSPECT(const EntryId& buffer,
                 FileSystemHashSource::GenerateHash(parentHash, childName)
                     .mapErr([](const auto& aRv) { return ToNSResult(aRv); }));

  nsCOMPtr<nsIVariant> result =
      new mozilla::storage::BlobVariant(std::make_pair(
          static_cast<const void*>(buffer.get()), int(buffer.Length())));

  result.forget(aResult);

  return NS_OK;
}

}  // namespace mozilla::dom::fs::data
