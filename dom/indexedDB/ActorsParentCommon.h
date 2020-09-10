/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_actorsparentcommon_h__
#define mozilla_dom_indexeddb_actorsparentcommon_h__

// Declares functions and types used locally within IndexedDB, which are defined
// in ActorsParent.cpp

#include <stdint.h>
#include <tuple>
#include <utility>
#include "ErrorList.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

struct JSContext;
class JSObject;
class mozIStorageConnection;
class mozIStorageStatement;
class mozIStorageValueArray;

namespace mozilla {
namespace dom {
namespace indexedDB {

class FileManager;
struct StructuredCloneFileParent;
struct StructuredCloneReadInfoParent;

static constexpr uint32_t kFileCopyBufferSize = 32768;

using IndexOrObjectStoreId = int64_t;

struct IndexDataValue final {
  IndexOrObjectStoreId mIndexId;
  Key mPosition;
  Key mLocaleAwarePosition;
  bool mUnique;

  IndexDataValue();

#ifdef NS_BUILD_REFCNT_LOGGING
  IndexDataValue(IndexDataValue&& aOther);

  MOZ_COUNTED_DTOR(IndexDataValue)
#endif

  IndexDataValue(IndexOrObjectStoreId aIndexId, bool aUnique,
                 const Key& aPosition);

  IndexDataValue(IndexOrObjectStoreId aIndexId, bool aUnique,
                 const Key& aPosition, const Key& aLocaleAwarePosition);

  bool operator==(const IndexDataValue& aOther) const;

  bool operator<(const IndexDataValue& aOther) const;
};

JSObject* GetSandbox(JSContext* aCx);

// The success value of the Result is a a pair of a pointer to the compressed
// index data values buffer and its size. The function does not return a
// nsTArray because the result is typically passed to a function that acquires
// ownership of the pointer.
Result<std::pair<UniqueFreePtr<uint8_t>, uint32_t>, nsresult>
MakeCompressedIndexDataValues(const nsTArray<IndexDataValue>& aIndexValues);

// aOutIndexValues is an output parameter, since its storage is reused.
nsresult ReadCompressedIndexDataValues(
    mozIStorageStatement& aStatement, uint32_t aColumnIndex,
    nsTArray<IndexDataValue>& aOutIndexValues);

using IndexDataValuesAutoArray = AutoTArray<IndexDataValue, 32>;

Result<IndexDataValuesAutoArray, nsresult> ReadCompressedIndexDataValues(
    mozIStorageValueArray& aValues, uint32_t aColumnIndex);

Result<std::tuple<IndexOrObjectStoreId, bool, Span<const uint8_t>>, nsresult>
ReadCompressedIndexId(Span<const uint8_t> aData);

Result<std::pair<uint64_t, mozilla::Span<const uint8_t>>, nsresult>
ReadCompressedNumber(Span<const uint8_t> aSpan);

Result<StructuredCloneReadInfoParent, nsresult>
GetStructuredCloneReadInfoFromValueArray(mozIStorageValueArray* aValues,
                                         uint32_t aDataIndex,
                                         uint32_t aFileIdsIndex,
                                         const FileManager& aFileManager);

Result<StructuredCloneReadInfoParent, nsresult>
GetStructuredCloneReadInfoFromStatement(mozIStorageStatement* aStatement,
                                        uint32_t aDataIndex,
                                        uint32_t aFileIdsIndex,
                                        const FileManager& aFileManager);

Result<nsTArray<StructuredCloneFileParent>, nsresult>
DeserializeStructuredCloneFiles(const FileManager& aFileManager,
                                const nsAString& aText);

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_actorsparent_h__
