/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include <algorithm>
#include <stdint.h>  // UINTPTR_MAX, uintptr_t
#include "FileInfo.h"
#include "FileManager.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"
#include "IndexedDatabaseManager.h"
#include "js/StructuredClone.h"
#include "js/Value.h"
#include "jsapi.h"
#include "KeyPath.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "mozilla/SnappyCompressOutputStream.h"
#include "mozilla/SnappyUncompressInputStream.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/storage.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/filehandle/ActorsParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBCursorParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseFileParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBTransactionParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBVersionChangeTransactionParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIndexedDBUtilsParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBPermissionRequestParent.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/ipc/IPCBlobInputStreamParent.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PBackground.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/Scoped.h"
#include "mozilla/storage/Variant.h"
#include "nsAutoPtr.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsEscape.h"
#include "nsHashKeys.h"
#include "nsNetUtil.h"
#include "nsIAsyncInputStream.h"
#include "nsISimpleEnumerator.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIFileProtocolHandler.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsInterfaceHashtable.h"
#include "nsIOutputStream.h"
#include "nsIPipe.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsISupportsPriority.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsQueryObject.h"
#include "nsRefPtrHashtable.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCID.h"
#include "PermissionRequestBase.h"
#include "ProfilerHelpers.h"
#include "prsystem.h"
#include "prtime.h"
#include "ReportInternalError.h"
#include "snappy/snappy.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

#define IDB_DEBUG_LOG(_args) \
  MOZ_LOG(IndexedDatabaseManager::GetLoggingModule(), LogLevel::Debug, _args)

#if defined(MOZ_WIDGET_ANDROID)
#  define IDB_MOBILE
#endif

namespace mozilla {

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc, PRFileDesc,
                                          PR_Close);

namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;
using mozilla::dom::quota::Client;

namespace {

class ConnectionPool;
class Cursor;
class Database;
struct DatabaseActorInfo;
class DatabaseFile;
class DatabaseLoggingInfo;
class DatabaseMaintenance;
class Factory;
class Maintenance;
class MutableFile;
class OpenDatabaseOp;
class TransactionBase;
class TransactionDatabaseOperationBase;
class VersionChangeTransaction;

/*******************************************************************************
 * Constants
 ******************************************************************************/

// If JS_STRUCTURED_CLONE_VERSION changes then we need to update our major
// schema version.
static_assert(JS_STRUCTURED_CLONE_VERSION == 8,
              "Need to update the major schema version.");

// Major schema version. Bump for almost everything.
const uint32_t kMajorSchemaVersion = 26;

// Minor schema version. Should almost always be 0 (maybe bump on release
// branches if we have to).
const uint32_t kMinorSchemaVersion = 0;

// The schema version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 4 bits so the max value is
// 0xFFFFFFF. The minor version occupies the lower 4 bits and its max is 0xF.
static_assert(kMajorSchemaVersion <= 0xFFFFFFF,
              "Major version needs to fit in 28 bits.");
static_assert(kMinorSchemaVersion <= 0xF,
              "Minor version needs to fit in 4 bits.");

const int32_t kSQLiteSchemaVersion =
    int32_t((kMajorSchemaVersion << 4) + kMinorSchemaVersion);

const int32_t kStorageProgressGranularity = 1000;

// Changing the value here will override the page size of new databases only.
// A journal mode change and VACUUM are needed to change existing databases, so
// the best way to do that is to use the schema version upgrade mechanism.
const uint32_t kSQLitePageSizeOverride =
#ifdef IDB_MOBILE
    2048;
#else
    4096;
#endif

static_assert(kSQLitePageSizeOverride == /* mozStorage default */ 0 ||
                  (kSQLitePageSizeOverride % 2 == 0 &&
                   kSQLitePageSizeOverride >= 512 &&
                   kSQLitePageSizeOverride <= 65536),
              "Must be 0 (disabled) or a power of 2 between 512 and 65536!");

// Set to -1 to use SQLite's default, 0 to disable, or some positive number to
// enforce a custom limit.
const int32_t kMaxWALPages = 5000;  // 20MB on desktop, 10MB on mobile.

// Set to some multiple of the page size to grow the database in larger chunks.
const uint32_t kSQLiteGrowthIncrement = kSQLitePageSizeOverride * 2;

static_assert(kSQLiteGrowthIncrement >= 0 &&
                  kSQLiteGrowthIncrement % kSQLitePageSizeOverride == 0 &&
                  kSQLiteGrowthIncrement < uint32_t(INT32_MAX),
              "Must be 0 (disabled) or a positive multiple of the page size!");

// The maximum number of threads that can be used for database activity at a
// single time.
const uint32_t kMaxConnectionThreadCount = 20;

static_assert(kMaxConnectionThreadCount, "Must have at least one thread!");

// The maximum number of threads to keep when idle. Threads that become idle in
// excess of this number will be shut down immediately.
const uint32_t kMaxIdleConnectionThreadCount = 2;

static_assert(kMaxConnectionThreadCount >= kMaxIdleConnectionThreadCount,
              "Idle thread limit must be less than total thread limit!");

// The length of time that database connections will be held open after all
// transactions have completed before doing idle maintenance.
const uint32_t kConnectionIdleMaintenanceMS = 2 * 1000;  // 2 seconds

// The length of time that database connections will be held open after all
// transactions and maintenance have completed.
const uint32_t kConnectionIdleCloseMS = 10 * 1000;  // 10 seconds

// The length of time that idle threads will stay alive before being shut down.
const uint32_t kConnectionThreadIdleMS = 30 * 1000;  // 30 seconds

#define SAVEPOINT_CLAUSE "SAVEPOINT sp;"

const uint32_t kFileCopyBufferSize = 32768;

#define JOURNAL_DIRECTORY_NAME "journals"

const char kFileManagerDirectoryNameSuffix[] = ".files";
const char kSQLiteSuffix[] = ".sqlite";
const char kSQLiteJournalSuffix[] = ".sqlite-journal";
const char kSQLiteSHMSuffix[] = ".sqlite-shm";
const char kSQLiteWALSuffix[] = ".sqlite-wal";

const char kPrefIndexedDBEnabled[] = "dom.indexedDB.enabled";

const char kPrefFileHandleEnabled[] = "dom.fileHandle.enabled";

#define IDB_PREFIX "indexedDB"

#define PERMISSION_STRING_CHROME_BASE IDB_PREFIX "-chrome-"
#define PERMISSION_STRING_CHROME_READ_SUFFIX "-read"
#define PERMISSION_STRING_CHROME_WRITE_SUFFIX "-write"

// The deletion marker file is created before RemoveDatabaseFilesAndDirectory
// begins deleting a database. It is removed as the last step of deletion. If a
// deletion marker file is found when initializing the origin, the deletion
// routine is run again to ensure that the database and all of its related files
// are removed. The primary goal of this mechanism is to avoid situations where
// a database has been partially deleted, leading to inconsistent state for the
// origin.
#define IDB_DELETION_MARKER_FILE_PREFIX "idb-deleting-"

const uint32_t kDeleteTimeoutMs = 1000;

#ifdef DEBUG

const int32_t kDEBUGThreadPriority = nsISupportsPriority::PRIORITY_NORMAL;
const uint32_t kDEBUGThreadSleepMS = 0;

const int32_t kDEBUGTransactionThreadPriority =
    nsISupportsPriority::PRIORITY_NORMAL;
const uint32_t kDEBUGTransactionThreadSleepMS = 0;

#endif

template <size_t N>
constexpr size_t LiteralStringLength(const char (&aArr)[N]) {
  static_assert(N, "Zero-length string literal?!");

  // Don't include the null terminator.
  return N - 1;
}

/*******************************************************************************
 * Metadata classes
 ******************************************************************************/

struct FullIndexMetadata {
  IndexMetadata mCommonMetadata;

  bool mDeleted;

 public:
  FullIndexMetadata()
      : mCommonMetadata(0, nsString(), KeyPath(0), nsCString(), false, false,
                        false),
        mDeleted(false) {
    // This can happen either on the QuotaManager IO thread or on a
    // versionchange transaction thread. These threads can never race so this is
    // totally safe.
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullIndexMetadata)

 private:
  ~FullIndexMetadata() = default;
};

typedef nsRefPtrHashtable<nsUint64HashKey, FullIndexMetadata> IndexTable;

struct FullObjectStoreMetadata {
  ObjectStoreMetadata mCommonMetadata;
  IndexTable mIndexes;

  // These two members are only ever touched on a transaction thread!
  int64_t mNextAutoIncrementId;
  int64_t mCommittedAutoIncrementId;

  bool mDeleted;

 public:
  FullObjectStoreMetadata()
      : mCommonMetadata(0, nsString(), KeyPath(0), false),
        mNextAutoIncrementId(0),
        mCommittedAutoIncrementId(0),
        mDeleted(false) {
    // This can happen either on the QuotaManager IO thread or on a
    // versionchange transaction thread. These threads can never race so this is
    // totally safe.
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullObjectStoreMetadata);

  bool HasLiveIndexes() const;

 private:
  ~FullObjectStoreMetadata() = default;
};

typedef nsRefPtrHashtable<nsUint64HashKey, FullObjectStoreMetadata>
    ObjectStoreTable;

struct FullDatabaseMetadata {
  DatabaseMetadata mCommonMetadata;
  nsCString mDatabaseId;
  nsString mFilePath;
  ObjectStoreTable mObjectStores;

  int64_t mNextObjectStoreId;
  int64_t mNextIndexId;

 public:
  explicit FullDatabaseMetadata(const DatabaseMetadata& aCommonMetadata)
      : mCommonMetadata(aCommonMetadata),
        mNextObjectStoreId(0),
        mNextIndexId(0) {
    AssertIsOnBackgroundThread();
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullDatabaseMetadata)

  already_AddRefed<FullDatabaseMetadata> Duplicate() const;

 private:
  ~FullDatabaseMetadata() = default;
};

template <class MetadataType>
class MOZ_STACK_CLASS MetadataNameOrIdMatcher final {
  typedef MetadataNameOrIdMatcher<MetadataType> SelfType;

  const int64_t mId;
  const nsString mName;
  RefPtr<MetadataType> mMetadata;
  bool mCheckName;

 public:
  template <class Enumerable>
  static MetadataType* Match(const Enumerable& aEnumerable, uint64_t aId,
                             const nsAString& aName) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);

    SelfType closure(aId, aName);
    MatchHelper(aEnumerable, &closure);

    return closure.mMetadata;
  }

  template <class Enumerable>
  static MetadataType* Match(const Enumerable& aEnumerable, uint64_t aId) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);

    SelfType closure(aId);
    MatchHelper(aEnumerable, &closure);

    return closure.mMetadata;
  }

 private:
  MetadataNameOrIdMatcher(const int64_t& aId, const nsAString& aName)
      : mId(aId), mName(aName), mMetadata(nullptr), mCheckName(true) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);
  }

  explicit MetadataNameOrIdMatcher(const int64_t& aId)
      : mId(aId), mMetadata(nullptr), mCheckName(false) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);
  }

  template <class Enumerable>
  static void MatchHelper(const Enumerable& aEnumerable, SelfType* aClosure) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aClosure);

    for (auto iter = aEnumerable.ConstIter(); !iter.Done(); iter.Next()) {
#ifdef DEBUG
      const uint64_t key = iter.Key();
#endif
      MetadataType* value = iter.UserData();
      MOZ_ASSERT(key != 0);
      MOZ_ASSERT(value);

      if (!value->mDeleted &&
          (aClosure->mId == value->mCommonMetadata.id() ||
           (aClosure->mCheckName &&
            aClosure->mName == value->mCommonMetadata.name()))) {
        aClosure->mMetadata = value;
        break;
      }
    }
  }
};

struct IndexDataValue final {
  int64_t mIndexId;
  Key mKey;
  Key mSortKey;
  bool mUnique;

  IndexDataValue() : mIndexId(0), mUnique(false) {
    MOZ_COUNT_CTOR(IndexDataValue);
  }

  explicit IndexDataValue(const IndexDataValue& aOther)
      : mIndexId(aOther.mIndexId),
        mKey(aOther.mKey),
        mSortKey(aOther.mSortKey),
        mUnique(aOther.mUnique) {
    MOZ_ASSERT(!aOther.mKey.IsUnset());

    MOZ_COUNT_CTOR(IndexDataValue);
  }

  IndexDataValue(int64_t aIndexId, bool aUnique, const Key& aKey)
      : mIndexId(aIndexId), mKey(aKey), mUnique(aUnique) {
    MOZ_ASSERT(!aKey.IsUnset());

    MOZ_COUNT_CTOR(IndexDataValue);
  }

  IndexDataValue(int64_t aIndexId, bool aUnique, const Key& aKey,
                 const Key& aSortKey)
      : mIndexId(aIndexId), mKey(aKey), mSortKey(aSortKey), mUnique(aUnique) {
    MOZ_ASSERT(!aKey.IsUnset());

    MOZ_COUNT_CTOR(IndexDataValue);
  }

  ~IndexDataValue() { MOZ_COUNT_DTOR(IndexDataValue); }

  bool operator==(const IndexDataValue& aOther) const {
    if (mIndexId != aOther.mIndexId) {
      return false;
    }
    if (mSortKey.IsUnset()) {
      return mKey == aOther.mKey;
    }
    return mSortKey == aOther.mSortKey;
  }

  bool operator<(const IndexDataValue& aOther) const {
    if (mIndexId == aOther.mIndexId) {
      if (mSortKey.IsUnset()) {
        return mKey < aOther.mKey;
      }
      return mSortKey < aOther.mSortKey;
    }

    return mIndexId < aOther.mIndexId;
  }
};

/*******************************************************************************
 * SQLite functions
 ******************************************************************************/

int32_t MakeSchemaVersion(uint32_t aMajorSchemaVersion,
                          uint32_t aMinorSchemaVersion) {
  return int32_t((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}

// WARNING: the hash function used for the database name must not change.
// That's why this function exists separately from mozilla::HashString(), even
// though it is (at the time of writing) equivalent. See bug 780408 and bug
// 940315 for details.
uint32_t HashName(const nsAString& aName) {
  struct Helper {
    static uint32_t RotateBitsLeft32(uint32_t aValue, uint8_t aBits) {
      MOZ_ASSERT(aBits < 32);
      return (aValue << aBits) | (aValue >> (32 - aBits));
    }
  };

  static const uint32_t kGoldenRatioU32 = 0x9e3779b9u;

  const char16_t* str = aName.BeginReading();
  size_t length = aName.Length();

  uint32_t hash = 0;
  for (size_t i = 0; i < length; i++) {
    hash = kGoldenRatioU32 * (Helper::RotateBitsLeft32(hash, 5) ^ str[i]);
  }

  return hash;
}

nsresult ClampResultCode(nsresult aResultCode) {
  if (NS_SUCCEEDED(aResultCode) ||
      NS_ERROR_GET_MODULE(aResultCode) == NS_ERROR_MODULE_DOM_INDEXEDDB) {
    return aResultCode;
  }

  switch (aResultCode) {
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
      return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    case NS_ERROR_STORAGE_CONSTRAINT:
      return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
    default:
#ifdef DEBUG
      nsPrintfCString message("Converting non-IndexedDB error code (0x%" PRIX32
                              ") to "
                              "NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR",
                              static_cast<uint32_t>(aResultCode));
      NS_WARNING(message.get());
#else
        ;
#endif
  }

  IDB_REPORT_INTERNAL_ERR();
  return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

void GetDatabaseFilename(const nsAString& aName,
                         nsAutoString& aDatabaseFilename) {
  MOZ_ASSERT(aDatabaseFilename.IsEmpty());

  // WARNING: do not change this hash function. See the comment in HashName()
  // for details.
  aDatabaseFilename.AppendInt(HashName(aName));

  nsCString escapedName;
  if (!NS_Escape(NS_ConvertUTF16toUTF8(aName), escapedName, url_XPAlphas)) {
    MOZ_CRASH("Can't escape database name!");
  }

  const char* forwardIter = escapedName.BeginReading();
  const char* backwardIter = escapedName.EndReading() - 1;

  nsAutoCString substring;
  while (forwardIter <= backwardIter && substring.Length() < 21) {
    if (substring.Length() % 2) {
      substring.Append(*backwardIter--);
    } else {
      substring.Append(*forwardIter++);
    }
  }

  aDatabaseFilename.AppendASCII(substring.get(), substring.Length());
}

uint32_t CompressedByteCountForNumber(uint64_t aNumber) {
  // All bytes have 7 bits available.
  uint32_t count = 1;
  while ((aNumber >>= 7)) {
    count++;
  }

  return count;
}

uint32_t CompressedByteCountForIndexId(int64_t aIndexId) {
  MOZ_ASSERT(aIndexId);
  MOZ_ASSERT(UINT64_MAX - uint64_t(aIndexId) >= uint64_t(aIndexId),
             "Overflow!");

  return CompressedByteCountForNumber(uint64_t(aIndexId * 2));
}

void WriteCompressedNumber(uint64_t aNumber, uint8_t** aIterator) {
  MOZ_ASSERT(aIterator);
  MOZ_ASSERT(*aIterator);

  uint8_t*& buffer = *aIterator;

#ifdef DEBUG
  const uint8_t* bufferStart = buffer;
  const uint64_t originalNumber = aNumber;
#endif

  while (true) {
    uint64_t shiftedNumber = aNumber >> 7;
    if (shiftedNumber) {
      *buffer++ = uint8_t(0x80 | (aNumber & 0x7f));
      aNumber = shiftedNumber;
    } else {
      *buffer++ = uint8_t(aNumber);
      break;
    }
  }

  MOZ_ASSERT(buffer > bufferStart);
  MOZ_ASSERT(uint32_t(buffer - bufferStart) ==
             CompressedByteCountForNumber(originalNumber));
}

uint64_t ReadCompressedNumber(const uint8_t** aIterator, const uint8_t* aEnd) {
  MOZ_ASSERT(aIterator);
  MOZ_ASSERT(*aIterator);
  MOZ_ASSERT(aEnd);
  MOZ_ASSERT(*aIterator < aEnd);

  const uint8_t*& buffer = *aIterator;

  uint8_t shiftCounter = 0;
  uint64_t result = 0;

  while (true) {
    MOZ_ASSERT(shiftCounter <= 56, "Shifted too many bits!");

    result += (uint64_t(*buffer & 0x7f) << shiftCounter);
    shiftCounter += 7;

    if (!(*buffer++ & 0x80)) {
      break;
    }

    if (NS_WARN_IF(buffer == aEnd)) {
      MOZ_ASSERT(false);
      break;
    }
  }

  return result;
}

void WriteCompressedIndexId(int64_t aIndexId, bool aUnique,
                            uint8_t** aIterator) {
  MOZ_ASSERT(aIndexId);
  MOZ_ASSERT(UINT64_MAX - uint64_t(aIndexId) >= uint64_t(aIndexId),
             "Overflow!");
  MOZ_ASSERT(aIterator);
  MOZ_ASSERT(*aIterator);

  const uint64_t indexId = (uint64_t(aIndexId * 2) | (aUnique ? 1 : 0));
  WriteCompressedNumber(indexId, aIterator);
}

void ReadCompressedIndexId(const uint8_t** aIterator, const uint8_t* aEnd,
                           int64_t* aIndexId, bool* aUnique) {
  MOZ_ASSERT(aIterator);
  MOZ_ASSERT(*aIterator);
  MOZ_ASSERT(aIndexId);
  MOZ_ASSERT(aUnique);

  uint64_t indexId = ReadCompressedNumber(aIterator, aEnd);

  if (indexId % 2) {
    *aUnique = true;
    indexId--;
  } else {
    *aUnique = false;
  }

  MOZ_ASSERT(UINT64_MAX / 2 >= uint64_t(indexId), "Bad index id!");

  *aIndexId = int64_t(indexId / 2);
}

// static
nsresult MakeCompressedIndexDataValues(
    const FallibleTArray<IndexDataValue>& aIndexValues,
    UniqueFreePtr<uint8_t>& aCompressedIndexDataValues,
    uint32_t* aCompressedIndexDataValuesLength) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aCompressedIndexDataValues);
  MOZ_ASSERT(aCompressedIndexDataValuesLength);

  AUTO_PROFILER_LABEL("MakeCompressedIndexDataValues", DOM);

  const uint32_t arrayLength = aIndexValues.Length();
  if (!arrayLength) {
    *aCompressedIndexDataValuesLength = 0;
    return NS_OK;
  }

  // First calculate the size of the final buffer.
  uint32_t blobDataLength = 0;

  for (uint32_t arrayIndex = 0; arrayIndex < arrayLength; arrayIndex++) {
    const IndexDataValue& info = aIndexValues[arrayIndex];
    const nsCString& keyBuffer = info.mKey.GetBuffer();
    const nsCString& sortKeyBuffer = info.mSortKey.GetBuffer();
    const uint32_t keyBufferLength = keyBuffer.Length();
    const uint32_t sortKeyBufferLength = sortKeyBuffer.Length();

    MOZ_ASSERT(!keyBuffer.IsEmpty());

    const CheckedUint32 infoLength =
        CheckedUint32(CompressedByteCountForIndexId(info.mIndexId)) +
        CompressedByteCountForNumber(keyBufferLength) +
        CompressedByteCountForNumber(sortKeyBufferLength) + keyBufferLength +
        sortKeyBufferLength;
    // Don't let |infoLength| overflow.
    if (NS_WARN_IF(!infoLength.isValid())) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    // Don't let |blobDataLength| overflow.
    if (NS_WARN_IF(UINT32_MAX - infoLength.value() < blobDataLength)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    blobDataLength += infoLength.value();
  }

  UniqueFreePtr<uint8_t> blobData(
      static_cast<uint8_t*>(malloc(blobDataLength)));
  if (NS_WARN_IF(!blobData)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint8_t* blobDataIter = blobData.get();

  for (uint32_t arrayIndex = 0; arrayIndex < arrayLength; arrayIndex++) {
    const IndexDataValue& info = aIndexValues[arrayIndex];
    const nsCString& keyBuffer = info.mKey.GetBuffer();
    const nsCString& sortKeyBuffer = info.mSortKey.GetBuffer();
    const uint32_t keyBufferLength = keyBuffer.Length();
    const uint32_t sortKeyBufferLength = sortKeyBuffer.Length();

    WriteCompressedIndexId(info.mIndexId, info.mUnique, &blobDataIter);
    WriteCompressedNumber(keyBufferLength, &blobDataIter);

    memcpy(blobDataIter, keyBuffer.get(), keyBufferLength);
    blobDataIter += keyBufferLength;

    WriteCompressedNumber(sortKeyBufferLength, &blobDataIter);

    memcpy(blobDataIter, sortKeyBuffer.get(), sortKeyBufferLength);
    blobDataIter += sortKeyBufferLength;
  }

  MOZ_ASSERT(blobDataIter == blobData.get() + blobDataLength);

  aCompressedIndexDataValues.swap(blobData);
  *aCompressedIndexDataValuesLength = uint32_t(blobDataLength);

  return NS_OK;
}

nsresult ReadCompressedIndexDataValuesFromBlob(
    const uint8_t* aBlobData, uint32_t aBlobDataLength,
    nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aBlobData);
  MOZ_ASSERT(aBlobDataLength);
  MOZ_ASSERT(aIndexValues.IsEmpty());

  AUTO_PROFILER_LABEL("ReadCompressedIndexDataValuesFromBlob", DOM);

  if (uintptr_t(aBlobData) > UINTPTR_MAX - aBlobDataLength) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  const uint8_t* blobDataIter = aBlobData;
  const uint8_t* blobDataEnd = aBlobData + aBlobDataLength;

  while (blobDataIter < blobDataEnd) {
    int64_t indexId;
    bool unique;
    ReadCompressedIndexId(&blobDataIter, blobDataEnd, &indexId, &unique);

    if (NS_WARN_IF(blobDataIter == blobDataEnd)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    // Read key buffer length.
    const uint64_t keyBufferLength =
        ReadCompressedNumber(&blobDataIter, blobDataEnd);

    if (NS_WARN_IF(blobDataIter == blobDataEnd) ||
        NS_WARN_IF(keyBufferLength > uint64_t(UINT32_MAX)) ||
        NS_WARN_IF(keyBufferLength > uintptr_t(blobDataEnd)) ||
        NS_WARN_IF(blobDataIter > blobDataEnd - keyBufferLength)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    nsCString keyBuffer(reinterpret_cast<const char*>(blobDataIter),
                        uint32_t(keyBufferLength));
    blobDataIter += keyBufferLength;

    IndexDataValue idv(indexId, unique, Key(keyBuffer));

    // Read sort key buffer length.
    const uint64_t sortKeyBufferLength =
        ReadCompressedNumber(&blobDataIter, blobDataEnd);

    if (sortKeyBufferLength > 0) {
      if (NS_WARN_IF(blobDataIter == blobDataEnd) ||
          NS_WARN_IF(sortKeyBufferLength > uint64_t(UINT32_MAX)) ||
          NS_WARN_IF(sortKeyBufferLength > uintptr_t(blobDataEnd)) ||
          NS_WARN_IF(blobDataIter > blobDataEnd - sortKeyBufferLength)) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_FILE_CORRUPTED;
      }

      nsCString sortKeyBuffer(reinterpret_cast<const char*>(blobDataIter),
                              uint32_t(sortKeyBufferLength));
      blobDataIter += sortKeyBufferLength;

      idv.mSortKey = Key(sortKeyBuffer);
    }

    if (NS_WARN_IF(!aIndexValues.InsertElementSorted(idv, fallible))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  MOZ_ASSERT(blobDataIter == blobDataEnd);

  return NS_OK;
}

// static
template <typename T>
nsresult ReadCompressedIndexDataValuesFromSource(
    T* aSource, uint32_t aColumnIndex, nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aSource);
  MOZ_ASSERT(aIndexValues.IsEmpty());

  int32_t columnType;
  nsresult rv = aSource->GetTypeOfIndex(aColumnIndex, &columnType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (columnType == mozIStorageStatement::VALUE_TYPE_NULL) {
    return NS_OK;
  }

  MOZ_ASSERT(columnType == mozIStorageStatement::VALUE_TYPE_BLOB);

  const uint8_t* blobData;
  uint32_t blobDataLength;
  rv = aSource->GetSharedBlob(aColumnIndex, &blobDataLength, &blobData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!blobDataLength)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  rv = ReadCompressedIndexDataValuesFromBlob(blobData, blobDataLength,
                                             aIndexValues);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult ReadCompressedIndexDataValues(mozIStorageStatement* aStatement,
                                       uint32_t aColumnIndex,
                                       nsTArray<IndexDataValue>& aIndexValues) {
  return ReadCompressedIndexDataValuesFromSource(aStatement, aColumnIndex,
                                                 aIndexValues);
}

nsresult ReadCompressedIndexDataValues(mozIStorageValueArray* aValues,
                                       uint32_t aColumnIndex,
                                       nsTArray<IndexDataValue>& aIndexValues) {
  return ReadCompressedIndexDataValuesFromSource(aValues, aColumnIndex,
                                                 aIndexValues);
}

nsresult CreateFileTables(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("CreateFileTables", DOM);

  // Table `file`
  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE file ("
                         "id INTEGER PRIMARY KEY, "
                         "refcount INTEGER NOT NULL"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER object_data_insert_trigger "
                         "AFTER INSERT ON object_data "
                         "FOR EACH ROW "
                         "WHEN NEW.file_ids IS NOT NULL "
                         "BEGIN "
                         "SELECT update_refcount(NULL, NEW.file_ids); "
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "FOR EACH ROW "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
      "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER object_data_delete_trigger "
                         "AFTER DELETE ON object_data "
                         "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
                         "BEGIN "
                         "SELECT update_refcount(OLD.file_ids, NULL); "
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER file_update_trigger "
                         "AFTER UPDATE ON file "
                         "FOR EACH ROW WHEN NEW.refcount = 0 "
                         "BEGIN "
                         "DELETE FROM file WHERE id = OLD.id; "
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult CreateTables(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("CreateTables", DOM);

  // Table `database`

  // There are two reasons for having the origin column.
  // First, we can ensure that we don't have collisions in the origin hash we
  // use for the path because when we open the db we can make sure that the
  // origins exactly match. Second, chrome code crawling through the idb
  // directory can figure out the origin of every db without having to
  // reverse-engineer our hash scheme.
  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE database"
                         "( name TEXT PRIMARY KEY"
                         ", origin TEXT NOT NULL"
                         ", version INTEGER NOT NULL DEFAULT 0"
                         ", last_vacuum_time INTEGER NOT NULL DEFAULT 0"
                         ", last_analyze_time INTEGER NOT NULL DEFAULT 0"
                         ", last_vacuum_size INTEGER NOT NULL DEFAULT 0"
                         ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `object_store`
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE object_store"
                         "( id INTEGER PRIMARY KEY"
                         ", auto_increment INTEGER NOT NULL DEFAULT 0"
                         ", name TEXT NOT NULL"
                         ", key_path TEXT"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `object_store_index`
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE object_store_index"
                         "( id INTEGER PRIMARY KEY"
                         ", object_store_id INTEGER NOT NULL"
                         ", name TEXT NOT NULL"
                         ", key_path TEXT NOT NULL"
                         ", unique_index INTEGER NOT NULL"
                         ", multientry INTEGER NOT NULL"
                         ", locale TEXT"
                         ", is_auto_locale BOOLEAN NOT NULL"
                         ", FOREIGN KEY (object_store_id) "
                         "REFERENCES object_store(id) "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `object_data`
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE object_data"
                         "( object_store_id INTEGER NOT NULL"
                         ", key BLOB NOT NULL"
                         ", index_data_values BLOB DEFAULT NULL"
                         ", file_ids TEXT"
                         ", data BLOB NOT NULL"
                         ", PRIMARY KEY (object_store_id, key)"
                         ", FOREIGN KEY (object_store_id) "
                         "REFERENCES object_store(id) "
                         ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `index_data`
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE index_data"
                         "( index_id INTEGER NOT NULL"
                         ", value BLOB NOT NULL"
                         ", object_data_key BLOB NOT NULL"
                         ", object_store_id INTEGER NOT NULL"
                         ", value_locale BLOB"
                         ", PRIMARY KEY (index_id, value, object_data_key)"
                         ", FOREIGN KEY (index_id) "
                         "REFERENCES object_store_index(id) "
                         ", FOREIGN KEY (object_store_id, object_data_key) "
                         "REFERENCES object_data(object_store_id, key) "
                         ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX index_data_value_locale_index "
      "ON index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `unique_index_data`
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE unique_index_data"
                         "( index_id INTEGER NOT NULL"
                         ", value BLOB NOT NULL"
                         ", object_store_id INTEGER NOT NULL"
                         ", object_data_key BLOB NOT NULL"
                         ", value_locale BLOB"
                         ", PRIMARY KEY (index_id, value)"
                         ", FOREIGN KEY (index_id) "
                         "REFERENCES object_store_index(id) "
                         ", FOREIGN KEY (object_store_id, object_data_key) "
                         "REFERENCES object_data(object_store_id, key) "
                         ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX unique_index_data_value_locale_index "
      "ON unique_index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateFileTables(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(kSQLiteSchemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom4To5(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom4To5", DOM);

  nsresult rv;

  // All we changed is the type of the version column, so lets try to
  // convert that to an integer, and if we fail, set it to 0.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT name, version, dataVersion "
                         "FROM database"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString name;
  int32_t intVersion;
  int64_t dataVersion;

  {
    mozStorageStatementScoper scoper(stmt);

    bool hasResults;
    rv = stmt->ExecuteStep(&hasResults);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (NS_WARN_IF(!hasResults)) {
      return NS_ERROR_FAILURE;
    }

    nsString version;
    rv = stmt->GetString(1, version);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    intVersion = version.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      intVersion = 0;
    }

    rv = stmt->GetString(0, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->GetInt64(2, &dataVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DROP TABLE database"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE database ("
                         "name TEXT NOT NULL, "
                         "version INTEGER NOT NULL DEFAULT 0, "
                         "dataVersion INTEGER NOT NULL"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("INSERT INTO database (name, version, dataVersion) "
                         "VALUES (:name, :version, :dataVersion)"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  {
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindStringByIndex(0, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt32ByIndex(1, intVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByIndex(2, dataVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aConnection->SetSchemaVersion(5);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom5To6(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom5To6", DOM);

  // First, drop all the indexes we're no longer going to use.
  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP INDEX key_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP INDEX ai_key_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP INDEX value_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP INDEX ai_value_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now, reorder the columns of object_data to put the blob data last. We do
  // this by copying into a temporary table, dropping the original, then copying
  // back into a newly created table.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "id INTEGER PRIMARY KEY, "
                         "object_store_id, "
                         "key_value, "
                         "data "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO temp_upgrade "
                         "SELECT id, object_store_id, key_value, data "
                         "FROM object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE object_data ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "key_value DEFAULT NULL, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, key_value), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO object_data "
                         "SELECT id, object_store_id, key_value, data "
                         "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We need to add a unique constraint to our ai_object_data table. Copy all
  // the data out of it using a temporary table as before.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "id INTEGER PRIMARY KEY, "
                         "object_store_id, "
                         "data "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO temp_upgrade "
                         "SELECT id, object_store_id, data "
                         "FROM ai_object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE ai_object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE ai_object_data ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "object_store_id INTEGER NOT NULL, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO ai_object_data "
                         "SELECT id, object_store_id, data "
                         "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "index_id, "
                         "value, "
                         "object_data_key, "
                         "object_data_id "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "object_data_key NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
      "CASCADE, "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT OR IGNORE INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX index_data_object_data_id_index "
                         "ON index_data (object_data_id);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the unique_index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "index_id, "
                         "value, "
                         "object_data_key, "
                         "object_data_id "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE unique_index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "object_data_key NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "UNIQUE (index_id, value), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
      "CASCADE "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX unique_index_data_object_data_id_index "
                         "ON unique_index_data (object_data_id);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the ai_index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "index_id, "
                         "value, "
                         "ai_object_data_id "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO temp_upgrade "
                         "SELECT index_id, value, ai_object_data_id "
                         "FROM ai_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE ai_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE ai_index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "ai_object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, ai_object_data_id), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
      "CASCADE, "
      "FOREIGN KEY (ai_object_data_id) REFERENCES ai_object_data(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT OR IGNORE INTO ai_index_data "
                         "SELECT index_id, value, ai_object_data_id "
                         "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX ai_index_data_ai_object_data_id_index "
                         "ON ai_index_data (ai_object_data_id);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the ai_unique_index_data table. We're reordering the columns as well
  // as changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "index_id, "
                         "value, "
                         "ai_object_data_id "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO temp_upgrade "
                         "SELECT index_id, value, ai_object_data_id "
                         "FROM ai_unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE ai_unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE ai_unique_index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "ai_object_data_id INTEGER NOT NULL, "
      "UNIQUE (index_id, value), "
      "PRIMARY KEY (index_id, value, ai_object_data_id), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
      "CASCADE, "
      "FOREIGN KEY (ai_object_data_id) REFERENCES ai_object_data(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO ai_unique_index_data "
                         "SELECT index_id, value, ai_object_data_id "
                         "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX ai_unique_index_data_ai_object_data_id_index "
      "ON ai_unique_index_data (ai_object_data_id);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(6);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom6To7(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom6To7", DOM);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "id, "
                         "name, "
                         "key_path, "
                         "auto_increment"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO temp_upgrade "
                         "SELECT id, name, key_path, auto_increment "
                         "FROM object_store;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE object_store;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE object_store ("
                         "id INTEGER PRIMARY KEY, "
                         "auto_increment INTEGER NOT NULL DEFAULT 0, "
                         "name TEXT NOT NULL, "
                         "key_path TEXT, "
                         "UNIQUE (name)"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO object_store "
      "SELECT id, auto_increment, name, nullif(key_path, '') "
      "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(7);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom7To8(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom7To8", DOM);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "id, "
                         "object_store_id, "
                         "name, "
                         "key_path, "
                         "unique_index, "
                         "object_store_autoincrement"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO temp_upgrade "
                         "SELECT id, object_store_id, name, key_path, "
                         "unique_index, object_store_autoincrement "
                         "FROM object_store_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE object_store_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE object_store_index ("
      "id INTEGER, "
      "object_store_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "key_path TEXT NOT NULL, "
      "unique_index INTEGER NOT NULL, "
      "multientry INTEGER NOT NULL, "
      "object_store_autoincrement INTERGER NOT NULL, "
      "PRIMARY KEY (id), "
      "UNIQUE (object_store_id, name), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO object_store_index "
                         "SELECT id, object_store_id, name, key_path, "
                         "unique_index, 0, object_store_autoincrement "
                         "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(8);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class CompressDataBlobsFunction final : public mozIStorageFunction {
 public:
  NS_DECL_ISUPPORTS

 private:
  ~CompressDataBlobsFunction() = default;

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    AUTO_PROFILER_LABEL("CompressDataBlobsFunction::OnFunctionCall", DOM);

    uint32_t argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    int32_t type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (type != mozIStorageStatement::VALUE_TYPE_BLOB) {
      NS_WARNING("Don't call me with the wrong type of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    const uint8_t* uncompressed;
    uint32_t uncompressedLength;
    rv = aArguments->GetSharedBlob(0, &uncompressedLength, &uncompressed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);
    UniqueFreePtr<uint8_t> compressed(
        static_cast<uint8_t*>(malloc(compressedLength)));
    if (NS_WARN_IF(!compressed)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    snappy::RawCompress(
        reinterpret_cast<const char*>(uncompressed), uncompressedLength,
        reinterpret_cast<char*>(compressed.get()), &compressedLength);

    std::pair<uint8_t*, int> data(compressed.release(), int(compressedLength));

    nsCOMPtr<nsIVariant> result =
        new mozilla::storage::AdoptedBlobVariant(data);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult UpgradeSchemaFrom8To9_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom8To9_0", DOM);

  // We no longer use the dataVersion column.
  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("UPDATE database SET dataVersion = 0;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageFunction> compressor = new CompressDataBlobsFunction();

  NS_NAMED_LITERAL_CSTRING(compressorName, "compress");

  rv = aConnection->CreateFunction(compressorName, 1, compressor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Turn off foreign key constraints before we do anything here.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("UPDATE object_data SET data = compress(data);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("UPDATE ai_object_data SET data = compress(data);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->RemoveFunction(compressorName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(9, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom9_0To10_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom9_0To10_0", DOM);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE object_data ADD COLUMN file_ids TEXT;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE ai_object_data ADD COLUMN file_ids TEXT;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateFileTables(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(10, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom10_0To11_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom10_0To11_0", DOM);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "id, "
                         "object_store_id, "
                         "name, "
                         "key_path, "
                         "unique_index, "
                         "multientry"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO temp_upgrade "
                         "SELECT id, object_store_id, name, key_path, "
                         "unique_index, multientry "
                         "FROM object_store_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE object_store_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE object_store_index ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "key_path TEXT NOT NULL, "
      "unique_index INTEGER NOT NULL, "
      "multientry INTEGER NOT NULL, "
      "UNIQUE (object_store_id, name), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO object_store_index "
                         "SELECT id, object_store_id, name, key_path, "
                         "unique_index, multientry "
                         "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TRIGGER object_data_insert_trigger;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO object_data (object_store_id, key_value, data, file_ids) "
      "SELECT object_store_id, id, data, file_ids "
      "FROM ai_object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER object_data_insert_trigger "
                         "AFTER INSERT ON object_data "
                         "FOR EACH ROW "
                         "WHEN NEW.file_ids IS NOT NULL "
                         "BEGIN "
                         "SELECT update_refcount(NULL, NEW.file_ids); "
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO index_data (index_id, value, object_data_key, "
      "object_data_id) "
      "SELECT ai_index_data.index_id, ai_index_data.value, "
      "ai_index_data.ai_object_data_id, object_data.id "
      "FROM ai_index_data "
      "INNER JOIN object_store_index ON "
      "object_store_index.id = ai_index_data.index_id "
      "INNER JOIN object_data ON "
      "object_data.object_store_id = object_store_index.object_store_id AND "
      "object_data.key_value = ai_index_data.ai_object_data_id;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO unique_index_data (index_id, value, object_data_key, "
      "object_data_id) "
      "SELECT ai_unique_index_data.index_id, ai_unique_index_data.value, "
      "ai_unique_index_data.ai_object_data_id, object_data.id "
      "FROM ai_unique_index_data "
      "INNER JOIN object_store_index ON "
      "object_store_index.id = ai_unique_index_data.index_id "
      "INNER JOIN object_data ON "
      "object_data.object_store_id = object_store_index.object_store_id AND "
      "object_data.key_value = ai_unique_index_data.ai_object_data_id;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "UPDATE object_store "
      "SET auto_increment = (SELECT max(id) FROM ai_object_data) + 1 "
      "WHERE auto_increment;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE ai_unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE ai_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE ai_object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(11, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class EncodeKeysFunction final : public mozIStorageFunction {
 public:
  NS_DECL_ISUPPORTS

 private:
  ~EncodeKeysFunction() = default;

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    AUTO_PROFILER_LABEL("EncodeKeysFunction::OnFunctionCall", DOM);

    uint32_t argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    int32_t type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    Key key;
    if (type == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      int64_t intKey;
      aArguments->GetInt64(0, &intKey);
      key.SetFromInteger(intKey);
    } else if (type == mozIStorageStatement::VALUE_TYPE_TEXT) {
      nsString stringKey;
      aArguments->GetString(0, stringKey);
      key.SetFromString(stringKey);
    } else {
      NS_WARNING("Don't call me with the wrong type of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    const nsCString& buffer = key.GetBuffer();

    std::pair<const void*, int> data(static_cast<const void*>(buffer.get()),
                                     int(buffer.Length()));

    nsCOMPtr<nsIVariant> result = new mozilla::storage::BlobVariant(data);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult UpgradeSchemaFrom11_0To12_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom11_0To12_0", DOM);

  NS_NAMED_LITERAL_CSTRING(encoderName, "encode");

  nsCOMPtr<mozIStorageFunction> encoder = new EncodeKeysFunction();

  nsresult rv = aConnection->CreateFunction(encoderName, 1, encoder);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "id INTEGER PRIMARY KEY, "
                         "object_store_id, "
                         "key_value, "
                         "data, "
                         "file_ids "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, encode(key_value), data, file_ids "
      "FROM object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE object_data ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "key_value BLOB DEFAULT NULL, "
      "file_ids TEXT, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, key_value), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO object_data "
      "SELECT id, object_store_id, key_value, file_ids, data "
      "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER object_data_insert_trigger "
                         "AFTER INSERT ON object_data "
                         "FOR EACH ROW "
                         "WHEN NEW.file_ids IS NOT NULL "
                         "BEGIN "
                         "SELECT update_refcount(NULL, NEW.file_ids); "
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "FOR EACH ROW "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
      "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER object_data_delete_trigger "
                         "AFTER DELETE ON object_data "
                         "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
                         "BEGIN "
                         "SELECT update_refcount(OLD.file_ids, NULL); "
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "index_id, "
                         "value, "
                         "object_data_key, "
                         "object_data_id "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE index_data ("
      "index_id INTEGER NOT NULL, "
      "value BLOB NOT NULL, "
      "object_data_key BLOB NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
      "CASCADE, "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX index_data_object_data_id_index "
                         "ON index_data (object_data_id);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TEMPORARY TABLE temp_upgrade ("
                         "index_id, "
                         "value, "
                         "object_data_key, "
                         "object_data_id "
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE unique_index_data ("
      "index_id INTEGER NOT NULL, "
      "value BLOB NOT NULL, "
      "object_data_key BLOB NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "UNIQUE (index_id, value), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
      "CASCADE "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
      "CASCADE"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE temp_upgrade;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX unique_index_data_object_data_id_index "
                         "ON unique_index_data (object_data_id);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->RemoveFunction(encoderName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(12, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom12_0To13_0(mozIStorageConnection* aConnection,
                                     bool* aVacuumNeeded) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom12_0To13_0", DOM);

  nsresult rv;

#ifdef IDB_MOBILE
  int32_t defaultPageSize;
  rv = aConnection->GetDefaultPageSize(&defaultPageSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Enable auto_vacuum mode and update the page size to the platform default.
  nsAutoCString upgradeQuery("PRAGMA auto_vacuum = FULL; PRAGMA page_size = ");
  upgradeQuery.AppendInt(defaultPageSize);

  rv = aConnection->ExecuteSimpleSQL(upgradeQuery);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aVacuumNeeded = true;
#endif

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(13, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom13_0To14_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The only change between 13 and 14 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(14, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom14_0To15_0(mozIStorageConnection* aConnection) {
  // The only change between 14 and 15 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(15, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom15_0To16_0(mozIStorageConnection* aConnection) {
  // The only change between 15 and 16 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(16, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom16_0To17_0(mozIStorageConnection* aConnection) {
  // The only change between 16 and 17 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(17, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class UpgradeSchemaFrom17_0To18_0Helper final {
  class InsertIndexDataValuesFunction;
  class UpgradeKeyFunction;

 public:
  static nsresult DoUpgrade(mozIStorageConnection* aConnection,
                            const nsACString& aOrigin);

 private:
  static nsresult DoUpgradeInternal(mozIStorageConnection* aConnection,
                                    const nsACString& aOrigin);

  UpgradeSchemaFrom17_0To18_0Helper() = delete;
  ~UpgradeSchemaFrom17_0To18_0Helper() = delete;
};

class UpgradeSchemaFrom17_0To18_0Helper::InsertIndexDataValuesFunction final
    : public mozIStorageFunction {
 public:
  InsertIndexDataValuesFunction() {}

  NS_DECL_ISUPPORTS

 private:
  ~InsertIndexDataValuesFunction() = default;

  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(
    UpgradeSchemaFrom17_0To18_0Helper::InsertIndexDataValuesFunction,
    mozIStorageFunction);

NS_IMETHODIMP
UpgradeSchemaFrom17_0To18_0Helper::InsertIndexDataValuesFunction::
    OnFunctionCall(mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 4);

    int32_t valueType;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(0, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(1, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_INTEGER);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(2, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_INTEGER);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(3, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);
  }
#endif

  // Read out the previous value. It may be NULL, in which case we'll just end
  // up with an empty array.
  AutoTArray<IndexDataValue, 32> indexValues;
  nsresult rv = ReadCompressedIndexDataValues(aValues, 0, indexValues);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t indexId;
  rv = aValues->GetInt64(1, &indexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t unique;
  rv = aValues->GetInt32(2, &unique);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Key value;
  rv = value.SetFromValueArray(aValues, 3);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the array with the new addition.
  if (NS_WARN_IF(
          !indexValues.SetCapacity(indexValues.Length() + 1, fallible))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ALWAYS_TRUE(indexValues.InsertElementSorted(
      IndexDataValue(indexId, !!unique, value), fallible));

  // Compress the array.
  UniqueFreePtr<uint8_t> indexValuesBlob;
  uint32_t indexValuesBlobLength;
  rv = MakeCompressedIndexDataValues(indexValues, indexValuesBlob,
                                     &indexValuesBlobLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The compressed blob is the result of this function.
  std::pair<uint8_t*, int> indexValuesBlobPair(indexValuesBlob.release(),
                                               indexValuesBlobLength);

  nsCOMPtr<nsIVariant> result =
      new storage::AdoptedBlobVariant(indexValuesBlobPair);

  result.forget(_retval);
  return NS_OK;
}

class UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction final
    : public mozIStorageFunction {
 public:
  UpgradeKeyFunction() {}

  static nsresult CopyAndUpgradeKeyBuffer(const uint8_t* aSource,
                                          const uint8_t* aSourceEnd,
                                          uint8_t* aDestination) {
    return CopyAndUpgradeKeyBufferInternal(aSource, aSourceEnd, aDestination,
                                           0 /* aTagOffset */,
                                           0 /* aRecursionDepth */);
  }

  NS_DECL_ISUPPORTS

 private:
  ~UpgradeKeyFunction() = default;

  static nsresult CopyAndUpgradeKeyBufferInternal(const uint8_t*& aSource,
                                                  const uint8_t* aSourceEnd,
                                                  uint8_t*& aDestination,
                                                  uint8_t aTagOffset,
                                                  uint8_t aRecursionDepth);

  static uint32_t AdjustedSize(uint32_t aMaxSize, const uint8_t* aSource,
                               const uint8_t* aSourceEnd) {
    MOZ_ASSERT(aMaxSize);
    MOZ_ASSERT(aSource);
    MOZ_ASSERT(aSourceEnd);
    MOZ_ASSERT(aSource <= aSourceEnd);

    return std::min(aMaxSize, uint32_t(aSourceEnd - aSource));
  }

  NS_DECL_MOZISTORAGEFUNCTION
};

// static
nsresult UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction::
    CopyAndUpgradeKeyBufferInternal(const uint8_t*& aSource,
                                    const uint8_t* aSourceEnd,
                                    uint8_t*& aDestination, uint8_t aTagOffset,
                                    uint8_t aRecursionDepth) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aSource);
  MOZ_ASSERT(*aSource);
  MOZ_ASSERT(aSourceEnd);
  MOZ_ASSERT(aSource < aSourceEnd);
  MOZ_ASSERT(aDestination);
  MOZ_ASSERT(aTagOffset <= Key::kMaxArrayCollapse);

  static constexpr uint8_t kOldNumberTag = 0x1;
  static constexpr uint8_t kOldDateTag = 0x2;
  static constexpr uint8_t kOldStringTag = 0x3;
  static constexpr uint8_t kOldArrayTag = 0x4;
  static constexpr uint8_t kOldMaxType = kOldArrayTag;

  if (NS_WARN_IF(aRecursionDepth > Key::kMaxRecursionDepth)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  const uint8_t sourceTag = *aSource - (aTagOffset * kOldMaxType);
  MOZ_ASSERT(sourceTag);

  if (NS_WARN_IF(sourceTag > kOldMaxType * Key::kMaxArrayCollapse)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  if (sourceTag == kOldNumberTag || sourceTag == kOldDateTag) {
    // Write the new tag.
    *aDestination++ = (sourceTag == kOldNumberTag ? Key::eFloat : Key::eDate) +
                      (aTagOffset * Key::eMaxType);
    aSource++;

    // Numbers and Dates are encoded as 64-bit integers, but trailing 0
    // bytes have been removed.
    const uint32_t byteCount =
        AdjustedSize(sizeof(uint64_t), aSource, aSourceEnd);

    for (uint32_t count = 0; count < byteCount; count++) {
      *aDestination++ = *aSource++;
    }

    return NS_OK;
  }

  if (sourceTag == kOldStringTag) {
    // Write the new tag.
    *aDestination++ = Key::eString + (aTagOffset * Key::eMaxType);
    aSource++;

    while (aSource < aSourceEnd) {
      const uint8_t byte = *aSource++;
      *aDestination++ = byte;

      if (!byte) {
        // Just copied the terminator.
        break;
      }

      // Maybe copy one or two extra bytes if the byte is tagged and we have
      // enough source space.
      if (byte & 0x80) {
        const uint32_t byteCount =
            AdjustedSize((byte & 0x40) ? 2 : 1, aSource, aSourceEnd);

        for (uint32_t count = 0; count < byteCount; count++) {
          *aDestination++ = *aSource++;
        }
      }
    }

    return NS_OK;
  }

  if (NS_WARN_IF(sourceTag < kOldArrayTag)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  aTagOffset++;

  if (aTagOffset == Key::kMaxArrayCollapse) {
    MOZ_ASSERT(sourceTag == kOldArrayTag);

    *aDestination++ = (aTagOffset * Key::eMaxType);
    aSource++;

    aTagOffset = 0;
  }

  while (aSource < aSourceEnd &&
         (*aSource - (aTagOffset * kOldMaxType)) != Key::eTerminator) {
    nsresult rv = CopyAndUpgradeKeyBufferInternal(
        aSource, aSourceEnd, aDestination, aTagOffset, aRecursionDepth + 1);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aTagOffset = 0;
  }

  if (aSource < aSourceEnd) {
    MOZ_ASSERT((*aSource - (aTagOffset * kOldMaxType)) == Key::eTerminator);
    *aDestination++ = Key::eTerminator + (aTagOffset * Key::eMaxType);
    aSource++;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction,
                  mozIStorageFunction);

NS_IMETHODIMP
UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction::OnFunctionCall(
    mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 1);

    int32_t valueType;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(0, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);
  }
#endif

  // Dig the old key out of the values.
  const uint8_t* blobData;
  uint32_t blobDataLength;
  nsresult rv = aValues->GetSharedBlob(0, &blobDataLength, &blobData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Upgrading the key doesn't change the amount of space needed to hold it.
  UniqueFreePtr<uint8_t> upgradedBlobData(
      static_cast<uint8_t*>(malloc(blobDataLength)));
  if (NS_WARN_IF(!upgradedBlobData)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = CopyAndUpgradeKeyBuffer(blobData, blobData + blobDataLength,
                               upgradedBlobData.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The upgraded key is the result of this function.
  std::pair<uint8_t*, int> data(upgradedBlobData.release(),
                                int(blobDataLength));

  nsCOMPtr<nsIVariant> result = new mozilla::storage::AdoptedBlobVariant(data);

  result.forget(_retval);
  return NS_OK;
}

// static
nsresult UpgradeSchemaFrom17_0To18_0Helper::DoUpgrade(
    mozIStorageConnection* aConnection, const nsACString& aOrigin) {
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!aOrigin.IsEmpty());

  // Register the |upgrade_key| function.
  RefPtr<UpgradeKeyFunction> updateFunction = new UpgradeKeyFunction();

  NS_NAMED_LITERAL_CSTRING(upgradeKeyFunctionName, "upgrade_key");

  nsresult rv =
      aConnection->CreateFunction(upgradeKeyFunctionName, 1, updateFunction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Register the |insert_idv| function.
  RefPtr<InsertIndexDataValuesFunction> insertIDVFunction =
      new InsertIndexDataValuesFunction();

  NS_NAMED_LITERAL_CSTRING(insertIDVFunctionName, "insert_idv");

  rv = aConnection->CreateFunction(insertIDVFunctionName, 4, insertIDVFunction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ALWAYS_SUCCEEDS(aConnection->RemoveFunction(upgradeKeyFunctionName));
    return rv;
  }

  rv = DoUpgradeInternal(aConnection, aOrigin);

  MOZ_ALWAYS_SUCCEEDS(aConnection->RemoveFunction(upgradeKeyFunctionName));
  MOZ_ALWAYS_SUCCEEDS(aConnection->RemoveFunction(insertIDVFunctionName));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
nsresult UpgradeSchemaFrom17_0To18_0Helper::DoUpgradeInternal(
    mozIStorageConnection* aConnection, const nsACString& aOrigin) {
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!aOrigin.IsEmpty());

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Drop these triggers to avoid unnecessary work during the upgrade
      // process.
      "DROP TRIGGER object_data_insert_trigger;"
      "DROP TRIGGER object_data_update_trigger;"
      "DROP TRIGGER object_data_delete_trigger;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Drop these indexes before we do anything else to free disk space.
      "DROP INDEX index_data_object_data_id_index;"
      "DROP INDEX unique_index_data_object_data_id_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Create the new tables and triggers first.

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // This will eventually become the |database| table.
      "CREATE TABLE database_upgrade "
      "( name TEXT PRIMARY KEY"
      ", origin TEXT NOT NULL"
      ", version INTEGER NOT NULL DEFAULT 0"
      ", last_vacuum_time INTEGER NOT NULL DEFAULT 0"
      ", last_analyze_time INTEGER NOT NULL DEFAULT 0"
      ", last_vacuum_size INTEGER NOT NULL DEFAULT 0"
      ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // This will eventually become the |object_store| table.
      "CREATE TABLE object_store_upgrade"
      "( id INTEGER PRIMARY KEY"
      ", auto_increment INTEGER NOT NULL DEFAULT 0"
      ", name TEXT NOT NULL"
      ", key_path TEXT"
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // This will eventually become the |object_store_index| table.
      "CREATE TABLE object_store_index_upgrade"
      "( id INTEGER PRIMARY KEY"
      ", object_store_id INTEGER NOT NULL"
      ", name TEXT NOT NULL"
      ", key_path TEXT NOT NULL"
      ", unique_index INTEGER NOT NULL"
      ", multientry INTEGER NOT NULL"
      ", FOREIGN KEY (object_store_id) "
      "REFERENCES object_store(id) "
      ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // This will eventually become the |object_data| table.
      "CREATE TABLE object_data_upgrade"
      "( object_store_id INTEGER NOT NULL"
      ", key BLOB NOT NULL"
      ", index_data_values BLOB DEFAULT NULL"
      ", file_ids TEXT"
      ", data BLOB NOT NULL"
      ", PRIMARY KEY (object_store_id, key)"
      ", FOREIGN KEY (object_store_id) "
      "REFERENCES object_store(id) "
      ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // This will eventually become the |index_data| table.
      "CREATE TABLE index_data_upgrade"
      "( index_id INTEGER NOT NULL"
      ", value BLOB NOT NULL"
      ", object_data_key BLOB NOT NULL"
      ", object_store_id INTEGER NOT NULL"
      ", PRIMARY KEY (index_id, value, object_data_key)"
      ", FOREIGN KEY (index_id) "
      "REFERENCES object_store_index(id) "
      ", FOREIGN KEY (object_store_id, object_data_key) "
      "REFERENCES object_data(object_store_id, key) "
      ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // This will eventually become the |unique_index_data| table.
      "CREATE TABLE unique_index_data_upgrade"
      "( index_id INTEGER NOT NULL"
      ", value BLOB NOT NULL"
      ", object_store_id INTEGER NOT NULL"
      ", object_data_key BLOB NOT NULL"
      ", PRIMARY KEY (index_id, value)"
      ", FOREIGN KEY (index_id) "
      "REFERENCES object_store_index(id) "
      ", FOREIGN KEY (object_store_id, object_data_key) "
      "REFERENCES object_data(object_store_id, key) "
      ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Temporarily store |index_data_values| that we build during the upgrade
      // of the index tables. We will later move this to the |object_data|
      // table.
      "CREATE TEMPORARY TABLE temp_index_data_values "
      "( object_store_id INTEGER NOT NULL"
      ", key BLOB NOT NULL"
      ", index_data_values BLOB DEFAULT NULL"
      ", PRIMARY KEY (object_store_id, key)"
      ") WITHOUT ROWID;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // These two triggers help build the |index_data_values| blobs. The nested
      // SELECT statements help us achieve an "INSERT OR UPDATE"-like behavior.
      "CREATE TEMPORARY TRIGGER unique_index_data_upgrade_insert_trigger "
      "AFTER INSERT ON unique_index_data_upgrade "
      "BEGIN "
      "INSERT OR REPLACE INTO temp_index_data_values "
      "VALUES "
      "( NEW.object_store_id"
      ", NEW.object_data_key"
      ", insert_idv("
      "( SELECT index_data_values "
      "FROM temp_index_data_values "
      "WHERE object_store_id = NEW.object_store_id "
      "AND key = NEW.object_data_key "
      "), NEW.index_id"
      ", 1" /* unique */
      ", NEW.value"
      ")"
      ");"
      "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TEMPORARY TRIGGER index_data_upgrade_insert_trigger "
      "AFTER INSERT ON index_data_upgrade "
      "BEGIN "
      "INSERT OR REPLACE INTO temp_index_data_values "
      "VALUES "
      "( NEW.object_store_id"
      ", NEW.object_data_key"
      ", insert_idv("
      "("
      "SELECT index_data_values "
      "FROM temp_index_data_values "
      "WHERE object_store_id = NEW.object_store_id "
      "AND key = NEW.object_data_key "
      "), NEW.index_id"
      ", 0" /* not unique */
      ", NEW.value"
      ")"
      ");"
      "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |unique_index_data| table to change the column order, remove the
  // ON DELETE CASCADE clauses, and to apply the WITHOUT ROWID optimization.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Insert all the data.
      "INSERT INTO unique_index_data_upgrade "
      "SELECT "
      "unique_index_data.index_id, "
      "upgrade_key(unique_index_data.value), "
      "object_data.object_store_id, "
      "upgrade_key(unique_index_data.object_data_key) "
      "FROM unique_index_data "
      "JOIN object_data "
      "ON unique_index_data.object_data_id = object_data.id;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // The trigger is no longer needed.
      "DROP TRIGGER unique_index_data_upgrade_insert_trigger;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // The old table is no longer needed.
      "DROP TABLE unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Rename the table.
      "ALTER TABLE unique_index_data_upgrade "
      "RENAME TO unique_index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |index_data| table to change the column order, remove the ON
  // DELETE CASCADE clauses, and to apply the WITHOUT ROWID optimization.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Insert all the data.
      "INSERT INTO index_data_upgrade "
      "SELECT "
      "index_data.index_id, "
      "upgrade_key(index_data.value), "
      "upgrade_key(index_data.object_data_key), "
      "object_data.object_store_id "
      "FROM index_data "
      "JOIN object_data "
      "ON index_data.object_data_id = object_data.id;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // The trigger is no longer needed.
      "DROP TRIGGER index_data_upgrade_insert_trigger;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // The old table is no longer needed.
      "DROP TABLE index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Rename the table.
      "ALTER TABLE index_data_upgrade "
      "RENAME TO index_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |object_data| table to add the |index_data_values| column,
  // remove the ON DELETE CASCADE clause, and apply the WITHOUT ROWID
  // optimization.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Insert all the data.
      "INSERT INTO object_data_upgrade "
      "SELECT "
      "object_data.object_store_id, "
      "upgrade_key(object_data.key_value), "
      "temp_index_data_values.index_data_values, "
      "object_data.file_ids, "
      "object_data.data "
      "FROM object_data "
      "LEFT JOIN temp_index_data_values "
      "ON object_data.object_store_id = "
      "temp_index_data_values.object_store_id "
      "AND upgrade_key(object_data.key_value) = "
      "temp_index_data_values.key;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // The temporary table is no longer needed.
      "DROP TABLE temp_index_data_values;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // The old table is no longer needed.
      "DROP TABLE object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Rename the table.
      "ALTER TABLE object_data_upgrade "
      "RENAME TO object_data;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |object_store_index| table to remove the UNIQUE constraint and
  // the ON DELETE CASCADE clause.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO object_store_index_upgrade "
                         "SELECT * "
                         "FROM object_store_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE object_store_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE object_store_index_upgrade "
                         "RENAME TO object_store_index;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |object_store| table to remove the UNIQUE constraint.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT INTO object_store_upgrade "
                         "SELECT * "
                         "FROM object_store;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE object_store;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE object_store_upgrade "
                         "RENAME TO object_store;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |database| table to include the origin, vacuum information, and
  // apply the WITHOUT ROWID optimization.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("INSERT INTO database_upgrade "
                         "SELECT name, :origin, version, 0, 0, 0 "
                         "FROM database;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"), aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv =
      aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DROP TABLE database;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE database_upgrade "
                         "RENAME TO database;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  {
    // Make sure there's only one entry in the |database| table.
    nsCOMPtr<mozIStorageStatement> stmt;
    MOZ_ASSERT(NS_SUCCEEDED(
        aConnection->CreateStatement(NS_LITERAL_CSTRING("SELECT COUNT(*) "
                                                        "FROM database;"),
                                     getter_AddRefs(stmt))));

    bool hasResult;
    MOZ_ASSERT(NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)));

    int64_t count;
    MOZ_ASSERT(NS_SUCCEEDED(stmt->GetInt64(0, &count)));

    MOZ_ASSERT(count == 1);
  }
#endif

  // Recreate file table triggers.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER object_data_insert_trigger "
                         "AFTER INSERT ON object_data "
                         "WHEN NEW.file_ids IS NOT NULL "
                         "BEGIN "
                         "SELECT update_refcount(NULL, NEW.file_ids);"
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids);"
      "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TRIGGER object_data_delete_trigger "
                         "AFTER DELETE ON object_data "
                         "WHEN OLD.file_ids IS NOT NULL "
                         "BEGIN "
                         "SELECT update_refcount(OLD.file_ids, NULL);"
                         "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Finally, turn on auto_vacuum mode. We use full auto_vacuum mode to reclaim
  // disk space on mobile devices (at the cost of some COMMIT speed), and
  // incremental auto_vacuum mode on desktop builds.
  rv = aConnection->ExecuteSimpleSQL(
#ifdef IDB_MOBILE
      NS_LITERAL_CSTRING("PRAGMA auto_vacuum = FULL;")
#else
      NS_LITERAL_CSTRING("PRAGMA auto_vacuum = INCREMENTAL;")
#endif
  );
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(18, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom17_0To18_0(mozIStorageConnection* aConnection,
                                     const nsACString& aOrigin) {
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!aOrigin.IsEmpty());

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom17_0To18_0", DOM);

  return UpgradeSchemaFrom17_0To18_0Helper::DoUpgrade(aConnection, aOrigin);
}

nsresult UpgradeSchemaFrom18_0To19_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv;
  AUTO_PROFILER_LABEL("UpgradeSchemaFrom18_0To19_0", DOM);

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE object_store_index "
                         "ADD COLUMN locale TEXT;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE object_store_index "
                         "ADD COLUMN is_auto_locale BOOLEAN;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE index_data "
                         "ADD COLUMN value_locale BLOB;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("ALTER TABLE unique_index_data "
                         "ADD COLUMN value_locale BLOB;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX index_data_value_locale_index "
      "ON index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX unique_index_data_value_locale_index "
      "ON unique_index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(19, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class UpgradeFileIdsFunction final : public mozIStorageFunction {
  RefPtr<FileManager> mFileManager;

 public:
  UpgradeFileIdsFunction() { AssertIsOnIOThread(); }

  nsresult Init(nsIFile* aFMDirectory, mozIStorageConnection* aConnection);

  NS_DECL_ISUPPORTS

 private:
  ~UpgradeFileIdsFunction() {
    AssertIsOnIOThread();

    if (mFileManager) {
      mFileManager->Invalidate();
    }
  }

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override;
};

nsresult UpgradeSchemaFrom19_0To20_0(nsIFile* aFMDirectory,
                                     mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom19_0To20_0", DOM);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT count(*) "
                         "FROM object_data "
                         "WHERE file_ids IS NOT NULL"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t count;

  {
    mozStorageStatementScoper scoper(stmt);

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!hasResult)) {
      MOZ_ASSERT(false, "This should never be possible!");
      return NS_ERROR_FAILURE;
    }

    count = stmt->AsInt64(0);
    if (NS_WARN_IF(count < 0)) {
      MOZ_ASSERT(false, "This should never be possible!");
      return NS_ERROR_FAILURE;
    }
  }

  if (count == 0) {
    // Nothing to upgrade.
    rv = aConnection->SetSchemaVersion(MakeSchemaVersion(20, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  RefPtr<UpgradeFileIdsFunction> function = new UpgradeFileIdsFunction();

  rv = function->Init(aFMDirectory, aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_NAMED_LITERAL_CSTRING(functionName, "upgrade");

  rv = aConnection->CreateFunction(functionName, 2, function);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Disable update trigger.
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TRIGGER object_data_update_trigger;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("UPDATE object_data "
                         "SET file_ids = upgrade(file_ids, data) "
                         "WHERE file_ids IS NOT NULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Enable update trigger.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "FOR EACH ROW "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
      "END;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->RemoveFunction(functionName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(20, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class UpgradeIndexDataValuesFunction final : public mozIStorageFunction {
 public:
  UpgradeIndexDataValuesFunction() { AssertIsOnIOThread(); }

  NS_DECL_ISUPPORTS

 private:
  ~UpgradeIndexDataValuesFunction() { AssertIsOnIOThread(); }

  nsresult ReadOldCompressedIDVFromBlob(const uint8_t* aBlobData,
                                        uint32_t aBlobDataLength,
                                        nsTArray<IndexDataValue>& aIndexValues);

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override;
};

NS_IMPL_ISUPPORTS(UpgradeIndexDataValuesFunction, mozIStorageFunction)

nsresult UpgradeIndexDataValuesFunction::ReadOldCompressedIDVFromBlob(
    const uint8_t* aBlobData, uint32_t aBlobDataLength,
    nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aBlobData);
  MOZ_ASSERT(aBlobDataLength);
  MOZ_ASSERT(aIndexValues.IsEmpty());

  const uint8_t* blobDataIter = aBlobData;
  const uint8_t* blobDataEnd = aBlobData + aBlobDataLength;

  int64_t indexId;
  bool unique;
  bool nextIndexIdAlreadyRead = false;

  while (blobDataIter < blobDataEnd) {
    if (!nextIndexIdAlreadyRead) {
      ReadCompressedIndexId(&blobDataIter, blobDataEnd, &indexId, &unique);
    }
    nextIndexIdAlreadyRead = false;

    if (NS_WARN_IF(blobDataIter == blobDataEnd)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    // Read key buffer length.
    const uint64_t keyBufferLength =
        ReadCompressedNumber(&blobDataIter, blobDataEnd);

    if (NS_WARN_IF(blobDataIter == blobDataEnd) ||
        NS_WARN_IF(keyBufferLength > uint64_t(UINT32_MAX)) ||
        NS_WARN_IF(blobDataIter + keyBufferLength > blobDataEnd)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    nsCString keyBuffer(reinterpret_cast<const char*>(blobDataIter),
                        uint32_t(keyBufferLength));
    blobDataIter += keyBufferLength;

    IndexDataValue idv(indexId, unique, Key(keyBuffer));

    if (blobDataIter < blobDataEnd) {
      // Read either a sort key buffer length or an index id.
      uint64_t maybeIndexId = ReadCompressedNumber(&blobDataIter, blobDataEnd);

      // Locale-aware indexes haven't been around long enough to have any users,
      // we can safely assume all sort key buffer lengths will be zero.
      if (maybeIndexId != 0) {
        if (maybeIndexId % 2) {
          unique = true;
          maybeIndexId--;
        } else {
          unique = false;
        }
        indexId = maybeIndexId / 2;
        nextIndexIdAlreadyRead = true;
      }
    }

    if (NS_WARN_IF(!aIndexValues.InsertElementSorted(idv, fallible))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  MOZ_ASSERT(blobDataIter == blobDataEnd);

  return NS_OK;
}

NS_IMETHODIMP
UpgradeIndexDataValuesFunction::OnFunctionCall(
    mozIStorageValueArray* aArguments, nsIVariant** aResult) {
  MOZ_ASSERT(aArguments);
  MOZ_ASSERT(aResult);

  AUTO_PROFILER_LABEL("UpgradeIndexDataValuesFunction::OnFunctionCall", DOM);

  uint32_t argc;
  nsresult rv = aArguments->GetNumEntries(&argc);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (argc != 1) {
    NS_WARNING("Don't call me with the wrong number of arguments!");
    return NS_ERROR_UNEXPECTED;
  }

  int32_t type;
  rv = aArguments->GetTypeOfIndex(0, &type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (type != mozIStorageStatement::VALUE_TYPE_BLOB) {
    NS_WARNING("Don't call me with the wrong type of arguments!");
    return NS_ERROR_UNEXPECTED;
  }

  const uint8_t* oldBlob;
  uint32_t oldBlobLength;
  rv = aArguments->GetSharedBlob(0, &oldBlobLength, &oldBlob);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoTArray<IndexDataValue, 32> oldIdv;
  rv = ReadOldCompressedIDVFromBlob(oldBlob, oldBlobLength, oldIdv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  UniqueFreePtr<uint8_t> newIdv;
  uint32_t newIdvLength;
  rv = MakeCompressedIndexDataValues(oldIdv, newIdv, &newIdvLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  std::pair<uint8_t*, int> data(newIdv.release(), newIdvLength);

  nsCOMPtr<nsIVariant> result = new storage::AdoptedBlobVariant(data);

  result.forget(aResult);
  return NS_OK;
}

nsresult UpgradeSchemaFrom20_0To21_0(mozIStorageConnection* aConnection) {
  // This should have been part of the 18 to 19 upgrade, where we changed the
  // layout of the index_data_values blobs but didn't upgrade the existing data.
  // See bug 1202788.

  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom20_0To21_0", DOM);

  RefPtr<UpgradeIndexDataValuesFunction> function =
      new UpgradeIndexDataValuesFunction();

  NS_NAMED_LITERAL_CSTRING(functionName, "upgrade_idv");

  nsresult rv = aConnection->CreateFunction(functionName, 1, function);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "UPDATE object_data "
      "SET index_data_values = upgrade_idv(index_data_values) "
      "WHERE index_data_values IS NOT NULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->RemoveFunction(functionName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(21, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom21_0To22_0(mozIStorageConnection* aConnection) {
  // The only change between 21 and 22 was a different structured clone format,
  // but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(22, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom22_0To23_0(mozIStorageConnection* aConnection,
                                     const nsACString& aOrigin) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!aOrigin.IsEmpty());

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom22_0To23_0", DOM);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv =
      aConnection->CreateStatement(NS_LITERAL_CSTRING("UPDATE database "
                                                      "SET origin = :origin;"),
                                   getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"), aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(23, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom23_0To24_0(mozIStorageConnection* aConnection) {
  // The only change between 23 and 24 was a different structured clone format,
  // but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(24, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom24_0To25_0(mozIStorageConnection* aConnection) {
  // The changes between 24 and 25 were an upgraded snappy library, a different
  // structured clone format and a different file_ds format. But everything is
  // backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(25, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class StripObsoleteOriginAttributesFunction final : public mozIStorageFunction {
 public:
  NS_DECL_ISUPPORTS

 private:
  ~StripObsoleteOriginAttributesFunction() {}

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    AUTO_PROFILER_LABEL("StripObsoleteOriginAttributesFunction::OnFunctionCall",
                        DOM);

#ifdef DEBUG
    {
      uint32_t argCount;
      MOZ_ALWAYS_SUCCEEDS(aArguments->GetNumEntries(&argCount));
      MOZ_ASSERT(argCount == 1);

      int32_t type;
      MOZ_ALWAYS_SUCCEEDS(aArguments->GetTypeOfIndex(0, &type));
      MOZ_ASSERT(type == mozIStorageValueArray::VALUE_TYPE_TEXT);
    }
#endif

    nsCString origin;
    nsresult rv = aArguments->GetUTF8String(0, origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Deserialize and re-serialize to automatically drop any obsolete origin
    // attributes.
    OriginAttributes oa;

    nsCString originNoSuffix;
    bool ok = oa.PopulateFromOrigin(origin, originNoSuffix);
    if (NS_WARN_IF(!ok)) {
      return NS_ERROR_FAILURE;
    }

    nsCString suffix;
    oa.CreateSuffix(suffix);

    nsCOMPtr<nsIVariant> result =
        new mozilla::storage::UTF8TextVariant(originNoSuffix + suffix);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult UpgradeSchemaFrom25_0To26_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom25_0To26_0", DOM);

  NS_NAMED_LITERAL_CSTRING(functionName, "strip_obsolete_attributes");

  nsCOMPtr<mozIStorageFunction> stripObsoleteAttributes =
      new StripObsoleteOriginAttributesFunction();

  nsresult rv = aConnection->CreateFunction(functionName,
                                            /* aNumArguments */ 1,
                                            stripObsoleteAttributes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("UPDATE DATABASE "
                         "SET origin = strip_obsolete_attributes(origin) "
                         "WHERE origin LIKE '%^%';"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->RemoveFunction(functionName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(26, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult GetDatabaseFileURL(nsIFile* aDatabaseFile,
                            PersistenceType aPersistenceType,
                            const nsACString& aGroup, const nsACString& aOrigin,
                            uint32_t aTelemetryId, nsIFileURL** aResult) {
  MOZ_ASSERT(aDatabaseFile);
  MOZ_ASSERT(aResult);

  nsresult rv;

  nsCOMPtr<nsIProtocolHandler> protocolHandler(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "file", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFileProtocolHandler> fileHandler(
      do_QueryInterface(protocolHandler, &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIURIMutator> mutator;
  rv = fileHandler->NewFileURIMutator(aDatabaseFile, getter_AddRefs(mutator));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFileURL> fileUrl;

  nsAutoCString type;
  PersistenceTypeToText(aPersistenceType, type);

  nsAutoCString telemetryFilenameClause;
  if (aTelemetryId) {
    telemetryFilenameClause.AssignLiteral("&telemetryFilename=indexedDB-");
    telemetryFilenameClause.AppendInt(aTelemetryId);
    telemetryFilenameClause.AppendLiteral(".sqlite");
  }

  rv = NS_MutateURI(mutator)
           .SetQuery(NS_LITERAL_CSTRING("persistenceType=") + type +
                     NS_LITERAL_CSTRING("&group=") + aGroup +
                     NS_LITERAL_CSTRING("&origin=") + aOrigin +
                     NS_LITERAL_CSTRING("&cache=private") +
                     telemetryFilenameClause)
           .Finalize(fileUrl);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  fileUrl.forget(aResult);
  return NS_OK;
}

nsresult SetDefaultPragmas(mozIStorageConnection* aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  static const char kBuiltInPragmas[] =
      // We use foreign keys in DEBUG builds only because there is a performance
      // cost to using them.
      "PRAGMA foreign_keys = "
#ifdef DEBUG
      "ON"
#else
      "OFF"
#endif
      ";"

      // The "INSERT OR REPLACE" statement doesn't fire the update trigger,
      // instead it fires only the insert trigger. This confuses the update
      // refcount function. This behavior changes with enabled recursive
      // triggers, so the statement fires the delete trigger first and then the
      // insert trigger.
      "PRAGMA recursive_triggers = ON;"

      // We aggressively truncate the database file when idle so don't bother
      // overwriting the WAL with 0 during active periods.
      "PRAGMA secure_delete = OFF;";

  nsresult rv = aConnection->ExecuteSimpleSQL(nsDependentCString(
      kBuiltInPragmas, LiteralStringLength(kBuiltInPragmas)));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString pragmaStmt;
  pragmaStmt.AssignLiteral("PRAGMA synchronous = ");

  if (IndexedDatabaseManager::FullSynchronous()) {
    pragmaStmt.AppendLiteral("FULL");
  } else {
    pragmaStmt.AppendLiteral("NORMAL");
  }
  pragmaStmt.Append(';');

  rv = aConnection->ExecuteSimpleSQL(pragmaStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifndef IDB_MOBILE
  if (kSQLiteGrowthIncrement) {
    // This is just an optimization so ignore the failure if the disk is
    // currently too full.
    rv =
        aConnection->SetGrowthIncrement(kSQLiteGrowthIncrement, EmptyCString());
    if (rv != NS_ERROR_FILE_TOO_BIG && NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif  // IDB_MOBILE

  return NS_OK;
}

nsresult SetJournalMode(mozIStorageConnection* aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  // Try enabling WAL mode. This can fail in various circumstances so we have to
  // check the results here.
  NS_NAMED_LITERAL_CSTRING(journalModeQueryStart, "PRAGMA journal_mode = ");
  NS_NAMED_LITERAL_CSTRING(journalModeWAL, "wal");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      journalModeQueryStart + journalModeWAL, getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  nsCString journalMode;
  rv = stmt->GetUTF8String(0, journalMode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (journalMode.Equals(journalModeWAL)) {
    // WAL mode successfully enabled. Maybe set limits on its size here.
    if (kMaxWALPages >= 0) {
      nsAutoCString pageCount;
      pageCount.AppendInt(kMaxWALPages);

      rv = aConnection->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("PRAGMA wal_autocheckpoint = ") + pageCount);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  } else {
    NS_WARNING("Failed to set WAL mode, falling back to normal journal mode.");
#ifdef IDB_MOBILE
    rv = aConnection->ExecuteSimpleSQL(journalModeQueryStart +
                                       NS_LITERAL_CSTRING("truncate"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
#endif
  }

  return NS_OK;
}

template <class FileOrURLType>
struct StorageOpenTraits;

template <>
struct StorageOpenTraits<nsIFileURL*> {
  static nsresult Open(mozIStorageService* aStorageService,
                       nsIFileURL* aFileURL,
                       mozIStorageConnection** aConnection) {
    return aStorageService->OpenDatabaseWithFileURL(aFileURL, aConnection);
  }

#ifdef DEBUG
  static void GetPath(nsIFileURL* aFileURL, nsCString& aPath) {
    MOZ_ALWAYS_SUCCEEDS(aFileURL->GetFileName(aPath));
  }
#endif
};

template <>
struct StorageOpenTraits<nsIFile*> {
  static nsresult Open(mozIStorageService* aStorageService, nsIFile* aFile,
                       mozIStorageConnection** aConnection) {
    return aStorageService->OpenUnsharedDatabase(aFile, aConnection);
  }

#ifdef DEBUG
  static void GetPath(nsIFile* aFile, nsCString& aPath) {
    nsString path;
    MOZ_ALWAYS_SUCCEEDS(aFile->GetPath(path));

    LossyCopyUTF16toASCII(path, aPath);
  }
#endif
};

template <template <class> class SmartPtr, class FileOrURLType>
struct StorageOpenTraits<SmartPtr<FileOrURLType>>
    : public StorageOpenTraits<FileOrURLType*> {};

template <class FileOrURLType>
nsresult OpenDatabaseAndHandleBusy(mozIStorageService* aStorageService,
                                   FileOrURLType aFileOrURL,
                                   mozIStorageConnection** aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aStorageService);
  MOZ_ASSERT(aFileOrURL);
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageConnection> connection;
  nsresult rv = StorageOpenTraits<FileOrURLType>::Open(
      aStorageService, aFileOrURL, getter_AddRefs(connection));

  if (rv == NS_ERROR_STORAGE_BUSY) {
#ifdef DEBUG
    {
      nsCString path;
      StorageOpenTraits<FileOrURLType>::GetPath(aFileOrURL, path);

      nsPrintfCString message(
          "Received NS_ERROR_STORAGE_BUSY when attempting "
          "to open database '%s', retrying for up to 10 "
          "seconds",
          path.get());
      NS_WARNING(message.get());
    }
#endif

    // Another thread must be checkpointing the WAL. Wait up to 10 seconds for
    // that to complete.
    TimeStamp start = TimeStamp::NowLoRes();

    while (true) {
      PR_Sleep(PR_MillisecondsToInterval(100));

      rv = StorageOpenTraits<FileOrURLType>::Open(aStorageService, aFileOrURL,
                                                  getter_AddRefs(connection));
      if (rv != NS_ERROR_STORAGE_BUSY ||
          TimeStamp::NowLoRes() - start > TimeDuration::FromSeconds(10)) {
        break;
      }
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult CreateStorageConnection(nsIFile* aDBFile, nsIFile* aFMDirectory,
                                 const nsAString& aName,
                                 PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 uint32_t aTelemetryId,
                                 mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDBFile);
  MOZ_ASSERT(aFMDirectory);
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("CreateStorageConnection", DOM);

  nsresult rv;
  bool exists;

  nsCOMPtr<nsIFileURL> dbFileUrl;
  rv = GetDatabaseFileURL(aDBFile, aPersistenceType, aGroup, aOrigin,
                          aTelemetryId, getter_AddRefs(dbFileUrl));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = OpenDatabaseAndHandleBusy(ss, dbFileUrl, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // If we're just opening the database during origin initialization, then
    // we don't want to erase any files. The failure here will fail origin
    // initialization too.
    if (aName.IsVoid()) {
      return rv;
    }

    // Nuke the database file.
    rv = aDBFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aFMDirectory->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (exists) {
      bool isDirectory;
      rv = aFMDirectory->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (NS_WARN_IF(!isDirectory)) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      rv = aFMDirectory->Remove(true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = OpenDatabaseAndHandleBusy(ss, dbFileUrl, getter_AddRefs(connection));
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = connection->EnableModule(NS_LITERAL_CSTRING("filesystem"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Check to make sure that the database schema is correct.
  int32_t schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Unknown schema will fail origin initialization too.
  if (!schemaVersion && aName.IsVoid()) {
    IDB_WARNING("Unable to open IndexedDB database, schema is not set!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (schemaVersion > kSQLiteSchemaVersion) {
    IDB_WARNING("Unable to open IndexedDB database, schema is too high!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  bool journalModeSet = false;

  if (schemaVersion != kSQLiteSchemaVersion) {
    const bool newDatabase = !schemaVersion;

    if (newDatabase) {
      // Set the page size first.
      if (kSQLitePageSizeOverride) {
        rv = connection->ExecuteSimpleSQL(nsPrintfCString(
            "PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // We have to set the auto_vacuum mode before opening a transaction.
      rv = connection->ExecuteSimpleSQL(
#ifdef IDB_MOBILE
          // Turn on full auto_vacuum mode to reclaim disk space on mobile
          // devices (at the cost of some COMMIT speed).
          NS_LITERAL_CSTRING("PRAGMA auto_vacuum = FULL;")
#else
          // Turn on incremental auto_vacuum mode on desktop builds.
          NS_LITERAL_CSTRING("PRAGMA auto_vacuum = INCREMENTAL;")
#endif
      );
      if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
        // mozstorage translates SQLITE_FULL to NS_ERROR_FILE_NO_DEVICE_SPACE,
        // which we know better as NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR.
        rv = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = SetJournalMode(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      journalModeSet = true;
    } else {
#ifdef DEBUG
      // Disable foreign key support while upgrading. This has to be done before
      // starting a transaction.
      MOZ_ALWAYS_SUCCEEDS(connection->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("PRAGMA foreign_keys = OFF;")));
#endif
    }

    bool vacuumNeeded = false;

    mozStorageTransaction transaction(
        connection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (newDatabase) {
      rv = CreateTables(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)));
      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);

      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = connection->CreateStatement(
          NS_LITERAL_CSTRING("INSERT INTO database (name, origin) "
                             "VALUES (:name, :origin)"),
          getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), aName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"), aOrigin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // This logic needs to change next time we change the schema!
      static_assert(kSQLiteSchemaVersion == int32_t((26 << 4) + 0),
                    "Upgrade function needed due to schema version increase.");

      while (schemaVersion != kSQLiteSchemaVersion) {
        if (schemaVersion == 4) {
          rv = UpgradeSchemaFrom4To5(connection);
        } else if (schemaVersion == 5) {
          rv = UpgradeSchemaFrom5To6(connection);
        } else if (schemaVersion == 6) {
          rv = UpgradeSchemaFrom6To7(connection);
        } else if (schemaVersion == 7) {
          rv = UpgradeSchemaFrom7To8(connection);
        } else if (schemaVersion == 8) {
          rv = UpgradeSchemaFrom8To9_0(connection);
          vacuumNeeded = true;
        } else if (schemaVersion == MakeSchemaVersion(9, 0)) {
          rv = UpgradeSchemaFrom9_0To10_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(10, 0)) {
          rv = UpgradeSchemaFrom10_0To11_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(11, 0)) {
          rv = UpgradeSchemaFrom11_0To12_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(12, 0)) {
          rv = UpgradeSchemaFrom12_0To13_0(connection, &vacuumNeeded);
        } else if (schemaVersion == MakeSchemaVersion(13, 0)) {
          rv = UpgradeSchemaFrom13_0To14_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(14, 0)) {
          rv = UpgradeSchemaFrom14_0To15_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(15, 0)) {
          rv = UpgradeSchemaFrom15_0To16_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(16, 0)) {
          rv = UpgradeSchemaFrom16_0To17_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(17, 0)) {
          rv = UpgradeSchemaFrom17_0To18_0(connection, aOrigin);
          vacuumNeeded = true;
        } else if (schemaVersion == MakeSchemaVersion(18, 0)) {
          rv = UpgradeSchemaFrom18_0To19_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(19, 0)) {
          rv = UpgradeSchemaFrom19_0To20_0(aFMDirectory, connection);
        } else if (schemaVersion == MakeSchemaVersion(20, 0)) {
          rv = UpgradeSchemaFrom20_0To21_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(21, 0)) {
          rv = UpgradeSchemaFrom21_0To22_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(22, 0)) {
          rv = UpgradeSchemaFrom22_0To23_0(connection, aOrigin);
        } else if (schemaVersion == MakeSchemaVersion(23, 0)) {
          rv = UpgradeSchemaFrom23_0To24_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(24, 0)) {
          rv = UpgradeSchemaFrom24_0To25_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(25, 0)) {
          rv = UpgradeSchemaFrom25_0To26_0(connection);
        } else {
          IDB_WARNING(
              "Unable to open IndexedDB database, no upgrade path is "
              "available!");
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = connection->GetSchemaVersion(&schemaVersion);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);
    }

    rv = transaction.Commit();
    if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      // mozstorage translates SQLITE_FULL to NS_ERROR_FILE_NO_DEVICE_SPACE,
      // which we know better as NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR.
      rv = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

#ifdef DEBUG
    if (!newDatabase) {
      // Re-enable foreign key support after doing a foreign key check.
      nsCOMPtr<mozIStorageStatement> checkStmt;
      MOZ_ALWAYS_SUCCEEDS(connection->CreateStatement(
          NS_LITERAL_CSTRING("PRAGMA foreign_key_check;"),
          getter_AddRefs(checkStmt)));

      bool hasResult;
      MOZ_ALWAYS_SUCCEEDS(checkStmt->ExecuteStep(&hasResult));
      MOZ_ASSERT(!hasResult, "Database has inconsisistent foreign keys!");

      MOZ_ALWAYS_SUCCEEDS(connection->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("PRAGMA foreign_keys = OFF;")));
    }
#endif

    if (kSQLitePageSizeOverride && !newDatabase) {
      nsCOMPtr<mozIStorageStatement> stmt;
      rv = connection->CreateStatement(NS_LITERAL_CSTRING("PRAGMA page_size;"),
                                       getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      bool hasResult;
      rv = stmt->ExecuteStep(&hasResult);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(hasResult);

      int32_t pageSize;
      rv = stmt->GetInt32(0, &pageSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(pageSize >= 512 && pageSize <= 65536);

      if (kSQLitePageSizeOverride != uint32_t(pageSize)) {
        // We must not be in WAL journal mode to change the page size.
        rv = connection->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("PRAGMA journal_mode = DELETE;"));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = connection->CreateStatement(
            NS_LITERAL_CSTRING("PRAGMA journal_mode;"), getter_AddRefs(stmt));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = stmt->ExecuteStep(&hasResult);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        MOZ_ASSERT(hasResult);

        nsCString journalMode;
        rv = stmt->GetUTF8String(0, journalMode);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (journalMode.EqualsLiteral("delete")) {
          // Successfully set to rollback journal mode so changing the page size
          // is possible with a VACUUM.
          rv = connection->ExecuteSimpleSQL(nsPrintfCString(
              "PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride));
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          // We will need to VACUUM in order to change the page size.
          vacuumNeeded = true;
        } else {
          NS_WARNING(
              "Failed to set journal_mode for database, unable to "
              "change the page size!");
        }
      }
    }

    if (vacuumNeeded) {
      rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM;"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (newDatabase || vacuumNeeded) {
      if (journalModeSet) {
        // Make sure we checkpoint to get an accurate file size.
        rv = connection->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("PRAGMA wal_checkpoint(FULL);"));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      int64_t fileSize;
      rv = aDBFile->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(fileSize > 0);

      PRTime vacuumTime = PR_Now();
      MOZ_ASSERT(vacuumTime);

      nsCOMPtr<mozIStorageStatement> vacuumTimeStmt;
      rv = connection->CreateStatement(
          NS_LITERAL_CSTRING("UPDATE database "
                             "SET last_vacuum_time = :time"
                             ", last_vacuum_size = :size;"),
          getter_AddRefs(vacuumTimeStmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("time"),
                                           vacuumTime);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv =
          vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("size"), fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  if (!journalModeSet) {
    rv = SetJournalMode(connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  connection.forget(aConnection);
  return NS_OK;
}

already_AddRefed<nsIFile> GetFileForPath(const nsAString& aPath) {
  MOZ_ASSERT(!aPath.IsEmpty());

  nsCOMPtr<nsIFile> file;
  if (NS_WARN_IF(
          NS_FAILED(NS_NewLocalFile(aPath, false, getter_AddRefs(file))))) {
    return nullptr;
  }

  return file.forget();
}

nsresult GetStorageConnection(nsIFile* aDatabaseFile,
                              PersistenceType aPersistenceType,
                              const nsACString& aGroup,
                              const nsACString& aOrigin, uint32_t aTelemetryId,
                              mozIStorageConnection** aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aDatabaseFile);
  MOZ_ASSERT(aConnection);

  AUTO_PROFILER_LABEL("GetStorageConnection", DOM);

  bool exists;
  nsresult rv = aDatabaseFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!exists)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFileURL> dbFileUrl;
  rv = GetDatabaseFileURL(aDatabaseFile, aPersistenceType, aGroup, aOrigin,
                          aTelemetryId, getter_AddRefs(dbFileUrl));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = OpenDatabaseAndHandleBusy(ss, dbFileUrl, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetJournalMode(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetStorageConnection(const nsAString& aDatabaseFilePath,
                              PersistenceType aPersistenceType,
                              const nsACString& aGroup,
                              const nsACString& aOrigin, uint32_t aTelemetryId,
                              mozIStorageConnection** aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")));
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> dbFile = GetFileForPath(aDatabaseFilePath);
  if (NS_WARN_IF(!dbFile)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return GetStorageConnection(dbFile, aPersistenceType, aGroup, aOrigin,
                              aTelemetryId, aConnection);
}

/*******************************************************************************
 * ConnectionPool declarations
 ******************************************************************************/

class DatabaseConnection final {
  friend class ConnectionPool;

  enum class CheckpointMode { Full, Restart, Truncate };

 public:
  class AutoSavepoint;
  class CachedStatement;
  class UpdateRefcountFunction;

 private:
  nsCOMPtr<mozIStorageConnection> mStorageConnection;
  RefPtr<FileManager> mFileManager;
  nsInterfaceHashtable<nsCStringHashKey, mozIStorageStatement>
      mCachedStatements;
  RefPtr<UpdateRefcountFunction> mUpdateRefcountFunction;
  RefPtr<QuotaObject> mQuotaObject;
  RefPtr<QuotaObject> mJournalQuotaObject;
  bool mInReadTransaction;
  bool mInWriteTransaction;

#ifdef DEBUG
  uint32_t mDEBUGSavepointCount;
#endif

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnConnectionThread() const {
    NS_ASSERT_OWNINGTHREAD(DatabaseConnection);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DatabaseConnection)

  mozIStorageConnection* GetStorageConnection() const {
    if (mStorageConnection) {
      AssertIsOnConnectionThread();
      return mStorageConnection;
    }

    return nullptr;
  }

  UpdateRefcountFunction* GetUpdateRefcountFunction() const {
    AssertIsOnConnectionThread();

    return mUpdateRefcountFunction;
  }

  nsresult GetCachedStatement(const nsACString& aQuery,
                              CachedStatement* aCachedStatement);

  nsresult BeginWriteTransaction();

  nsresult CommitWriteTransaction();

  void RollbackWriteTransaction();

  void FinishWriteTransaction();

  nsresult StartSavepoint();

  nsresult ReleaseSavepoint();

  nsresult RollbackSavepoint();

  nsresult Checkpoint() {
    AssertIsOnConnectionThread();

    return CheckpointInternal(CheckpointMode::Full);
  }

  void DoIdleProcessing(bool aNeedsCheckpoint);

  void Close();

  nsresult DisableQuotaChecks();

  void EnableQuotaChecks();

 private:
  DatabaseConnection(mozIStorageConnection* aStorageConnection,
                     FileManager* aFileManager);

  ~DatabaseConnection();

  nsresult Init();

  nsresult CheckpointInternal(CheckpointMode aMode);

  nsresult GetFreelistCount(CachedStatement& aCachedStatement,
                            uint32_t* aFreelistCount);

  nsresult ReclaimFreePagesWhileIdle(CachedStatement& aFreelistStatement,
                                     CachedStatement& aRollbackStatement,
                                     uint32_t aFreelistCount,
                                     bool aNeedsCheckpoint,
                                     bool* aFreedSomePages);

  nsresult GetFileSize(const nsAString& aPath, int64_t* aResult);
};

class MOZ_STACK_CLASS DatabaseConnection::AutoSavepoint final {
  DatabaseConnection* mConnection;
#ifdef DEBUG
  const TransactionBase* mDEBUGTransaction;
#endif

 public:
  AutoSavepoint();
  ~AutoSavepoint();

  nsresult Start(const TransactionBase* aTransaction);

  nsresult Commit();
};

class DatabaseConnection::CachedStatement final {
  friend class DatabaseConnection;

  nsCOMPtr<mozIStorageStatement> mStatement;
  Maybe<mozStorageStatementScoper> mScoper;

#ifdef DEBUG
  DatabaseConnection* mDEBUGConnection;
#endif

 public:
  CachedStatement();
  ~CachedStatement();

  void AssertIsOnConnectionThread() const {
#ifdef DEBUG
    if (mDEBUGConnection) {
      mDEBUGConnection->AssertIsOnConnectionThread();
    }
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsOnBackgroundThread());
#endif
  }

  operator mozIStorageStatement*() const;

  mozIStorageStatement* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN;

  void Reset();

 private:
  // Only called by DatabaseConnection.
  void Assign(DatabaseConnection* aConnection,
              already_AddRefed<mozIStorageStatement> aStatement);

  // No funny business allowed.
  CachedStatement(const CachedStatement&) = delete;
  CachedStatement& operator=(const CachedStatement&) = delete;
};

class DatabaseConnection::UpdateRefcountFunction final
    : public mozIStorageFunction {
  class DatabaseUpdateFunction;
  class FileInfoEntry;

  enum class UpdateType { Increment, Decrement };

  DatabaseConnection* mConnection;
  FileManager* mFileManager;
  nsClassHashtable<nsUint64HashKey, FileInfoEntry> mFileInfoEntries;
  nsDataHashtable<nsUint64HashKey, FileInfoEntry*> mSavepointEntriesIndex;

  nsTArray<int64_t> mJournalsToCreateBeforeCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterAbort;

  bool mInSavepoint;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  UpdateRefcountFunction(DatabaseConnection* aConnection,
                         FileManager* aFileManager);

  nsresult WillCommit();

  void DidCommit();

  void DidAbort();

  void StartSavepoint();

  void ReleaseSavepoint();

  void RollbackSavepoint();

  void Reset();

 private:
  ~UpdateRefcountFunction() = default;

  nsresult ProcessValue(mozIStorageValueArray* aValues, int32_t aIndex,
                        UpdateType aUpdateType);

  nsresult CreateJournals();

  nsresult RemoveJournals(const nsTArray<int64_t>& aJournals);
};

class DatabaseConnection::UpdateRefcountFunction::DatabaseUpdateFunction final {
  CachedStatement mUpdateStatement;
  CachedStatement mSelectStatement;
  CachedStatement mInsertStatement;

  UpdateRefcountFunction* mFunction;

  nsresult mErrorCode;

 public:
  explicit DatabaseUpdateFunction(UpdateRefcountFunction* aFunction)
      : mFunction(aFunction), mErrorCode(NS_OK) {
    MOZ_COUNT_CTOR(
        DatabaseConnection::UpdateRefcountFunction::DatabaseUpdateFunction);
  }

  ~DatabaseUpdateFunction() {
    MOZ_COUNT_DTOR(
        DatabaseConnection::UpdateRefcountFunction::DatabaseUpdateFunction);
  }

  bool Update(int64_t aId, int32_t aDelta);

  nsresult ErrorCode() const { return mErrorCode; }

 private:
  nsresult UpdateInternal(int64_t aId, int32_t aDelta);
};

class DatabaseConnection::UpdateRefcountFunction::FileInfoEntry final {
  friend class UpdateRefcountFunction;

  RefPtr<FileInfo> mFileInfo;
  int32_t mDelta;
  int32_t mSavepointDelta;

 public:
  explicit FileInfoEntry(FileInfo* aFileInfo)
      : mFileInfo(aFileInfo), mDelta(0), mSavepointDelta(0) {
    MOZ_COUNT_CTOR(DatabaseConnection::UpdateRefcountFunction::FileInfoEntry);
  }

  ~FileInfoEntry() {
    MOZ_COUNT_DTOR(DatabaseConnection::UpdateRefcountFunction::FileInfoEntry);
  }
};

class ConnectionPool final {
 public:
  class FinishCallback;

 private:
  class ConnectionRunnable;
  class CloseConnectionRunnable;
  struct DatabaseInfo;
  struct DatabasesCompleteCallback;
  class FinishCallbackWrapper;
  class IdleConnectionRunnable;
  struct IdleDatabaseInfo;
  struct IdleResource;
  struct IdleThreadInfo;
  struct ThreadInfo;
  class ThreadRunnable;
  class TransactionInfo;
  struct TransactionInfoPair;

  // This mutex guards mDatabases, see below.
  Mutex mDatabasesMutex;

  nsTArray<IdleThreadInfo> mIdleThreads;
  nsTArray<IdleDatabaseInfo> mIdleDatabases;
  nsTArray<DatabaseInfo*> mDatabasesPerformingIdleMaintenance;
  nsCOMPtr<nsITimer> mIdleTimer;
  TimeStamp mTargetIdleTime;

  // Only modifed on the owning thread, but read on multiple threads. Therefore
  // all modifications and all reads off the owning thread must be protected by
  // mDatabasesMutex.
  nsClassHashtable<nsCStringHashKey, DatabaseInfo> mDatabases;

  nsClassHashtable<nsUint64HashKey, TransactionInfo> mTransactions;
  nsTArray<TransactionInfo*> mQueuedTransactions;

  nsTArray<nsAutoPtr<DatabasesCompleteCallback>> mCompleteCallbacks;

  uint64_t mNextTransactionId;
  uint32_t mTotalThreadCount;
  bool mShutdownRequested;
  bool mShutdownComplete;

 public:
  ConnectionPool();

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(ConnectionPool);
  }

  nsresult GetOrCreateConnection(const Database* aDatabase,
                                 DatabaseConnection** aConnection);

  uint64_t Start(const nsID& aBackgroundChildLoggingId,
                 const nsACString& aDatabaseId, int64_t aLoggingSerialNumber,
                 const nsTArray<nsString>& aObjectStoreNames,
                 bool aIsWriteTransaction,
                 TransactionDatabaseOperationBase* aTransactionOp);

  void Dispatch(uint64_t aTransactionId, nsIRunnable* aRunnable);

  void Finish(uint64_t aTransactionId, FinishCallback* aCallback);

  void CloseDatabaseWhenIdle(const nsACString& aDatabaseId) {
    Unused << CloseDatabaseWhenIdleInternal(aDatabaseId);
  }

  void WaitForDatabasesToComplete(nsTArray<nsCString>&& aDatabaseIds,
                                  nsIRunnable* aCallback);

  void Shutdown();

  NS_INLINE_DECL_REFCOUNTING(ConnectionPool)

 private:
  ~ConnectionPool();

  static void IdleTimerCallback(nsITimer* aTimer, void* aClosure);

  void Cleanup();

  void AdjustIdleTimer();

  void CancelIdleTimer();

  void ShutdownThread(ThreadInfo& aThreadInfo);

  void CloseIdleDatabases();

  void ShutdownIdleThreads();

  bool ScheduleTransaction(TransactionInfo* aTransactionInfo,
                           bool aFromQueuedTransactions);

  void NoteFinishedTransaction(uint64_t aTransactionId);

  void ScheduleQueuedTransactions(ThreadInfo& aThreadInfo);

  void NoteIdleDatabase(DatabaseInfo* aDatabaseInfo);

  void NoteClosedDatabase(DatabaseInfo* aDatabaseInfo);

  bool MaybeFireCallback(DatabasesCompleteCallback* aCallback);

  void PerformIdleDatabaseMaintenance(DatabaseInfo* aDatabaseInfo);

  void CloseDatabase(DatabaseInfo* aDatabaseInfo);

  bool CloseDatabaseWhenIdleInternal(const nsACString& aDatabaseId);
};

class ConnectionPool::ConnectionRunnable : public Runnable {
 protected:
  DatabaseInfo* mDatabaseInfo;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  explicit ConnectionRunnable(DatabaseInfo* aDatabaseInfo);

  ~ConnectionRunnable() override = default;
};

class ConnectionPool::IdleConnectionRunnable final : public ConnectionRunnable {
  bool mNeedsCheckpoint;

 public:
  IdleConnectionRunnable(DatabaseInfo* aDatabaseInfo, bool aNeedsCheckpoint)
      : ConnectionRunnable(aDatabaseInfo), mNeedsCheckpoint(aNeedsCheckpoint) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(IdleConnectionRunnable,
                                       ConnectionRunnable)

 private:
  ~IdleConnectionRunnable() override = default;

  NS_DECL_NSIRUNNABLE
};

class ConnectionPool::CloseConnectionRunnable final
    : public ConnectionRunnable {
 public:
  explicit CloseConnectionRunnable(DatabaseInfo* aDatabaseInfo)
      : ConnectionRunnable(aDatabaseInfo) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(CloseConnectionRunnable,
                                       ConnectionRunnable)

 private:
  ~CloseConnectionRunnable() override = default;

  NS_DECL_NSIRUNNABLE
};

struct ConnectionPool::ThreadInfo {
  nsCOMPtr<nsIThread> mThread;
  RefPtr<ThreadRunnable> mRunnable;

  ThreadInfo();

  explicit ThreadInfo(const ThreadInfo& aOther);

  ~ThreadInfo();
};

struct ConnectionPool::DatabaseInfo final {
  friend class nsAutoPtr<DatabaseInfo>;

  RefPtr<ConnectionPool> mConnectionPool;
  const nsCString mDatabaseId;
  RefPtr<DatabaseConnection> mConnection;
  nsClassHashtable<nsStringHashKey, TransactionInfoPair> mBlockingTransactions;
  nsTArray<TransactionInfo*> mTransactionsScheduledDuringClose;
  nsTArray<TransactionInfo*> mScheduledWriteTransactions;
  TransactionInfo* mRunningWriteTransaction;
  ThreadInfo mThreadInfo;
  uint32_t mReadTransactionCount;
  uint32_t mWriteTransactionCount;
  bool mNeedsCheckpoint;
  bool mIdle;
  bool mCloseOnIdle;
  bool mClosing;

#ifdef DEBUG
  PRThread* mDEBUGConnectionThread;
#endif

  DatabaseInfo(ConnectionPool* aConnectionPool, const nsACString& aDatabaseId);

  void AssertIsOnConnectionThread() const {
    MOZ_ASSERT(mDEBUGConnectionThread);
    MOZ_ASSERT(GetCurrentPhysicalThread() == mDEBUGConnectionThread);
  }

  uint64_t TotalTransactionCount() const {
    return mReadTransactionCount + mWriteTransactionCount;
  }

 private:
  ~DatabaseInfo();

  DatabaseInfo(const DatabaseInfo&) = delete;
  DatabaseInfo& operator=(const DatabaseInfo&) = delete;
};

struct ConnectionPool::DatabasesCompleteCallback final {
  friend class nsAutoPtr<DatabasesCompleteCallback>;

  nsTArray<nsCString> mDatabaseIds;
  nsCOMPtr<nsIRunnable> mCallback;

  DatabasesCompleteCallback(nsTArray<nsCString>&& aDatabaseIds,
                            nsIRunnable* aCallback);

 private:
  ~DatabasesCompleteCallback();
};

class NS_NO_VTABLE ConnectionPool::FinishCallback : public nsIRunnable {
 public:
  // Called on the owning thread before any additional transactions are
  // unblocked.
  virtual void TransactionFinishedBeforeUnblock() = 0;

  // Called on the owning thread after additional transactions may have been
  // unblocked.
  virtual void TransactionFinishedAfterUnblock() = 0;

 protected:
  FinishCallback() {}

  virtual ~FinishCallback() = default;
};

class ConnectionPool::FinishCallbackWrapper final : public Runnable {
  RefPtr<ConnectionPool> mConnectionPool;
  RefPtr<FinishCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  uint64_t mTransactionId;
  bool mHasRunOnce;

 public:
  FinishCallbackWrapper(ConnectionPool* aConnectionPool,
                        uint64_t aTransactionId, FinishCallback* aCallback);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(FinishCallbackWrapper, Runnable)

 private:
  ~FinishCallbackWrapper() override;

  NS_DECL_NSIRUNNABLE
};

struct ConnectionPool::IdleResource {
  TimeStamp mIdleTime;

 protected:
  explicit IdleResource(const TimeStamp& aIdleTime);

  explicit IdleResource(const IdleResource& aOther) = delete;

  ~IdleResource();
};

struct ConnectionPool::IdleDatabaseInfo final : public IdleResource {
  DatabaseInfo* mDatabaseInfo;

 public:
  MOZ_IMPLICIT
  IdleDatabaseInfo(DatabaseInfo* aDatabaseInfo);

  explicit IdleDatabaseInfo(const IdleDatabaseInfo& aOther) = delete;

  ~IdleDatabaseInfo();

  bool operator==(const IdleDatabaseInfo& aOther) const {
    return mDatabaseInfo == aOther.mDatabaseInfo;
  }

  bool operator<(const IdleDatabaseInfo& aOther) const {
    return mIdleTime < aOther.mIdleTime;
  }
};

struct ConnectionPool::IdleThreadInfo final : public IdleResource {
  ThreadInfo mThreadInfo;

 public:
  // Boo, this is needed because nsTArray::InsertElementSorted() doesn't yet
  // work with rvalue references.
  MOZ_IMPLICIT
  IdleThreadInfo(const ThreadInfo& aThreadInfo);

  explicit IdleThreadInfo(const IdleThreadInfo& aOther) = delete;

  ~IdleThreadInfo();

  bool operator==(const IdleThreadInfo& aOther) const {
    return mThreadInfo.mRunnable == aOther.mThreadInfo.mRunnable &&
           mThreadInfo.mThread == aOther.mThreadInfo.mThread;
  }

  bool operator<(const IdleThreadInfo& aOther) const {
    return mIdleTime < aOther.mIdleTime;
  }
};

class ConnectionPool::ThreadRunnable final : public Runnable {
  // Only touched on the background thread.
  static uint32_t sNextSerialNumber;

  // Set at construction for logging.
  const uint32_t mSerialNumber;

  // These two values are only modified on the connection thread.
  bool mFirstRun;
  bool mContinueRunning;

 public:
  ThreadRunnable();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ThreadRunnable, Runnable)

  uint32_t SerialNumber() const { return mSerialNumber; }

  nsCString GetThreadName() const {
    return nsPrintfCString("IndexedDB #%" PRIu32, mSerialNumber);
  }

 private:
  ~ThreadRunnable() override;

  NS_DECL_NSIRUNNABLE
};

class ConnectionPool::TransactionInfo final {
  friend class nsAutoPtr<TransactionInfo>;

  nsTHashtable<nsPtrHashKey<TransactionInfo>> mBlocking;
  nsTArray<TransactionInfo*> mBlockingOrdered;

 public:
  DatabaseInfo* mDatabaseInfo;
  const nsID mBackgroundChildLoggingId;
  const nsCString mDatabaseId;
  const uint64_t mTransactionId;
  const int64_t mLoggingSerialNumber;
  const nsTArray<nsString> mObjectStoreNames;
  nsTHashtable<nsPtrHashKey<TransactionInfo>> mBlockedOn;
  nsTArray<nsCOMPtr<nsIRunnable>> mQueuedRunnables;
  const bool mIsWriteTransaction;
  bool mRunning;

#ifdef DEBUG
  bool mFinished;
#endif

  TransactionInfo(DatabaseInfo* aDatabaseInfo,
                  const nsID& aBackgroundChildLoggingId,
                  const nsACString& aDatabaseId, uint64_t aTransactionId,
                  int64_t aLoggingSerialNumber,
                  const nsTArray<nsString>& aObjectStoreNames,
                  bool aIsWriteTransaction,
                  TransactionDatabaseOperationBase* aTransactionOp);

  void AddBlockingTransaction(TransactionInfo* aTransactionInfo);

  void RemoveBlockingTransactions();

 private:
  ~TransactionInfo();

  void MaybeUnblock(TransactionInfo* aTransactionInfo);
};

struct ConnectionPool::TransactionInfoPair final {
  friend class nsAutoPtr<TransactionInfoPair>;

  // Multiple reading transactions can block future writes.
  nsTArray<TransactionInfo*> mLastBlockingWrites;
  // But only a single writing transaction can block future reads.
  TransactionInfo* mLastBlockingReads;

  TransactionInfoPair();

 private:
  ~TransactionInfoPair();
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class DatabaseOperationBase : public Runnable,
                              public mozIStorageProgressHandler {
  friend class UpgradeFileIdsFunction;

 protected:
  class AutoSetProgressHandler;

  typedef nsDataHashtable<nsUint64HashKey, bool> UniqueIndexTable;

  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  const nsID mBackgroundChildLoggingId;
  const uint64_t mLoggingSerialNumber;
  nsresult mResultCode;

 private:
  Atomic<bool> mOperationMayProceed;
  bool mActorDestroyed;

 public:
  NS_DECL_ISUPPORTS_INHERITED

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const {
    MOZ_ASSERT(IsOnBackgroundThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  void NoteActorDestroyed() {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
    mOperationMayProceed = false;
  }

  bool IsActorDestroyed() const {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

  // May be called on any thread, but you should call IsActorDestroyed() if
  // you know you're on the background thread because it is slightly faster.
  bool OperationMayProceed() const { return mOperationMayProceed; }

  const nsID& BackgroundChildLoggingId() const {
    return mBackgroundChildLoggingId;
  }

  uint64_t LoggingSerialNumber() const { return mLoggingSerialNumber; }

  nsresult ResultCode() const { return mResultCode; }

  void SetFailureCode(nsresult aErrorCode) {
    MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    mResultCode = aErrorCode;
  }

 protected:
  DatabaseOperationBase(const nsID& aBackgroundChildLoggingId,
                        uint64_t aLoggingSerialNumber)
      : Runnable("dom::indexedDB::DatabaseOperationBase"),
        mOwningEventTarget(GetCurrentThreadEventTarget()),
        mBackgroundChildLoggingId(aBackgroundChildLoggingId),
        mLoggingSerialNumber(aLoggingSerialNumber),
        mResultCode(NS_OK),
        mOperationMayProceed(true),
        mActorDestroyed(false) {
    AssertIsOnOwningThread();
  }

  ~DatabaseOperationBase() override { MOZ_ASSERT(mActorDestroyed); }

  static void GetBindingClauseForKeyRange(const SerializedKeyRange& aKeyRange,
                                          const nsACString& aKeyColumnName,
                                          nsAutoCString& aBindingClause);

  static uint64_t ReinterpretDoubleAsUInt64(double aDouble);

  static nsresult GetStructuredCloneReadInfoFromStatement(
      mozIStorageStatement* aStatement, uint32_t aDataIndex,
      uint32_t aFileIdsIndex, FileManager* aFileManager,
      StructuredCloneReadInfo* aInfo) {
    return GetStructuredCloneReadInfoFromSource(
        aStatement, aDataIndex, aFileIdsIndex, aFileManager, aInfo);
  }

  static nsresult GetStructuredCloneReadInfoFromValueArray(
      mozIStorageValueArray* aValues, uint32_t aDataIndex,
      uint32_t aFileIdsIndex, FileManager* aFileManager,
      StructuredCloneReadInfo* aInfo) {
    return GetStructuredCloneReadInfoFromSource(
        aValues, aDataIndex, aFileIdsIndex, aFileManager, aInfo);
  }

  static nsresult BindKeyRangeToStatement(const SerializedKeyRange& aKeyRange,
                                          mozIStorageStatement* aStatement);

  static nsresult BindKeyRangeToStatement(const SerializedKeyRange& aKeyRange,
                                          mozIStorageStatement* aStatement,
                                          const nsCString& aLocale);

  static void AppendConditionClause(const nsACString& aColumnName,
                                    const nsACString& aArgName, bool aLessThan,
                                    bool aEquals, nsAutoCString& aResult);

  static nsresult GetUniqueIndexTableForObjectStore(
      TransactionBase* aTransaction, int64_t aObjectStoreId,
      Maybe<UniqueIndexTable>& aMaybeUniqueIndexTable);

  static nsresult IndexDataValuesFromUpdateInfos(
      const nsTArray<IndexUpdateInfo>& aUpdateInfos,
      const UniqueIndexTable& aUniqueIndexTable,
      nsTArray<IndexDataValue>& aIndexValues);

  static nsresult InsertIndexTableRows(
      DatabaseConnection* aConnection, const int64_t aObjectStoreId,
      const Key& aObjectStoreKey,
      const FallibleTArray<IndexDataValue>& aIndexValues);

  static nsresult DeleteIndexDataTableRows(
      DatabaseConnection* aConnection, const Key& aObjectStoreKey,
      const FallibleTArray<IndexDataValue>& aIndexValues);

  static nsresult DeleteObjectStoreDataTableRowsWithIndexes(
      DatabaseConnection* aConnection, const int64_t aObjectStoreId,
      const Maybe<SerializedKeyRange>& aKeyRange);

  static nsresult UpdateIndexValues(
      DatabaseConnection* aConnection, const int64_t aObjectStoreId,
      const Key& aObjectStoreKey,
      const FallibleTArray<IndexDataValue>& aIndexValues);

  static nsresult ObjectStoreHasIndexes(DatabaseConnection* aConnection,
                                        const int64_t aObjectStoreId,
                                        bool* aHasIndexes);

 private:
  template <typename T>
  static nsresult GetStructuredCloneReadInfoFromSource(
      T* aSource, uint32_t aDataIndex, uint32_t aFileIdsIndex,
      FileManager* aFileManager, StructuredCloneReadInfo* aInfo);

  static nsresult GetStructuredCloneReadInfoFromBlob(
      const uint8_t* aBlobData, uint32_t aBlobDataLength,
      FileManager* aFileManager, const nsAString& aFileIds,
      StructuredCloneReadInfo* aInfo);

  static nsresult GetStructuredCloneReadInfoFromExternalBlob(
      uint64_t aIntData, FileManager* aFileManager, const nsAString& aFileIds,
      StructuredCloneReadInfo* aInfo);

  // Not to be overridden by subclasses.
  NS_DECL_MOZISTORAGEPROGRESSHANDLER
};

class MOZ_STACK_CLASS DatabaseOperationBase::AutoSetProgressHandler final {
  mozIStorageConnection* mConnection;
#ifdef DEBUG
  DatabaseOperationBase* mDEBUGDatabaseOp;
#endif

 public:
  AutoSetProgressHandler();

  ~AutoSetProgressHandler();

  nsresult Register(mozIStorageConnection* aConnection,
                    DatabaseOperationBase* aDatabaseOp);

  void Unregister();
};

class TransactionDatabaseOperationBase : public DatabaseOperationBase {
  enum class InternalState {
    Initial,
    DatabaseWork,
    SendingPreprocess,
    WaitingForContinue,
    SendingResults,
    Completed
  };

  RefPtr<TransactionBase> mTransaction;
  const int64_t mTransactionLoggingSerialNumber;
  InternalState mInternalState;
  const bool mTransactionIsAborted;

 public:
  void AssertIsOnConnectionThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  uint64_t StartOnConnectionPool(const nsID& aBackgroundChildLoggingId,
                                 const nsACString& aDatabaseId,
                                 int64_t aLoggingSerialNumber,
                                 const nsTArray<nsString>& aObjectStoreNames,
                                 bool aIsWriteTransaction);

  void DispatchToConnectionPool();

  TransactionBase* Transaction() const {
    MOZ_ASSERT(mTransaction);

    return mTransaction;
  }

  void NoteContinueReceived();

  // May be overridden by subclasses if they need to perform work on the
  // background thread before being dispatched. Returning false will kill the
  // child actors and prevent dispatch.
  virtual bool Init(TransactionBase* aTransaction);

  // This callback will be called on the background thread before releasing the
  // final reference to this request object. Subclasses may perform any
  // additional cleanup here but must always call the base class implementation.
  virtual void Cleanup();

 protected:
  explicit TransactionDatabaseOperationBase(TransactionBase* aTransaction);

  TransactionDatabaseOperationBase(TransactionBase* aTransaction,
                                   uint64_t aLoggingSerialNumber);

  ~TransactionDatabaseOperationBase() override;

  virtual void RunOnConnectionThread();

  // Must be overridden in subclasses. Called on the target thread to allow the
  // subclass to perform necessary database or file operations. A successful
  // return value will trigger a SendSuccessResult callback on the background
  // thread while a failure value will trigger a SendFailureResult callback.
  virtual nsresult DoDatabaseWork(DatabaseConnection* aConnection) = 0;

  // May be overriden in subclasses. Called on the background thread to decide
  // if the subclass needs to send any preprocess info to the child actor.
  virtual bool HasPreprocessInfo();

  // May be overriden in subclasses. Called on the background thread to allow
  // the subclass to serialize its preprocess info and send it to the child
  // actor. A successful return value will trigger a wait for a
  // NoteContinueReceived callback on the background thread while a failure
  // value will trigger a SendFailureResult callback.
  virtual nsresult SendPreprocessInfo();

  // Must be overridden in subclasses. Called on the background thread to allow
  // the subclass to serialize its results and send them to the child actor. A
  // failed return value will trigger a SendFailureResult callback.
  virtual nsresult SendSuccessResult() = 0;

  // Must be overridden in subclasses. Called on the background thread to allow
  // the subclass to send its failure code. Returning false will cause the
  // transaction to be aborted with aResultCode. Returning true will not cause
  // the transaction to be aborted.
  virtual bool SendFailureResult(nsresult aResultCode) = 0;

 private:
  void SendToConnectionPool();

  void SendPreprocess();

  void SendResults();

  void SendPreprocessInfoOrResults(bool aSendPreprocessInfo);

  // Not to be overridden by subclasses.
  NS_DECL_NSIRUNNABLE
};

class Factory final : public PBackgroundIDBFactoryParent {
  RefPtr<DatabaseLoggingInfo> mLoggingInfo;

#ifdef DEBUG
  bool mActorDestroyed;
#endif

 public:
  static already_AddRefed<Factory> Create(const LoggingInfo& aLoggingInfo);

  DatabaseLoggingInfo* GetLoggingInfo() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo);

    return mLoggingInfo;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Factory)

 private:
  // Only constructed in Create().
  explicit Factory(already_AddRefed<DatabaseLoggingInfo> aLoggingInfo);

  // Reference counted.
  ~Factory() override;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvIncrementLoggingRequestSerialNumber() override;

  PBackgroundIDBFactoryRequestParent* AllocPBackgroundIDBFactoryRequestParent(
      const FactoryRequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBFactoryRequestConstructor(
      PBackgroundIDBFactoryRequestParent* aActor,
      const FactoryRequestParams& aParams) override;

  bool DeallocPBackgroundIDBFactoryRequestParent(
      PBackgroundIDBFactoryRequestParent* aActor) override;

  PBackgroundIDBDatabaseParent* AllocPBackgroundIDBDatabaseParent(
      const DatabaseSpec& aSpec,
      PBackgroundIDBFactoryRequestParent* aRequest) override;

  bool DeallocPBackgroundIDBDatabaseParent(
      PBackgroundIDBDatabaseParent* aActor) override;
};

class WaitForTransactionsHelper final : public Runnable {
  const nsCString mDatabaseId;
  nsCOMPtr<nsIRunnable> mCallback;

  enum class State {
    Initial = 0,
    WaitingForTransactions,
    WaitingForFileHandles,
    Complete
  } mState;

 public:
  WaitForTransactionsHelper(const nsCString& aDatabaseId,
                            nsIRunnable* aCallback)
      : Runnable("dom::indexedDB::WaitForTransactionsHelper"),
        mDatabaseId(aDatabaseId),
        mCallback(aCallback),
        mState(State::Initial) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!aDatabaseId.IsEmpty());
    MOZ_ASSERT(aCallback);
  }

  void WaitForTransactions();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(WaitForTransactionsHelper, Runnable)

 private:
  ~WaitForTransactionsHelper() override {
    MOZ_ASSERT(!mCallback);
    MOZ_ASSERT(mState == State::Complete);
  }

  void MaybeWaitForTransactions();

  void MaybeWaitForFileHandles();

  void CallCallback();

  NS_DECL_NSIRUNNABLE
};

class Database final : public PBackgroundIDBDatabaseParent {
  friend class VersionChangeTransaction;

  class StartTransactionOp;
  class UnmapBlobCallback;

 private:
  RefPtr<Factory> mFactory;
  RefPtr<FullDatabaseMetadata> mMetadata;
  RefPtr<FileManager> mFileManager;
  RefPtr<DirectoryLock> mDirectoryLock;
  nsTHashtable<nsPtrHashKey<TransactionBase>> mTransactions;
  nsTHashtable<nsPtrHashKey<MutableFile>> mMutableFiles;
  nsRefPtrHashtable<nsIDHashKey, FileInfo> mMappedBlobs;
  RefPtr<DatabaseConnection> mConnection;
  const PrincipalInfo mPrincipalInfo;
  const Maybe<ContentParentId> mOptionalContentParentId;
  const nsCString mGroup;
  const nsCString mOrigin;
  const nsCString mId;
  const nsString mFilePath;
  uint32_t mActiveMutableFileCount;
  const uint32_t mTelemetryId;
  const PersistenceType mPersistenceType;
  const bool mFileHandleDisabled;
  const bool mChromeWriteAccessAllowed;
  bool mClosed;
  bool mInvalidated;
  bool mActorWasAlive;
  bool mActorDestroyed;
#ifdef DEBUG
  bool mAllBlobsUnmapped;
#endif

 public:
  // Created by OpenDatabaseOp.
  Database(Factory* aFactory, const PrincipalInfo& aPrincipalInfo,
           const Maybe<ContentParentId>& aOptionalContentParentId,
           const nsACString& aGroup, const nsACString& aOrigin,
           uint32_t aTelemetryId, FullDatabaseMetadata* aMetadata,
           FileManager* aFileManager,
           already_AddRefed<DirectoryLock> aDirectoryLock,
           bool aFileHandleDisabled, bool aChromeWriteAccessAllowed);

  void AssertIsOnConnectionThread() const {
#ifdef DEBUG
    if (mConnection) {
      MOZ_ASSERT(mConnection);
      mConnection->AssertIsOnConnectionThread();
    } else {
      MOZ_ASSERT(!NS_IsMainThread());
      MOZ_ASSERT(!IsOnBackgroundThread());
      MOZ_ASSERT(mInvalidated);
    }
#endif
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Database)

  void Invalidate();

  bool IsOwnedByProcess(ContentParentId aContentParentId) const {
    return mOptionalContentParentId &&
           mOptionalContentParentId.value() == aContentParentId;
  }

  const nsCString& Group() const { return mGroup; }

  const nsCString& Origin() const { return mOrigin; }

  const nsCString& Id() const { return mId; }

  uint32_t TelemetryId() const { return mTelemetryId; }

  PersistenceType Type() const { return mPersistenceType; }

  const nsString& FilePath() const { return mFilePath; }

  FileManager* GetFileManager() const { return mFileManager; }

  FullDatabaseMetadata* Metadata() const {
    MOZ_ASSERT(mMetadata);
    return mMetadata;
  }

  PBackgroundParent* GetBackgroundParent() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!IsActorDestroyed());

    return Manager()->Manager();
  }

  DatabaseLoggingInfo* GetLoggingInfo() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mFactory);

    return mFactory->GetLoggingInfo();
  }

  bool RegisterTransaction(TransactionBase* aTransaction);

  void UnregisterTransaction(TransactionBase* aTransaction);

  bool IsFileHandleDisabled() const { return mFileHandleDisabled; }

  bool RegisterMutableFile(MutableFile* aMutableFile);

  void UnregisterMutableFile(MutableFile* aMutableFile);

  void NoteActiveMutableFile();

  void NoteInactiveMutableFile();

  void SetActorAlive();

  void MapBlob(const IPCBlob& aIPCBlob, FileInfo* aFileInfo);

  bool IsActorAlive() const {
    AssertIsOnBackgroundThread();

    return mActorWasAlive && !mActorDestroyed;
  }

  bool IsActorDestroyed() const {
    AssertIsOnBackgroundThread();

    return mActorWasAlive && mActorDestroyed;
  }

  bool IsClosed() const {
    AssertIsOnBackgroundThread();

    return mClosed;
  }

  bool IsInvalidated() const {
    AssertIsOnBackgroundThread();

    return mInvalidated;
  }

  nsresult EnsureConnection();

  DatabaseConnection* GetConnection() const {
#ifdef DEBUG
    if (mConnection) {
      mConnection->AssertIsOnConnectionThread();
    }
#endif

    return mConnection;
  }

 private:
  // Reference counted.
  ~Database() override {
    MOZ_ASSERT(mClosed);
    MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
  }

  already_AddRefed<FileInfo> GetBlob(const IPCBlob& aID);

  void UnmapBlob(const nsID& aID);

  void UnmapAllBlobs();

  bool CloseInternal();

  void MaybeCloseConnection();

  void ConnectionClosedCallback();

  void CleanupMetadata();

  bool VerifyRequestParams(const DatabaseRequestParams& aParams) const;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundIDBDatabaseFileParent* AllocPBackgroundIDBDatabaseFileParent(
      const IPCBlob& aIPCBlob) override;

  bool DeallocPBackgroundIDBDatabaseFileParent(
      PBackgroundIDBDatabaseFileParent* aActor) override;

  PBackgroundIDBDatabaseRequestParent* AllocPBackgroundIDBDatabaseRequestParent(
      const DatabaseRequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBDatabaseRequestConstructor(
      PBackgroundIDBDatabaseRequestParent* aActor,
      const DatabaseRequestParams& aParams) override;

  bool DeallocPBackgroundIDBDatabaseRequestParent(
      PBackgroundIDBDatabaseRequestParent* aActor) override;

  PBackgroundIDBTransactionParent* AllocPBackgroundIDBTransactionParent(
      const nsTArray<nsString>& aObjectStoreNames, const Mode& aMode) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBTransactionConstructor(
      PBackgroundIDBTransactionParent* aActor,
      InfallibleTArray<nsString>&& aObjectStoreNames,
      const Mode& aMode) override;

  bool DeallocPBackgroundIDBTransactionParent(
      PBackgroundIDBTransactionParent* aActor) override;

  PBackgroundIDBVersionChangeTransactionParent*
  AllocPBackgroundIDBVersionChangeTransactionParent(
      const uint64_t& aCurrentVersion, const uint64_t& aRequestedVersion,
      const int64_t& aNextObjectStoreId, const int64_t& aNextIndexId) override;

  bool DeallocPBackgroundIDBVersionChangeTransactionParent(
      PBackgroundIDBVersionChangeTransactionParent* aActor) override;

  PBackgroundMutableFileParent* AllocPBackgroundMutableFileParent(
      const nsString& aName, const nsString& aType) override;

  bool DeallocPBackgroundMutableFileParent(
      PBackgroundMutableFileParent* aActor) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvBlocked() override;

  mozilla::ipc::IPCResult RecvClose() override;
};

class Database::StartTransactionOp final
    : public TransactionDatabaseOperationBase {
  friend class Database;

 private:
  explicit StartTransactionOp(TransactionBase* aTransaction)
      : TransactionDatabaseOperationBase(aTransaction,
                                         /* aLoggingSerialNumber */ 0) {}

  ~StartTransactionOp() override = default;

  void RunOnConnectionThread() override;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;

  void Cleanup() override;
};

class Database::UnmapBlobCallback final
    : public IPCBlobInputStreamParentCallback {
  RefPtr<Database> mDatabase;

 public:
  explicit UnmapBlobCallback(Database* aDatabase) : mDatabase(aDatabase) {
    AssertIsOnBackgroundThread();
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Database::UnmapBlobCallback, override)

  void ActorDestroyed(const nsID& aID) override {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatabase);

    RefPtr<Database> database;
    mDatabase.swap(database);

    database->UnmapBlob(aID);
  }

 private:
  ~UnmapBlobCallback() = default;
};

/**
 * In coordination with IDBDatabase's mFileActors weak-map on the child side, a
 * long-lived mapping from a child process's live Blobs to their corresponding
 * FileInfo in our owning database.  Assists in avoiding redundant IPC traffic
 * and disk storage.  This includes both:
 * - Blobs retrieved from this database and sent to the child that do not need
 *   to be written to disk because they already exist on disk in this database's
 *   files directory.
 * - Blobs retrieved from other databases or from anywhere else that will need
 *   to be written to this database's files directory.  In this case we will
 *   hold a reference to its BlobImpl in mBlobImpl until we have successfully
 *   written the Blob to disk.
 *
 * Relevant Blob context: Blobs sent from the parent process to child processes
 * are automatically linked back to their source BlobImpl when the child process
 * references the Blob via IPC. This is done using the internal IPCBlob
 * inputStream actor ID to FileInfo mapping. However, when getting an actor
 * in the child process for sending an in-child-created Blob to the parent
 * process, there is (currently) no Blob machinery to automatically establish
 * and reuse a long-lived Actor.  As a result, without IDB's weak-map
 * cleverness, a memory-backed Blob repeatedly sent from the child to the parent
 * would appear as a different Blob each time, requiring the Blob data to be
 * sent over IPC each time as well as potentially needing to be written to disk
 * each time.
 *
 * This object remains alive as long as there is an active child actor or an
 * ObjectStoreAddOrPutRequestOp::StoredFileInfo for a queued or active add/put
 * op is holding a reference to us.
 */
class DatabaseFile final : public PBackgroundIDBDatabaseFileParent {
  friend class Database;

  // mBlobImpl's ownership lifecycle:
  // - Initialized on the background thread at creation time.  Then
  //   responsibility is handed off to the connection thread.
  // - Checked and used by the connection thread to generate a stream to write
  //   the blob to disk by an add/put operation.
  // - Cleared on the connection thread once the file has successfully been
  //   written to disk.
  RefPtr<BlobImpl> mBlobImpl;
  RefPtr<FileInfo> mFileInfo;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::DatabaseFile);

  FileInfo* GetFileInfo() const {
    AssertIsOnBackgroundThread();

    return mFileInfo;
  }

  /**
   * If mBlobImpl is non-null (implying the contents of this file have not yet
   * been written to disk), then return an input stream. Otherwise, if mBlobImpl
   * is null (because the contents have been written to disk), returns null.
   */
  already_AddRefed<nsIInputStream> GetInputStream(ErrorResult& rv) const;

  /**
   * To be called upon successful copying of the stream GetInputStream()
   * returned so that we won't try and redundantly write the file to disk in the
   * future.  This is a separate step from GetInputStream() because
   * the write could fail due to quota errors that happen now but that might
   * not happen in a future attempt.
   */
  void WriteSucceededClearBlobImpl() {
    MOZ_ASSERT(!IsOnBackgroundThread());

    mBlobImpl = nullptr;
  }

 private:
  // Called when sending to the child.
  explicit DatabaseFile(FileInfo* aFileInfo) : mFileInfo(aFileInfo) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aFileInfo);
  }

  // Called when receiving from the child.
  DatabaseFile(BlobImpl* aBlobImpl, FileInfo* aFileInfo)
      : mBlobImpl(aBlobImpl), mFileInfo(aFileInfo) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aBlobImpl);
    MOZ_ASSERT(aFileInfo);
  }

  ~DatabaseFile() override = default;

  void ActorDestroy(ActorDestroyReason aWhy) override {
    AssertIsOnBackgroundThread();
  }
};

already_AddRefed<nsIInputStream> DatabaseFile::GetInputStream(
    ErrorResult& rv) const {
  // We should only be called from our DB connection thread, not the background
  // thread.
  MOZ_ASSERT(!IsOnBackgroundThread());

  if (!mBlobImpl) {
    return nullptr;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  mBlobImpl->CreateInputStream(getter_AddRefs(inputStream), rv);
  if (rv.Failed()) {
    return nullptr;
  }

  return inputStream.forget();
}

class TransactionBase {
  friend class Cursor;

  class CommitOp;

 protected:
  typedef IDBTransaction::Mode Mode;

 private:
  RefPtr<Database> mDatabase;
  nsTArray<RefPtr<FullObjectStoreMetadata>>
      mModifiedAutoIncrementObjectStoreMetadataArray;
  uint64_t mTransactionId;
  const nsCString mDatabaseId;
  const int64_t mLoggingSerialNumber;
  uint64_t mActiveRequestCount;
  Atomic<bool> mInvalidatedOnAnyThread;
  const Mode mMode;
  bool mHasBeenActive;
  bool mHasBeenActiveOnConnectionThread;
  bool mActorDestroyed;
  bool mInvalidated;

 protected:
  nsresult mResultCode;
  bool mCommitOrAbortReceived;
  bool mCommittedOrAborted;
  bool mForceAborted;

 public:
  void AssertIsOnConnectionThread() const {
    MOZ_ASSERT(mDatabase);
    mDatabase->AssertIsOnConnectionThread();
  }

  bool IsActorDestroyed() const {
    AssertIsOnBackgroundThread();

    return mActorDestroyed;
  }

  // Must be called on the background thread.
  bool IsInvalidated() const {
    MOZ_ASSERT(IsOnBackgroundThread(), "Use IsInvalidatedOnAnyThread()");
    MOZ_ASSERT_IF(mInvalidated, NS_FAILED(mResultCode));

    return mInvalidated;
  }

  // May be called on any thread, but is more expensive than IsInvalidated().
  bool IsInvalidatedOnAnyThread() const { return mInvalidatedOnAnyThread; }

  void SetActive(uint64_t aTransactionId) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aTransactionId);

    mTransactionId = aTransactionId;
    mHasBeenActive = true;
  }

  void SetActiveOnConnectionThread() {
    AssertIsOnConnectionThread();
    mHasBeenActiveOnConnectionThread = true;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(
      mozilla::dom::indexedDB::TransactionBase)

  void Abort(nsresult aResultCode, bool aForce);

  uint64_t TransactionId() const { return mTransactionId; }

  const nsCString& DatabaseId() const { return mDatabaseId; }

  Mode GetMode() const { return mMode; }

  Database* GetDatabase() const {
    MOZ_ASSERT(mDatabase);

    return mDatabase;
  }

  DatabaseLoggingInfo* GetLoggingInfo() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatabase);

    return mDatabase->GetLoggingInfo();
  }

  int64_t LoggingSerialNumber() const { return mLoggingSerialNumber; }

  bool IsAborted() const {
    AssertIsOnBackgroundThread();

    return NS_FAILED(mResultCode);
  }

  already_AddRefed<FullObjectStoreMetadata> GetMetadataForObjectStoreId(
      int64_t aObjectStoreId) const;

  already_AddRefed<FullIndexMetadata> GetMetadataForIndexId(
      FullObjectStoreMetadata* const aObjectStoreMetadata,
      int64_t aIndexId) const;

  PBackgroundParent* GetBackgroundParent() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!IsActorDestroyed());

    return GetDatabase()->GetBackgroundParent();
  }

  void NoteModifiedAutoIncrementObjectStore(FullObjectStoreMetadata* aMetadata);

  void ForgetModifiedAutoIncrementObjectStore(
      FullObjectStoreMetadata* aMetadata);

  void NoteActiveRequest();

  void NoteFinishedRequest();

  void Invalidate();

 protected:
  TransactionBase(Database* aDatabase, Mode aMode);

  virtual ~TransactionBase();

  void NoteActorDestroyed() {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!mActorDestroyed);

    mActorDestroyed = true;
  }

#ifdef DEBUG
  // Only called by VersionChangeTransaction.
  void FakeActorDestroyed() { mActorDestroyed = true; }
#endif

  bool RecvCommit();

  bool RecvAbort(nsresult aResultCode);

  void MaybeCommitOrAbort() {
    AssertIsOnBackgroundThread();

    // If we've already committed or aborted then there's nothing else to do.
    if (mCommittedOrAborted) {
      return;
    }

    // If there are active requests then we have to wait for those requests to
    // complete (see NoteFinishedRequest).
    if (mActiveRequestCount) {
      return;
    }

    // If we haven't yet received a commit or abort message then there could be
    // additional requests coming so we should wait unless we're being forced to
    // abort.
    if (!mCommitOrAbortReceived && !mForceAborted) {
      return;
    }

    CommitOrAbort();
  }

  PBackgroundIDBRequestParent* AllocRequest(const RequestParams& aParams,
                                            bool aTrustParams);

  bool StartRequest(PBackgroundIDBRequestParent* aActor);

  bool DeallocRequest(PBackgroundIDBRequestParent* aActor);

  PBackgroundIDBCursorParent* AllocCursor(const OpenCursorParams& aParams,
                                          bool aTrustParams);

  bool StartCursor(PBackgroundIDBCursorParent* aActor,
                   const OpenCursorParams& aParams);

  bool DeallocCursor(PBackgroundIDBCursorParent* aActor);

  virtual void UpdateMetadata(nsresult aResult) {}

  virtual void SendCompleteNotification(nsresult aResult) = 0;

 private:
  bool VerifyRequestParams(const RequestParams& aParams) const;

  bool VerifyRequestParams(const SerializedKeyRange& aKeyRange) const;

  bool VerifyRequestParams(const ObjectStoreAddPutParams& aParams) const;

  bool VerifyRequestParams(const Maybe<SerializedKeyRange>& aKeyRange) const;

  void CommitOrAbort();
};

class TransactionBase::CommitOp final : public DatabaseOperationBase,
                                        public ConnectionPool::FinishCallback {
  friend class TransactionBase;

  RefPtr<TransactionBase> mTransaction;
  nsresult mResultCode;

 private:
  CommitOp(TransactionBase* aTransaction, nsresult aResultCode);

  ~CommitOp() override = default;

  // Writes new autoIncrement counts to database.
  nsresult WriteAutoIncrementCounts();

  // Updates counts after a database activity has finished.
  void CommitOrRollbackAutoIncrementCounts();

  void AssertForeignKeyConsistency(DatabaseConnection* aConnection)
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  NS_DECL_NSIRUNNABLE

  void TransactionFinishedBeforeUnblock() override;

  void TransactionFinishedAfterUnblock() override;

 public:
  // We need to declare all of nsISupports, because FinishCallback has
  // a pure-virtual nsISupports declaration.
  NS_DECL_ISUPPORTS_INHERITED
};

class NormalTransaction final : public TransactionBase,
                                public PBackgroundIDBTransactionParent {
  friend class Database;

  nsTArray<RefPtr<FullObjectStoreMetadata>> mObjectStores;

 private:
  // This constructor is only called by Database.
  NormalTransaction(Database* aDatabase, TransactionBase::Mode aMode,
                    nsTArray<RefPtr<FullObjectStoreMetadata>>& aObjectStores);

  // Reference counted.
  ~NormalTransaction() override = default;

  bool IsSameProcessActor();

  // Only called by TransactionBase.
  void SendCompleteNotification(nsresult aResult) override;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvCommit() override;

  mozilla::ipc::IPCResult RecvAbort(const nsresult& aResultCode) override;

  PBackgroundIDBRequestParent* AllocPBackgroundIDBRequestParent(
      const RequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBRequestConstructor(
      PBackgroundIDBRequestParent* aActor,
      const RequestParams& aParams) override;

  bool DeallocPBackgroundIDBRequestParent(
      PBackgroundIDBRequestParent* aActor) override;

  PBackgroundIDBCursorParent* AllocPBackgroundIDBCursorParent(
      const OpenCursorParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBCursorConstructor(
      PBackgroundIDBCursorParent* aActor,
      const OpenCursorParams& aParams) override;

  bool DeallocPBackgroundIDBCursorParent(
      PBackgroundIDBCursorParent* aActor) override;
};

class VersionChangeTransaction final
    : public TransactionBase,
      public PBackgroundIDBVersionChangeTransactionParent {
  friend class OpenDatabaseOp;

  RefPtr<OpenDatabaseOp> mOpenDatabaseOp;
  RefPtr<FullDatabaseMetadata> mOldMetadata;

  bool mActorWasAlive;

 private:
  // Only called by OpenDatabaseOp.
  explicit VersionChangeTransaction(OpenDatabaseOp* aOpenDatabaseOp);

  // Reference counted.
  ~VersionChangeTransaction() override;

  bool IsSameProcessActor();

  // Only called by OpenDatabaseOp.
  bool CopyDatabaseMetadata();

  void SetActorAlive();

  // Only called by TransactionBase.
  void UpdateMetadata(nsresult aResult) override;

  // Only called by TransactionBase.
  void SendCompleteNotification(nsresult aResult) override;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvCommit() override;

  mozilla::ipc::IPCResult RecvAbort(const nsresult& aResultCode) override;

  mozilla::ipc::IPCResult RecvCreateObjectStore(
      const ObjectStoreMetadata& aMetadata) override;

  mozilla::ipc::IPCResult RecvDeleteObjectStore(
      const int64_t& aObjectStoreId) override;

  mozilla::ipc::IPCResult RecvRenameObjectStore(const int64_t& aObjectStoreId,
                                                const nsString& aName) override;

  mozilla::ipc::IPCResult RecvCreateIndex(
      const int64_t& aObjectStoreId, const IndexMetadata& aMetadata) override;

  mozilla::ipc::IPCResult RecvDeleteIndex(const int64_t& aObjectStoreId,
                                          const int64_t& aIndexId) override;

  mozilla::ipc::IPCResult RecvRenameIndex(const int64_t& aObjectStoreId,
                                          const int64_t& aIndexId,
                                          const nsString& aName) override;

  PBackgroundIDBRequestParent* AllocPBackgroundIDBRequestParent(
      const RequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBRequestConstructor(
      PBackgroundIDBRequestParent* aActor,
      const RequestParams& aParams) override;

  bool DeallocPBackgroundIDBRequestParent(
      PBackgroundIDBRequestParent* aActor) override;

  PBackgroundIDBCursorParent* AllocPBackgroundIDBCursorParent(
      const OpenCursorParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBCursorConstructor(
      PBackgroundIDBCursorParent* aActor,
      const OpenCursorParams& aParams) override;

  bool DeallocPBackgroundIDBCursorParent(
      PBackgroundIDBCursorParent* aActor) override;
};

class MutableFile : public BackgroundMutableFileParentBase {
  RefPtr<Database> mDatabase;
  RefPtr<FileInfo> mFileInfo;

 public:
  static already_AddRefed<MutableFile> Create(nsIFile* aFile,
                                              Database* aDatabase,
                                              FileInfo* aFileInfo);

  Database* GetDatabase() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatabase);

    return mDatabase;
  }

  FileInfo* GetFileInfo() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mFileInfo);

    return mFileInfo;
  }

  void NoteActiveState() override;

  void NoteInactiveState() override;

  PBackgroundParent* GetBackgroundParent() const override;

  already_AddRefed<nsISupports> CreateStream(bool aReadOnly) override;

  already_AddRefed<BlobImpl> CreateBlobImpl() override;

 private:
  MutableFile(nsIFile* aFile, Database* aDatabase, FileInfo* aFileInfo);

  ~MutableFile() override;

  PBackgroundFileHandleParent* AllocPBackgroundFileHandleParent(
      const FileMode& aMode) final;

  mozilla::ipc::IPCResult RecvPBackgroundFileHandleConstructor(
      PBackgroundFileHandleParent* aActor, const FileMode& aMode) final;

  mozilla::ipc::IPCResult RecvGetFileId(int64_t* aFileId) override;
};

class FactoryOp : public DatabaseOperationBase,
                  public OpenDirectoryListener,
                  public PBackgroundIDBFactoryRequestParent {
 public:
  struct MaybeBlockedDatabaseInfo;

 protected:
  enum class State {
    // Just created on the PBackground thread, dispatched to the main thread.
    // Next step is either SendingResults if permission is denied,
    // PermissionChallenge if the permission is unknown, or FinishOpen
    // if permission is granted.
    Initial,

    // Sending a permission challenge message to the child on the PBackground
    // thread. Next step is PermissionRetry.
    PermissionChallenge,

    // Retrying permission check after a challenge on the main thread. Next step
    // is either SendingResults if permission is denied or FinishOpen
    // if permission is granted.
    PermissionRetry,

    // Opening directory or initializing quota manager on the PBackground
    // thread. Next step is either DirectoryOpenPending if quota manager is
    // already initialized or QuotaManagerPending if quota manager needs to be
    // initialized.
    FinishOpen,

    // Waiting for quota manager initialization to complete on the PBackground
    // thread. Next step is either SendingResults if initialization failed or
    // DirectoryOpenPending if initialization succeeded.
    QuotaManagerPending,

    // Waiting for directory open allowed on the PBackground thread. The next
    // step is either SendingResults if directory lock failed to acquire, or
    // DatabaseOpenPending if directory lock is acquired.
    DirectoryOpenPending,

    // Waiting for database open allowed on the PBackground thread. The next
    // step is DatabaseWorkOpen.
    DatabaseOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. Its next step is
    // either BeginVersionChange if the requested version doesn't match the
    // existing database version or SendingResults if the versions match.
    DatabaseWorkOpen,

    // Starting a version change transaction or deleting a database on the
    // PBackground thread. We need to notify other databases that a version
    // change is about to happen, and maybe tell the request that a version
    // change has been blocked. If databases are notified then the next step is
    // WaitingForOtherDatabasesToClose. Otherwise the next step is
    // WaitingForTransactionsToComplete.
    BeginVersionChange,

    // Waiting for other databases to close on the PBackground thread. This
    // state may persist until all databases are closed. The next state is
    // WaitingForTransactionsToComplete.
    WaitingForOtherDatabasesToClose,

    // Waiting for all transactions that could interfere with this operation to
    // complete on the PBackground thread. Next state is
    // DatabaseWorkVersionChange.
    WaitingForTransactionsToComplete,

    // Waiting to do/doing work on the "work thread". This involves waiting for
    // the VersionChangeOp (OpenDatabaseOp and DeleteDatabaseOp each have a
    // different implementation) to do its work. Eventually the state will
    // transition to SendingResults.
    DatabaseWorkVersionChange,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  // Must be released on the background thread!
  RefPtr<Factory> mFactory;

  // Must be released on the main thread!
  RefPtr<ContentParent> mContentParent;

  // Must be released on the main thread!
  RefPtr<DirectoryLock> mDirectoryLock;

  RefPtr<FactoryOp> mDelayedOp;
  nsTArray<MaybeBlockedDatabaseInfo> mMaybeBlockedDatabases;

  const CommonFactoryRequestParams mCommonParams;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;
  nsCString mDatabaseId;
  nsString mDatabaseFilePath;
  State mState;
  bool mWaitingForPermissionRetry;
  bool mEnforcingQuota;
  const bool mDeleting;
  bool mChromeWriteAccessAllowed;
  bool mFileHandleDisabled;

 public:
  void NoteDatabaseBlocked(Database* aDatabase);

  virtual void NoteDatabaseClosed(Database* aDatabase) = 0;

#ifdef DEBUG
  bool HasBlockedDatabases() const { return !mMaybeBlockedDatabases.IsEmpty(); }
#endif

  bool DatabaseFilePathIsKnown() const {
    AssertIsOnOwningThread();

    return !mDatabaseFilePath.IsEmpty();
  }

  const nsString& DatabaseFilePath() const {
    AssertIsOnOwningThread();
    MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());

    return mDatabaseFilePath;
  }

 protected:
  FactoryOp(Factory* aFactory, already_AddRefed<ContentParent> aContentParent,
            const CommonFactoryRequestParams& aCommonParams, bool aDeleting);

  ~FactoryOp() override {
    // Normally this would be out-of-line since it is a virtual function but
    // MSVC 2010 fails to link for some reason if it is not inlined here...
    MOZ_ASSERT_IF(OperationMayProceed(),
                  mState == State::Initial || mState == State::Completed);
  }

  nsresult Open();

  nsresult ChallengePermission();

  void PermissionRetry();

  nsresult RetryCheckPermission();

  nsresult DirectoryOpen();

  nsresult SendToIOThread();

  void WaitForTransactions();

  void CleanupMetadata();

  void FinishSendResults();

  nsresult SendVersionChangeMessages(DatabaseActorInfo* aDatabaseActorInfo,
                                     Database* aOpeningDatabase,
                                     uint64_t aOldVersion,
                                     const Maybe<uint64_t>& aNewVersion);

  // Methods that subclasses must implement.
  virtual nsresult DatabaseOpen() = 0;

  virtual nsresult DoDatabaseWork() = 0;

  virtual nsresult BeginVersionChange() = 0;

  virtual nsresult DispatchToWorkThread() = 0;

  // Should only be called by Run().
  virtual void SendResults() = 0;

  // We need to declare refcounting unconditionally, because
  // OpenDirectoryListener has pure-virtual refcounting.
  NS_DECL_ISUPPORTS_INHERITED

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // OpenDirectoryListener overrides.
  void DirectoryLockAcquired(DirectoryLock* aLock) override;

  void DirectoryLockFailed() override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvPermissionRetry() final;

  virtual void SendBlockedNotification() = 0;

 private:
  nsresult CheckPermission(ContentParent* aContentParent,
                           PermissionRequestBase::PermissionValue* aPermission);

  static bool CheckAtLeastOneAppHasPermission(
      ContentParent* aContentParent, const nsACString& aPermissionString);

  nsresult FinishOpen();

  nsresult QuotaManagerOpen();

  nsresult OpenDirectory();

  // Test whether this FactoryOp needs to wait for the given op.
  bool MustWaitFor(const FactoryOp& aExistingOp);
};

struct FactoryOp::MaybeBlockedDatabaseInfo final {
  RefPtr<Database> mDatabase;
  bool mBlocked;

  MOZ_IMPLICIT MaybeBlockedDatabaseInfo(Database* aDatabase)
      : mDatabase(aDatabase), mBlocked(false) {
    MOZ_ASSERT(aDatabase);

    MOZ_COUNT_CTOR(FactoryOp::MaybeBlockedDatabaseInfo);
  }

  ~MaybeBlockedDatabaseInfo() {
    MOZ_COUNT_DTOR(FactoryOp::MaybeBlockedDatabaseInfo);
  }

  bool operator==(const MaybeBlockedDatabaseInfo& aOther) const {
    return mDatabase == aOther.mDatabase;
  }

  Database* operator->() MOZ_NO_ADDREF_RELEASE_ON_RETURN { return mDatabase; }
};

class OpenDatabaseOp final : public FactoryOp {
  friend class Database;
  friend class VersionChangeTransaction;

  class VersionChangeOp;

  Maybe<ContentParentId> mOptionalContentParentId;

  RefPtr<FullDatabaseMetadata> mMetadata;

  uint64_t mRequestedVersion;
  RefPtr<FileManager> mFileManager;

  RefPtr<Database> mDatabase;
  RefPtr<VersionChangeTransaction> mVersionChangeTransaction;

  // This is only set while a VersionChangeOp is live. It holds a strong
  // reference to its OpenDatabaseOp object so this is a weak pointer to avoid
  // cycles.
  VersionChangeOp* mVersionChangeOp;

  uint32_t mTelemetryId;

 public:
  OpenDatabaseOp(Factory* aFactory,
                 already_AddRefed<ContentParent> aContentParent,
                 const CommonFactoryRequestParams& aParams);

 private:
  ~OpenDatabaseOp() override { MOZ_ASSERT(!mVersionChangeOp); }

  nsresult LoadDatabaseInformation(mozIStorageConnection* aConnection);

  nsresult SendUpgradeNeeded();

  void EnsureDatabaseActor();

  nsresult EnsureDatabaseActorIsAlive();

  void MetadataToSpec(DatabaseSpec& aSpec);

  void AssertMetadataConsistency(const FullDatabaseMetadata* aMetadata)
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void ConnectionClosedCallback();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult DatabaseOpen() override;

  nsresult DoDatabaseWork() override;

  nsresult BeginVersionChange() override;

  void NoteDatabaseClosed(Database* aDatabase) override;

  void SendBlockedNotification() override;

  nsresult DispatchToWorkThread() override;

  void SendResults() override;

  static nsresult UpdateLocaleAwareIndex(mozIStorageConnection* aConnection,
                                         const IndexMetadata& aIndexMetadata,
                                         const nsCString& aLocale);
};

class OpenDatabaseOp::VersionChangeOp final
    : public TransactionDatabaseOperationBase {
  friend class OpenDatabaseOp;

  RefPtr<OpenDatabaseOp> mOpenDatabaseOp;
  const uint64_t mRequestedVersion;
  uint64_t mPreviousVersion;

 private:
  explicit VersionChangeOp(OpenDatabaseOp* aOpenDatabaseOp)
      : TransactionDatabaseOperationBase(
            aOpenDatabaseOp->mVersionChangeTransaction,
            aOpenDatabaseOp->LoggingSerialNumber()),
        mOpenDatabaseOp(aOpenDatabaseOp),
        mRequestedVersion(aOpenDatabaseOp->mRequestedVersion),
        mPreviousVersion(
            aOpenDatabaseOp->mMetadata->mCommonMetadata.version()) {
    MOZ_ASSERT(aOpenDatabaseOp);
    MOZ_ASSERT(mRequestedVersion);
  }

  ~VersionChangeOp() override { MOZ_ASSERT(!mOpenDatabaseOp); }

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;

  void Cleanup() override;
};

class DeleteDatabaseOp final : public FactoryOp {
  class VersionChangeOp;

  nsString mDatabaseDirectoryPath;
  nsString mDatabaseFilenameBase;
  uint64_t mPreviousVersion;

 public:
  DeleteDatabaseOp(Factory* aFactory,
                   already_AddRefed<ContentParent> aContentParent,
                   const CommonFactoryRequestParams& aParams)
      : FactoryOp(aFactory, std::move(aContentParent), aParams,
                  /* aDeleting */ true),
        mPreviousVersion(0) {}

 private:
  ~DeleteDatabaseOp() override = default;

  void LoadPreviousVersion(nsIFile* aDatabaseFile);

  nsresult DatabaseOpen() override;

  nsresult DoDatabaseWork() override;

  nsresult BeginVersionChange() override;

  void NoteDatabaseClosed(Database* aDatabase) override;

  void SendBlockedNotification() override;

  nsresult DispatchToWorkThread() override;

  void SendResults() override;
};

class DeleteDatabaseOp::VersionChangeOp final : public DatabaseOperationBase {
  friend class DeleteDatabaseOp;

  RefPtr<DeleteDatabaseOp> mDeleteDatabaseOp;

 private:
  explicit VersionChangeOp(DeleteDatabaseOp* aDeleteDatabaseOp)
      : DatabaseOperationBase(aDeleteDatabaseOp->BackgroundChildLoggingId(),
                              aDeleteDatabaseOp->LoggingSerialNumber()),
        mDeleteDatabaseOp(aDeleteDatabaseOp) {
    MOZ_ASSERT(aDeleteDatabaseOp);
    MOZ_ASSERT(!aDeleteDatabaseOp->mDatabaseDirectoryPath.IsEmpty());
  }

  ~VersionChangeOp() override = default;

  nsresult RunOnIOThread();

  void RunOnOwningThread();

  NS_DECL_NSIRUNNABLE
};

class DatabaseOp : public DatabaseOperationBase,
                   public PBackgroundIDBDatabaseRequestParent {
 protected:
  RefPtr<Database> mDatabase;

  enum class State {
    // Just created on the PBackground thread, dispatched to the main thread.
    // Next step is DatabaseWork.
    Initial,

    // Waiting to do/doing work on the QuotaManager IO thread. Next step is
    // SendingResults.
    DatabaseWork,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  State mState;

 public:
  void RunImmediately() {
    MOZ_ASSERT(mState == State::Initial);

    Unused << this->Run();
  }

 protected:
  DatabaseOp(Database* aDatabase);

  ~DatabaseOp() override {
    MOZ_ASSERT_IF(OperationMayProceed(),
                  mState == State::Initial || mState == State::Completed);
  }

  nsresult SendToIOThread();

  // Methods that subclasses must implement.
  virtual nsresult DoDatabaseWork() = 0;

  virtual void SendResults() = 0;

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

class CreateFileOp final : public DatabaseOp {
  const CreateFileParams mParams;

  RefPtr<FileInfo> mFileInfo;

 public:
  CreateFileOp(Database* aDatabase, const DatabaseRequestParams& aParams);

 private:
  ~CreateFileOp() override = default;

  nsresult CreateMutableFile(MutableFile** aMutableFile);

  nsresult DoDatabaseWork() override;

  void SendResults() override;
};

class VersionChangeTransactionOp : public TransactionDatabaseOperationBase {
 public:
  void Cleanup() override;

 protected:
  explicit VersionChangeTransactionOp(VersionChangeTransaction* aTransaction)
      : TransactionDatabaseOperationBase(aTransaction) {}

  ~VersionChangeTransactionOp() override = default;

 private:
  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;
};

class CreateObjectStoreOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const ObjectStoreMetadata mMetadata;

 private:
  // Only created by VersionChangeTransaction.
  CreateObjectStoreOp(VersionChangeTransaction* aTransaction,
                      const ObjectStoreMetadata& aMetadata)
      : VersionChangeTransactionOp(aTransaction), mMetadata(aMetadata) {
    MOZ_ASSERT(aMetadata.id());
  }

  ~CreateObjectStoreOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class DeleteObjectStoreOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const RefPtr<FullObjectStoreMetadata> mMetadata;
  const bool mIsLastObjectStore;

 private:
  // Only created by VersionChangeTransaction.
  DeleteObjectStoreOp(VersionChangeTransaction* aTransaction,
                      FullObjectStoreMetadata* const aMetadata,
                      const bool aIsLastObjectStore)
      : VersionChangeTransactionOp(aTransaction),
        mMetadata(aMetadata),
        mIsLastObjectStore(aIsLastObjectStore) {
    MOZ_ASSERT(aMetadata->mCommonMetadata.id());
  }

  ~DeleteObjectStoreOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class RenameObjectStoreOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const int64_t mId;
  const nsString mNewName;

 private:
  // Only created by VersionChangeTransaction.
  RenameObjectStoreOp(VersionChangeTransaction* aTransaction,
                      FullObjectStoreMetadata* const aMetadata)
      : VersionChangeTransactionOp(aTransaction),
        mId(aMetadata->mCommonMetadata.id()),
        mNewName(aMetadata->mCommonMetadata.name()) {
    MOZ_ASSERT(mId);
  }

  ~RenameObjectStoreOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class CreateIndexOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  class UpdateIndexDataValuesFunction;

  const IndexMetadata mMetadata;
  Maybe<UniqueIndexTable> mMaybeUniqueIndexTable;
  RefPtr<FileManager> mFileManager;
  const nsCString mDatabaseId;
  const uint64_t mObjectStoreId;

 private:
  // Only created by VersionChangeTransaction.
  CreateIndexOp(VersionChangeTransaction* aTransaction,
                const int64_t aObjectStoreId, const IndexMetadata& aMetadata);

  ~CreateIndexOp() override = default;

  nsresult InsertDataFromObjectStore(DatabaseConnection* aConnection);

  nsresult InsertDataFromObjectStoreInternal(DatabaseConnection* aConnection);

  bool Init(TransactionBase* aTransaction) override;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class CreateIndexOp::UpdateIndexDataValuesFunction final
    : public mozIStorageFunction {
  RefPtr<CreateIndexOp> mOp;
  RefPtr<DatabaseConnection> mConnection;

 public:
  UpdateIndexDataValuesFunction(CreateIndexOp* aOp,
                                DatabaseConnection* aConnection)
      : mOp(aOp), mConnection(aConnection) {
    MOZ_ASSERT(aOp);
    MOZ_ASSERT(aConnection);
    aConnection->AssertIsOnConnectionThread();
  }

  NS_DECL_ISUPPORTS

 private:
  ~UpdateIndexDataValuesFunction() = default;

  NS_DECL_MOZISTORAGEFUNCTION
};

class DeleteIndexOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const int64_t mObjectStoreId;
  const int64_t mIndexId;
  const bool mUnique;
  const bool mIsLastIndex;

 private:
  // Only created by VersionChangeTransaction.
  DeleteIndexOp(VersionChangeTransaction* aTransaction,
                const int64_t aObjectStoreId, const int64_t aIndexId,
                const bool aUnique, const bool aIsLastIndex);

  ~DeleteIndexOp() override = default;

  nsresult RemoveReferencesToIndex(DatabaseConnection* aConnection,
                                   const Key& aObjectDataKey,
                                   nsTArray<IndexDataValue>& aIndexValues);

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class RenameIndexOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const int64_t mObjectStoreId;
  const int64_t mIndexId;
  const nsString mNewName;

 private:
  // Only created by VersionChangeTransaction.
  RenameIndexOp(VersionChangeTransaction* aTransaction,
                FullIndexMetadata* const aMetadata, int64_t aObjectStoreId)
      : VersionChangeTransactionOp(aTransaction),
        mObjectStoreId(aObjectStoreId),
        mIndexId(aMetadata->mCommonMetadata.id()),
        mNewName(aMetadata->mCommonMetadata.name()) {
    MOZ_ASSERT(mIndexId);
  }

  ~RenameIndexOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class NormalTransactionOp : public TransactionDatabaseOperationBase,
                            public PBackgroundIDBRequestParent {
#ifdef DEBUG
  bool mResponseSent;
#endif

 public:
  void Cleanup() override;

 protected:
  explicit NormalTransactionOp(TransactionBase* aTransaction)
      : TransactionDatabaseOperationBase(aTransaction)
#ifdef DEBUG
        ,
        mResponseSent(false)
#endif
  {
  }

  ~NormalTransactionOp() override = default;

  // An overload of DatabaseOperationBase's function that can avoid doing extra
  // work on non-versionchange transactions.
  static nsresult ObjectStoreHasIndexes(NormalTransactionOp* aOp,
                                        DatabaseConnection* aConnection,
                                        const int64_t aObjectStoreId,
                                        const bool aMayHaveIndexes,
                                        bool* aHasIndexes);

  virtual nsresult GetPreprocessParams(PreprocessParams& aParams);

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(RequestResponse& aResponse,
                           size_t* aResponseSize) = 0;

 private:
  nsresult SendPreprocessInfo() override;

  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvContinue(
      const PreprocessResponse& aResponse) final;
};

class ObjectStoreAddOrPutRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  struct StoredFileInfo;
  class SCInputStream;

  const ObjectStoreAddPutParams mParams;
  Maybe<UniqueIndexTable> mUniqueIndexTable;

  // This must be non-const so that we can update the mNextAutoIncrementId field
  // if we are modifying an autoIncrement objectStore.
  RefPtr<FullObjectStoreMetadata> mMetadata;

  FallibleTArray<StoredFileInfo> mStoredFileInfos;

  Key mResponse;
  const nsCString mGroup;
  const nsCString mOrigin;
  const PersistenceType mPersistenceType;
  const bool mOverwrite;
  bool mObjectStoreMayHaveIndexes;
  bool mDataOverThreshold;

 private:
  // Only created by TransactionBase.
  ObjectStoreAddOrPutRequestOp(TransactionBase* aTransaction,
                               const RequestParams& aParams);

  ~ObjectStoreAddOrPutRequestOp() override = default;

  nsresult RemoveOldIndexDataValues(DatabaseConnection* aConnection);

  bool Init(TransactionBase* aTransaction) override;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;

  void Cleanup() override;
};

struct ObjectStoreAddOrPutRequestOp::StoredFileInfo final {
  RefPtr<DatabaseFile> mFileActor;
  RefPtr<FileInfo> mFileInfo;
  // A non-Blob-backed inputstream to write to disk.  If null, mFileActor may
  // still have a stream for us to write.
  nsCOMPtr<nsIInputStream> mInputStream;
  StructuredCloneFile::FileType mType;

  StoredFileInfo() : mType(StructuredCloneFile::eBlob) {
    AssertIsOnBackgroundThread();

    MOZ_COUNT_CTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
  }

  ~StoredFileInfo() {
    AssertIsOnBackgroundThread();

    MOZ_COUNT_DTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
  }

  void Serialize(nsString& aText) {
    MOZ_ASSERT(mFileInfo);

    const int64_t id = mFileInfo->Id();

    switch (mType) {
      case StructuredCloneFile::eBlob:
        aText.AppendInt(id);
        break;

      case StructuredCloneFile::eMutableFile:
        aText.AppendInt(-id);
        break;

      case StructuredCloneFile::eStructuredClone:
        aText.Append('.');
        aText.AppendInt(id);
        break;

      case StructuredCloneFile::eWasmBytecode:
        aText.Append('/');
        aText.AppendInt(id);
        break;

      case StructuredCloneFile::eWasmCompiled:
        aText.Append('\\');
        aText.AppendInt(id);
        break;

      default:
        MOZ_CRASH("Should never get here!");
    }
  }
};

class ObjectStoreAddOrPutRequestOp::SCInputStream final
    : public nsIInputStream {
  const JSStructuredCloneData& mData;
  JSStructuredCloneData::Iterator mIter;

 public:
  explicit SCInputStream(const JSStructuredCloneData& aData)
      : mData(aData), mIter(aData.Start()) {}

 private:
  virtual ~SCInputStream() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
};

class ObjectStoreGetRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const uint32_t mObjectStoreId;
  RefPtr<Database> mDatabase;
  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  AutoTArray<StructuredCloneReadInfo, 1> mResponse;
  PBackgroundParent* mBackgroundParent;
  uint32_t mPreprocessInfoCount;
  const uint32_t mLimit;
  const bool mGetAll;

 private:
  // Only created by TransactionBase.
  ObjectStoreGetRequestOp(TransactionBase* aTransaction,
                          const RequestParams& aParams, bool aGetAll);

  ~ObjectStoreGetRequestOp() override = default;

  template <bool aForPreprocess, typename T>
  nsresult ConvertResponse(StructuredCloneReadInfo& aInfo, T& aResult);

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  bool HasPreprocessInfo() override;

  nsresult GetPreprocessParams(PreprocessParams& aParams) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class ObjectStoreGetKeyRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const uint32_t mObjectStoreId;
  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  const uint32_t mLimit;
  const bool mGetAll;
  FallibleTArray<Key> mResponse;

 private:
  // Only created by TransactionBase.
  ObjectStoreGetKeyRequestOp(TransactionBase* aTransaction,
                             const RequestParams& aParams, bool aGetAll);

  ~ObjectStoreGetKeyRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class ObjectStoreDeleteRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const ObjectStoreDeleteParams mParams;
  ObjectStoreDeleteResponse mResponse;
  bool mObjectStoreMayHaveIndexes;

 private:
  ObjectStoreDeleteRequestOp(TransactionBase* aTransaction,
                             const ObjectStoreDeleteParams& aParams);

  ~ObjectStoreDeleteRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = 0;
  }
};

class ObjectStoreClearRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const ObjectStoreClearParams mParams;
  ObjectStoreClearResponse mResponse;
  bool mObjectStoreMayHaveIndexes;

 private:
  ObjectStoreClearRequestOp(TransactionBase* aTransaction,
                            const ObjectStoreClearParams& aParams);

  ~ObjectStoreClearRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = 0;
  }
};

class ObjectStoreCountRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const ObjectStoreCountParams mParams;
  ObjectStoreCountResponse mResponse;

 private:
  ObjectStoreCountRequestOp(TransactionBase* aTransaction,
                            const ObjectStoreCountParams& aParams)
      : NormalTransactionOp(aTransaction), mParams(aParams) {}

  ~ObjectStoreCountRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = sizeof(uint64_t);
  }
};

class IndexRequestOpBase : public NormalTransactionOp {
 protected:
  const RefPtr<FullIndexMetadata> mMetadata;

 protected:
  IndexRequestOpBase(TransactionBase* aTransaction,
                     const RequestParams& aParams)
      : NormalTransactionOp(aTransaction),
        mMetadata(IndexMetadataForParams(aTransaction, aParams)) {}

  ~IndexRequestOpBase() override = default;

 private:
  static already_AddRefed<FullIndexMetadata> IndexMetadataForParams(
      TransactionBase* aTransaction, const RequestParams& aParams);
};

class IndexGetRequestOp final : public IndexRequestOpBase {
  friend class TransactionBase;

  RefPtr<Database> mDatabase;
  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  AutoTArray<StructuredCloneReadInfo, 1> mResponse;
  PBackgroundParent* mBackgroundParent;
  const uint32_t mLimit;
  const bool mGetAll;

 private:
  // Only created by TransactionBase.
  IndexGetRequestOp(TransactionBase* aTransaction, const RequestParams& aParams,
                    bool aGetAll);

  ~IndexGetRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class IndexGetKeyRequestOp final : public IndexRequestOpBase {
  friend class TransactionBase;

  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  AutoTArray<Key, 1> mResponse;
  const uint32_t mLimit;
  const bool mGetAll;

 private:
  // Only created by TransactionBase.
  IndexGetKeyRequestOp(TransactionBase* aTransaction,
                       const RequestParams& aParams, bool aGetAll);

  ~IndexGetKeyRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class IndexCountRequestOp final : public IndexRequestOpBase {
  friend class TransactionBase;

  const IndexCountParams mParams;
  IndexCountResponse mResponse;

 private:
  // Only created by TransactionBase.
  IndexCountRequestOp(TransactionBase* aTransaction,
                      const RequestParams& aParams)
      : IndexRequestOpBase(aTransaction, aParams),
        mParams(aParams.get_IndexCountParams()) {}

  ~IndexCountRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = sizeof(uint64_t);
  }
};

class Cursor final : public PBackgroundIDBCursorParent {
  friend class TransactionBase;

  class ContinueOp;
  class CursorOpBase;
  class OpenOp;

 public:
  typedef OpenCursorParams::Type Type;

 private:
  RefPtr<TransactionBase> mTransaction;
  RefPtr<Database> mDatabase;
  RefPtr<FileManager> mFileManager;
  PBackgroundParent* mBackgroundParent;

  // These should only be touched on the PBackground thread to check whether the
  // objectStore or index has been deleted. Holding these saves a hash lookup
  // for every call to continue()/advance().
  RefPtr<FullObjectStoreMetadata> mObjectStoreMetadata;
  RefPtr<FullIndexMetadata> mIndexMetadata;

  const int64_t mObjectStoreId;
  const int64_t mIndexId;

  nsCString mContinueQuery;
  nsCString mContinueToQuery;
  nsCString mContinuePrimaryKeyQuery;
  nsCString mLocale;

  Key mKey;
  Key mObjectKey;
  Key mRangeKey;
  Key mSortKey;

  CursorOpBase* mCurrentlyRunningOp;

  const Type mType;
  const Direction mDirection;

  const bool mUniqueIndex;
  const bool mIsSameProcessActor;
  bool mActorDestroyed;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Cursor)

 private:
  // Only created by TransactionBase.
  Cursor(TransactionBase* aTransaction, Type aType,
         FullObjectStoreMetadata* aObjectStoreMetadata,
         FullIndexMetadata* aIndexMetadata, Direction aDirection);

  // Reference counted.
  ~Cursor() override { MOZ_ASSERT(mActorDestroyed); }

  bool VerifyRequestParams(const CursorRequestParams& aParams) const;

  // Only called by TransactionBase.
  bool Start(const OpenCursorParams& aParams);

  void SendResponseInternal(
      CursorResponse& aResponse,
      const nsTArray<FallibleTArray<StructuredCloneFile>>& aFiles);

  // Must call SendResponseInternal!
  bool SendResponse(const CursorResponse& aResponse) = delete;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvContinue(
      const CursorRequestParams& aParams) override;

  bool IsLocaleAware() const { return !mLocale.IsEmpty(); }
};

class Cursor::CursorOpBase : public TransactionDatabaseOperationBase {
 protected:
  RefPtr<Cursor> mCursor;
  nsTArray<FallibleTArray<StructuredCloneFile>> mFiles;

  CursorResponse mResponse;

#ifdef DEBUG
  bool mResponseSent;
#endif

 protected:
  explicit CursorOpBase(Cursor* aCursor)
      : TransactionDatabaseOperationBase(aCursor->mTransaction),
        mCursor(aCursor)
#ifdef DEBUG
        ,
        mResponseSent(false)
#endif
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aCursor);
  }

  ~CursorOpBase() override = default;

  bool SendFailureResult(nsresult aResultCode) override;

  void Cleanup() override;

  nsresult PopulateResponseFromStatement(
      DatabaseConnection::CachedStatement& aStmt, bool aInitializeResponse);
};

class Cursor::OpenOp final : public Cursor::CursorOpBase {
  friend class Cursor;

  const Maybe<SerializedKeyRange> mOptionalKeyRange;

 private:
  // Only created by Cursor.
  OpenOp(Cursor* aCursor, const Maybe<SerializedKeyRange>& aOptionalKeyRange)
      : CursorOpBase(aCursor), mOptionalKeyRange(aOptionalKeyRange) {}

  // Reference counted.
  ~OpenOp() override = default;

  void GetRangeKeyInfo(bool aLowerBound, Key* aKey, bool* aOpen);

  nsresult DoObjectStoreDatabaseWork(DatabaseConnection* aConnection);

  nsresult DoObjectStoreKeyDatabaseWork(DatabaseConnection* aConnection);

  nsresult DoIndexDatabaseWork(DatabaseConnection* aConnection);

  nsresult DoIndexKeyDatabaseWork(DatabaseConnection* aConnection);

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  nsresult SendSuccessResult() override;
};

class Cursor::ContinueOp final : public Cursor::CursorOpBase {
  friend class Cursor;

  const CursorRequestParams mParams;

 private:
  // Only created by Cursor.
  ContinueOp(Cursor* aCursor, const CursorRequestParams& aParams)
      : CursorOpBase(aCursor), mParams(aParams) {
    MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);
  }

  // Reference counted.
  ~ContinueOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  nsresult SendSuccessResult() override;
};

class Utils final : public PBackgroundIndexedDBUtilsParent {
#ifdef DEBUG
  bool mActorDestroyed;
#endif

 public:
  Utils();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Utils)

 private:
  // Reference counted.
  ~Utils() override;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvGetFileReferences(
      const PersistenceType& aPersistenceType, const nsCString& aOrigin,
      const nsString& aDatabaseName, const int64_t& aFileId, int32_t* aRefCnt,
      int32_t* aDBRefCnt, int32_t* aSliceRefCnt, bool* aResult) override;
};

class GetFileReferencesHelper final : public Runnable {
  PersistenceType mPersistenceType;
  nsCString mOrigin;
  nsString mDatabaseName;
  int64_t mFileId;

  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  int32_t mMemRefCnt;
  int32_t mDBRefCnt;
  int32_t mSliceRefCnt;
  bool mResult;
  bool mWaiting;

 public:
  GetFileReferencesHelper(PersistenceType aPersistenceType,
                          const nsACString& aOrigin,
                          const nsAString& aDatabaseName, int64_t aFileId)
      : Runnable("dom::indexedDB::GetFileReferencesHelper"),
        mPersistenceType(aPersistenceType),
        mOrigin(aOrigin),
        mDatabaseName(aDatabaseName),
        mFileId(aFileId),
        mMutex("GetFileReferencesHelper::mMutex"),
        mCondVar(mMutex, "GetFileReferencesHelper::mCondVar"),
        mMemRefCnt(-1),
        mDBRefCnt(-1),
        mSliceRefCnt(-1),
        mResult(false),
        mWaiting(true) {}

  nsresult DispatchAndReturnFileReferences(int32_t* aMemRefCnt,
                                           int32_t* aDBRefCnt,
                                           int32_t* aSliceRefCnt,
                                           bool* aResult);

 private:
  ~GetFileReferencesHelper() override = default;

  NS_DECL_NSIRUNNABLE
};

class PermissionRequestHelper final : public PermissionRequestBase,
                                      public PIndexedDBPermissionRequestParent {
  bool mActorDestroyed;

 public:
  PermissionRequestHelper(Element* aOwnerElement, nsIPrincipal* aPrincipal)
      : PermissionRequestBase(aOwnerElement, aPrincipal),
        mActorDestroyed(false) {}

 protected:
  ~PermissionRequestHelper() override = default;

 private:
  void OnPromptComplete(PermissionValue aPermissionValue) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

struct DatabaseActorInfo final {
  friend class nsAutoPtr<DatabaseActorInfo>;

  RefPtr<FullDatabaseMetadata> mMetadata;
  nsTArray<Database*> mLiveDatabases;
  RefPtr<FactoryOp> mWaitingFactoryOp;

  DatabaseActorInfo(FullDatabaseMetadata* aMetadata, Database* aDatabase)
      : mMetadata(aMetadata) {
    MOZ_ASSERT(aDatabase);

    MOZ_COUNT_CTOR(DatabaseActorInfo);

    mLiveDatabases.AppendElement(aDatabase);
  }

 private:
  ~DatabaseActorInfo() {
    MOZ_ASSERT(mLiveDatabases.IsEmpty());
    MOZ_ASSERT(!mWaitingFactoryOp || !mWaitingFactoryOp->HasBlockedDatabases());

    MOZ_COUNT_DTOR(DatabaseActorInfo);
  }
};

class DatabaseLoggingInfo final {
#ifdef DEBUG
  // Just for potential warnings.
  friend class Factory;
#endif

  LoggingInfo mLoggingInfo;

 public:
  explicit DatabaseLoggingInfo(const LoggingInfo& aLoggingInfo)
      : mLoggingInfo(aLoggingInfo) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aLoggingInfo.nextTransactionSerialNumber());
    MOZ_ASSERT(aLoggingInfo.nextVersionChangeTransactionSerialNumber());
    MOZ_ASSERT(aLoggingInfo.nextRequestSerialNumber());
  }

  const nsID& Id() const {
    AssertIsOnBackgroundThread();

    return mLoggingInfo.backgroundChildLoggingId();
  }

  int64_t NextTransactionSN(IDBTransaction::Mode aMode) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo.nextTransactionSerialNumber() < INT64_MAX);
    MOZ_ASSERT(mLoggingInfo.nextVersionChangeTransactionSerialNumber() >
               INT64_MIN);

    if (aMode == IDBTransaction::VERSION_CHANGE) {
      return mLoggingInfo.nextVersionChangeTransactionSerialNumber()--;
    }

    return mLoggingInfo.nextTransactionSerialNumber()++;
  }

  uint64_t NextRequestSN() {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo.nextRequestSerialNumber() < UINT64_MAX);

    return mLoggingInfo.nextRequestSerialNumber()++;
  }

  NS_INLINE_DECL_REFCOUNTING(DatabaseLoggingInfo)

 private:
  ~DatabaseLoggingInfo();
};

class QuotaClient final : public mozilla::dom::quota::Client {
  static QuotaClient* sInstance;

  nsCOMPtr<nsIEventTarget> mBackgroundThread;
  nsCOMPtr<nsITimer> mDeleteTimer;
  nsTArray<RefPtr<Maintenance>> mMaintenanceQueue;
  RefPtr<Maintenance> mCurrentMaintenance;
  RefPtr<nsThreadPool> mMaintenanceThreadPool;
  nsClassHashtable<nsRefPtrHashKey<FileManager>, nsTArray<int64_t>>
      mPendingDeleteInfos;
  bool mShutdownRequested;

 public:
  QuotaClient();

  static QuotaClient* GetInstance() {
    AssertIsOnBackgroundThread();

    return sInstance;
  }

  static bool IsShuttingDownOnBackgroundThread() {
    AssertIsOnBackgroundThread();

    if (sInstance) {
      return sInstance->IsShuttingDown();
    }

    return QuotaManager::IsShuttingDown();
  }

  static bool IsShuttingDownOnNonBackgroundThread() {
    MOZ_ASSERT(!IsOnBackgroundThread());

    return QuotaManager::IsShuttingDown();
  }

  nsIEventTarget* BackgroundThread() const {
    MOZ_ASSERT(mBackgroundThread);
    return mBackgroundThread;
  }

  bool IsShuttingDown() const {
    AssertIsOnBackgroundThread();

    return mShutdownRequested;
  }

  nsresult AsyncDeleteFile(FileManager* aFileManager, int64_t aFileId);

  nsresult FlushPendingFileDeletions();

  already_AddRefed<Maintenance> GetCurrentMaintenance() const {
    RefPtr<Maintenance> result = mCurrentMaintenance;
    return result.forget();
  }

  void NoteFinishedMaintenance(Maintenance* aMaintenance) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aMaintenance);
    MOZ_ASSERT(mCurrentMaintenance == aMaintenance);

    mCurrentMaintenance = nullptr;
    ProcessMaintenanceQueue();
  }

  nsThreadPool* GetOrCreateThreadPool();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::QuotaClient,
                                        override)

  mozilla::dom::quota::Client::Type GetType() override;

  nsresult UpgradeStorageFrom1_0To2_0(nsIFile* aDirectory) override;

  nsresult InitOrigin(PersistenceType aPersistenceType,
                      const nsACString& aGroup, const nsACString& aOrigin,
                      const AtomicBool& aCanceled,
                      UsageInfo* aUsageInfo) override;

  nsresult GetUsageForOrigin(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin,
                             const AtomicBool& aCanceled,
                             UsageInfo* aUsageInfo) override;

  void OnOriginClearCompleted(PersistenceType aPersistenceType,
                              const nsACString& aOrigin) override;

  void ReleaseIOThreadObjects() override;

  void AbortOperations(const nsACString& aOrigin) override;

  void AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void StartIdleMaintenance() override;

  void StopIdleMaintenance() override;

  void ShutdownWorkThreads() override;

 private:
  ~QuotaClient() override;

  static void DeleteTimerCallback(nsITimer* aTimer, void* aClosure);

  nsresult GetDirectory(PersistenceType aPersistenceType,
                        const nsACString& aOrigin, nsIFile** aDirectory);

  // The aObsoleteFiles will collect files based on the marker files. For now,
  // InitOrigin() is the only consumer of this argument because it checks those
  // unfinished deletion and clean them up after that.
  nsresult GetDatabaseFilenames(
      nsIFile* aDirectory, const AtomicBool& aCanceled, bool aForUpgrade,
      nsTArray<nsString>& aSubdirsToProcess,
      nsTHashtable<nsStringHashKey>& aDatabaseFilename,
      nsTHashtable<nsStringHashKey>* aObsoleteFilenames = nullptr);

  nsresult GetUsageForDirectoryInternal(nsIFile* aDirectory,
                                        const AtomicBool& aCanceled,
                                        UsageInfo* aUsageInfo,
                                        bool aDatabaseFiles);

  // Runs on the PBackground thread. Checks to see if there's a queued
  // Maintenance to run.
  void ProcessMaintenanceQueue();
};

class DeleteFilesRunnable final : public Runnable,
                                  public OpenDirectoryListener {
  typedef mozilla::dom::quota::DirectoryLock DirectoryLock;

  enum State {
    // Just created on the PBackground thread. Next step is
    // State_DirectoryOpenPending.
    State_Initial,

    // Waiting for directory open allowed on the main thread. The next step is
    // State_DatabaseWorkOpen.
    State_DirectoryOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. The next step is
    // State_UnblockingOpen.
    State_DatabaseWorkOpen,

    // Notifying the QuotaManager that it can proceed to the next operation on
    // the main thread. Next step is State_Completed.
    State_UnblockingOpen,

    // All done.
    State_Completed
  };

  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  RefPtr<FileManager> mFileManager;
  RefPtr<DirectoryLock> mDirectoryLock;
  nsCOMPtr<nsIFile> mDirectory;
  nsCOMPtr<nsIFile> mJournalDirectory;
  nsTArray<int64_t> mFileIds;
  State mState;

 public:
  DeleteFilesRunnable(FileManager* aFileManager, nsTArray<int64_t>&& aFileIds);

  void RunImmediately();

 private:
  ~DeleteFilesRunnable() = default;

  nsresult Open();

  nsresult DeleteFile(int64_t aFileId);

  nsresult DoDatabaseWork();

  void Finish();

  void UnblockOpen();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  // OpenDirectoryListener overrides.
  virtual void DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void DirectoryLockFailed() override;
};

class Maintenance final : public Runnable, public OpenDirectoryListener {
  struct DirectoryInfo;

  enum class State {
    // Newly created on the PBackground thread. Will proceed immediately or be
    // added to the maintenance queue. The next step is either
    // DirectoryOpenPending if IndexedDatabaseManager is running, or
    // CreateIndexedDatabaseManager if not.
    Initial = 0,

    // Create IndexedDatabaseManager on the main thread. The next step is either
    // Finishing if IndexedDatabaseManager initialization fails, or
    // IndexedDatabaseManagerOpen if initialization succeeds.
    CreateIndexedDatabaseManager,

    // Call OpenDirectory() on the PBackground thread. The next step is
    // DirectoryOpenPending.
    IndexedDatabaseManagerOpen,

    // Waiting for directory open allowed on the PBackground thread. The next
    // step is either Finishing if directory lock failed to acquire, or
    // DirectoryWorkOpen if directory lock is acquired.
    DirectoryOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. The next step is
    // BeginDatabaseMaintenance.
    DirectoryWorkOpen,

    // Dispatching a runnable for each database on the PBackground thread. The
    // next state is either WaitingForDatabaseMaintenancesToComplete if at least
    // one runnable has been dispatched, or Finishing otherwise.
    BeginDatabaseMaintenance,

    // Waiting for DatabaseMaintenance to finish on maintenance thread pool.
    // The next state is Finishing if the last runnable has finished.
    WaitingForDatabaseMaintenancesToComplete,

    // Waiting to finish/finishing on the PBackground thread. The next step is
    // Completed.
    Finishing,

    // All done.
    Complete
  };

  RefPtr<QuotaClient> mQuotaClient;
  PRTime mStartTime;
  RefPtr<DirectoryLock> mDirectoryLock;
  nsTArray<DirectoryInfo> mDirectoryInfos;
  nsDataHashtable<nsStringHashKey, DatabaseMaintenance*> mDatabaseMaintenances;
  nsresult mResultCode;
  Atomic<bool> mAborted;
  State mState;

 public:
  explicit Maintenance(QuotaClient* aQuotaClient)
      : Runnable("dom::indexedDB::Maintenance"),
        mQuotaClient(aQuotaClient),
        mStartTime(PR_Now()),
        mResultCode(NS_OK),
        mAborted(false),
        mState(State::Initial) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aQuotaClient);
    MOZ_ASSERT(QuotaClient::GetInstance() == aQuotaClient);
    MOZ_ASSERT(mStartTime);
  }

  nsIEventTarget* BackgroundThread() const {
    MOZ_ASSERT(mQuotaClient);
    return mQuotaClient->BackgroundThread();
  }

  PRTime StartTime() const { return mStartTime; }

  bool IsAborted() const { return mAborted; }

  void RunImmediately() {
    MOZ_ASSERT(mState == State::Initial);

    Unused << this->Run();
  }

  void Abort() {
    AssertIsOnBackgroundThread();

    mAborted = true;
  }

  void RegisterDatabaseMaintenance(DatabaseMaintenance* aDatabaseMaintenance);

  void UnregisterDatabaseMaintenance(DatabaseMaintenance* aDatabaseMaintenance);

  already_AddRefed<DatabaseMaintenance> GetDatabaseMaintenance(
      const nsAString& aDatabasePath) const {
    AssertIsOnBackgroundThread();

    RefPtr<DatabaseMaintenance> result =
        mDatabaseMaintenances.Get(aDatabasePath);
    return result.forget();
  }

 private:
  ~Maintenance() override {
    MOZ_ASSERT(mState == State::Complete);
    MOZ_ASSERT(!mDatabaseMaintenances.Count());
  }

  // Runs on the PBackground thread. Checks if IndexedDatabaseManager is
  // running. Calls OpenDirectory() or dispatches to the main thread on which
  // CreateIndexedDatabaseManager() is called.
  nsresult Start();

  // Runs on the main thread. Once IndexedDatabaseManager is created it will
  // dispatch to the PBackground thread on which OpenDirectory() is called.
  nsresult CreateIndexedDatabaseManager();

  // Runs on the PBackground thread. Once QuotaManager has given a lock it will
  // call DirectoryOpen().
  nsresult OpenDirectory();

  // Runs on the PBackground thread. Dispatches to the QuotaManager I/O thread.
  nsresult DirectoryOpen();

  // Runs on the QuotaManager I/O thread. Once it finds databases it will
  // dispatch to the PBackground thread on which BeginDatabaseMaintenance()
  // is called.
  nsresult DirectoryWork();

  // Runs on the PBackground thread. It dispatches a runnable for each database.
  nsresult BeginDatabaseMaintenance();

  // Runs on the PBackground thread. Called when the maintenance is finished or
  // if any of above methods fails.
  void Finish();

  // We need to declare refcounting unconditionally, because
  // OpenDirectoryListener has pure-virtual refcounting.
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIRUNNABLE

  // OpenDirectoryListener overrides.
  void DirectoryLockAcquired(DirectoryLock* aLock) override;

  void DirectoryLockFailed() override;
};

struct Maintenance::DirectoryInfo final {
  const nsCString mGroup;
  const nsCString mOrigin;
  nsTArray<nsString> mDatabasePaths;
  const PersistenceType mPersistenceType;

  DirectoryInfo(PersistenceType aPersistenceType, const nsACString& aGroup,
                const nsACString& aOrigin, nsTArray<nsString>&& aDatabasePaths)
      : mGroup(aGroup),
        mOrigin(aOrigin),
        mDatabasePaths(std::move(aDatabasePaths)),
        mPersistenceType(aPersistenceType) {
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
    MOZ_ASSERT(!aGroup.IsEmpty());
    MOZ_ASSERT(!aOrigin.IsEmpty());
#ifdef DEBUG
    MOZ_ASSERT(!mDatabasePaths.IsEmpty());
    for (const nsString& databasePath : mDatabasePaths) {
      MOZ_ASSERT(!databasePath.IsEmpty());
    }
#endif

    MOZ_COUNT_CTOR(Maintenance::DirectoryInfo);
  }

  DirectoryInfo(DirectoryInfo&& aOther)
      : mGroup(std::move(aOther.mGroup)),
        mOrigin(std::move(aOther.mOrigin)),
        mDatabasePaths(std::move(aOther.mDatabasePaths)),
        mPersistenceType(std::move(aOther.mPersistenceType)) {
#ifdef DEBUG
    MOZ_ASSERT(!mDatabasePaths.IsEmpty());
    for (const nsString& databasePath : mDatabasePaths) {
      MOZ_ASSERT(!databasePath.IsEmpty());
    }
#endif

    MOZ_COUNT_CTOR(Maintenance::DirectoryInfo);
  }

  ~DirectoryInfo() { MOZ_COUNT_DTOR(Maintenance::DirectoryInfo); }

  DirectoryInfo(const DirectoryInfo& aOther) = delete;
};

class DatabaseMaintenance final : public Runnable {
  // The minimum amount of time that has passed since the last vacuum before we
  // will attempt to analyze the database for fragmentation.
  static const PRTime kMinVacuumAge =
      PRTime(PR_USEC_PER_SEC) * 60 * 60 * 24 * 7;

  // If the percent of database pages that are not in contiguous order is higher
  // than this percentage we will attempt a vacuum.
  static const int32_t kPercentUnorderedThreshold = 30;

  // If the percent of file size growth since the last vacuum is higher than
  // this percentage we will attempt a vacuum.
  static const int32_t kPercentFileSizeGrowthThreshold = 10;

  // The number of freelist pages beyond which we will favor an incremental
  // vacuum over a full vacuum.
  static const int32_t kMaxFreelistThreshold = 5;

  // If the percent of unused file bytes in the database exceeds this percentage
  // then we will attempt a full vacuum.
  static const int32_t kPercentUnusedThreshold = 20;

  class AutoProgressHandler;

  enum class MaintenanceAction { Nothing = 0, IncrementalVacuum, FullVacuum };

  RefPtr<Maintenance> mMaintenance;
  const nsCString mGroup;
  const nsCString mOrigin;
  const nsString mDatabasePath;
  nsCOMPtr<nsIRunnable> mCompleteCallback;
  const PersistenceType mPersistenceType;

 public:
  DatabaseMaintenance(Maintenance* aMaintenance,
                      PersistenceType aPersistenceType, const nsCString& aGroup,
                      const nsCString& aOrigin, const nsString& aDatabasePath)
      : Runnable("dom::indexedDB::DatabaseMaintenance"),
        mMaintenance(aMaintenance),
        mGroup(aGroup),
        mOrigin(aOrigin),
        mDatabasePath(aDatabasePath),
        mPersistenceType(aPersistenceType) {}

  const nsString& DatabasePath() const { return mDatabasePath; }

  void WaitForCompletion(nsIRunnable* aCallback) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!mCompleteCallback);

    mCompleteCallback = aCallback;
  }

 private:
  ~DatabaseMaintenance() override = default;

  // Runs on maintenance thread pool. Does maintenance on the database.
  void PerformMaintenanceOnDatabase();

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  nsresult CheckIntegrity(mozIStorageConnection* aConnection, bool* aOk);

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  nsresult DetermineMaintenanceAction(mozIStorageConnection* aConnection,
                                      nsIFile* aDatabaseFile,
                                      MaintenanceAction* aMaintenanceAction);

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  void IncrementalVacuum(mozIStorageConnection* aConnection);

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  void FullVacuum(mozIStorageConnection* aConnection, nsIFile* aDatabaseFile);

  // Runs on the PBackground thread. It dispatches a complete callback and
  // unregisters from Maintenance.
  void RunOnOwningThread();

  // Runs on maintenance thread pool. Once it performs database maintenance
  // it will dispatch to the PBackground thread on which RunOnOwningThread()
  // is called.
  void RunOnConnectionThread();

  NS_DECL_NSIRUNNABLE
};

class MOZ_STACK_CLASS DatabaseMaintenance::AutoProgressHandler final
    : public mozIStorageProgressHandler {
  Maintenance* mMaintenance;
  mozIStorageConnection* mConnection;

  NS_DECL_OWNINGTHREAD

#ifdef DEBUG
  // This class is stack-based so we never actually allow AddRef/Release to do
  // anything. But we need to know if any consumer *thinks* that they have a
  // reference to this object so we track the reference countin DEBUG builds.
  nsrefcnt mDEBUGRefCnt;
#endif

 public:
  explicit AutoProgressHandler(Maintenance* aMaintenance)
      : mMaintenance(aMaintenance),
        mConnection(nullptr)
#ifdef DEBUG
        ,
        mDEBUGRefCnt(0)
#endif
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsOnBackgroundThread());
    NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);
    MOZ_ASSERT(aMaintenance);
  }

  ~AutoProgressHandler() {
    NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);

    if (mConnection) {
      Unregister();
    }

    MOZ_ASSERT(!mDEBUGRefCnt);
  }

  nsresult Register(mozIStorageConnection* aConnection);

  // We don't want the mRefCnt member but this class does not "inherit"
  // nsISupports.
  NS_DECL_ISUPPORTS_INHERITED

 private:
  void Unregister();

  NS_DECL_MOZISTORAGEPROGRESSHANDLER

  // Not available for the heap!
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
  void operator delete(void*) = delete;
  void operator delete[](void*) = delete;
};

class IntString : public nsAutoString {
 public:
  explicit IntString(int64_t aInteger) { AppendInt(aInteger); }
};

#ifdef DEBUG

class DEBUGThreadSlower final : public nsIThreadObserver {
 public:
  DEBUGThreadSlower() {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(kDEBUGThreadSleepMS);
  }

  NS_DECL_ISUPPORTS

 private:
  ~DEBUGThreadSlower() { AssertIsOnBackgroundThread(); }

  NS_DECL_NSITHREADOBSERVER
};

#endif  // DEBUG

/*******************************************************************************
 * Helper classes
 ******************************************************************************/

class MOZ_STACK_CLASS FileHelper final {
  RefPtr<FileManager> mFileManager;

  nsCOMPtr<nsIFile> mFileDirectory;
  nsCOMPtr<nsIFile> mJournalDirectory;

  class ReadCallback;
  RefPtr<ReadCallback> mReadCallback;

 public:
  explicit FileHelper(FileManager* aFileManager) : mFileManager(aFileManager) {}

  nsresult Init();

  already_AddRefed<nsIFile> GetFile(FileInfo* aFileInfo);

  already_AddRefed<nsIFile> GetJournalFile(FileInfo* aFileInfo);

  nsresult CreateFileFromStream(nsIFile* aFile, nsIFile* aJournalFile,
                                nsIInputStream* aInputStream, bool aCompress);

  nsresult RemoveFile(nsIFile* aFile, nsIFile* aJournalFile);

 private:
  nsresult SyncCopy(nsIInputStream* aInputStream,
                    nsIOutputStream* aOutputStream, char* aBuffer,
                    uint32_t aBufferSize);

  nsresult SyncRead(nsIInputStream* aInputStream, char* aBuffer,
                    uint32_t aBufferSize, uint32_t* aRead);
};

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

bool TokenizerIgnoreNothing(char16_t /* aChar */) { return false; }

nsresult DeserializeStructuredCloneFile(FileManager* aFileManager,
                                        const nsString& aText,
                                        StructuredCloneFile* aFile) {
  MOZ_ASSERT(!aText.IsEmpty());
  MOZ_ASSERT(aFile);

  StructuredCloneFile::FileType type;

  switch (aText.First()) {
    case char16_t('-'):
      type = StructuredCloneFile::eMutableFile;
      break;

    case char16_t('.'):
      type = StructuredCloneFile::eStructuredClone;
      break;

    case char16_t('/'):
      type = StructuredCloneFile::eWasmBytecode;
      break;

    case char16_t('\\'):
      type = StructuredCloneFile::eWasmCompiled;
      break;

    default:
      type = StructuredCloneFile::eBlob;
  }

  nsresult rv;
  int32_t id;

  if (type == StructuredCloneFile::eBlob) {
    id = aText.ToInteger(&rv);
  } else {
    nsString text(Substring(aText, 1));

    id = text.ToInteger(&rv);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<FileInfo> fileInfo = aFileManager->GetFileInfo(id);
  MOZ_ASSERT(fileInfo);
  // XXX In bug 1432133, for some reasons FileInfo object cannot be got. This
  // is just a short-term fix, and we are working on finding the real cause
  // in bug 1519859.
  if (!fileInfo) {
    IDB_WARNING(
        "Corrupt structured clone data detected in IndexedDB. Failing the "
        "database request. Bug 1519859 will address this problem.");
    Telemetry::ScalarAdd(Telemetry::ScalarID::IDB_FAILURE_FILEINFO_ERROR, 1);

    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  aFile->mFileInfo.swap(fileInfo);
  aFile->mType = type;

  return NS_OK;
}

nsresult DeserializeStructuredCloneFiles(FileManager* aFileManager,
                                         const nsAString& aText,
                                         nsTArray<StructuredCloneFile>& aResult,
                                         bool* aHasPreprocessInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  nsCharSeparatedTokenizerTemplate<TokenizerIgnoreNothing> tokenizer(aText,
                                                                     ' ');

  nsAutoString token;
  nsresult rv;

  while (tokenizer.hasMoreTokens()) {
    token = tokenizer.nextToken();
    MOZ_ASSERT(!token.IsEmpty());

    StructuredCloneFile* file = aResult.AppendElement();
    rv = DeserializeStructuredCloneFile(aFileManager, token, file);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!aHasPreprocessInfo) {
      continue;
    }

    if (file->mType == StructuredCloneFile::eWasmBytecode) {
      *aHasPreprocessInfo = true;
    } else if (file->mType == StructuredCloneFile::eWasmCompiled) {
      MOZ_ASSERT(aResult.Length() > 1);
      MOZ_ASSERT(aResult[aResult.Length() - 2].mType ==
                 StructuredCloneFile::eWasmBytecode);

      *aHasPreprocessInfo = true;
    }
  }

  return NS_OK;
}

bool GetBaseFilename(const nsAString& aFilename, const nsAString& aSuffix,
                     nsDependentSubstring& aBaseFilename) {
  MOZ_ASSERT(!aFilename.IsEmpty());
  MOZ_ASSERT(aBaseFilename.IsEmpty());

  if (!StringEndsWith(aFilename, aSuffix) ||
      aFilename.Length() == aSuffix.Length()) {
    return false;
  }

  MOZ_ASSERT(aFilename.Length() > aSuffix.Length());

  aBaseFilename.Rebind(aFilename, 0, aFilename.Length() - aSuffix.Length());
  return true;
}

nsresult SerializeStructuredCloneFiles(
    PBackgroundParent* aBackgroundActor, Database* aDatabase,
    const nsTArray<StructuredCloneFile>& aFiles, bool aForPreprocess,
    FallibleTArray<SerializedStructuredCloneFile>& aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(aResult.IsEmpty());

  if (aFiles.IsEmpty()) {
    return NS_OK;
  }

  FileManager* fileManager = aDatabase->GetFileManager();

  nsCOMPtr<nsIFile> directory = fileManager->GetCheckedDirectory();
  if (NS_WARN_IF(!directory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const uint32_t count = aFiles.Length();

  if (NS_WARN_IF(!aResult.SetCapacity(count, fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t index = 0; index < count; index++) {
    const StructuredCloneFile& file = aFiles[index];

    if (aForPreprocess && file.mType != StructuredCloneFile::eWasmBytecode) {
      continue;
    }

    const int64_t fileId = file.mFileInfo->Id();
    MOZ_ASSERT(fileId > 0);

    nsCOMPtr<nsIFile> nativeFile =
        mozilla::dom::indexedDB::FileManager::GetCheckedFileForId(directory,
                                                                  fileId);
    if (NS_WARN_IF(!nativeFile)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    switch (file.mType) {
      case StructuredCloneFile::eBlob: {
        RefPtr<FileBlobImpl> impl = new FileBlobImpl(nativeFile);
        impl->SetFileId(file.mFileInfo->Id());

        IPCBlob ipcBlob;
        nsresult rv = IPCBlobUtils::Serialize(impl, aBackgroundActor, ipcBlob);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          // This can only fail if the child has crashed.
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        SerializedStructuredCloneFile* serializedFile =
            aResult.AppendElement(fallible);
        MOZ_ASSERT(serializedFile);

        serializedFile->file() = ipcBlob;
        serializedFile->type() = StructuredCloneFile::eBlob;

        aDatabase->MapBlob(ipcBlob, file.mFileInfo);
        break;
      }

      case StructuredCloneFile::eMutableFile: {
        if (aDatabase->IsFileHandleDisabled()) {
          SerializedStructuredCloneFile* file = aResult.AppendElement(fallible);
          MOZ_ASSERT(file);

          file->file() = null_t();
          file->type() = StructuredCloneFile::eMutableFile;
        } else {
          RefPtr<MutableFile> actor =
              MutableFile::Create(nativeFile, aDatabase, file.mFileInfo);
          if (!actor) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          // Transfer ownership to IPDL.
          actor->SetActorAlive();

          if (!aDatabase->SendPBackgroundMutableFileConstructor(
                  actor, EmptyString(), EmptyString())) {
            // This can only fail if the child has crashed.
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          SerializedStructuredCloneFile* file = aResult.AppendElement(fallible);
          MOZ_ASSERT(file);

          file->file() = actor;
          file->type() = StructuredCloneFile::eMutableFile;
        }

        break;
      }

      case StructuredCloneFile::eStructuredClone: {
        SerializedStructuredCloneFile* file = aResult.AppendElement(fallible);
        MOZ_ASSERT(file);

        file->file() = null_t();
        file->type() = StructuredCloneFile::eStructuredClone;

        break;
      }

      case StructuredCloneFile::eWasmBytecode: {
        if (!aForPreprocess) {
          SerializedStructuredCloneFile* serializedFile =
              aResult.AppendElement(fallible);
          MOZ_ASSERT(serializedFile);

          serializedFile->file() = null_t();
          serializedFile->type() = StructuredCloneFile::eWasmBytecode;
        } else {
          RefPtr<FileBlobImpl> impl = new FileBlobImpl(nativeFile);
          impl->SetFileId(file.mFileInfo->Id());

          IPCBlob ipcBlob;
          nsresult rv =
              IPCBlobUtils::Serialize(impl, aBackgroundActor, ipcBlob);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            // This can only fail if the child has crashed.
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          SerializedStructuredCloneFile* serializedFile =
              aResult.AppendElement(fallible);
          MOZ_ASSERT(serializedFile);

          serializedFile->file() = ipcBlob;
          serializedFile->type() = StructuredCloneFile::eWasmBytecode;

          aDatabase->MapBlob(ipcBlob, file.mFileInfo);
        }

        break;
      }

      case StructuredCloneFile::eWasmCompiled: {
        SerializedStructuredCloneFile* serializedFile =
            aResult.AppendElement(fallible);
        MOZ_ASSERT(serializedFile);

        serializedFile->file() = null_t();
        serializedFile->type() = StructuredCloneFile::eWasmCompiled;
        break;
      }

      default:
        MOZ_CRASH("Should never get here!");
    }
  }

  return NS_OK;
}

// Idempotently delete a file, decreasing the quota usage as appropriate. If the
// file no longer exists, success is returned, although quota usage can't be
// decreased. (With the assumption being that the file was already deleted prior
// to this logic running, and the non-existent file was no longer tracked by
// quota because it didn't exist at initialization time or a previous deletion
// call updated the usage.)
nsresult DeleteFile(nsIFile* aDirectory, const nsAString& aFilename,
                    QuotaManager* aQuotaManager,
                    const PersistenceType aPersistenceType,
                    const nsACString& aGroup, const nsACString& aOrigin) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(!aFilename.IsEmpty());

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t fileSize;
  if (aQuotaManager) {
    rv = file->GetFileSize(&fileSize);
    if (rv == NS_ERROR_FILE_NOT_FOUND ||
        rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      return NS_OK;
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(fileSize >= 0);
  }

  rv = file->Remove(false);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aQuotaManager && fileSize > 0) {
    aQuotaManager->DecreaseUsageForOrigin(aPersistenceType, aGroup, aOrigin,
                                          fileSize);
  }

  return NS_OK;
}

nsresult DeleteFilesNoQuota(nsIFile* aDirectory, const nsAString& aFilename) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(!aFilename.IsEmpty());

  // The current using function hasn't initialzed the origin, so in here we
  // don't update the size of origin. Adding this assertion for preventing from
  // misusing.
  DebugOnly<QuotaManager*> quotaManager = QuotaManager::Get();
  MOZ_ASSERT(!quotaManager->IsTemporaryStorageInitialized());

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Remove(true);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// CreateMarkerFile and RemoveMarkerFile are a pair of functions to indicate
// whether having removed all the files successfully. The marker file should
// be checked before executing the next operation or initialization.
nsresult CreateMarkerFile(nsIFile* aBaseDirectory,
                          const nsAString& aDatabaseNameBase,
                          nsIFile** aMarkerFileOut) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aBaseDirectory);
  MOZ_ASSERT(!aDatabaseNameBase.IsEmpty());

  nsCOMPtr<nsIFile> markerFile;
  nsresult rv = aBaseDirectory->Clone(getter_AddRefs(markerFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = markerFile->Append(NS_LITERAL_STRING(IDB_DELETION_MARKER_FILE_PREFIX) +
                          aDatabaseNameBase);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = markerFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  markerFile.forget(aMarkerFileOut);

  return NS_OK;
}

nsresult RemoveMarkerFile(nsIFile* aMarkerFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aMarkerFile);

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(aMarkerFile->Exists(&exists)));
  MOZ_ASSERT(exists);

  nsresult rv = aMarkerFile->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// Idempotently delete all the parts of an IndexedDB database including its
// SQLite database file, its WAL journal, it's shared-memory file, and its
// Blob/Files sub-directory. A marker file is created prior to performing the
// deletion so that in the event we crash or fail to successfully delete the
// database and its files, we will re-attempt the deletion the next time the
// origin is initialized using this method. Because this means the method may be
// called on a partially deleted database, this method uses DeleteFile which
// succeeds when the file we ask it to delete does not actually exist. The
// marker file is removed once deletion has successfully completed.
nsresult RemoveDatabaseFilesAndDirectory(nsIFile* aBaseDirectory,
                                         const nsAString& aFilenameBase,
                                         QuotaManager* aQuotaManager,
                                         const PersistenceType aPersistenceType,
                                         const nsACString& aGroup,
                                         const nsACString& aOrigin,
                                         const nsAString& aDatabaseName) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aBaseDirectory);
  MOZ_ASSERT(!aFilenameBase.IsEmpty());

  AUTO_PROFILER_LABEL("RemoveDatabaseFilesAndDirectory", DOM);

  nsCOMPtr<nsIFile> markerFile;
  nsresult rv = CreateMarkerFile(aBaseDirectory, aFilenameBase,
                                 getter_AddRefs(markerFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The database file counts towards quota.
  nsAutoString filename = aFilenameBase + NS_LITERAL_STRING(".sqlite");

  rv = DeleteFile(aBaseDirectory, filename, aQuotaManager, aPersistenceType,
                  aGroup, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // .sqlite-journal files don't count towards quota.
  const NS_ConvertASCIItoUTF16 journalSuffix(
      kSQLiteJournalSuffix, LiteralStringLength(kSQLiteJournalSuffix));

  filename = aFilenameBase + journalSuffix;

  rv = DeleteFile(aBaseDirectory, filename, /* doesn't count */ nullptr,
                  aPersistenceType, aGroup, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // .sqlite-shm files don't count towards quota.
  const NS_ConvertASCIItoUTF16 shmSuffix(kSQLiteSHMSuffix,
                                         LiteralStringLength(kSQLiteSHMSuffix));

  filename = aFilenameBase + shmSuffix;

  rv = DeleteFile(aBaseDirectory, filename, /* doesn't count */ nullptr,
                  aPersistenceType, aGroup, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // .sqlite-wal files do count towards quota.
  const NS_ConvertASCIItoUTF16 walSuffix(kSQLiteWALSuffix,
                                         LiteralStringLength(kSQLiteWALSuffix));

  filename = aFilenameBase + walSuffix;

  rv = DeleteFile(aBaseDirectory, filename, aQuotaManager, aPersistenceType,
                  aGroup, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> fmDirectory;
  rv = aBaseDirectory->Clone(getter_AddRefs(fmDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The files directory counts towards quota.
  const NS_ConvertASCIItoUTF16 filesSuffix(
      kFileManagerDirectoryNameSuffix,
      LiteralStringLength(kFileManagerDirectoryNameSuffix));

  rv = fmDirectory->Append(aFilenameBase + filesSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = fmDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = fmDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    uint64_t usage = 0;

    if (aQuotaManager) {
      rv = FileManager::GetUsage(fmDirectory, &usage);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = fmDirectory->Remove(true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // We may have deleted some files, check if we can and update quota
      // information before returning the error.
      if (aQuotaManager) {
        uint64_t newUsage;
        if (NS_SUCCEEDED(FileManager::GetUsage(fmDirectory, &newUsage))) {
          MOZ_ASSERT(newUsage <= usage);
          usage = usage - newUsage;
        }
      }
    }

    if (aQuotaManager && usage) {
      aQuotaManager->DecreaseUsageForOrigin(aPersistenceType, aGroup, aOrigin,
                                            usage);
    }

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  MOZ_ASSERT_IF(aQuotaManager, mgr);

  if (mgr) {
    mgr->InvalidateFileManager(aPersistenceType, aOrigin, aDatabaseName);
  }

  rv = RemoveMarkerFile(markerFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/*******************************************************************************
 * Globals
 ******************************************************************************/

// Counts the number of "live" Factory, FactoryOp and Database instances.
uint64_t gBusyCount = 0;

typedef nsTArray<RefPtr<FactoryOp>> FactoryOpArray;

StaticAutoPtr<FactoryOpArray> gFactoryOps;

// Maps a database id to information about live database actors.
typedef nsClassHashtable<nsCStringHashKey, DatabaseActorInfo>
    DatabaseActorHashtable;

StaticAutoPtr<DatabaseActorHashtable> gLiveDatabaseHashtable;

StaticRefPtr<ConnectionPool> gConnectionPool;

StaticRefPtr<FileHandleThreadPool> gFileHandleThreadPool;

typedef nsDataHashtable<nsIDHashKey, DatabaseLoggingInfo*>
    DatabaseLoggingInfoHashtable;

StaticAutoPtr<DatabaseLoggingInfoHashtable> gLoggingInfoHashtable;

typedef nsDataHashtable<nsUint32HashKey, uint32_t> TelemetryIdHashtable;

StaticAutoPtr<TelemetryIdHashtable> gTelemetryIdHashtable;

// Protects all reads and writes to gTelemetryIdHashtable.
StaticAutoPtr<Mutex> gTelemetryIdMutex;

#ifdef DEBUG

StaticRefPtr<DEBUGThreadSlower> gDEBUGThreadSlower;

#endif  // DEBUG

void IncreaseBusyCount() {
  AssertIsOnBackgroundThread();

  // If this is the first instance then we need to do some initialization.
  if (!gBusyCount) {
    MOZ_ASSERT(!gFactoryOps);
    gFactoryOps = new FactoryOpArray();

    MOZ_ASSERT(!gLiveDatabaseHashtable);
    gLiveDatabaseHashtable = new DatabaseActorHashtable();

    MOZ_ASSERT(!gLoggingInfoHashtable);
    gLoggingInfoHashtable = new DatabaseLoggingInfoHashtable();

#ifdef DEBUG
    if (kDEBUGThreadPriority != nsISupportsPriority::PRIORITY_NORMAL) {
      NS_WARNING(
          "PBackground thread debugging enabled, priority has been "
          "modified!");
      nsCOMPtr<nsISupportsPriority> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(thread->SetPriority(kDEBUGThreadPriority));
    }

    if (kDEBUGThreadSleepMS) {
      NS_WARNING(
          "PBackground thread debugging enabled, sleeping after every "
          "event!");
      nsCOMPtr<nsIThreadInternal> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      gDEBUGThreadSlower = new DEBUGThreadSlower();

      MOZ_ALWAYS_SUCCEEDS(thread->AddObserver(gDEBUGThreadSlower));
    }
#endif  // DEBUG
  }

  gBusyCount++;
}

void DecreaseBusyCount() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(gBusyCount);

  // Clean up if there are no more instances.
  if (--gBusyCount == 0) {
    MOZ_ASSERT(gLoggingInfoHashtable);
    gLoggingInfoHashtable = nullptr;

    MOZ_ASSERT(gLiveDatabaseHashtable);
    MOZ_ASSERT(!gLiveDatabaseHashtable->Count());
    gLiveDatabaseHashtable = nullptr;

    MOZ_ASSERT(gFactoryOps);
    MOZ_ASSERT(gFactoryOps->IsEmpty());
    gFactoryOps = nullptr;

#ifdef DEBUG
    if (kDEBUGThreadPriority != nsISupportsPriority::PRIORITY_NORMAL) {
      nsCOMPtr<nsISupportsPriority> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(
          thread->SetPriority(nsISupportsPriority::PRIORITY_NORMAL));
    }

    if (kDEBUGThreadSleepMS) {
      MOZ_ASSERT(gDEBUGThreadSlower);

      nsCOMPtr<nsIThreadInternal> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(thread->RemoveObserver(gDEBUGThreadSlower));

      gDEBUGThreadSlower = nullptr;
    }
#endif  // DEBUG
  }
}

uint32_t TelemetryIdForFile(nsIFile* aFile) {
  // May be called on any thread!

  MOZ_ASSERT(aFile);
  MOZ_ASSERT(gTelemetryIdMutex);

  // The storage directory is structured like this:
  //
  //   <profile>/storage/<persistence>/<origin>/idb/<filename>.sqlite
  //
  // For the purposes of this function we're only concerned with the
  // <persistence>, <origin>, and <filename> pieces.

  nsString filename;
  MOZ_ALWAYS_SUCCEEDS(aFile->GetLeafName(filename));

  // Make sure we were given a database file.
  NS_NAMED_LITERAL_STRING(sqliteExtension, ".sqlite");

  MOZ_ASSERT(StringEndsWith(filename, sqliteExtension));

  filename.Truncate(filename.Length() - sqliteExtension.Length());

  // Get the "idb" directory.
  nsCOMPtr<nsIFile> idbDirectory;
  MOZ_ALWAYS_SUCCEEDS(aFile->GetParent(getter_AddRefs(idbDirectory)));

  DebugOnly<nsString> idbLeafName;
  MOZ_ASSERT(NS_SUCCEEDED(idbDirectory->GetLeafName(idbLeafName)));
  MOZ_ASSERT(static_cast<nsString&>(idbLeafName).EqualsLiteral("idb"));

  // Get the <origin> directory.
  nsCOMPtr<nsIFile> originDirectory;
  MOZ_ALWAYS_SUCCEEDS(idbDirectory->GetParent(getter_AddRefs(originDirectory)));

  nsString origin;
  MOZ_ALWAYS_SUCCEEDS(originDirectory->GetLeafName(origin));

  // Any databases in these directories are owned by the application and should
  // not have their filenames masked. Hopefully they also appear in the
  // Telemetry.cpp whitelist.
  if (origin.EqualsLiteral("chrome") ||
      origin.EqualsLiteral("moz-safe-about+home")) {
    return 0;
  }

  // Get the <persistence> directory.
  nsCOMPtr<nsIFile> persistenceDirectory;
  MOZ_ALWAYS_SUCCEEDS(
      originDirectory->GetParent(getter_AddRefs(persistenceDirectory)));

  nsString persistence;
  MOZ_ALWAYS_SUCCEEDS(persistenceDirectory->GetLeafName(persistence));

  NS_NAMED_LITERAL_STRING(separator, "*");

  uint32_t hashValue =
      HashString(persistence + separator + origin + separator + filename);

  MutexAutoLock lock(*gTelemetryIdMutex);

  if (!gTelemetryIdHashtable) {
    gTelemetryIdHashtable = new TelemetryIdHashtable();
  }

  uint32_t id;
  if (!gTelemetryIdHashtable->Get(hashValue, &id)) {
    static uint32_t sNextId = 1;

    // We're locked, no need for atomics.
    id = sNextId++;

    gTelemetryIdHashtable->Put(hashValue, id);
  }

  return id;
}

}  // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PBackgroundIDBFactoryParent* AllocPBackgroundIDBFactoryParent(
    const LoggingInfo& aLoggingInfo) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  if (NS_WARN_IF(!aLoggingInfo.nextTransactionSerialNumber()) ||
      NS_WARN_IF(!aLoggingInfo.nextVersionChangeTransactionSerialNumber()) ||
      NS_WARN_IF(!aLoggingInfo.nextRequestSerialNumber())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<Factory> actor = Factory::Create(aLoggingInfo);
  MOZ_ASSERT(actor);

  return actor.forget().take();
}

bool RecvPBackgroundIDBFactoryConstructor(
    PBackgroundIDBFactoryParent* aActor,
    const LoggingInfo& /* aLoggingInfo */) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  return true;
}

bool DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Factory> actor = dont_AddRef(static_cast<Factory*>(aActor));
  return true;
}

PBackgroundIndexedDBUtilsParent* AllocPBackgroundIndexedDBUtilsParent() {
  AssertIsOnBackgroundThread();

  RefPtr<Utils> actor = new Utils();

  return actor.forget().take();
}

bool DeallocPBackgroundIndexedDBUtilsParent(
    PBackgroundIndexedDBUtilsParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Utils> actor = dont_AddRef(static_cast<Utils*>(aActor));
  return true;
}

bool RecvFlushPendingFileDeletions() {
  AssertIsOnBackgroundThread();

  QuotaClient* quotaClient = QuotaClient::GetInstance();
  if (quotaClient) {
    if (NS_FAILED(quotaClient->FlushPendingFileDeletions())) {
      NS_WARNING("Failed to flush pending file deletions!");
    }
  }

  return true;
}

PIndexedDBPermissionRequestParent* AllocPIndexedDBPermissionRequestParent(
    Element* aOwnerElement, nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<PermissionRequestHelper> actor =
      new PermissionRequestHelper(aOwnerElement, aPrincipal);
  return actor.forget().take();
}

bool RecvPIndexedDBPermissionRequestConstructor(
    PIndexedDBPermissionRequestParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  auto* actor = static_cast<PermissionRequestHelper*>(aActor);

  PermissionRequestBase::PermissionValue permission;
  nsresult rv = actor->PromptIfNeeded(&permission);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (permission != PermissionRequestBase::kPermissionPrompt) {
    Unused << PIndexedDBPermissionRequestParent::Send__delete__(actor,
                                                                permission);
  }

  return true;
}

bool DeallocPIndexedDBPermissionRequestParent(
    PIndexedDBPermissionRequestParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  RefPtr<PermissionRequestHelper> actor =
      dont_AddRef(static_cast<PermissionRequestHelper*>(aActor));
  return true;
}

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient() {
  AssertIsOnBackgroundThread();

  RefPtr<QuotaClient> client = new QuotaClient();
  return client.forget();
}

FileHandleThreadPool* GetFileHandleThreadPool() {
  AssertIsOnBackgroundThread();

  if (!gFileHandleThreadPool) {
    RefPtr<FileHandleThreadPool> fileHandleThreadPool =
        FileHandleThreadPool::Create();
    if (NS_WARN_IF(!fileHandleThreadPool)) {
      return nullptr;
    }

    gFileHandleThreadPool = fileHandleThreadPool;
  }

  return gFileHandleThreadPool;
}

nsresult AsyncDeleteFile(FileManager* aFileManager, int64_t aFileId) {
  AssertIsOnBackgroundThread();

  QuotaClient* quotaClient = QuotaClient::GetInstance();
  if (quotaClient) {
    nsresult rv = quotaClient->AsyncDeleteFile(aFileManager, aFileId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

/*******************************************************************************
 * DatabaseConnection implementation
 ******************************************************************************/

DatabaseConnection::DatabaseConnection(
    mozIStorageConnection* aStorageConnection, FileManager* aFileManager)
    : mStorageConnection(aStorageConnection),
      mFileManager(aFileManager),
      mInReadTransaction(false),
      mInWriteTransaction(false)
#ifdef DEBUG
      ,
      mDEBUGSavepointCount(0)
#endif
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aStorageConnection);
  MOZ_ASSERT(aFileManager);
}

DatabaseConnection::~DatabaseConnection() {
  MOZ_ASSERT(!mStorageConnection);
  MOZ_ASSERT(!mFileManager);
  MOZ_ASSERT(!mCachedStatements.Count());
  MOZ_ASSERT(!mUpdateRefcountFunction);
  MOZ_ASSERT(!mInWriteTransaction);
  MOZ_ASSERT(!mDEBUGSavepointCount);
}

nsresult DatabaseConnection::Init() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(NS_LITERAL_CSTRING("BEGIN;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInReadTransaction = true;

  return NS_OK;
}

nsresult DatabaseConnection::GetCachedStatement(
    const nsACString& aQuery, CachedStatement* aCachedStatement) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aQuery.IsEmpty());
  MOZ_ASSERT(aCachedStatement);
  MOZ_ASSERT(mStorageConnection);

  AUTO_PROFILER_LABEL("DatabaseConnection::GetCachedStatement", DOM);

  nsCOMPtr<mozIStorageStatement> stmt;

  if (!mCachedStatements.Get(aQuery, getter_AddRefs(stmt))) {
    nsresult rv =
        mStorageConnection->CreateStatement(aQuery, getter_AddRefs(stmt));
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      nsCString msg;
      MOZ_ALWAYS_SUCCEEDS(mStorageConnection->GetLastErrorString(msg));

      nsAutoCString error =
          NS_LITERAL_CSTRING("The statement '") + aQuery +
          NS_LITERAL_CSTRING("' failed to compile with the error message '") +
          msg + NS_LITERAL_CSTRING("'.");

      NS_WARNING(error.get());
#endif
      return rv;
    }

    mCachedStatements.Put(aQuery, stmt);
  }

  aCachedStatement->Assign(this, stmt.forget());
  return NS_OK;
}

nsresult DatabaseConnection::BeginWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::BeginWriteTransaction", DOM);

  // Release our read locks.
  CachedStatement rollbackStmt;
  nsresult rv =
      GetCachedStatement(NS_LITERAL_CSTRING("ROLLBACK;"), &rollbackStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = rollbackStmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInReadTransaction = false;

  if (!mUpdateRefcountFunction) {
    MOZ_ASSERT(mFileManager);

    RefPtr<UpdateRefcountFunction> function =
        new UpdateRefcountFunction(this, mFileManager);

    rv = mStorageConnection->CreateFunction(
        NS_LITERAL_CSTRING("update_refcount"),
        /* aNumArguments */ 2, function);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mUpdateRefcountFunction.swap(function);
  }

  CachedStatement beginStmt;
  rv = GetCachedStatement(NS_LITERAL_CSTRING("BEGIN IMMEDIATE;"), &beginStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = beginStmt->Execute();
  if (rv == NS_ERROR_STORAGE_BUSY) {
    NS_WARNING(
        "Received NS_ERROR_STORAGE_BUSY when attempting to start write "
        "transaction, retrying for up to 10 seconds");

    // Another thread must be using the database. Wait up to 10 seconds for
    // that to complete.
    TimeStamp start = TimeStamp::NowLoRes();

    while (true) {
      PR_Sleep(PR_MillisecondsToInterval(100));

      rv = beginStmt->Execute();
      if (rv != NS_ERROR_STORAGE_BUSY ||
          TimeStamp::NowLoRes() - start > TimeDuration::FromSeconds(10)) {
        break;
      }
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInWriteTransaction = true;

  return NS_OK;
}

nsresult DatabaseConnection::CommitWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::CommitWriteTransaction", DOM);

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(NS_LITERAL_CSTRING("COMMIT;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInWriteTransaction = false;
  return NS_OK;
}

void DatabaseConnection::RollbackWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_DIAGNOSTIC_ASSERT(mStorageConnection);

  AUTO_PROFILER_LABEL("DatabaseConnection::RollbackWriteTransaction", DOM);

  if (!mInWriteTransaction) {
    return;
  }

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = GetCachedStatement(NS_LITERAL_CSTRING("ROLLBACK;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // This may fail if SQLite already rolled back the transaction so ignore any
  // errors.
  Unused << stmt->Execute();

  mInWriteTransaction = false;
}

void DatabaseConnection::FinishWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::FinishWriteTransaction", DOM);

  if (mUpdateRefcountFunction) {
    mUpdateRefcountFunction->Reset();
  }

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(NS_LITERAL_CSTRING("BEGIN;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mInReadTransaction = true;
}

nsresult DatabaseConnection::StartSavepoint() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(mUpdateRefcountFunction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::StartSavepoint", DOM);

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(NS_LITERAL_CSTRING(SAVEPOINT_CLAUSE), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mUpdateRefcountFunction->StartSavepoint();

#ifdef DEBUG
  MOZ_ASSERT(mDEBUGSavepointCount < UINT32_MAX);
  mDEBUGSavepointCount++;
#endif

  return NS_OK;
}

nsresult DatabaseConnection::ReleaseSavepoint() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(mUpdateRefcountFunction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::ReleaseSavepoint", DOM);

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(
      NS_LITERAL_CSTRING("RELEASE " SAVEPOINT_CLAUSE), &stmt);
  if (NS_SUCCEEDED(rv)) {
    rv = stmt->Execute();
    if (NS_SUCCEEDED(rv)) {
      mUpdateRefcountFunction->ReleaseSavepoint();

#ifdef DEBUG
      MOZ_ASSERT(mDEBUGSavepointCount);
      mDEBUGSavepointCount--;
#endif
    }
  }

  return rv;
}

nsresult DatabaseConnection::RollbackSavepoint() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(mUpdateRefcountFunction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::RollbackSavepoint", DOM);

#ifdef DEBUG
  MOZ_ASSERT(mDEBUGSavepointCount);
  mDEBUGSavepointCount--;
#endif

  mUpdateRefcountFunction->RollbackSavepoint();

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(
      NS_LITERAL_CSTRING("ROLLBACK TO " SAVEPOINT_CLAUSE), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This may fail if SQLite already rolled back the savepoint so ignore any
  // errors.
  Unused << stmt->Execute();

  return NS_OK;
}

nsresult DatabaseConnection::CheckpointInternal(CheckpointMode aMode) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::CheckpointInternal", DOM);

  nsAutoCString stmtString;
  stmtString.AssignLiteral("PRAGMA wal_checkpoint(");

  switch (aMode) {
    case CheckpointMode::Full:
      // Ensures that the database is completely checkpointed and flushed to
      // disk.
      stmtString.AppendLiteral("FULL");
      break;

    case CheckpointMode::Restart:
      // Like Full, but also ensures that the next write will start overwriting
      // the existing WAL file rather than letting the WAL file grow.
      stmtString.AppendLiteral("RESTART");
      break;

    case CheckpointMode::Truncate:
      // Like Restart but also truncates the existing WAL file.
      stmtString.AppendLiteral("TRUNCATE");
      break;

    default:
      MOZ_CRASH("Unknown CheckpointMode!");
  }

  stmtString.AppendLiteral(");");

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(stmtString, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void DatabaseConnection::DoIdleProcessing(bool aNeedsCheckpoint) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::DoIdleProcessing", DOM);

  DatabaseConnection::CachedStatement freelistStmt;
  uint32_t freelistCount;
  nsresult rv = GetFreelistCount(freelistStmt, &freelistCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    freelistCount = 0;
  }

  CachedStatement rollbackStmt;
  CachedStatement beginStmt;
  if (aNeedsCheckpoint || freelistCount) {
    rv = GetCachedStatement(NS_LITERAL_CSTRING("ROLLBACK;"), &rollbackStmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    rv = GetCachedStatement(NS_LITERAL_CSTRING("BEGIN;"), &beginStmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    // Release the connection's normal transaction. It's possible that it could
    // fail, but that isn't a problem here.
    Unused << rollbackStmt->Execute();

    mInReadTransaction = false;
  }

  bool freedSomePages = false;

  if (freelistCount) {
    rv = ReclaimFreePagesWhileIdle(freelistStmt, rollbackStmt, freelistCount,
                                   aNeedsCheckpoint, &freedSomePages);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_ASSERT(!freedSomePages);
    }

    // Make sure we didn't leave a transaction running.
    MOZ_ASSERT(!mInReadTransaction);
    MOZ_ASSERT(!mInWriteTransaction);
  }

  // Truncate the WAL if we were asked to or if we managed to free some space.
  if (aNeedsCheckpoint || freedSomePages) {
    rv = CheckpointInternal(CheckpointMode::Truncate);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  // Finally try to restart the read transaction if we rolled it back earlier.
  if (beginStmt) {
    rv = beginStmt->Execute();
    if (NS_SUCCEEDED(rv)) {
      mInReadTransaction = true;
    } else {
      NS_WARNING("Falied to restart read transaction!");
    }
  }
}

nsresult DatabaseConnection::ReclaimFreePagesWhileIdle(
    CachedStatement& aFreelistStatement, CachedStatement& aRollbackStatement,
    uint32_t aFreelistCount, bool aNeedsCheckpoint, bool* aFreedSomePages) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aFreelistStatement);
  MOZ_ASSERT(aRollbackStatement);
  MOZ_ASSERT(aFreelistCount);
  MOZ_ASSERT(aFreedSomePages);
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::ReclaimFreePagesWhileIdle", DOM);

  // Make sure we don't keep working if anything else needs this thread.
  nsIThread* currentThread = NS_GetCurrentThread();
  MOZ_ASSERT(currentThread);

  if (NS_HasPendingEvents(currentThread)) {
    *aFreedSomePages = false;
    return NS_OK;
  }

  // Only try to free 10% at a time so that we can bail out if this connection
  // suddenly becomes active or if the thread is needed otherwise.
  nsAutoCString stmtString;
  stmtString.AssignLiteral("PRAGMA incremental_vacuum(");
  stmtString.AppendInt(std::max(uint64_t(1), uint64_t(aFreelistCount / 10)));
  stmtString.AppendLiteral(");");

  // Make all the statements we'll need up front.
  CachedStatement incrementalVacuumStmt;
  nsresult rv = GetCachedStatement(stmtString, &incrementalVacuumStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  CachedStatement beginImmediateStmt;
  rv = GetCachedStatement(NS_LITERAL_CSTRING("BEGIN IMMEDIATE;"),
                          &beginImmediateStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  CachedStatement commitStmt;
  rv = GetCachedStatement(NS_LITERAL_CSTRING("COMMIT;"), &commitStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aNeedsCheckpoint) {
    // Freeing pages is a journaled operation, so it will require additional WAL
    // space. However, we're idle and are about to checkpoint anyway, so doing a
    // RESTART checkpoint here should allow us to reuse any existing space.
    rv = CheckpointInternal(CheckpointMode::Restart);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Start the write transaction.
  rv = beginImmediateStmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInWriteTransaction = true;

  bool freedSomePages = false;

  while (aFreelistCount) {
    if (NS_HasPendingEvents(currentThread)) {
      // Something else wants to use the thread so roll back this transaction.
      // It's ok if we never make progress here because the idle service should
      // eventually reclaim this space.
      rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      break;
    }

    rv = incrementalVacuumStmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    freedSomePages = true;

    rv = GetFreelistCount(aFreelistStatement, &aFreelistCount);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }
  }

  if (NS_SUCCEEDED(rv) && freedSomePages) {
    // Commit the write transaction.
    rv = commitStmt->Execute();
    if (NS_SUCCEEDED(rv)) {
      mInWriteTransaction = false;
    } else {
      NS_WARNING("Failed to commit!");
    }
  }

  if (NS_FAILED(rv)) {
    MOZ_ASSERT(mInWriteTransaction);

    // Something failed, make sure we roll everything back.
    Unused << aRollbackStatement->Execute();

    mInWriteTransaction = false;

    return rv;
  }

  *aFreedSomePages = freedSomePages;
  return NS_OK;
}

nsresult DatabaseConnection::GetFreelistCount(CachedStatement& aCachedStatement,
                                              uint32_t* aFreelistCount) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aFreelistCount);

  AUTO_PROFILER_LABEL("DatabaseConnection::GetFreelistCount", DOM);

  nsresult rv;

  if (!aCachedStatement) {
    rv = GetCachedStatement(NS_LITERAL_CSTRING("PRAGMA freelist_count;"),
                            &aCachedStatement);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = aCachedStatement->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  // Make sure this statement is reset when leaving this function since we're
  // not using the normal stack-based protection of CachedStatement.
  mozStorageStatementScoper scoper(aCachedStatement);

  int32_t freelistCount;
  rv = aCachedStatement->GetInt32(0, &freelistCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(freelistCount >= 0);

  *aFreelistCount = uint32_t(freelistCount);
  return NS_OK;
}

void DatabaseConnection::Close() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(!mDEBUGSavepointCount);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::Close", DOM);

  if (mUpdateRefcountFunction) {
    MOZ_ALWAYS_SUCCEEDS(mStorageConnection->RemoveFunction(
        NS_LITERAL_CSTRING("update_refcount")));
    mUpdateRefcountFunction = nullptr;
  }

  mCachedStatements.Clear();

  MOZ_ALWAYS_SUCCEEDS(mStorageConnection->Close());
  mStorageConnection = nullptr;

  mFileManager = nullptr;
}

nsresult DatabaseConnection::DisableQuotaChecks() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);

  if (!mQuotaObject) {
    MOZ_ASSERT(!mJournalQuotaObject);

    nsresult rv = mStorageConnection->GetQuotaObjects(
        getter_AddRefs(mQuotaObject), getter_AddRefs(mJournalQuotaObject));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(mQuotaObject);
    MOZ_ASSERT(mJournalQuotaObject);
  }

  mQuotaObject->DisableQuotaCheck();
  mJournalQuotaObject->DisableQuotaCheck();

  return NS_OK;
}

void DatabaseConnection::EnableQuotaChecks() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mQuotaObject);
  MOZ_ASSERT(mJournalQuotaObject);

  RefPtr<QuotaObject> quotaObject;
  RefPtr<QuotaObject> journalQuotaObject;

  mQuotaObject.swap(quotaObject);
  mJournalQuotaObject.swap(journalQuotaObject);

  quotaObject->EnableQuotaCheck();
  journalQuotaObject->EnableQuotaCheck();

  int64_t fileSize;
  nsresult rv = GetFileSize(quotaObject->Path(), &fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  int64_t journalFileSize;
  rv = GetFileSize(journalQuotaObject->Path(), &journalFileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  DebugOnly<bool> result = journalQuotaObject->MaybeUpdateSize(
      journalFileSize, /* aTruncate */ true);
  MOZ_ASSERT(result);

  result = quotaObject->MaybeUpdateSize(fileSize, /* aTruncate */ true);
  MOZ_ASSERT(result);
}

nsresult DatabaseConnection::GetFileSize(const nsAString& aPath,
                                         int64_t* aResult) {
  MOZ_ASSERT(!aPath.IsEmpty());
  MOZ_ASSERT(aResult);

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(aPath, false, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t fileSize;

  bool exists;
  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = file->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    fileSize = 0;
  }

  *aResult = fileSize;
  return NS_OK;
}

DatabaseConnection::CachedStatement::CachedStatement()
#ifdef DEBUG
    : mDEBUGConnection(nullptr)
#endif
{
  AssertIsOnConnectionThread();

  MOZ_COUNT_CTOR(DatabaseConnection::CachedStatement);
}

DatabaseConnection::CachedStatement::~CachedStatement() {
  AssertIsOnConnectionThread();

  MOZ_COUNT_DTOR(DatabaseConnection::CachedStatement);
}

DatabaseConnection::CachedStatement::operator mozIStorageStatement*() const {
  AssertIsOnConnectionThread();

  return mStatement;
}

mozIStorageStatement* DatabaseConnection::CachedStatement::operator->() const {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStatement);

  return mStatement;
}

void DatabaseConnection::CachedStatement::Reset() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(mStatement, mScoper);

  if (mStatement) {
    mScoper.reset();
    mScoper.emplace(mStatement);
  }
}

void DatabaseConnection::CachedStatement::Assign(
    DatabaseConnection* aConnection,
    already_AddRefed<mozIStorageStatement> aStatement) {
#ifdef DEBUG
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(mDEBUGConnection, mDEBUGConnection == aConnection);

  mDEBUGConnection = aConnection;
#endif
  AssertIsOnConnectionThread();

  mScoper.reset();

  mStatement = aStatement;

  if (mStatement) {
    mScoper.emplace(mStatement);
  }
}

DatabaseConnection::AutoSavepoint::AutoSavepoint()
    : mConnection(nullptr)
#ifdef DEBUG
      ,
      mDEBUGTransaction(nullptr)
#endif
{
  MOZ_COUNT_CTOR(DatabaseConnection::AutoSavepoint);
}

DatabaseConnection::AutoSavepoint::~AutoSavepoint() {
  MOZ_COUNT_DTOR(DatabaseConnection::AutoSavepoint);

  if (mConnection) {
    mConnection->AssertIsOnConnectionThread();
    MOZ_ASSERT(mDEBUGTransaction);
    MOZ_ASSERT(mDEBUGTransaction->GetMode() == IDBTransaction::READ_WRITE ||
               mDEBUGTransaction->GetMode() ==
                   IDBTransaction::READ_WRITE_FLUSH ||
               mDEBUGTransaction->GetMode() == IDBTransaction::CLEANUP ||
               mDEBUGTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);

    if (NS_FAILED(mConnection->RollbackSavepoint())) {
      NS_WARNING("Failed to rollback savepoint!");
    }
  }
}

nsresult DatabaseConnection::AutoSavepoint::Start(
    const TransactionBase* aTransaction) {
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(aTransaction->GetMode() == IDBTransaction::READ_WRITE ||
             aTransaction->GetMode() == IDBTransaction::READ_WRITE_FLUSH ||
             aTransaction->GetMode() == IDBTransaction::CLEANUP ||
             aTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);

  DatabaseConnection* connection = aTransaction->GetDatabase()->GetConnection();
  MOZ_ASSERT(connection);
  connection->AssertIsOnConnectionThread();

  // This is just a quick fix for preventing accessing the nullptr. The cause is
  // probably because the connection was unexpectedly closed.
  if (!connection->GetUpdateRefcountFunction()) {
    NS_WARNING("The connection was closed for some reasons!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  MOZ_ASSERT(!mConnection);
  MOZ_ASSERT(!mDEBUGTransaction);

  nsresult rv = connection->StartSavepoint();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mConnection = connection;
#ifdef DEBUG
  mDEBUGTransaction = aTransaction;
#endif

  return NS_OK;
}

nsresult DatabaseConnection::AutoSavepoint::Commit() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mDEBUGTransaction);

  nsresult rv = mConnection->ReleaseSavepoint();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mConnection = nullptr;
#ifdef DEBUG
  mDEBUGTransaction = nullptr;
#endif

  return NS_OK;
}

DatabaseConnection::UpdateRefcountFunction::UpdateRefcountFunction(
    DatabaseConnection* aConnection, FileManager* aFileManager)
    : mConnection(aConnection),
      mFileManager(aFileManager),
      mInSavepoint(false) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aFileManager);
}

nsresult DatabaseConnection::UpdateRefcountFunction::WillCommit() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DatabaseConnection::UpdateRefcountFunction::WillCommit",
                      DOM);

  DatabaseUpdateFunction function(this);
  for (auto iter = mFileInfoEntries.ConstIter(); !iter.Done(); iter.Next()) {
    auto key = iter.Key();
    FileInfoEntry* value = iter.Data();
    MOZ_ASSERT(value);

    if (value->mDelta && !function.Update(key, value->mDelta)) {
      break;
    }
  }

  nsresult rv = function.ErrorCode();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateJournals();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void DatabaseConnection::UpdateRefcountFunction::DidCommit() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DatabaseConnection::UpdateRefcountFunction::DidCommit",
                      DOM);

  for (auto iter = mFileInfoEntries.ConstIter(); !iter.Done(); iter.Next()) {
    FileInfoEntry* value = iter.Data();

    MOZ_ASSERT(value);

    if (value->mDelta) {
      value->mFileInfo->UpdateDBRefs(value->mDelta);
    }
  }

  if (NS_FAILED(RemoveJournals(mJournalsToRemoveAfterCommit))) {
    NS_WARNING("RemoveJournals failed!");
  }
}

void DatabaseConnection::UpdateRefcountFunction::DidAbort() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DatabaseConnection::UpdateRefcountFunction::DidAbort",
                      DOM);

  if (NS_FAILED(RemoveJournals(mJournalsToRemoveAfterAbort))) {
    NS_WARNING("RemoveJournals failed!");
  }
}

void DatabaseConnection::UpdateRefcountFunction::StartSavepoint() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInSavepoint);
  MOZ_ASSERT(!mSavepointEntriesIndex.Count());

  mInSavepoint = true;
}

void DatabaseConnection::UpdateRefcountFunction::ReleaseSavepoint() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mInSavepoint);

  mSavepointEntriesIndex.Clear();
  mInSavepoint = false;
}

void DatabaseConnection::UpdateRefcountFunction::RollbackSavepoint() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mInSavepoint);

  for (auto iter = mSavepointEntriesIndex.ConstIter(); !iter.Done();
       iter.Next()) {
    auto value = iter.Data();
    value->mDelta -= value->mSavepointDelta;
  }

  mInSavepoint = false;
  mSavepointEntriesIndex.Clear();
}

void DatabaseConnection::UpdateRefcountFunction::Reset() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!mSavepointEntriesIndex.Count());
  MOZ_ASSERT(!mInSavepoint);

  class MOZ_STACK_CLASS CustomCleanupCallback final
      : public FileInfo::CustomCleanupCallback {
    nsCOMPtr<nsIFile> mDirectory;
    nsCOMPtr<nsIFile> mJournalDirectory;

   public:
    nsresult Cleanup(FileManager* aFileManager, int64_t aId) override {
      if (!mDirectory) {
        MOZ_ASSERT(!mJournalDirectory);

        mDirectory = aFileManager->GetDirectory();
        if (NS_WARN_IF(!mDirectory)) {
          return NS_ERROR_FAILURE;
        }

        mJournalDirectory = aFileManager->GetJournalDirectory();
        if (NS_WARN_IF(!mJournalDirectory)) {
          return NS_ERROR_FAILURE;
        }
      }

      nsCOMPtr<nsIFile> file = FileManager::GetFileForId(mDirectory, aId);
      if (NS_WARN_IF(!file)) {
        return NS_ERROR_FAILURE;
      }

      nsresult rv;
      int64_t fileSize;

      if (aFileManager->EnforcingQuota()) {
        rv = file->GetFileSize(&fileSize);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      rv = file->Remove(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (aFileManager->EnforcingQuota()) {
        QuotaManager* quotaManager = QuotaManager::Get();
        MOZ_ASSERT(quotaManager);

        quotaManager->DecreaseUsageForOrigin(aFileManager->Type(),
                                             aFileManager->Group(),
                                             aFileManager->Origin(), fileSize);
      }

      file = FileManager::GetFileForId(mJournalDirectory, aId);
      if (NS_WARN_IF(!file)) {
        return NS_ERROR_FAILURE;
      }

      rv = file->Remove(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }
  };

  mJournalsToCreateBeforeCommit.Clear();
  mJournalsToRemoveAfterCommit.Clear();
  mJournalsToRemoveAfterAbort.Clear();

  // FileInfo implementation automatically removes unreferenced files, but it's
  // done asynchronously and with a delay. We want to remove them (and decrease
  // quota usage) before we fire the commit event.
  for (auto iter = mFileInfoEntries.ConstIter(); !iter.Done(); iter.Next()) {
    FileInfoEntry* value = iter.Data();

    MOZ_ASSERT(value);

    FileInfo* fileInfo = value->mFileInfo.forget().take();

    MOZ_ASSERT(fileInfo);

    CustomCleanupCallback customCleanupCallback;
    fileInfo->Release(&customCleanupCallback);
  }

  mFileInfoEntries.Clear();
}

nsresult DatabaseConnection::UpdateRefcountFunction::ProcessValue(
    mozIStorageValueArray* aValues, int32_t aIndex, UpdateType aUpdateType) {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aValues);

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::ProcessValue", DOM);

  int32_t type;
  nsresult rv = aValues->GetTypeOfIndex(aIndex, &type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (type == mozIStorageValueArray::VALUE_TYPE_NULL) {
    return NS_OK;
  }

  nsString ids;
  rv = aValues->GetString(aIndex, ids);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<StructuredCloneFile> files;
  rv = DeserializeStructuredCloneFiles(mFileManager, ids, files, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (uint32_t i = 0; i < files.Length(); i++) {
    const StructuredCloneFile& file = files[i];

    const int64_t id = file.mFileInfo->Id();
    MOZ_ASSERT(id > 0);

    FileInfoEntry* entry;
    if (!mFileInfoEntries.Get(id, &entry)) {
      entry = new FileInfoEntry(file.mFileInfo);
      mFileInfoEntries.Put(id, entry);
    }

    if (mInSavepoint) {
      mSavepointEntriesIndex.Put(id, entry);
    }

    switch (aUpdateType) {
      case UpdateType::Increment:
        entry->mDelta++;
        if (mInSavepoint) {
          entry->mSavepointDelta++;
        }
        break;
      case UpdateType::Decrement:
        entry->mDelta--;
        if (mInSavepoint) {
          entry->mSavepointDelta--;
        }
        break;
      default:
        MOZ_CRASH("Unknown update type!");
    }
  }

  return NS_OK;
}

nsresult DatabaseConnection::UpdateRefcountFunction::CreateJournals() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::CreateJournals", DOM);

  nsCOMPtr<nsIFile> journalDirectory = mFileManager->GetJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < mJournalsToCreateBeforeCommit.Length(); i++) {
    int64_t id = mJournalsToCreateBeforeCommit[i];

    nsCOMPtr<nsIFile> file = FileManager::GetFileForId(journalDirectory, id);
    if (NS_WARN_IF(!file)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mJournalsToRemoveAfterAbort.AppendElement(id);
  }

  return NS_OK;
}

nsresult DatabaseConnection::UpdateRefcountFunction::RemoveJournals(
    const nsTArray<int64_t>& aJournals) {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::RemoveJournals", DOM);

  nsCOMPtr<nsIFile> journalDirectory = mFileManager->GetJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t index = 0; index < aJournals.Length(); index++) {
    nsCOMPtr<nsIFile> file =
        FileManager::GetFileForId(journalDirectory, aJournals[index]);
    if (NS_WARN_IF(!file)) {
      return NS_ERROR_FAILURE;
    }

    if (NS_FAILED(file->Remove(false))) {
      NS_WARNING("Failed to removed journal!");
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(DatabaseConnection::UpdateRefcountFunction,
                  mozIStorageFunction)

NS_IMETHODIMP
DatabaseConnection::UpdateRefcountFunction::OnFunctionCall(
    mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::OnFunctionCall", DOM);

  uint32_t numEntries;
  nsresult rv = aValues->GetNumEntries(&numEntries);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(numEntries == 2);

#ifdef DEBUG
  {
    int32_t type1 = mozIStorageValueArray::VALUE_TYPE_NULL;
    MOZ_ASSERT(NS_SUCCEEDED(aValues->GetTypeOfIndex(0, &type1)));

    int32_t type2 = mozIStorageValueArray::VALUE_TYPE_NULL;
    MOZ_ASSERT(NS_SUCCEEDED(aValues->GetTypeOfIndex(1, &type2)));

    MOZ_ASSERT(!(type1 == mozIStorageValueArray::VALUE_TYPE_NULL &&
                 type2 == mozIStorageValueArray::VALUE_TYPE_NULL));
  }
#endif

  rv = ProcessValue(aValues, 0, UpdateType::Decrement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = ProcessValue(aValues, 1, UpdateType::Increment);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool DatabaseConnection::UpdateRefcountFunction::DatabaseUpdateFunction::Update(
    int64_t aId, int32_t aDelta) {
  nsresult rv = UpdateInternal(aId, aDelta);
  if (NS_FAILED(rv)) {
    mErrorCode = rv;
    return false;
  }

  return true;
}

nsresult DatabaseConnection::UpdateRefcountFunction::DatabaseUpdateFunction::
    UpdateInternal(int64_t aId, int32_t aDelta) {
  MOZ_ASSERT(mFunction);

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::"
      "DatabaseUpdateFunction::UpdateInternal",
      DOM);

  DatabaseConnection* connection = mFunction->mConnection;
  MOZ_ASSERT(connection);
  connection->AssertIsOnConnectionThread();

  MOZ_ASSERT(connection->GetStorageConnection());

  nsresult rv;
  if (!mUpdateStatement) {
    rv = connection->GetCachedStatement(
        NS_LITERAL_CSTRING("UPDATE file "
                           "SET refcount = refcount + :delta "
                           "WHERE id = :id"),
        &mUpdateStatement);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mozStorageStatementScoper updateScoper(mUpdateStatement);

  rv = mUpdateStatement->BindInt32ByName(NS_LITERAL_CSTRING("delta"), aDelta);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mUpdateStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mUpdateStatement->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t rows;
  rv = connection->GetStorageConnection()->GetAffectedRows(&rows);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (rows > 0) {
    if (!mSelectStatement) {
      rv = connection->GetCachedStatement(NS_LITERAL_CSTRING("SELECT id "
                                                             "FROM file "
                                                             "WHERE id = :id"),
                                          &mSelectStatement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    mozStorageStatementScoper selectScoper(mSelectStatement);

    rv = mSelectStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasResult;
    rv = mSelectStatement->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!hasResult) {
      // Don't have to create the journal here, we can create all at once,
      // just before commit
      mFunction->mJournalsToCreateBeforeCommit.AppendElement(aId);
    }

    return NS_OK;
  }

  if (!mInsertStatement) {
    rv = connection->GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO file (id, refcount) "
                           "VALUES(:id, :delta)"),
        &mInsertStatement);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mozStorageStatementScoper insertScoper(mInsertStatement);

  rv = mInsertStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mInsertStatement->BindInt32ByName(NS_LITERAL_CSTRING("delta"), aDelta);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mInsertStatement->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mFunction->mJournalsToRemoveAfterCommit.AppendElement(aId);
  return NS_OK;
}

/*******************************************************************************
 * ConnectionPool implementation
 ******************************************************************************/

ConnectionPool::ConnectionPool()
    : mDatabasesMutex("ConnectionPool::mDatabasesMutex"),
      mIdleTimer(NS_NewTimer()),
      mNextTransactionId(0),
      mTotalThreadCount(0),
      mShutdownRequested(false),
      mShutdownComplete(false) {
  AssertIsOnOwningThread();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mIdleTimer);
}

ConnectionPool::~ConnectionPool() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleThreads.IsEmpty());
  MOZ_ASSERT(mIdleDatabases.IsEmpty());
  MOZ_ASSERT(!mIdleTimer);
  MOZ_ASSERT(mTargetIdleTime.IsNull());
  MOZ_ASSERT(!mDatabases.Count());
  MOZ_ASSERT(!mTransactions.Count());
  MOZ_ASSERT(mQueuedTransactions.IsEmpty());
  MOZ_ASSERT(mCompleteCallbacks.IsEmpty());
  MOZ_ASSERT(!mTotalThreadCount);
  MOZ_ASSERT(mShutdownRequested);
  MOZ_ASSERT(mShutdownComplete);
}

// static
void ConnectionPool::IdleTimerCallback(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aTimer);

  AUTO_PROFILER_LABEL("ConnectionPool::IdleTimerCallback", DOM);

  auto* self = static_cast<ConnectionPool*>(aClosure);
  MOZ_ASSERT(self);
  MOZ_ASSERT(self->mIdleTimer);
  MOZ_ASSERT(SameCOMIdentity(self->mIdleTimer, aTimer));
  MOZ_ASSERT(!self->mTargetIdleTime.IsNull());
  MOZ_ASSERT_IF(self->mIdleDatabases.IsEmpty(), !self->mIdleThreads.IsEmpty());
  MOZ_ASSERT_IF(self->mIdleThreads.IsEmpty(), !self->mIdleDatabases.IsEmpty());

  self->mTargetIdleTime = TimeStamp();

  // Cheat a little.
  TimeStamp now = TimeStamp::NowLoRes() + TimeDuration::FromMilliseconds(500);

  uint32_t index = 0;

  for (uint32_t count = self->mIdleDatabases.Length(); index < count; index++) {
    IdleDatabaseInfo& info = self->mIdleDatabases[index];

    if (now >= info.mIdleTime) {
      if (info.mDatabaseInfo->mIdle) {
        self->PerformIdleDatabaseMaintenance(info.mDatabaseInfo);
      } else {
        self->CloseDatabase(info.mDatabaseInfo);
      }
    } else {
      break;
    }
  }

  if (index) {
    self->mIdleDatabases.RemoveElementsAt(0, index);

    index = 0;
  }

  for (uint32_t count = self->mIdleThreads.Length(); index < count; index++) {
    IdleThreadInfo& info = self->mIdleThreads[index];
    MOZ_ASSERT(info.mThreadInfo.mThread);
    MOZ_ASSERT(info.mThreadInfo.mRunnable);

    if (now >= info.mIdleTime) {
      self->ShutdownThread(info.mThreadInfo);
    } else {
      break;
    }
  }

  if (index) {
    self->mIdleThreads.RemoveElementsAt(0, index);
  }

  self->AdjustIdleTimer();
}

nsresult ConnectionPool::GetOrCreateConnection(
    const Database* aDatabase, DatabaseConnection** aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aDatabase);

  AUTO_PROFILER_LABEL("ConnectionPool::GetOrCreateConnection", DOM);

  DatabaseInfo* dbInfo;
  {
    MutexAutoLock lock(mDatabasesMutex);

    dbInfo = mDatabases.Get(aDatabase->Id());
  }

  MOZ_ASSERT(dbInfo);

  RefPtr<DatabaseConnection> connection = dbInfo->mConnection;
  if (!connection) {
    MOZ_ASSERT(!dbInfo->mDEBUGConnectionThread);

    nsCOMPtr<mozIStorageConnection> storageConnection;
    nsresult rv = GetStorageConnection(aDatabase->FilePath(), aDatabase->Type(),
                                       aDatabase->Group(), aDatabase->Origin(),
                                       aDatabase->TelemetryId(),
                                       getter_AddRefs(storageConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    connection =
        new DatabaseConnection(storageConnection, aDatabase->GetFileManager());

    rv = connection->Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    dbInfo->mConnection = connection;

    IDB_DEBUG_LOG(("ConnectionPool created connection 0x%p for '%s'",
                   dbInfo->mConnection.get(),
                   NS_ConvertUTF16toUTF8(aDatabase->FilePath()).get()));

#ifdef DEBUG
    dbInfo->mDEBUGConnectionThread = GetCurrentPhysicalThread();
#endif
  }

  dbInfo->AssertIsOnConnectionThread();

  connection.forget(aConnection);
  return NS_OK;
}

uint64_t ConnectionPool::Start(
    const nsID& aBackgroundChildLoggingId, const nsACString& aDatabaseId,
    int64_t aLoggingSerialNumber, const nsTArray<nsString>& aObjectStoreNames,
    bool aIsWriteTransaction,
    TransactionDatabaseOperationBase* aTransactionOp) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseId.IsEmpty());
  MOZ_ASSERT(mNextTransactionId < UINT64_MAX);
  MOZ_ASSERT(!mShutdownRequested);

  AUTO_PROFILER_LABEL("ConnectionPool::Start", DOM);

  const uint64_t transactionId = ++mNextTransactionId;

  DatabaseInfo* dbInfo = mDatabases.Get(aDatabaseId);

  const bool databaseInfoIsNew = !dbInfo;

  if (databaseInfoIsNew) {
    dbInfo = new DatabaseInfo(this, aDatabaseId);

    MutexAutoLock lock(mDatabasesMutex);

    mDatabases.Put(aDatabaseId, dbInfo);
  }

  auto* transactionInfo = new TransactionInfo(
      dbInfo, aBackgroundChildLoggingId, aDatabaseId, transactionId,
      aLoggingSerialNumber, aObjectStoreNames, aIsWriteTransaction,
      aTransactionOp);

  MOZ_ASSERT(!mTransactions.Get(transactionId));
  mTransactions.Put(transactionId, transactionInfo);

  if (aIsWriteTransaction) {
    MOZ_ASSERT(dbInfo->mWriteTransactionCount < UINT32_MAX);
    dbInfo->mWriteTransactionCount++;
  } else {
    MOZ_ASSERT(dbInfo->mReadTransactionCount < UINT32_MAX);
    dbInfo->mReadTransactionCount++;
  }

  auto& blockingTransactions = dbInfo->mBlockingTransactions;

  for (uint32_t nameIndex = 0, nameCount = aObjectStoreNames.Length();
       nameIndex < nameCount; nameIndex++) {
    const nsString& objectStoreName = aObjectStoreNames[nameIndex];

    TransactionInfoPair* blockInfo = blockingTransactions.Get(objectStoreName);
    if (!blockInfo) {
      blockInfo = new TransactionInfoPair();
      blockingTransactions.Put(objectStoreName, blockInfo);
    }

    // Mark what we are blocking on.
    if (TransactionInfo* blockingRead = blockInfo->mLastBlockingReads) {
      transactionInfo->mBlockedOn.PutEntry(blockingRead);
      blockingRead->AddBlockingTransaction(transactionInfo);
    }

    if (aIsWriteTransaction) {
      if (const uint32_t writeCount = blockInfo->mLastBlockingWrites.Length()) {
        for (uint32_t writeIndex = 0; writeIndex < writeCount; writeIndex++) {
          TransactionInfo* blockingWrite =
              blockInfo->mLastBlockingWrites[writeIndex];
          MOZ_ASSERT(blockingWrite);

          transactionInfo->mBlockedOn.PutEntry(blockingWrite);
          blockingWrite->AddBlockingTransaction(transactionInfo);
        }
      }

      blockInfo->mLastBlockingReads = transactionInfo;
      blockInfo->mLastBlockingWrites.Clear();
    } else {
      blockInfo->mLastBlockingWrites.AppendElement(transactionInfo);
    }
  }

  if (!transactionInfo->mBlockedOn.Count()) {
    Unused << ScheduleTransaction(transactionInfo,
                                  /* aFromQueuedTransactions */ false);
  }

  if (!databaseInfoIsNew &&
      (mIdleDatabases.RemoveElement(dbInfo) ||
       mDatabasesPerformingIdleMaintenance.RemoveElement(dbInfo))) {
    AdjustIdleTimer();
  }

  return transactionId;
}

void ConnectionPool::Dispatch(uint64_t aTransactionId, nsIRunnable* aRunnable) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRunnable);

  AUTO_PROFILER_LABEL("ConnectionPool::Dispatch", DOM);

  TransactionInfo* transactionInfo = mTransactions.Get(aTransactionId);
  MOZ_ASSERT(transactionInfo);
  MOZ_ASSERT(!transactionInfo->mFinished);

  if (transactionInfo->mRunning) {
    DatabaseInfo* dbInfo = transactionInfo->mDatabaseInfo;
    MOZ_ASSERT(dbInfo);
    MOZ_ASSERT(dbInfo->mThreadInfo.mThread);
    MOZ_ASSERT(dbInfo->mThreadInfo.mRunnable);
    MOZ_ASSERT(!dbInfo->mClosing);
    MOZ_ASSERT_IF(transactionInfo->mIsWriteTransaction,
                  dbInfo->mRunningWriteTransaction == transactionInfo);

    MOZ_ALWAYS_SUCCEEDS(
        dbInfo->mThreadInfo.mThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL));
  } else {
    transactionInfo->mQueuedRunnables.AppendElement(aRunnable);
  }
}

void ConnectionPool::Finish(uint64_t aTransactionId,
                            FinishCallback* aCallback) {
  AssertIsOnOwningThread();

#ifdef DEBUG
  TransactionInfo* transactionInfo = mTransactions.Get(aTransactionId);
  MOZ_ASSERT(transactionInfo);
  MOZ_ASSERT(!transactionInfo->mFinished);
#endif

  AUTO_PROFILER_LABEL("ConnectionPool::Finish", DOM);

  RefPtr<FinishCallbackWrapper> wrapper =
      new FinishCallbackWrapper(this, aTransactionId, aCallback);

  Dispatch(aTransactionId, wrapper);

#ifdef DEBUG
  MOZ_ASSERT(!transactionInfo->mFinished);
  transactionInfo->mFinished = true;
#endif
}

void ConnectionPool::WaitForDatabasesToComplete(
    nsTArray<nsCString>&& aDatabaseIds, nsIRunnable* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseIds.IsEmpty());
  MOZ_ASSERT(aCallback);

  AUTO_PROFILER_LABEL("ConnectionPool::WaitForDatabasesToComplete", DOM);

  bool mayRunCallbackImmediately = true;

  for (uint32_t index = 0, count = aDatabaseIds.Length(); index < count;
       index++) {
    const nsCString& databaseId = aDatabaseIds[index];
    MOZ_ASSERT(!databaseId.IsEmpty());

    if (CloseDatabaseWhenIdleInternal(databaseId)) {
      mayRunCallbackImmediately = false;
    }
  }

  if (mayRunCallbackImmediately) {
    Unused << aCallback->Run();
    return;
  }

  nsAutoPtr<DatabasesCompleteCallback> callback(
      new DatabasesCompleteCallback(std::move(aDatabaseIds), aCallback));
  mCompleteCallbacks.AppendElement(callback.forget());
}

void ConnectionPool::Shutdown() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mShutdownRequested);
  MOZ_ASSERT(!mShutdownComplete);

  AUTO_PROFILER_LABEL("ConnectionPool::Shutdown", DOM);

  mShutdownRequested = true;

  CancelIdleTimer();
  MOZ_ASSERT(mTargetIdleTime.IsNull());

  mIdleTimer = nullptr;

  CloseIdleDatabases();

  ShutdownIdleThreads();

  if (!mDatabases.Count()) {
    MOZ_ASSERT(!mTransactions.Count());

    Cleanup();

    MOZ_ASSERT(mShutdownComplete);
    return;
  }

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return mShutdownComplete; }));
}

void ConnectionPool::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mShutdownRequested);
  MOZ_ASSERT(!mShutdownComplete);
  MOZ_ASSERT(!mDatabases.Count());
  MOZ_ASSERT(!mTransactions.Count());
  MOZ_ASSERT(mIdleThreads.IsEmpty());

  AUTO_PROFILER_LABEL("ConnectionPool::Cleanup", DOM);

  if (!mCompleteCallbacks.IsEmpty()) {
    // Run all callbacks manually now.
    for (uint32_t count = mCompleteCallbacks.Length(), index = 0; index < count;
         index++) {
      nsAutoPtr<DatabasesCompleteCallback> completeCallback(
          mCompleteCallbacks[index].forget());
      MOZ_ASSERT(completeCallback);
      MOZ_ASSERT(completeCallback->mCallback);

      Unused << completeCallback->mCallback->Run();
    }

    mCompleteCallbacks.Clear();

    // And make sure they get processed.
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

    MOZ_ALWAYS_SUCCEEDS(NS_ProcessPendingEvents(currentThread));
  }

  mShutdownComplete = true;
}

void ConnectionPool::AdjustIdleTimer() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleTimer);

  AUTO_PROFILER_LABEL("ConnectionPool::AdjustIdleTimer", DOM);

  // Figure out the next time at which we should release idle resources. This
  // includes both databases and threads.
  TimeStamp newTargetIdleTime;
  MOZ_ASSERT(newTargetIdleTime.IsNull());

  if (!mIdleDatabases.IsEmpty()) {
    newTargetIdleTime = mIdleDatabases[0].mIdleTime;
  }

  if (!mIdleThreads.IsEmpty()) {
    const TimeStamp& idleTime = mIdleThreads[0].mIdleTime;

    if (newTargetIdleTime.IsNull() || idleTime < newTargetIdleTime) {
      newTargetIdleTime = idleTime;
    }
  }

  MOZ_ASSERT_IF(newTargetIdleTime.IsNull(), mIdleDatabases.IsEmpty());
  MOZ_ASSERT_IF(newTargetIdleTime.IsNull(), mIdleThreads.IsEmpty());

  // Cancel the timer if it was running and the new target time is different.
  if (!mTargetIdleTime.IsNull() &&
      (newTargetIdleTime.IsNull() || mTargetIdleTime != newTargetIdleTime)) {
    CancelIdleTimer();

    MOZ_ASSERT(mTargetIdleTime.IsNull());
  }

  // Schedule the timer if we have a target time different than before.
  if (!newTargetIdleTime.IsNull() &&
      (mTargetIdleTime.IsNull() || mTargetIdleTime != newTargetIdleTime)) {
    double delta = (newTargetIdleTime - TimeStamp::NowLoRes()).ToMilliseconds();

    uint32_t delay;
    if (delta > 0) {
      delay = uint32_t(std::min(delta, double(UINT32_MAX)));
    } else {
      delay = 0;
    }

    MOZ_ALWAYS_SUCCEEDS(mIdleTimer->InitWithNamedFuncCallback(
        IdleTimerCallback, this, delay, nsITimer::TYPE_ONE_SHOT,
        "ConnectionPool::IdleTimerCallback"));

    mTargetIdleTime = newTargetIdleTime;
  }
}

void ConnectionPool::CancelIdleTimer() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleTimer);

  if (!mTargetIdleTime.IsNull()) {
    MOZ_ALWAYS_SUCCEEDS(mIdleTimer->Cancel());

    mTargetIdleTime = TimeStamp();
    MOZ_ASSERT(mTargetIdleTime.IsNull());
  }
}

void ConnectionPool::ShutdownThread(ThreadInfo& aThreadInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aThreadInfo.mThread);
  MOZ_ASSERT(aThreadInfo.mRunnable);
  MOZ_ASSERT(mTotalThreadCount);

  RefPtr<ThreadRunnable> runnable;
  aThreadInfo.mRunnable.swap(runnable);

  nsCOMPtr<nsIThread> thread;
  aThreadInfo.mThread.swap(thread);

  IDB_DEBUG_LOG(("ConnectionPool shutting down thread %" PRIu32,
                 runnable->SerialNumber()));

  // This should clean up the thread with the profiler.
  MOZ_ALWAYS_SUCCEEDS(thread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(NewRunnableMethod(
      "nsIThread::AsyncShutdown", thread, &nsIThread::AsyncShutdown)));

  mTotalThreadCount--;
}

void ConnectionPool::CloseIdleDatabases() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mShutdownRequested);

  AUTO_PROFILER_LABEL("ConnectionPool::CloseIdleDatabases", DOM);

  if (!mIdleDatabases.IsEmpty()) {
    for (IdleDatabaseInfo& idleInfo : mIdleDatabases) {
      CloseDatabase(idleInfo.mDatabaseInfo);
    }
    mIdleDatabases.Clear();
  }

  if (!mDatabasesPerformingIdleMaintenance.IsEmpty()) {
    for (DatabaseInfo* dbInfo : mDatabasesPerformingIdleMaintenance) {
      MOZ_ASSERT(dbInfo);
      CloseDatabase(dbInfo);
    }
    mDatabasesPerformingIdleMaintenance.Clear();
  }
}

void ConnectionPool::ShutdownIdleThreads() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mShutdownRequested);

  AUTO_PROFILER_LABEL("ConnectionPool::ShutdownIdleThreads", DOM);

  if (!mIdleThreads.IsEmpty()) {
    for (uint32_t threadCount = mIdleThreads.Length(), threadIndex = 0;
         threadIndex < threadCount; threadIndex++) {
      ShutdownThread(mIdleThreads[threadIndex].mThreadInfo);
    }
    mIdleThreads.Clear();
  }
}

bool ConnectionPool::ScheduleTransaction(TransactionInfo* aTransactionInfo,
                                         bool aFromQueuedTransactions) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTransactionInfo);

  AUTO_PROFILER_LABEL("ConnectionPool::ScheduleTransaction", DOM);

  DatabaseInfo* dbInfo = aTransactionInfo->mDatabaseInfo;
  MOZ_ASSERT(dbInfo);

  dbInfo->mIdle = false;

  if (dbInfo->mClosing) {
    MOZ_ASSERT(!mIdleDatabases.Contains(dbInfo));
    MOZ_ASSERT(
        !dbInfo->mTransactionsScheduledDuringClose.Contains(aTransactionInfo));

    dbInfo->mTransactionsScheduledDuringClose.AppendElement(aTransactionInfo);
    return true;
  }

  if (!dbInfo->mThreadInfo.mThread) {
    MOZ_ASSERT(!dbInfo->mThreadInfo.mRunnable);

    if (mIdleThreads.IsEmpty()) {
      bool created = false;

      if (mTotalThreadCount < kMaxConnectionThreadCount) {
        // This will set the thread up with the profiler.
        RefPtr<ThreadRunnable> runnable = new ThreadRunnable();

        nsCOMPtr<nsIThread> newThread;
        nsresult rv = NS_NewNamedThread(runnable->GetThreadName(),
                                        getter_AddRefs(newThread), runnable);
        if (NS_SUCCEEDED(rv)) {
          MOZ_ASSERT(newThread);

          IDB_DEBUG_LOG(("ConnectionPool created thread %" PRIu32,
                         runnable->SerialNumber()));

          dbInfo->mThreadInfo.mThread.swap(newThread);
          dbInfo->mThreadInfo.mRunnable.swap(runnable);

          mTotalThreadCount++;
          created = true;
        } else {
          NS_WARNING("Failed to make new thread!");
        }
      } else if (!mDatabasesPerformingIdleMaintenance.IsEmpty()) {
        // We need a thread right now so force all idle processing to stop by
        // posting a dummy runnable to each thread that might be doing idle
        // maintenance.
        nsCOMPtr<nsIRunnable> runnable = new Runnable("IndexedDBDummyRunnable");

        for (uint32_t index = mDatabasesPerformingIdleMaintenance.Length();
             index > 0; index--) {
          DatabaseInfo* dbInfo = mDatabasesPerformingIdleMaintenance[index - 1];
          MOZ_ASSERT(dbInfo);
          MOZ_ASSERT(dbInfo->mThreadInfo.mThread);

          MOZ_ALWAYS_SUCCEEDS(dbInfo->mThreadInfo.mThread->Dispatch(
              runnable.forget(), NS_DISPATCH_NORMAL));
        }
      }

      if (!created) {
        if (!aFromQueuedTransactions) {
          MOZ_ASSERT(!mQueuedTransactions.Contains(aTransactionInfo));
          mQueuedTransactions.AppendElement(aTransactionInfo);
        }
        return false;
      }
    } else {
      const uint32_t lastIndex = mIdleThreads.Length() - 1;

      ThreadInfo& threadInfo = mIdleThreads[lastIndex].mThreadInfo;

      dbInfo->mThreadInfo.mRunnable.swap(threadInfo.mRunnable);
      dbInfo->mThreadInfo.mThread.swap(threadInfo.mThread);

      mIdleThreads.RemoveElementAt(lastIndex);

      AdjustIdleTimer();
    }
  }

  MOZ_ASSERT(dbInfo->mThreadInfo.mThread);
  MOZ_ASSERT(dbInfo->mThreadInfo.mRunnable);

  if (aTransactionInfo->mIsWriteTransaction) {
    if (dbInfo->mRunningWriteTransaction) {
      // SQLite only allows one write transaction at a time so queue this
      // transaction for later.
      MOZ_ASSERT(
          !dbInfo->mScheduledWriteTransactions.Contains(aTransactionInfo));

      dbInfo->mScheduledWriteTransactions.AppendElement(aTransactionInfo);
      return true;
    }

    dbInfo->mRunningWriteTransaction = aTransactionInfo;
    dbInfo->mNeedsCheckpoint = true;
  }

  MOZ_ASSERT(!aTransactionInfo->mRunning);
  aTransactionInfo->mRunning = true;

  nsTArray<nsCOMPtr<nsIRunnable>>& queuedRunnables =
      aTransactionInfo->mQueuedRunnables;

  if (!queuedRunnables.IsEmpty()) {
    for (uint32_t index = 0, count = queuedRunnables.Length(); index < count;
         index++) {
      nsCOMPtr<nsIRunnable> runnable;
      queuedRunnables[index].swap(runnable);

      MOZ_ALWAYS_SUCCEEDS(dbInfo->mThreadInfo.mThread->Dispatch(
          runnable.forget(), NS_DISPATCH_NORMAL));
    }

    queuedRunnables.Clear();
  }

  return true;
}

void ConnectionPool::NoteFinishedTransaction(uint64_t aTransactionId) {
  AssertIsOnOwningThread();

  AUTO_PROFILER_LABEL("ConnectionPool::NoteFinishedTransaction", DOM);

  TransactionInfo* transactionInfo = mTransactions.Get(aTransactionId);
  MOZ_ASSERT(transactionInfo);
  MOZ_ASSERT(transactionInfo->mRunning);
  MOZ_ASSERT(transactionInfo->mFinished);

  transactionInfo->mRunning = false;

  DatabaseInfo* dbInfo = transactionInfo->mDatabaseInfo;
  MOZ_ASSERT(dbInfo);
  MOZ_ASSERT(mDatabases.Get(transactionInfo->mDatabaseId) == dbInfo);
  MOZ_ASSERT(dbInfo->mThreadInfo.mThread);
  MOZ_ASSERT(dbInfo->mThreadInfo.mRunnable);

  // Schedule the next write transaction if there are any queued.
  if (dbInfo->mRunningWriteTransaction == transactionInfo) {
    MOZ_ASSERT(transactionInfo->mIsWriteTransaction);
    MOZ_ASSERT(dbInfo->mNeedsCheckpoint);

    dbInfo->mRunningWriteTransaction = nullptr;

    if (!dbInfo->mScheduledWriteTransactions.IsEmpty()) {
      TransactionInfo* nextWriteTransaction =
          dbInfo->mScheduledWriteTransactions[0];
      MOZ_ASSERT(nextWriteTransaction);

      dbInfo->mScheduledWriteTransactions.RemoveElementAt(0);

      MOZ_ALWAYS_TRUE(ScheduleTransaction(nextWriteTransaction,
                                          /* aFromQueuedTransactions */ false));
    }
  }

  const nsTArray<nsString>& objectStoreNames =
      transactionInfo->mObjectStoreNames;

  for (uint32_t index = 0, count = objectStoreNames.Length(); index < count;
       index++) {
    TransactionInfoPair* blockInfo =
        dbInfo->mBlockingTransactions.Get(objectStoreNames[index]);
    MOZ_ASSERT(blockInfo);

    if (transactionInfo->mIsWriteTransaction &&
        blockInfo->mLastBlockingReads == transactionInfo) {
      blockInfo->mLastBlockingReads = nullptr;
    }

    blockInfo->mLastBlockingWrites.RemoveElement(transactionInfo);
  }

  transactionInfo->RemoveBlockingTransactions();

  if (transactionInfo->mIsWriteTransaction) {
    MOZ_ASSERT(dbInfo->mWriteTransactionCount);
    dbInfo->mWriteTransactionCount--;
  } else {
    MOZ_ASSERT(dbInfo->mReadTransactionCount);
    dbInfo->mReadTransactionCount--;
  }

  mTransactions.Remove(aTransactionId);

#ifdef DEBUG
  // That just deleted |transactionInfo|.
  transactionInfo = nullptr;
#endif

  if (!dbInfo->TotalTransactionCount()) {
    MOZ_ASSERT(!dbInfo->mIdle);
    dbInfo->mIdle = true;

    NoteIdleDatabase(dbInfo);
  }
}

void ConnectionPool::ScheduleQueuedTransactions(ThreadInfo& aThreadInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aThreadInfo.mThread);
  MOZ_ASSERT(aThreadInfo.mRunnable);
  MOZ_ASSERT(!mQueuedTransactions.IsEmpty());
  MOZ_ASSERT(!mIdleThreads.Contains(aThreadInfo));

  AUTO_PROFILER_LABEL("ConnectionPool::ScheduleQueuedTransactions", DOM);

  mIdleThreads.InsertElementSorted(aThreadInfo);

  aThreadInfo.mRunnable = nullptr;
  aThreadInfo.mThread = nullptr;

  uint32_t index = 0;
  for (uint32_t count = mQueuedTransactions.Length(); index < count; index++) {
    if (!ScheduleTransaction(mQueuedTransactions[index],
                             /* aFromQueuedTransactions */ true)) {
      break;
    }
  }

  if (index) {
    mQueuedTransactions.RemoveElementsAt(0, index);
  }

  AdjustIdleTimer();
}

void ConnectionPool::NoteIdleDatabase(DatabaseInfo* aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseInfo);
  MOZ_ASSERT(!aDatabaseInfo->TotalTransactionCount());
  MOZ_ASSERT(aDatabaseInfo->mThreadInfo.mThread);
  MOZ_ASSERT(aDatabaseInfo->mThreadInfo.mRunnable);
  MOZ_ASSERT(!mIdleDatabases.Contains(aDatabaseInfo));

  AUTO_PROFILER_LABEL("ConnectionPool::NoteIdleDatabase", DOM);

  const bool otherDatabasesWaiting = !mQueuedTransactions.IsEmpty();

  if (mShutdownRequested || otherDatabasesWaiting ||
      aDatabaseInfo->mCloseOnIdle) {
    // Make sure we close the connection if we're shutting down or giving the
    // thread to another database.
    CloseDatabase(aDatabaseInfo);

    if (otherDatabasesWaiting) {
      // Let another database use this thread.
      ScheduleQueuedTransactions(aDatabaseInfo->mThreadInfo);
    } else if (mShutdownRequested) {
      // If there are no other databases that need to run then we can shut this
      // thread down immediately instead of going through the idle thread
      // mechanism.
      ShutdownThread(aDatabaseInfo->mThreadInfo);
    }

    return;
  }

  mIdleDatabases.InsertElementSorted(aDatabaseInfo);

  AdjustIdleTimer();
}

void ConnectionPool::NoteClosedDatabase(DatabaseInfo* aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseInfo);
  MOZ_ASSERT(aDatabaseInfo->mClosing);
  MOZ_ASSERT(!mIdleDatabases.Contains(aDatabaseInfo));

  AUTO_PROFILER_LABEL("ConnectionPool::NoteClosedDatabase", DOM);

  aDatabaseInfo->mClosing = false;

  // Figure out what to do with this database's thread. It may have already been
  // given to another database, in which case there's nothing to do here.
  // Otherwise we prioritize the thread as follows:
  //   1. Databases that haven't had an opportunity to run at all are highest
  //      priority. Those live in the |mQueuedTransactions| list.
  //   2. If this database has additional transactions that were started after
  //      we began closing the connection then the thread can be reused for
  //      those transactions.
  //   3. If we're shutting down then we can get rid of the thread.
  //   4. Finally, if nothing above took the thread then we can add it to our
  //      list of idle threads. It may be reused or it may time out. If we have
  //      too many idle threads then we will shut down the oldest.
  if (aDatabaseInfo->mThreadInfo.mThread) {
    MOZ_ASSERT(aDatabaseInfo->mThreadInfo.mRunnable);

    if (!mQueuedTransactions.IsEmpty()) {
      // Give the thread to another database.
      ScheduleQueuedTransactions(aDatabaseInfo->mThreadInfo);
    } else if (!aDatabaseInfo->TotalTransactionCount()) {
      if (mShutdownRequested) {
        ShutdownThread(aDatabaseInfo->mThreadInfo);
      } else {
        MOZ_ASSERT(!mIdleThreads.Contains(aDatabaseInfo->mThreadInfo));

        mIdleThreads.InsertElementSorted(aDatabaseInfo->mThreadInfo);

        aDatabaseInfo->mThreadInfo.mRunnable = nullptr;
        aDatabaseInfo->mThreadInfo.mThread = nullptr;

        if (mIdleThreads.Length() > kMaxIdleConnectionThreadCount) {
          ShutdownThread(mIdleThreads[0].mThreadInfo);
          mIdleThreads.RemoveElementAt(0);
        }

        AdjustIdleTimer();
      }
    }
  }

  // Schedule any transactions that were started while we were closing the
  // connection.
  if (aDatabaseInfo->TotalTransactionCount()) {
    nsTArray<TransactionInfo*>& scheduledTransactions =
        aDatabaseInfo->mTransactionsScheduledDuringClose;

    MOZ_ASSERT(!scheduledTransactions.IsEmpty());

    for (uint32_t index = 0, count = scheduledTransactions.Length();
         index < count; index++) {
      Unused << ScheduleTransaction(scheduledTransactions[index],
                                    /* aFromQueuedTransactions */ false);
    }

    scheduledTransactions.Clear();

    return;
  }

  // There are no more transactions and the connection has been closed. We're
  // done with this database.
  {
    MutexAutoLock lock(mDatabasesMutex);

    mDatabases.Remove(aDatabaseInfo->mDatabaseId);
  }

#ifdef DEBUG
  // That just deleted |aDatabaseInfo|.
  aDatabaseInfo = nullptr;
#endif

  // See if we need to fire any complete callbacks now that the database is
  // finished.
  for (uint32_t index = 0; index < mCompleteCallbacks.Length();
       /* conditionally incremented */) {
    if (MaybeFireCallback(mCompleteCallbacks[index])) {
      mCompleteCallbacks.RemoveElementAt(index);
    } else {
      index++;
    }
  }

  // If that was the last database and we're supposed to be shutting down then
  // we are finished.
  if (mShutdownRequested && !mDatabases.Count()) {
    MOZ_ASSERT(!mTransactions.Count());
    Cleanup();
  }
}

bool ConnectionPool::MaybeFireCallback(DatabasesCompleteCallback* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!aCallback->mDatabaseIds.IsEmpty());
  MOZ_ASSERT(aCallback->mCallback);

  AUTO_PROFILER_LABEL("ConnectionPool::MaybeFireCallback", DOM);

  for (uint32_t count = aCallback->mDatabaseIds.Length(), index = 0;
       index < count; index++) {
    const nsCString& databaseId = aCallback->mDatabaseIds[index];
    MOZ_ASSERT(!databaseId.IsEmpty());

    if (mDatabases.Get(databaseId)) {
      return false;
    }
  }

  Unused << aCallback->mCallback->Run();
  return true;
}

void ConnectionPool::PerformIdleDatabaseMaintenance(
    DatabaseInfo* aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseInfo);
  MOZ_ASSERT(!aDatabaseInfo->TotalTransactionCount());
  MOZ_ASSERT(aDatabaseInfo->mThreadInfo.mThread);
  MOZ_ASSERT(aDatabaseInfo->mThreadInfo.mRunnable);
  MOZ_ASSERT(aDatabaseInfo->mIdle);
  MOZ_ASSERT(!aDatabaseInfo->mCloseOnIdle);
  MOZ_ASSERT(!aDatabaseInfo->mClosing);
  MOZ_ASSERT(mIdleDatabases.Contains(aDatabaseInfo));
  MOZ_ASSERT(!mDatabasesPerformingIdleMaintenance.Contains(aDatabaseInfo));

  nsCOMPtr<nsIRunnable> runnable = new IdleConnectionRunnable(
      aDatabaseInfo, aDatabaseInfo->mNeedsCheckpoint);

  aDatabaseInfo->mNeedsCheckpoint = false;
  aDatabaseInfo->mIdle = false;

  mDatabasesPerformingIdleMaintenance.AppendElement(aDatabaseInfo);

  MOZ_ALWAYS_SUCCEEDS(aDatabaseInfo->mThreadInfo.mThread->Dispatch(
      runnable.forget(), NS_DISPATCH_NORMAL));
}

void ConnectionPool::CloseDatabase(DatabaseInfo* aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseInfo);
  MOZ_ASSERT(!aDatabaseInfo->TotalTransactionCount());
  MOZ_ASSERT(aDatabaseInfo->mThreadInfo.mThread);
  MOZ_ASSERT(aDatabaseInfo->mThreadInfo.mRunnable);
  MOZ_ASSERT(!aDatabaseInfo->mClosing);

  aDatabaseInfo->mIdle = false;
  aDatabaseInfo->mNeedsCheckpoint = false;
  aDatabaseInfo->mClosing = true;

  nsCOMPtr<nsIRunnable> runnable = new CloseConnectionRunnable(aDatabaseInfo);

  MOZ_ALWAYS_SUCCEEDS(aDatabaseInfo->mThreadInfo.mThread->Dispatch(
      runnable.forget(), NS_DISPATCH_NORMAL));
}

bool ConnectionPool::CloseDatabaseWhenIdleInternal(
    const nsACString& aDatabaseId) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseId.IsEmpty());

  AUTO_PROFILER_LABEL("ConnectionPool::CloseDatabaseWhenIdleInternal", DOM);

  if (DatabaseInfo* dbInfo = mDatabases.Get(aDatabaseId)) {
    if (mIdleDatabases.RemoveElement(dbInfo) ||
        mDatabasesPerformingIdleMaintenance.RemoveElement(dbInfo)) {
      CloseDatabase(dbInfo);
      AdjustIdleTimer();
    } else {
      dbInfo->mCloseOnIdle = true;
    }

    return true;
  }

  return false;
}

ConnectionPool::ConnectionRunnable::ConnectionRunnable(
    DatabaseInfo* aDatabaseInfo)
    : Runnable("dom::indexedDB::ConnectionPool::ConnectionRunnable"),
      mDatabaseInfo(aDatabaseInfo),
      mOwningEventTarget(GetCurrentThreadEventTarget()) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseInfo);
  MOZ_ASSERT(aDatabaseInfo->mConnectionPool);
  aDatabaseInfo->mConnectionPool->AssertIsOnOwningThread();
  MOZ_ASSERT(mOwningEventTarget);
}

NS_IMETHODIMP
ConnectionPool::IdleConnectionRunnable::Run() {
  MOZ_ASSERT(mDatabaseInfo);
  MOZ_ASSERT(!mDatabaseInfo->mIdle);

  nsCOMPtr<nsIEventTarget> owningThread;
  mOwningEventTarget.swap(owningThread);

  if (owningThread) {
    mDatabaseInfo->AssertIsOnConnectionThread();

    // The connection could be null if EnsureConnection() didn't run or was not
    // successful in TransactionDatabaseOperationBase::RunOnConnectionThread().
    if (mDatabaseInfo->mConnection) {
      mDatabaseInfo->mConnection->DoIdleProcessing(mNeedsCheckpoint);
    }

    MOZ_ALWAYS_SUCCEEDS(owningThread->Dispatch(this, NS_DISPATCH_NORMAL));
    return NS_OK;
  }

  AssertIsOnBackgroundThread();

  RefPtr<ConnectionPool> connectionPool = mDatabaseInfo->mConnectionPool;
  MOZ_ASSERT(connectionPool);

  if (mDatabaseInfo->mClosing || mDatabaseInfo->TotalTransactionCount()) {
    MOZ_ASSERT(!connectionPool->mDatabasesPerformingIdleMaintenance.Contains(
        mDatabaseInfo));
  } else {
    MOZ_ALWAYS_TRUE(
        connectionPool->mDatabasesPerformingIdleMaintenance.RemoveElement(
            mDatabaseInfo));

    connectionPool->NoteIdleDatabase(mDatabaseInfo);
  }

  return NS_OK;
}

NS_IMETHODIMP
ConnectionPool::CloseConnectionRunnable::Run() {
  MOZ_ASSERT(mDatabaseInfo);

  AUTO_PROFILER_LABEL("ConnectionPool::CloseConnectionRunnable::Run", DOM);

  if (mOwningEventTarget) {
    MOZ_ASSERT(mDatabaseInfo->mClosing);

    nsCOMPtr<nsIEventTarget> owningThread;
    mOwningEventTarget.swap(owningThread);

    // The connection could be null if EnsureConnection() didn't run or was not
    // successful in TransactionDatabaseOperationBase::RunOnConnectionThread().
    if (mDatabaseInfo->mConnection) {
      mDatabaseInfo->AssertIsOnConnectionThread();

      mDatabaseInfo->mConnection->Close();

      IDB_DEBUG_LOG(("ConnectionPool closed connection 0x%p",
                     mDatabaseInfo->mConnection.get()));

      mDatabaseInfo->mConnection = nullptr;

#ifdef DEBUG
      mDatabaseInfo->mDEBUGConnectionThread = nullptr;
#endif
    }

    MOZ_ALWAYS_SUCCEEDS(owningThread->Dispatch(this, NS_DISPATCH_NORMAL));
    return NS_OK;
  }

  RefPtr<ConnectionPool> connectionPool = mDatabaseInfo->mConnectionPool;
  MOZ_ASSERT(connectionPool);

  connectionPool->NoteClosedDatabase(mDatabaseInfo);
  return NS_OK;
}

ConnectionPool::DatabaseInfo::DatabaseInfo(ConnectionPool* aConnectionPool,
                                           const nsACString& aDatabaseId)
    : mConnectionPool(aConnectionPool),
      mDatabaseId(aDatabaseId),
      mRunningWriteTransaction(nullptr),
      mReadTransactionCount(0),
      mWriteTransactionCount(0),
      mNeedsCheckpoint(false),
      mIdle(false),
      mCloseOnIdle(false),
      mClosing(false)
#ifdef DEBUG
      ,
      mDEBUGConnectionThread(nullptr)
#endif
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aConnectionPool);
  aConnectionPool->AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseId.IsEmpty());

  MOZ_COUNT_CTOR(ConnectionPool::DatabaseInfo);
}

ConnectionPool::DatabaseInfo::~DatabaseInfo() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mConnection);
  MOZ_ASSERT(mScheduledWriteTransactions.IsEmpty());
  MOZ_ASSERT(!mRunningWriteTransaction);
  MOZ_ASSERT(!mThreadInfo.mThread);
  MOZ_ASSERT(!mThreadInfo.mRunnable);
  MOZ_ASSERT(!TotalTransactionCount());

  MOZ_COUNT_DTOR(ConnectionPool::DatabaseInfo);
}

ConnectionPool::DatabasesCompleteCallback::DatabasesCompleteCallback(
    nsTArray<nsCString>&& aDatabaseIds, nsIRunnable* aCallback)
    : mDatabaseIds(std::move(aDatabaseIds)), mCallback(aCallback) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mDatabaseIds.IsEmpty());
  MOZ_ASSERT(aCallback);

  MOZ_COUNT_CTOR(ConnectionPool::DatabasesCompleteCallback);
}

ConnectionPool::DatabasesCompleteCallback::~DatabasesCompleteCallback() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::DatabasesCompleteCallback);
}

ConnectionPool::FinishCallbackWrapper::FinishCallbackWrapper(
    ConnectionPool* aConnectionPool, uint64_t aTransactionId,
    FinishCallback* aCallback)
    : Runnable("dom::indexedDB::ConnectionPool::FinishCallbackWrapper"),
      mConnectionPool(aConnectionPool),
      mCallback(aCallback),
      mOwningEventTarget(GetCurrentThreadEventTarget()),
      mTransactionId(aTransactionId),
      mHasRunOnce(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aConnectionPool);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(mOwningEventTarget);
}

ConnectionPool::FinishCallbackWrapper::~FinishCallbackWrapper() {
  MOZ_ASSERT(!mConnectionPool);
  MOZ_ASSERT(!mCallback);
}

nsresult ConnectionPool::FinishCallbackWrapper::Run() {
  MOZ_ASSERT(mConnectionPool);
  MOZ_ASSERT(mCallback);
  MOZ_ASSERT(mOwningEventTarget);

  AUTO_PROFILER_LABEL("ConnectionPool::FinishCallbackWrapper::Run", DOM);

  if (!mHasRunOnce) {
    MOZ_ASSERT(!IsOnBackgroundThread());

    mHasRunOnce = true;

    Unused << mCallback->Run();

    MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

    return NS_OK;
  }

  mConnectionPool->AssertIsOnOwningThread();
  MOZ_ASSERT(mHasRunOnce);

  RefPtr<ConnectionPool> connectionPool = std::move(mConnectionPool);
  RefPtr<FinishCallback> callback = std::move(mCallback);

  callback->TransactionFinishedBeforeUnblock();

  connectionPool->NoteFinishedTransaction(mTransactionId);

  callback->TransactionFinishedAfterUnblock();

  return NS_OK;
}

uint32_t ConnectionPool::ThreadRunnable::sNextSerialNumber = 0;

ConnectionPool::ThreadRunnable::ThreadRunnable()
    : Runnable("dom::indexedDB::ConnectionPool::ThreadRunnable"),
      mSerialNumber(++sNextSerialNumber),
      mFirstRun(true),
      mContinueRunning(true) {
  AssertIsOnBackgroundThread();
}

ConnectionPool::ThreadRunnable::~ThreadRunnable() {
  MOZ_ASSERT(!mFirstRun);
  MOZ_ASSERT(!mContinueRunning);
}

nsresult ConnectionPool::ThreadRunnable::Run() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mContinueRunning);

  if (!mFirstRun) {
    mContinueRunning = false;
    return NS_OK;
  }

  mFirstRun = false;

  {
    // Scope for the profiler label.
    AUTO_PROFILER_LABEL("ConnectionPool::ThreadRunnable::Run", DOM);

    DebugOnly<nsIThread*> currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

#ifdef DEBUG
    if (kDEBUGTransactionThreadPriority !=
        nsISupportsPriority::PRIORITY_NORMAL) {
      NS_WARNING(
          "ConnectionPool thread debugging enabled, priority has been "
          "modified!");

      nsCOMPtr<nsISupportsPriority> thread = do_QueryInterface(currentThread);
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(thread->SetPriority(kDEBUGTransactionThreadPriority));
    }

    if (kDEBUGTransactionThreadSleepMS) {
      NS_WARNING(
          "TransactionThreadPool thread debugging enabled, sleeping "
          "after every event!");
    }
#endif  // DEBUG

    DebugOnly<bool> b = SpinEventLoopUntil([&]() -> bool {
      if (!mContinueRunning) {
        return true;
      }

#ifdef DEBUG
      if (kDEBUGTransactionThreadSleepMS) {
        MOZ_ALWAYS_TRUE(PR_Sleep(PR_MillisecondsToInterval(
                            kDEBUGTransactionThreadSleepMS)) == PR_SUCCESS);
      }
#endif  // DEBUG

      return false;
    });
    // MSVC can't stringify lambdas, so we have to separate the expression
    // generating the value from the assert itself.
#if DEBUG
    MOZ_ALWAYS_TRUE(b);
#endif
  }

  return NS_OK;
}

ConnectionPool::ThreadInfo::ThreadInfo() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_CTOR(ConnectionPool::ThreadInfo);
}

ConnectionPool::ThreadInfo::ThreadInfo(const ThreadInfo& aOther)
    : mThread(aOther.mThread), mRunnable(aOther.mRunnable) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aOther.mThread);
  MOZ_ASSERT(aOther.mRunnable);

  MOZ_COUNT_CTOR(ConnectionPool::ThreadInfo);
}

ConnectionPool::ThreadInfo::~ThreadInfo() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::ThreadInfo);
}

ConnectionPool::IdleResource::IdleResource(const TimeStamp& aIdleTime)
    : mIdleTime(aIdleTime) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!aIdleTime.IsNull());

  MOZ_COUNT_CTOR(ConnectionPool::IdleResource);
}

ConnectionPool::IdleResource::~IdleResource() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::IdleResource);
}

ConnectionPool::IdleDatabaseInfo::IdleDatabaseInfo(DatabaseInfo* aDatabaseInfo)
    : IdleResource(
          TimeStamp::NowLoRes() +
          (aDatabaseInfo->mIdle
               ? TimeDuration::FromMilliseconds(kConnectionIdleMaintenanceMS)
               : TimeDuration::FromMilliseconds(kConnectionIdleCloseMS))),
      mDatabaseInfo(aDatabaseInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseInfo);

  MOZ_COUNT_CTOR(ConnectionPool::IdleDatabaseInfo);
}

ConnectionPool::IdleDatabaseInfo::~IdleDatabaseInfo() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatabaseInfo);

  MOZ_COUNT_DTOR(ConnectionPool::IdleDatabaseInfo);
}

ConnectionPool::IdleThreadInfo::IdleThreadInfo(const ThreadInfo& aThreadInfo)
    : IdleResource(TimeStamp::NowLoRes() +
                   TimeDuration::FromMilliseconds(kConnectionThreadIdleMS)),
      mThreadInfo(aThreadInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aThreadInfo.mRunnable);
  MOZ_ASSERT(aThreadInfo.mThread);

  MOZ_COUNT_CTOR(ConnectionPool::IdleThreadInfo);
}

ConnectionPool::IdleThreadInfo::~IdleThreadInfo() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::IdleThreadInfo);
}

ConnectionPool::TransactionInfo::TransactionInfo(
    DatabaseInfo* aDatabaseInfo, const nsID& aBackgroundChildLoggingId,
    const nsACString& aDatabaseId, uint64_t aTransactionId,
    int64_t aLoggingSerialNumber, const nsTArray<nsString>& aObjectStoreNames,
    bool aIsWriteTransaction, TransactionDatabaseOperationBase* aTransactionOp)
    : mDatabaseInfo(aDatabaseInfo),
      mBackgroundChildLoggingId(aBackgroundChildLoggingId),
      mDatabaseId(aDatabaseId),
      mTransactionId(aTransactionId),
      mLoggingSerialNumber(aLoggingSerialNumber),
      mObjectStoreNames(aObjectStoreNames),
      mIsWriteTransaction(aIsWriteTransaction),
      mRunning(false)
#ifdef DEBUG
      ,
      mFinished(false)
#endif
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseInfo);
  aDatabaseInfo->mConnectionPool->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(ConnectionPool::TransactionInfo);

  if (aTransactionOp) {
    mQueuedRunnables.AppendElement(aTransactionOp);
  }
}

ConnectionPool::TransactionInfo::~TransactionInfo() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mBlockedOn.Count());
  MOZ_ASSERT(mQueuedRunnables.IsEmpty());
  MOZ_ASSERT(!mRunning);
  MOZ_ASSERT(mFinished);

  MOZ_COUNT_DTOR(ConnectionPool::TransactionInfo);
}

void ConnectionPool::TransactionInfo::AddBlockingTransaction(
    TransactionInfo* aTransactionInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransactionInfo);

  if (!mBlocking.Contains(aTransactionInfo)) {
    mBlocking.PutEntry(aTransactionInfo);
    mBlockingOrdered.AppendElement(aTransactionInfo);
  }
}

void ConnectionPool::TransactionInfo::RemoveBlockingTransactions() {
  AssertIsOnBackgroundThread();

  for (uint32_t index = 0, count = mBlockingOrdered.Length(); index < count;
       index++) {
    TransactionInfo* blockedInfo = mBlockingOrdered[index];
    MOZ_ASSERT(blockedInfo);

    blockedInfo->MaybeUnblock(this);
  }

  mBlocking.Clear();
  mBlockingOrdered.Clear();
}

void ConnectionPool::TransactionInfo::MaybeUnblock(
    TransactionInfo* aTransactionInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mBlockedOn.Contains(aTransactionInfo));

  mBlockedOn.RemoveEntry(aTransactionInfo);
  if (!mBlockedOn.Count()) {
    MOZ_ASSERT(mDatabaseInfo);

    ConnectionPool* connectionPool = mDatabaseInfo->mConnectionPool;
    MOZ_ASSERT(connectionPool);
    connectionPool->AssertIsOnOwningThread();

    Unused << connectionPool->ScheduleTransaction(
        this,
        /* aFromQueuedTransactions */ false);
  }
}

ConnectionPool::TransactionInfoPair::TransactionInfoPair()
    : mLastBlockingReads(nullptr) {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_CTOR(ConnectionPool::TransactionInfoPair);
}

ConnectionPool::TransactionInfoPair::~TransactionInfoPair() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::TransactionInfoPair);
}

/*******************************************************************************
 * Metadata classes
 ******************************************************************************/

bool FullObjectStoreMetadata::HasLiveIndexes() const {
  AssertIsOnBackgroundThread();

  for (auto iter = mIndexes.ConstIter(); !iter.Done(); iter.Next()) {
    if (!iter.Data()->mDeleted) {
      return true;
    }
  }

  return false;
}

already_AddRefed<FullDatabaseMetadata> FullDatabaseMetadata::Duplicate() const {
  AssertIsOnBackgroundThread();

  // FullDatabaseMetadata contains two hash tables of pointers that we need to
  // duplicate so we can't just use the copy constructor.
  RefPtr<FullDatabaseMetadata> newMetadata =
      new FullDatabaseMetadata(mCommonMetadata);

  newMetadata->mDatabaseId = mDatabaseId;
  newMetadata->mFilePath = mFilePath;
  newMetadata->mNextObjectStoreId = mNextObjectStoreId;
  newMetadata->mNextIndexId = mNextIndexId;

  for (auto iter = mObjectStores.ConstIter(); !iter.Done(); iter.Next()) {
    auto key = iter.Key();
    auto value = iter.Data();

    RefPtr<FullObjectStoreMetadata> newOSMetadata =
        new FullObjectStoreMetadata();

    newOSMetadata->mCommonMetadata = value->mCommonMetadata;
    newOSMetadata->mNextAutoIncrementId = value->mNextAutoIncrementId;
    newOSMetadata->mCommittedAutoIncrementId = value->mCommittedAutoIncrementId;

    for (auto iter = value->mIndexes.ConstIter(); !iter.Done(); iter.Next()) {
      auto key = iter.Key();
      auto value = iter.Data();

      RefPtr<FullIndexMetadata> newIndexMetadata = new FullIndexMetadata();

      newIndexMetadata->mCommonMetadata = value->mCommonMetadata;

      if (NS_WARN_IF(
              !newOSMetadata->mIndexes.Put(key, newIndexMetadata, fallible))) {
        return nullptr;
      }
    }

    MOZ_ASSERT(value->mIndexes.Count() == newOSMetadata->mIndexes.Count());

    if (NS_WARN_IF(
            !newMetadata->mObjectStores.Put(key, newOSMetadata, fallible))) {
      return nullptr;
    }
  }

  MOZ_ASSERT(mObjectStores.Count() == newMetadata->mObjectStores.Count());

  return newMetadata.forget();
}

DatabaseLoggingInfo::~DatabaseLoggingInfo() {
  AssertIsOnBackgroundThread();

  if (gLoggingInfoHashtable) {
    const nsID& backgroundChildLoggingId =
        mLoggingInfo.backgroundChildLoggingId();

    MOZ_ASSERT(gLoggingInfoHashtable->Get(backgroundChildLoggingId) == this);

    gLoggingInfoHashtable->Remove(backgroundChildLoggingId);
  }
}

/*******************************************************************************
 * Factory
 ******************************************************************************/

Factory::Factory(already_AddRefed<DatabaseLoggingInfo> aLoggingInfo)
    : mLoggingInfo(std::move(aLoggingInfo))
#ifdef DEBUG
      ,
      mActorDestroyed(false)
#endif
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
}

Factory::~Factory() { MOZ_ASSERT(mActorDestroyed); }

// static
already_AddRefed<Factory> Factory::Create(const LoggingInfo& aLoggingInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // Balanced in ActoryDestroy().
  IncreaseBusyCount();

  MOZ_ASSERT(gLoggingInfoHashtable);
  RefPtr<DatabaseLoggingInfo> loggingInfo =
      gLoggingInfoHashtable->Get(aLoggingInfo.backgroundChildLoggingId());
  if (loggingInfo) {
    MOZ_ASSERT(aLoggingInfo.backgroundChildLoggingId() == loggingInfo->Id());
#if !DISABLE_ASSERTS_FOR_FUZZING
    NS_WARNING_ASSERTION(
        aLoggingInfo.nextTransactionSerialNumber() ==
            loggingInfo->mLoggingInfo.nextTransactionSerialNumber(),
        "NextTransactionSerialNumber doesn't match!");
    NS_WARNING_ASSERTION(
        aLoggingInfo.nextVersionChangeTransactionSerialNumber() ==
            loggingInfo->mLoggingInfo
                .nextVersionChangeTransactionSerialNumber(),
        "NextVersionChangeTransactionSerialNumber doesn't match!");
    NS_WARNING_ASSERTION(
        aLoggingInfo.nextRequestSerialNumber() ==
            loggingInfo->mLoggingInfo.nextRequestSerialNumber(),
        "NextRequestSerialNumber doesn't match!");
#endif  // !DISABLE_ASSERTS_FOR_FUZZING
  } else {
    loggingInfo = new DatabaseLoggingInfo(aLoggingInfo);
    gLoggingInfoHashtable->Put(aLoggingInfo.backgroundChildLoggingId(),
                               loggingInfo);
  }

  RefPtr<Factory> actor = new Factory(loggingInfo.forget());

  return actor.forget();
}

void Factory::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorDestroyed = true;
#endif

  // Match the IncreaseBusyCount in Create().
  DecreaseBusyCount();
}

mozilla::ipc::IPCResult Factory::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBFactoryParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Factory::RecvIncrementLoggingRequestSerialNumber() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLoggingInfo);

  mLoggingInfo->NextRequestSN();
  return IPC_OK();
}

PBackgroundIDBFactoryRequestParent*
Factory::AllocPBackgroundIDBFactoryRequestParent(
    const FactoryRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  const CommonFactoryRequestParams* commonParams;

  switch (aParams.type()) {
    case FactoryRequestParams::TOpenDatabaseRequestParams: {
      const OpenDatabaseRequestParams& params =
          aParams.get_OpenDatabaseRequestParams();
      commonParams = &params.commonParams();
      break;
    }

    case FactoryRequestParams::TDeleteDatabaseRequestParams: {
      const DeleteDatabaseRequestParams& params =
          aParams.get_DeleteDatabaseRequestParams();
      commonParams = &params.commonParams();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(commonParams);

  const DatabaseMetadata& metadata = commonParams->metadata();
  if (NS_WARN_IF(metadata.persistenceType() != PERSISTENCE_TYPE_PERSISTENT &&
                 metadata.persistenceType() != PERSISTENCE_TYPE_TEMPORARY &&
                 metadata.persistenceType() != PERSISTENCE_TYPE_DEFAULT)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  const PrincipalInfo& principalInfo = commonParams->principalInfo();
  if (NS_WARN_IF(principalInfo.type() == PrincipalInfo::TNullPrincipalInfo)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo &&
                 metadata.persistenceType() != PERSISTENCE_TYPE_PERSISTENT)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(principalInfo))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<ContentParent> contentParent =
      BackgroundParent::GetContentParent(Manager());

  RefPtr<FactoryOp> actor;
  if (aParams.type() == FactoryRequestParams::TOpenDatabaseRequestParams) {
    actor = new OpenDatabaseOp(this, contentParent.forget(), *commonParams);
  } else {
    actor = new DeleteDatabaseOp(this, contentParent.forget(), *commonParams);
  }

  gFactoryOps->AppendElement(actor);

  // Balanced in CleanupMetadata() which is/must always called by SendResults().
  IncreaseBusyCount();

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Factory::RecvPBackgroundIDBFactoryRequestConstructor(
    PBackgroundIDBFactoryRequestParent* aActor,
    const FactoryRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  auto* op = static_cast<FactoryOp*>(aActor);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(op));
  return IPC_OK();
}

bool Factory::DeallocPBackgroundIDBFactoryRequestParent(
    PBackgroundIDBFactoryRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<FactoryOp> op = dont_AddRef(static_cast<FactoryOp*>(aActor));
  return true;
}

PBackgroundIDBDatabaseParent* Factory::AllocPBackgroundIDBDatabaseParent(
    const DatabaseSpec& aSpec, PBackgroundIDBFactoryRequestParent* aRequest) {
  MOZ_CRASH(
      "PBackgroundIDBDatabaseParent actors should be constructed "
      "manually!");
}

bool Factory::DeallocPBackgroundIDBDatabaseParent(
    PBackgroundIDBDatabaseParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Database> database = dont_AddRef(static_cast<Database*>(aActor));
  return true;
}

/*******************************************************************************
 * WaitForTransactionsHelper
 ******************************************************************************/

void WaitForTransactionsHelper::WaitForTransactions() {
  MOZ_ASSERT(mState == State::Initial);

  Unused << this->Run();
}

void WaitForTransactionsHelper::MaybeWaitForTransactions() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial);

  RefPtr<ConnectionPool> connectionPool = gConnectionPool.get();
  if (connectionPool) {
    nsTArray<nsCString> ids(1);
    ids.AppendElement(mDatabaseId);

    mState = State::WaitingForTransactions;

    connectionPool->WaitForDatabasesToComplete(std::move(ids), this);
    return;
  }

  MaybeWaitForFileHandles();
}

void WaitForTransactionsHelper::MaybeWaitForFileHandles() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial ||
             mState == State::WaitingForTransactions);

  RefPtr<FileHandleThreadPool> fileHandleThreadPool =
      gFileHandleThreadPool.get();
  if (fileHandleThreadPool) {
    nsTArray<nsCString> ids(1);
    ids.AppendElement(mDatabaseId);

    mState = State::WaitingForFileHandles;

    fileHandleThreadPool->WaitForDirectoriesToComplete(std::move(ids), this);
    return;
  }

  CallCallback();
}

void WaitForTransactionsHelper::CallCallback() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial ||
             mState == State::WaitingForTransactions ||
             mState == State::WaitingForFileHandles);

  nsCOMPtr<nsIRunnable> callback;
  mCallback.swap(callback);

  callback->Run();

  mState = State::Complete;
}

NS_IMETHODIMP
WaitForTransactionsHelper::Run() {
  MOZ_ASSERT(mState != State::Complete);
  MOZ_ASSERT(mCallback);

  switch (mState) {
    case State::Initial:
      MaybeWaitForTransactions();
      break;

    case State::WaitingForTransactions:
      MaybeWaitForFileHandles();
      break;

    case State::WaitingForFileHandles:
      CallCallback();
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database(Factory* aFactory, const PrincipalInfo& aPrincipalInfo,
                   const Maybe<ContentParentId>& aOptionalContentParentId,
                   const nsACString& aGroup, const nsACString& aOrigin,
                   uint32_t aTelemetryId, FullDatabaseMetadata* aMetadata,
                   FileManager* aFileManager,
                   already_AddRefed<DirectoryLock> aDirectoryLock,
                   bool aFileHandleDisabled, bool aChromeWriteAccessAllowed)
    : mFactory(aFactory),
      mMetadata(aMetadata),
      mFileManager(aFileManager),
      mDirectoryLock(std::move(aDirectoryLock)),
      mPrincipalInfo(aPrincipalInfo),
      mOptionalContentParentId(aOptionalContentParentId),
      mGroup(aGroup),
      mOrigin(aOrigin),
      mId(aMetadata->mDatabaseId),
      mFilePath(aMetadata->mFilePath),
      mActiveMutableFileCount(0),
      mTelemetryId(aTelemetryId),
      mPersistenceType(aMetadata->mCommonMetadata.persistenceType()),
      mFileHandleDisabled(aFileHandleDisabled),
      mChromeWriteAccessAllowed(aChromeWriteAccessAllowed),
      mClosed(false),
      mInvalidated(false),
      mActorWasAlive(false),
      mActorDestroyed(false)
#ifdef DEBUG
      ,
      mAllBlobsUnmapped(false)
#endif
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(aMetadata);
  MOZ_ASSERT(aFileManager);
  MOZ_ASSERT_IF(aChromeWriteAccessAllowed,
                aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo);
}

void Database::Invalidate() {
  AssertIsOnBackgroundThread();

  class MOZ_STACK_CLASS Helper final {
   public:
    static bool InvalidateTransactions(
        nsTHashtable<nsPtrHashKey<TransactionBase>>& aTable) {
      AssertIsOnBackgroundThread();

      const uint32_t count = aTable.Count();
      if (!count) {
        return true;
      }

      FallibleTArray<RefPtr<TransactionBase>> transactions;
      if (NS_WARN_IF(!transactions.SetCapacity(count, fallible))) {
        return false;
      }

      for (auto iter = aTable.Iter(); !iter.Done(); iter.Next()) {
        if (NS_WARN_IF(
                !transactions.AppendElement(iter.Get()->GetKey(), fallible))) {
          return false;
        }
      }

      if (count) {
        IDB_REPORT_INTERNAL_ERR();

        for (uint32_t index = 0; index < count; index++) {
          RefPtr<TransactionBase> transaction = transactions[index].forget();
          MOZ_ASSERT(transaction);

          transaction->Invalidate();
        }
      }

      return true;
    }

    static bool InvalidateMutableFiles(
        nsTHashtable<nsPtrHashKey<MutableFile>>& aTable) {
      AssertIsOnBackgroundThread();

      const uint32_t count = aTable.Count();
      if (!count) {
        return true;
      }

      FallibleTArray<RefPtr<MutableFile>> mutableFiles;
      if (NS_WARN_IF(!mutableFiles.SetCapacity(count, fallible))) {
        return false;
      }

      for (auto iter = aTable.Iter(); !iter.Done(); iter.Next()) {
        if (NS_WARN_IF(
                !mutableFiles.AppendElement(iter.Get()->GetKey(), fallible))) {
          return false;
        }
      }

      if (count) {
        IDB_REPORT_INTERNAL_ERR();

        for (uint32_t index = 0; index < count; index++) {
          RefPtr<MutableFile> mutableFile = mutableFiles[index].forget();
          MOZ_ASSERT(mutableFile);

          mutableFile->Invalidate();
        }
      }

      return true;
    }
  };

  if (mInvalidated) {
    return;
  }

  mInvalidated = true;

  if (mActorWasAlive && !mActorDestroyed) {
    Unused << SendInvalidate();
  }

  if (!Helper::InvalidateTransactions(mTransactions)) {
    NS_WARNING("Failed to abort all transactions!");
  }

  if (!Helper::InvalidateMutableFiles(mMutableFiles)) {
    NS_WARNING("Failed to abort all mutable files!");
  }

  MOZ_ALWAYS_TRUE(CloseInternal());
}

nsresult Database::EnsureConnection() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  AUTO_PROFILER_LABEL("Database::EnsureConnection", DOM);

  if (!mConnection || !mConnection->GetStorageConnection()) {
    nsresult rv = gConnectionPool->GetOrCreateConnection(
        this, getter_AddRefs(mConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  AssertIsOnConnectionThread();

  return NS_OK;
}

bool Database::RegisterTransaction(TransactionBase* aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(!mTransactions.GetEntry(aTransaction));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mInvalidated);
  MOZ_ASSERT(!mClosed);

  if (NS_WARN_IF(!mTransactions.PutEntry(aTransaction, fallible))) {
    return false;
  }

  return true;
}

void Database::UnregisterTransaction(TransactionBase* aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(mTransactions.GetEntry(aTransaction));

  mTransactions.RemoveEntry(aTransaction);

  MaybeCloseConnection();
}

bool Database::RegisterMutableFile(MutableFile* aMutableFile) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMutableFile);
  MOZ_ASSERT(!mMutableFiles.GetEntry(aMutableFile));
  MOZ_ASSERT(mDirectoryLock);

  if (NS_WARN_IF(!mMutableFiles.PutEntry(aMutableFile, fallible))) {
    return false;
  }

  return true;
}

void Database::UnregisterMutableFile(MutableFile* aMutableFile) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMutableFile);
  MOZ_ASSERT(mMutableFiles.GetEntry(aMutableFile));

  mMutableFiles.RemoveEntry(aMutableFile);
}

void Database::NoteActiveMutableFile() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mActiveMutableFileCount < UINT32_MAX);

  ++mActiveMutableFileCount;
}

void Database::NoteInactiveMutableFile() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveMutableFileCount > 0);

  --mActiveMutableFileCount;

  MaybeCloseConnection();
}

void Database::SetActorAlive() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

  mActorWasAlive = true;

  // This reference will be absorbed by IPDL and released when the actor is
  // destroyed.
  AddRef();
}

void Database::MapBlob(const IPCBlob& aIPCBlob, FileInfo* aFileInfo) {
  AssertIsOnBackgroundThread();

  const IPCBlobStream& stream = aIPCBlob.inputStream();
  MOZ_ASSERT(stream.type() == IPCBlobStream::TPIPCBlobInputStreamParent);

  IPCBlobInputStreamParent* actor = static_cast<IPCBlobInputStreamParent*>(
      stream.get_PIPCBlobInputStreamParent());

  MOZ_ASSERT(!mMappedBlobs.GetWeak(actor->ID()));
  mMappedBlobs.Put(actor->ID(), aFileInfo);

  RefPtr<UnmapBlobCallback> callback = new UnmapBlobCallback(this);
  actor->SetCallback(callback);
}

already_AddRefed<FileInfo> Database::GetBlob(const IPCBlob& aIPCBlob) {
  AssertIsOnBackgroundThread();

  const IPCBlobStream& stream = aIPCBlob.inputStream();
  MOZ_ASSERT(stream.type() == IPCBlobStream::TIPCStream);

  const IPCStream& ipcStream = stream.get_IPCStream();

  const InputStreamParams& inputStreamParams = ipcStream.stream();
  if (inputStreamParams.type() !=
      InputStreamParams::TIPCBlobInputStreamParams) {
    return nullptr;
  }

  const nsID& id = inputStreamParams.get_IPCBlobInputStreamParams().id();

  RefPtr<FileInfo> fileInfo;
  if (!mMappedBlobs.Get(id, getter_AddRefs(fileInfo))) {
    return nullptr;
  }

  return fileInfo.forget();
}

void Database::UnmapBlob(const nsID& aID) {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT_IF(!mAllBlobsUnmapped, mMappedBlobs.GetWeak(aID));
  mMappedBlobs.Remove(aID);
}

void Database::UnmapAllBlobs() {
  AssertIsOnBackgroundThread();

#ifdef DEBUG
  mAllBlobsUnmapped = true;
#endif

  mMappedBlobs.Clear();
}

bool Database::CloseInternal() {
  AssertIsOnBackgroundThread();

  if (mClosed) {
    if (NS_WARN_IF(!IsInvalidated())) {
      // Kill misbehaving child for sending the close message twice.
      return false;
    }

    // Ignore harmless race when we just invalidated the database.
    return true;
  }

  mClosed = true;

  if (gConnectionPool) {
    gConnectionPool->CloseDatabaseWhenIdle(Id());
  }

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(this));

  if (info->mWaitingFactoryOp) {
    info->mWaitingFactoryOp->NoteDatabaseClosed(this);
  }

  MaybeCloseConnection();

  return true;
}

void Database::MaybeCloseConnection() {
  AssertIsOnBackgroundThread();

  if (!mTransactions.Count() && !mActiveMutableFileCount && IsClosed() &&
      mDirectoryLock) {
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::indexedDB::Database::ConnectionClosedCallback",
                          this, &Database::ConnectionClosedCallback);

    RefPtr<WaitForTransactionsHelper> helper =
        new WaitForTransactionsHelper(Id(), callback);
    helper->WaitForTransactions();
  }
}

void Database::ConnectionClosedCallback() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mClosed);
  MOZ_ASSERT(!mTransactions.Count());
  MOZ_ASSERT(!mActiveMutableFileCount);

  mDirectoryLock = nullptr;

  CleanupMetadata();

  UnmapAllBlobs();

  if (IsInvalidated() && IsActorAlive()) {
    // Step 3 and 4 of "5.2 Closing a Database":
    // 1. Wait for all transactions to complete.
    // 2. Fire a close event if forced flag is set, i.e., IsInvalidated() in our
    //    implementation.
    Unused << SendCloseAfterInvalidationComplete();
  }
}

void Database::CleanupMetadata() {
  AssertIsOnBackgroundThread();

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));
  MOZ_ALWAYS_TRUE(info->mLiveDatabases.RemoveElement(this));

  if (info->mLiveDatabases.IsEmpty()) {
    MOZ_ASSERT(!info->mWaitingFactoryOp ||
               !info->mWaitingFactoryOp->HasBlockedDatabases());
    gLiveDatabaseHashtable->Remove(Id());
  }

  // Match the IncreaseBusyCount in OpenDatabaseOp::EnsureDatabaseActor().
  DecreaseBusyCount();
}

bool Database::VerifyRequestParams(const DatabaseRequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != DatabaseRequestParams::T__None);

  switch (aParams.type()) {
    case DatabaseRequestParams::TCreateFileParams: {
      if (NS_WARN_IF(mFileHandleDisabled)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const CreateFileParams& params = aParams.get_CreateFileParams();

      if (NS_WARN_IF(params.name().IsEmpty())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

void Database::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!IsInvalidated()) {
    Invalidate();
  }
}

PBackgroundIDBDatabaseFileParent*
Database::AllocPBackgroundIDBDatabaseFileParent(const IPCBlob& aIPCBlob) {
  AssertIsOnBackgroundThread();

  RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(aIPCBlob);
  MOZ_ASSERT(blobImpl);

  RefPtr<FileInfo> fileInfo = GetBlob(aIPCBlob);
  RefPtr<DatabaseFile> actor;

  if (fileInfo) {
    actor = new DatabaseFile(fileInfo);
  } else {
    // This is a blob we haven't seen before.
    fileInfo = mFileManager->GetNewFileInfo();
    MOZ_ASSERT(fileInfo);

    actor = new DatabaseFile(blobImpl, fileInfo);
  }

  MOZ_ASSERT(actor);

  return actor.forget().take();
}

bool Database::DeallocPBackgroundIDBDatabaseFileParent(
    PBackgroundIDBDatabaseFileParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<DatabaseFile> actor = dont_AddRef(static_cast<DatabaseFile*>(aActor));
  return true;
}

PBackgroundIDBDatabaseRequestParent*
Database::AllocPBackgroundIDBDatabaseRequestParent(
    const DatabaseRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != DatabaseRequestParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  PBackgroundParent* backgroundActor = GetBackgroundParent();
  MOZ_ASSERT(backgroundActor);

  bool trustParams = !BackgroundParent::IsOtherProcessActor(backgroundActor);
#endif

  if (NS_WARN_IF(!trustParams && !VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<DatabaseOp> actor;

  switch (aParams.type()) {
    case DatabaseRequestParams::TCreateFileParams: {
      actor = new CreateFileOp(this, aParams);
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Database::RecvPBackgroundIDBDatabaseRequestConstructor(
    PBackgroundIDBDatabaseRequestParent* aActor,
    const DatabaseRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != DatabaseRequestParams::T__None);

  auto* op = static_cast<DatabaseOp*>(aActor);

  op->RunImmediately();

  return IPC_OK();
}

bool Database::DeallocPBackgroundIDBDatabaseRequestParent(
    PBackgroundIDBDatabaseRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<DatabaseOp> op = dont_AddRef(static_cast<DatabaseOp*>(aActor));
  return true;
}

PBackgroundIDBTransactionParent* Database::AllocPBackgroundIDBTransactionParent(
    const nsTArray<nsString>& aObjectStoreNames, const Mode& aMode) {
  AssertIsOnBackgroundThread();

  // Once a database is closed it must not try to open new transactions.
  if (NS_WARN_IF(mClosed)) {
    if (!mInvalidated) {
      ASSERT_UNLESS_FUZZING();
    }
    return nullptr;
  }

  if (NS_WARN_IF(aObjectStoreNames.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(aMode != IDBTransaction::READ_ONLY &&
                 aMode != IDBTransaction::READ_WRITE &&
                 aMode != IDBTransaction::READ_WRITE_FLUSH &&
                 aMode != IDBTransaction::CLEANUP)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // If this is a readwrite transaction to a chrome database make sure the child
  // has write access.
  if (NS_WARN_IF((aMode == IDBTransaction::READ_WRITE ||
                  aMode == IDBTransaction::READ_WRITE_FLUSH ||
                  aMode == IDBTransaction::CLEANUP) &&
                 mPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo &&
                 !mChromeWriteAccessAllowed)) {
    return nullptr;
  }

  const ObjectStoreTable& objectStores = mMetadata->mObjectStores;
  const uint32_t nameCount = aObjectStoreNames.Length();

  if (NS_WARN_IF(nameCount > objectStores.Count())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  FallibleTArray<RefPtr<FullObjectStoreMetadata>> fallibleObjectStores;
  if (NS_WARN_IF(!fallibleObjectStores.SetCapacity(nameCount, fallible))) {
    return nullptr;
  }

  for (uint32_t nameIndex = 0; nameIndex < nameCount; nameIndex++) {
    const nsString& name = aObjectStoreNames[nameIndex];

    if (nameIndex) {
      // Make sure that this name is sorted properly and not a duplicate.
      if (NS_WARN_IF(name <= aObjectStoreNames[nameIndex - 1])) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
    }

    for (auto iter = objectStores.ConstIter(); !iter.Done(); iter.Next()) {
      auto value = iter.Data();
      MOZ_ASSERT(iter.Key());

      if (name == value->mCommonMetadata.name() && !value->mDeleted) {
        if (NS_WARN_IF(!fallibleObjectStores.AppendElement(value, fallible))) {
          return nullptr;
        }
        break;
      }
    }
  }

  nsTArray<RefPtr<FullObjectStoreMetadata>> infallibleObjectStores;
  infallibleObjectStores.SwapElements(fallibleObjectStores);

  RefPtr<NormalTransaction> transaction =
      new NormalTransaction(this, aMode, infallibleObjectStores);

  MOZ_ASSERT(infallibleObjectStores.IsEmpty());

  return transaction.forget().take();
}

mozilla::ipc::IPCResult Database::RecvPBackgroundIDBTransactionConstructor(
    PBackgroundIDBTransactionParent* aActor,
    InfallibleTArray<nsString>&& aObjectStoreNames, const Mode& aMode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!aObjectStoreNames.IsEmpty());
  MOZ_ASSERT(aMode == IDBTransaction::READ_ONLY ||
             aMode == IDBTransaction::READ_WRITE ||
             aMode == IDBTransaction::READ_WRITE_FLUSH ||
             aMode == IDBTransaction::CLEANUP);
  MOZ_ASSERT(!mClosed);

  if (IsInvalidated()) {
    // This is an expected race. We don't want the child to die here, just don't
    // actually do any work.
    return IPC_OK();
  }

  if (!gConnectionPool) {
    gConnectionPool = new ConnectionPool();
  }

  auto* transaction = static_cast<NormalTransaction*>(aActor);

  RefPtr<StartTransactionOp> startOp = new StartTransactionOp(transaction);

  uint64_t transactionId = startOp->StartOnConnectionPool(
      GetLoggingInfo()->Id(), mMetadata->mDatabaseId,
      transaction->LoggingSerialNumber(), aObjectStoreNames,
      aMode != IDBTransaction::READ_ONLY);

  transaction->SetActive(transactionId);

  if (NS_WARN_IF(!RegisterTransaction(transaction))) {
    IDB_REPORT_INTERNAL_ERR();
    transaction->Abort(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, /* aForce */ false);
    return IPC_OK();
  }

  return IPC_OK();
}

bool Database::DeallocPBackgroundIDBTransactionParent(
    PBackgroundIDBTransactionParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<NormalTransaction> transaction =
      dont_AddRef(static_cast<NormalTransaction*>(aActor));
  return true;
}

PBackgroundIDBVersionChangeTransactionParent*
Database::AllocPBackgroundIDBVersionChangeTransactionParent(
    const uint64_t& aCurrentVersion, const uint64_t& aRequestedVersion,
    const int64_t& aNextObjectStoreId, const int64_t& aNextIndexId) {
  MOZ_CRASH(
      "PBackgroundIDBVersionChangeTransactionParent actors should be "
      "constructed manually!");
}

bool Database::DeallocPBackgroundIDBVersionChangeTransactionParent(
    PBackgroundIDBVersionChangeTransactionParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<VersionChangeTransaction> transaction =
      dont_AddRef(static_cast<VersionChangeTransaction*>(aActor));
  return true;
}

Database::PBackgroundMutableFileParent*
Database::AllocPBackgroundMutableFileParent(const nsString& aName,
                                            const nsString& aType) {
  MOZ_CRASH(
      "PBackgroundMutableFileParent actors should be constructed "
      "manually!");
}

bool Database::DeallocPBackgroundMutableFileParent(
    PBackgroundMutableFileParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<MutableFile> mutableFile =
      dont_AddRef(static_cast<MutableFile*>(aActor));
  return true;
}

mozilla::ipc::IPCResult Database::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBDatabaseParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Database::RecvBlocked() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mClosed)) {
    return IPC_FAIL_NO_REASON(this);
  }

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(this));
  MOZ_ASSERT(info->mWaitingFactoryOp);

  info->mWaitingFactoryOp->NoteDatabaseBlocked(this);

  return IPC_OK();
}

mozilla::ipc::IPCResult Database::RecvClose() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!CloseInternal())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

void Database::StartTransactionOp::RunOnConnectionThread() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(Transaction());
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));

  IDB_LOG_MARK(
      "IndexedDB %s: Parent Transaction[%lld]: "
      "Beginning database work",
      "IndexedDB %s: P T[%lld]: DB Start",
      IDB_LOG_ID_STRING(mBackgroundChildLoggingId), mLoggingSerialNumber);

  TransactionDatabaseOperationBase::RunOnConnectionThread();
}

nsresult Database::StartTransactionOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  Transaction()->SetActiveOnConnectionThread();

  if (Transaction()->GetMode() == IDBTransaction::CLEANUP) {
    nsresult rv = aConnection->DisableQuotaChecks();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (Transaction()->GetMode() != IDBTransaction::READ_ONLY) {
    nsresult rv = aConnection->BeginWriteTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult Database::StartTransactionOp::SendSuccessResult() {
  // We don't need to do anything here.
  return NS_OK;
}

bool Database::StartTransactionOp::SendFailureResult(
    nsresult /* aResultCode */) {
  IDB_REPORT_INTERNAL_ERR();

  // Abort the transaction.
  return false;
}

void Database::StartTransactionOp::Cleanup() {
#ifdef DEBUG
  // StartTransactionOp is not a normal database operation that is tied to an
  // actor. Do this to make our assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

/*******************************************************************************
 * TransactionBase
 ******************************************************************************/

TransactionBase::TransactionBase(Database* aDatabase, Mode aMode)
    : mDatabase(aDatabase),
      mTransactionId(0),
      mDatabaseId(aDatabase->Id()),
      mLoggingSerialNumber(
          aDatabase->GetLoggingInfo()->NextTransactionSN(aMode)),
      mActiveRequestCount(0),
      mInvalidatedOnAnyThread(false),
      mMode(aMode),
      mHasBeenActive(false),
      mHasBeenActiveOnConnectionThread(false),
      mActorDestroyed(false),
      mInvalidated(false),
      mResultCode(NS_OK),
      mCommitOrAbortReceived(false),
      mCommittedOrAborted(false),
      mForceAborted(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mLoggingSerialNumber);
}

TransactionBase::~TransactionBase() {
  MOZ_ASSERT(!mActiveRequestCount);
  MOZ_ASSERT(mActorDestroyed);
  MOZ_ASSERT_IF(mHasBeenActive, mCommittedOrAborted);
}

void TransactionBase::Abort(nsresult aResultCode, bool aForce) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));

  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = aResultCode;
  }

  if (aForce) {
    mForceAborted = true;
  }

  MaybeCommitOrAbort();
}

bool TransactionBase::RecvCommit() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  mCommitOrAbortReceived = true;

  MaybeCommitOrAbort();
  return true;
}

bool TransactionBase::RecvAbort(nsresult aResultCode) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(NS_SUCCEEDED(aResultCode))) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(NS_ERROR_GET_MODULE(aResultCode) !=
                 NS_ERROR_MODULE_DOM_INDEXEDDB)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  mCommitOrAbortReceived = true;

  Abort(aResultCode, /* aForce */ false);
  return true;
}

void TransactionBase::CommitOrAbort() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mCommittedOrAborted);

  mCommittedOrAborted = true;

  if (!mHasBeenActive) {
    return;
  }

  RefPtr<CommitOp> commitOp = new CommitOp(this, ClampResultCode(mResultCode));

  gConnectionPool->Finish(TransactionId(), commitOp);
}

already_AddRefed<FullObjectStoreMetadata>
TransactionBase::GetMetadataForObjectStoreId(int64_t aObjectStoreId) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aObjectStoreId);

  if (!aObjectStoreId) {
    return nullptr;
  }

  RefPtr<FullObjectStoreMetadata> metadata;
  if (!mDatabase->Metadata()->mObjectStores.Get(aObjectStoreId,
                                                getter_AddRefs(metadata)) ||
      metadata->mDeleted) {
    return nullptr;
  }

  MOZ_ASSERT(metadata->mCommonMetadata.id() == aObjectStoreId);

  return metadata.forget();
}

already_AddRefed<FullIndexMetadata> TransactionBase::GetMetadataForIndexId(
    FullObjectStoreMetadata* const aObjectStoreMetadata,
    int64_t aIndexId) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aIndexId);

  if (!aIndexId) {
    return nullptr;
  }

  RefPtr<FullIndexMetadata> metadata;
  if (!aObjectStoreMetadata->mIndexes.Get(aIndexId, getter_AddRefs(metadata)) ||
      metadata->mDeleted) {
    return nullptr;
  }

  MOZ_ASSERT(metadata->mCommonMetadata.id() == aIndexId);

  return metadata.forget();
}

void TransactionBase::NoteModifiedAutoIncrementObjectStore(
    FullObjectStoreMetadata* aMetadata) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aMetadata);

  if (!mModifiedAutoIncrementObjectStoreMetadataArray.Contains(aMetadata)) {
    mModifiedAutoIncrementObjectStoreMetadataArray.AppendElement(aMetadata);
  }
}

void TransactionBase::ForgetModifiedAutoIncrementObjectStore(
    FullObjectStoreMetadata* aMetadata) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aMetadata);

  mModifiedAutoIncrementObjectStoreMetadataArray.RemoveElement(aMetadata);
}

bool TransactionBase::VerifyRequestParams(const RequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  switch (aParams.type()) {
    case RequestParams::TObjectStoreAddParams: {
      const ObjectStoreAddPutParams& params =
          aParams.get_ObjectStoreAddParams().commonParams();
      if (NS_WARN_IF(!VerifyRequestParams(params))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStorePutParams: {
      const ObjectStoreAddPutParams& params =
          aParams.get_ObjectStorePutParams().commonParams();
      if (NS_WARN_IF(!VerifyRequestParams(params))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetParams: {
      const ObjectStoreGetParams& params = aParams.get_ObjectStoreGetParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetKeyParams: {
      const ObjectStoreGetKeyParams& params =
          aParams.get_ObjectStoreGetKeyParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetAllParams: {
      const ObjectStoreGetAllParams& params =
          aParams.get_ObjectStoreGetAllParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetAllKeysParams: {
      const ObjectStoreGetAllKeysParams& params =
          aParams.get_ObjectStoreGetAllKeysParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreDeleteParams: {
      if (NS_WARN_IF(mMode != IDBTransaction::READ_WRITE &&
                     mMode != IDBTransaction::READ_WRITE_FLUSH &&
                     mMode != IDBTransaction::CLEANUP &&
                     mMode != IDBTransaction::VERSION_CHANGE)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const ObjectStoreDeleteParams& params =
          aParams.get_ObjectStoreDeleteParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreClearParams: {
      if (NS_WARN_IF(mMode != IDBTransaction::READ_WRITE &&
                     mMode != IDBTransaction::READ_WRITE_FLUSH &&
                     mMode != IDBTransaction::CLEANUP &&
                     mMode != IDBTransaction::VERSION_CHANGE)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const ObjectStoreClearParams& params =
          aParams.get_ObjectStoreClearParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreCountParams: {
      const ObjectStoreCountParams& params =
          aParams.get_ObjectStoreCountParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetParams: {
      const IndexGetParams& params = aParams.get_IndexGetParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetKeyParams: {
      const IndexGetKeyParams& params = aParams.get_IndexGetKeyParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetAllParams: {
      const IndexGetAllParams& params = aParams.get_IndexGetAllParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetAllKeysParams: {
      const IndexGetAllKeysParams& params = aParams.get_IndexGetAllKeysParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexCountParams: {
      const IndexCountParams& params = aParams.get_IndexCountParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

bool TransactionBase::VerifyRequestParams(
    const SerializedKeyRange& aParams) const {
  AssertIsOnBackgroundThread();

  // XXX Check more here?

  if (aParams.isOnly()) {
    if (NS_WARN_IF(aParams.lower().IsUnset())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
    if (NS_WARN_IF(!aParams.upper().IsUnset())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
    if (NS_WARN_IF(aParams.lowerOpen())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
    if (NS_WARN_IF(aParams.upperOpen())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  } else if (NS_WARN_IF(aParams.lower().IsUnset() &&
                        aParams.upper().IsUnset())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  return true;
}

bool TransactionBase::VerifyRequestParams(
    const ObjectStoreAddPutParams& aParams) const {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mMode != IDBTransaction::READ_WRITE &&
                 mMode != IDBTransaction::READ_WRITE_FLUSH &&
                 mMode != IDBTransaction::VERSION_CHANGE)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  RefPtr<FullObjectStoreMetadata> objMetadata =
      GetMetadataForObjectStoreId(aParams.objectStoreId());
  if (NS_WARN_IF(!objMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(!aParams.cloneInfo().data().data.Size())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (objMetadata->mCommonMetadata.autoIncrement() &&
      objMetadata->mCommonMetadata.keyPath().IsValid() &&
      aParams.key().IsUnset()) {
    const SerializedStructuredCloneWriteInfo cloneInfo = aParams.cloneInfo();

    if (NS_WARN_IF(!cloneInfo.offsetToKeyProp())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(cloneInfo.data().data.Size() < sizeof(uint64_t))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(cloneInfo.offsetToKeyProp() >
                   (cloneInfo.data().data.Size() - sizeof(uint64_t)))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  } else if (NS_WARN_IF(aParams.cloneInfo().offsetToKeyProp())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const nsTArray<IndexUpdateInfo>& updates = aParams.indexUpdateInfos();

  for (uint32_t index = 0; index < updates.Length(); index++) {
    RefPtr<FullIndexMetadata> indexMetadata =
        GetMetadataForIndexId(objMetadata, updates[index].indexId());
    if (NS_WARN_IF(!indexMetadata)) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(updates[index].value().IsUnset())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    MOZ_ASSERT(!updates[index].value().GetBuffer().IsEmpty());
  }

  const nsTArray<FileAddInfo>& fileAddInfos = aParams.fileAddInfos();

  for (uint32_t index = 0; index < fileAddInfos.Length(); index++) {
    const FileAddInfo& fileAddInfo = fileAddInfos[index];

    const DatabaseOrMutableFile& file = fileAddInfo.file();
    MOZ_ASSERT(file.type() != DatabaseOrMutableFile::T__None);

    switch (fileAddInfo.type()) {
      case StructuredCloneFile::eBlob:
        if (NS_WARN_IF(
                file.type() !=
                DatabaseOrMutableFile::TPBackgroundIDBDatabaseFileParent)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        if (NS_WARN_IF(!file.get_PBackgroundIDBDatabaseFileParent())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        break;

      case StructuredCloneFile::eMutableFile: {
        if (NS_WARN_IF(file.type() !=
                       DatabaseOrMutableFile::TPBackgroundMutableFileParent)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        if (NS_WARN_IF(mDatabase->IsFileHandleDisabled())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        auto mutableFile =
            static_cast<MutableFile*>(file.get_PBackgroundMutableFileParent());

        if (NS_WARN_IF(!mutableFile)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        Database* database = mutableFile->GetDatabase();
        if (NS_WARN_IF(!database)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        if (NS_WARN_IF(database->Id() != mDatabase->Id())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        break;
      }

      case StructuredCloneFile::eStructuredClone:
        ASSERT_UNLESS_FUZZING();
        return false;

      case StructuredCloneFile::eWasmBytecode:
      case StructuredCloneFile::eWasmCompiled:
        if (NS_WARN_IF(
                file.type() !=
                DatabaseOrMutableFile::TPBackgroundIDBDatabaseFileParent)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        if (NS_WARN_IF(!file.get_PBackgroundIDBDatabaseFileParent())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        break;

      case StructuredCloneFile::eEndGuard:
        ASSERT_UNLESS_FUZZING();
        return false;

      default:
        MOZ_CRASH("Should never get here!");
    }
  }

  return true;
}

bool TransactionBase::VerifyRequestParams(
    const Maybe<SerializedKeyRange>& aParams) const {
  AssertIsOnBackgroundThread();

  if (aParams.isSome()) {
    if (NS_WARN_IF(!VerifyRequestParams(aParams.ref()))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  }

  return true;
}

void TransactionBase::NoteActiveRequest() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount < UINT64_MAX);

  mActiveRequestCount++;
}

void TransactionBase::NoteFinishedRequest() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount);

  mActiveRequestCount--;

  MaybeCommitOrAbort();
}

void TransactionBase::Invalidate() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mInvalidated == mInvalidatedOnAnyThread);

  if (!mInvalidated) {
    mInvalidated = true;
    mInvalidatedOnAnyThread = true;

    Abort(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR, /* aForce */ false);
  }
}

PBackgroundIDBRequestParent* TransactionBase::AllocRequest(
    const RequestParams& aParams, bool aTrustParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  aTrustParams = false;
#endif

  if (!aTrustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<NormalTransactionOp> actor;

  switch (aParams.type()) {
    case RequestParams::TObjectStoreAddParams:
    case RequestParams::TObjectStorePutParams:
      actor = new ObjectStoreAddOrPutRequestOp(this, aParams);
      break;

    case RequestParams::TObjectStoreGetParams:
      actor = new ObjectStoreGetRequestOp(this, aParams, /* aGetAll */ false);
      break;

    case RequestParams::TObjectStoreGetAllParams:
      actor = new ObjectStoreGetRequestOp(this, aParams, /* aGetAll */ true);
      break;

    case RequestParams::TObjectStoreGetKeyParams:
      actor =
          new ObjectStoreGetKeyRequestOp(this, aParams, /* aGetAll */ false);
      break;

    case RequestParams::TObjectStoreGetAllKeysParams:
      actor = new ObjectStoreGetKeyRequestOp(this, aParams, /* aGetAll */ true);
      break;

    case RequestParams::TObjectStoreDeleteParams:
      actor = new ObjectStoreDeleteRequestOp(
          this, aParams.get_ObjectStoreDeleteParams());
      break;

    case RequestParams::TObjectStoreClearParams:
      actor = new ObjectStoreClearRequestOp(
          this, aParams.get_ObjectStoreClearParams());
      break;

    case RequestParams::TObjectStoreCountParams:
      actor = new ObjectStoreCountRequestOp(
          this, aParams.get_ObjectStoreCountParams());
      break;

    case RequestParams::TIndexGetParams:
      actor = new IndexGetRequestOp(this, aParams, /* aGetAll */ false);
      break;

    case RequestParams::TIndexGetKeyParams:
      actor = new IndexGetKeyRequestOp(this, aParams, /* aGetAll */ false);
      break;

    case RequestParams::TIndexGetAllParams:
      actor = new IndexGetRequestOp(this, aParams, /* aGetAll */ true);
      break;

    case RequestParams::TIndexGetAllKeysParams:
      actor = new IndexGetKeyRequestOp(this, aParams, /* aGetAll */ true);
      break;

    case RequestParams::TIndexCountParams:
      actor = new IndexCountRequestOp(this, aParams);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool TransactionBase::StartRequest(PBackgroundIDBRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  auto* op = static_cast<NormalTransactionOp*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return false;
  }

  op->DispatchToConnectionPool();
  return true;
}

bool TransactionBase::DeallocRequest(PBackgroundIDBRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<NormalTransactionOp> actor =
      dont_AddRef(static_cast<NormalTransactionOp*>(aActor));
  return true;
}

PBackgroundIDBCursorParent* TransactionBase::AllocCursor(
    const OpenCursorParams& aParams, bool aTrustParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  aTrustParams = false;
#endif

  OpenCursorParams::Type type = aParams.type();
  RefPtr<FullObjectStoreMetadata> objectStoreMetadata;
  RefPtr<FullIndexMetadata> indexMetadata;
  Cursor::Direction direction;

  switch (type) {
    case OpenCursorParams::TObjectStoreOpenCursorParams: {
      const ObjectStoreOpenCursorParams& params =
          aParams.get_ObjectStoreOpenCursorParams();
      objectStoreMetadata = GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      if (aTrustParams &&
          NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      direction = params.direction();
      break;
    }

    case OpenCursorParams::TObjectStoreOpenKeyCursorParams: {
      const ObjectStoreOpenKeyCursorParams& params =
          aParams.get_ObjectStoreOpenKeyCursorParams();
      objectStoreMetadata = GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      if (aTrustParams &&
          NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      direction = params.direction();
      break;
    }

    case OpenCursorParams::TIndexOpenCursorParams: {
      const IndexOpenCursorParams& params = aParams.get_IndexOpenCursorParams();
      objectStoreMetadata = GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      if (aTrustParams &&
          NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      direction = params.direction();
      break;
    }

    case OpenCursorParams::TIndexOpenKeyCursorParams: {
      const IndexOpenKeyCursorParams& params =
          aParams.get_IndexOpenKeyCursorParams();
      objectStoreMetadata = GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      if (aTrustParams &&
          NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
      direction = params.direction();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<Cursor> actor =
      new Cursor(this, type, objectStoreMetadata, indexMetadata, direction);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool TransactionBase::StartCursor(PBackgroundIDBCursorParent* aActor,
                                  const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  auto* op = static_cast<Cursor*>(aActor);

  if (NS_WARN_IF(!op->Start(aParams))) {
    return false;
  }

  return true;
}

bool TransactionBase::DeallocCursor(PBackgroundIDBCursorParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Cursor> actor = dont_AddRef(static_cast<Cursor*>(aActor));
  return true;
}

/*******************************************************************************
 * NormalTransaction
 ******************************************************************************/

NormalTransaction::NormalTransaction(
    Database* aDatabase, TransactionBase::Mode aMode,
    nsTArray<RefPtr<FullObjectStoreMetadata>>& aObjectStores)
    : TransactionBase(aDatabase, aMode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!aObjectStores.IsEmpty());

  mObjectStores.SwapElements(aObjectStores);
}

bool NormalTransaction::IsSameProcessActor() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager()->Manager()->Manager();
  MOZ_ASSERT(actor);

  return !BackgroundParent::IsOtherProcessActor(actor);
}

void NormalTransaction::SendCompleteNotification(nsresult aResult) {
  AssertIsOnBackgroundThread();

  if (!IsActorDestroyed()) {
    Unused << SendComplete(aResult);
  }
}

void NormalTransaction::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();

  if (!mCommittedOrAborted) {
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mForceAborted = true;

    MaybeCommitOrAbort();
  }
}

mozilla::ipc::IPCResult NormalTransaction::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBTransactionParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NormalTransaction::RecvCommit() {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvCommit()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NormalTransaction::RecvAbort(
    const nsresult& aResultCode) {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvAbort(aResultCode)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PBackgroundIDBRequestParent*
NormalTransaction::AllocPBackgroundIDBRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return AllocRequest(aParams, IsSameProcessActor());
}

mozilla::ipc::IPCResult NormalTransaction::RecvPBackgroundIDBRequestConstructor(
    PBackgroundIDBRequestParent* aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (!StartRequest(aActor)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool NormalTransaction::DeallocPBackgroundIDBRequestParent(
    PBackgroundIDBRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocRequest(aActor);
}

PBackgroundIDBCursorParent* NormalTransaction::AllocPBackgroundIDBCursorParent(
    const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();

  return AllocCursor(aParams, IsSameProcessActor());
}

mozilla::ipc::IPCResult NormalTransaction::RecvPBackgroundIDBCursorConstructor(
    PBackgroundIDBCursorParent* aActor, const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  if (!StartCursor(aActor, aParams)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool NormalTransaction::DeallocPBackgroundIDBCursorParent(
    PBackgroundIDBCursorParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocCursor(aActor);
}

/*******************************************************************************
 * VersionChangeTransaction
 ******************************************************************************/

VersionChangeTransaction::VersionChangeTransaction(
    OpenDatabaseOp* aOpenDatabaseOp)
    : TransactionBase(aOpenDatabaseOp->mDatabase,
                      IDBTransaction::VERSION_CHANGE),
      mOpenDatabaseOp(aOpenDatabaseOp),
      mActorWasAlive(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aOpenDatabaseOp);
}

VersionChangeTransaction::~VersionChangeTransaction() {
#ifdef DEBUG
  // Silence the base class' destructor assertion if we never made this actor
  // live.
  FakeActorDestroyed();
#endif
}

bool VersionChangeTransaction::IsSameProcessActor() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager()->Manager()->Manager();
  MOZ_ASSERT(actor);

  return !BackgroundParent::IsOtherProcessActor(actor);
}

void VersionChangeTransaction::SetActorAlive() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!IsActorDestroyed());

  mActorWasAlive = true;

  // This reference will be absorbed by IPDL and released when the actor is
  // destroyed.
  AddRef();
}

bool VersionChangeTransaction::CopyDatabaseMetadata() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mOldMetadata);

  const RefPtr<FullDatabaseMetadata> origMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(origMetadata);

  RefPtr<FullDatabaseMetadata> newMetadata = origMetadata->Duplicate();
  if (NS_WARN_IF(!newMetadata)) {
    return false;
  }

  // Replace the live metadata with the new mutable copy.
  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(
      gLiveDatabaseHashtable->Get(origMetadata->mDatabaseId, &info));
  MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());
  MOZ_ASSERT(info->mMetadata == origMetadata);

  mOldMetadata = info->mMetadata.forget();
  info->mMetadata.swap(newMetadata);

  // Replace metadata pointers for all live databases.
  for (uint32_t count = info->mLiveDatabases.Length(), index = 0; index < count;
       index++) {
    info->mLiveDatabases[index]->mMetadata = info->mMetadata;
  }

  return true;
}

void VersionChangeTransaction::UpdateMetadata(nsresult aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(GetDatabase());
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(!!mActorWasAlive == !!mOpenDatabaseOp->mDatabase);
  MOZ_ASSERT_IF(mActorWasAlive, !mOpenDatabaseOp->mDatabaseId.IsEmpty());

  if (IsActorDestroyed() || !mActorWasAlive) {
    return;
  }

  RefPtr<FullDatabaseMetadata> oldMetadata;
  mOldMetadata.swap(oldMetadata);

  DatabaseActorInfo* info;
  if (!gLiveDatabaseHashtable->Get(oldMetadata->mDatabaseId, &info)) {
    return;
  }

  MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());

  if (NS_SUCCEEDED(aResult)) {
    // Remove all deleted objectStores and indexes, then mark immutable.
    for (auto objectStoreIter = info->mMetadata->mObjectStores.Iter();
         !objectStoreIter.Done(); objectStoreIter.Next()) {
      MOZ_ASSERT(objectStoreIter.Key());
      RefPtr<FullObjectStoreMetadata>& metadata = objectStoreIter.Data();
      MOZ_ASSERT(metadata);

      if (metadata->mDeleted) {
        objectStoreIter.Remove();
        continue;
      }

      for (auto indexIter = metadata->mIndexes.Iter(); !indexIter.Done();
           indexIter.Next()) {
        MOZ_ASSERT(indexIter.Key());
        RefPtr<FullIndexMetadata>& index = indexIter.Data();
        MOZ_ASSERT(index);

        if (index->mDeleted) {
          indexIter.Remove();
        }
      }
#ifdef DEBUG
      metadata->mIndexes.MarkImmutable();
#endif
    }
#ifdef DEBUG
    info->mMetadata->mObjectStores.MarkImmutable();
#endif
  } else {
    // Replace metadata pointers for all live databases.
    info->mMetadata = oldMetadata.forget();

    for (uint32_t count = info->mLiveDatabases.Length(), index = 0;
         index < count; index++) {
      info->mLiveDatabases[index]->mMetadata = info->mMetadata;
    }
  }
}

void VersionChangeTransaction::SendCompleteNotification(nsresult aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT_IF(!mActorWasAlive, NS_FAILED(mOpenDatabaseOp->mResultCode));
  MOZ_ASSERT_IF(!mActorWasAlive, mOpenDatabaseOp->mState >
                                     OpenDatabaseOp::State::SendingResults);

  RefPtr<OpenDatabaseOp> openDatabaseOp;
  mOpenDatabaseOp.swap(openDatabaseOp);

  if (!mActorWasAlive) {
    return;
  }

  if (NS_FAILED(aResult) && NS_SUCCEEDED(openDatabaseOp->mResultCode)) {
    // 3.3.1 Opening a database:
    // "If the upgrade transaction was aborted, run the steps for closing a
    //  database connection with connection, create and return a new AbortError
    //  exception and abort these steps."
    openDatabaseOp->mResultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  openDatabaseOp->mState = OpenDatabaseOp::State::SendingResults;

  if (!IsActorDestroyed()) {
    Unused << SendComplete(aResult);
  }

  MOZ_ALWAYS_SUCCEEDS(openDatabaseOp->Run());
}

void VersionChangeTransaction::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();

  if (!mCommittedOrAborted) {
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mForceAborted = true;

    MaybeCommitOrAbort();
  }
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBVersionChangeTransactionParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvCommit() {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvCommit()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvAbort(
    const nsresult& aResultCode) {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvAbort(aResultCode)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvCreateObjectStore(
    const ObjectStoreMetadata& aMetadata) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aMetadata.id())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const RefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);

  if (NS_WARN_IF(aMetadata.id() != dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  auto* foundMetadata = MetadataNameOrIdMatcher<FullObjectStoreMetadata>::Match(
      dbMetadata->mObjectStores, aMetadata.id(), aMetadata.name());

  if (NS_WARN_IF(foundMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> newMetadata = new FullObjectStoreMetadata();
  newMetadata->mCommonMetadata = aMetadata;
  newMetadata->mNextAutoIncrementId = aMetadata.autoIncrement() ? 1 : 0;
  newMetadata->mCommittedAutoIncrementId = newMetadata->mNextAutoIncrementId;

  if (NS_WARN_IF(!dbMetadata->mObjectStores.Put(aMetadata.id(), newMetadata,
                                                fallible))) {
    return IPC_FAIL_NO_REASON(this);
  }

  dbMetadata->mNextObjectStoreId++;

  RefPtr<CreateObjectStoreOp> op = new CreateObjectStoreOp(this, aMetadata);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvDeleteObjectStore(
    const int64_t& aObjectStoreId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const RefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);
  MOZ_ASSERT(dbMetadata->mNextObjectStoreId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundMetadata->mDeleted = true;

  bool isLastObjectStore = true;
  DebugOnly<bool> foundTargetId = false;
  for (auto iter = dbMetadata->mObjectStores.Iter(); !iter.Done();
       iter.Next()) {
    if (uint64_t(aObjectStoreId) == iter.Key()) {
      foundTargetId = true;
    } else if (!iter.UserData()->mDeleted) {
      isLastObjectStore = false;
      break;
    }
  }
  MOZ_ASSERT_IF(isLastObjectStore, foundTargetId);

  RefPtr<DeleteObjectStoreOp> op =
      new DeleteObjectStoreOp(this, foundMetadata, isLastObjectStore);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvRenameObjectStore(
    const int64_t& aObjectStoreId, const nsString& aName) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const RefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);
  MOZ_ASSERT(dbMetadata->mNextObjectStoreId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundMetadata->mCommonMetadata.name() = aName;

  RefPtr<RenameObjectStoreOp> renameOp =
      new RenameObjectStoreOp(this, foundMetadata);

  if (NS_WARN_IF(!renameOp->Init(this))) {
    renameOp->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  renameOp->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvCreateIndex(
    const int64_t& aObjectStoreId, const IndexMetadata& aMetadata) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!aMetadata.id())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const RefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);

  if (NS_WARN_IF(aMetadata.id() != dbMetadata->mNextIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullIndexMetadata> foundIndexMetadata =
      MetadataNameOrIdMatcher<FullIndexMetadata>::Match(
          foundObjectStoreMetadata->mIndexes, aMetadata.id(), aMetadata.name());

  if (NS_WARN_IF(foundIndexMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullIndexMetadata> newMetadata = new FullIndexMetadata();
  newMetadata->mCommonMetadata = aMetadata;

  if (NS_WARN_IF(!foundObjectStoreMetadata->mIndexes.Put(
          aMetadata.id(), newMetadata, fallible))) {
    return IPC_FAIL_NO_REASON(this);
  }

  dbMetadata->mNextIndexId++;

  RefPtr<CreateIndexOp> op = new CreateIndexOp(this, aObjectStoreId, aMetadata);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvDeleteIndex(
    const int64_t& aObjectStoreId, const int64_t& aIndexId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!aIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const RefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);
  MOZ_ASSERT(dbMetadata->mNextObjectStoreId > 0);
  MOZ_ASSERT(dbMetadata->mNextIndexId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aIndexId >= dbMetadata->mNextIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullIndexMetadata> foundIndexMetadata =
      GetMetadataForIndexId(foundObjectStoreMetadata, aIndexId);

  if (NS_WARN_IF(!foundIndexMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundIndexMetadata->mDeleted = true;

  bool isLastIndex = true;
  DebugOnly<bool> foundTargetId = false;
  for (auto iter = foundObjectStoreMetadata->mIndexes.ConstIter(); !iter.Done();
       iter.Next()) {
    if (uint64_t(aIndexId) == iter.Key()) {
      foundTargetId = true;
    } else if (!iter.UserData()->mDeleted) {
      isLastIndex = false;
      break;
    }
  }
  MOZ_ASSERT_IF(isLastIndex, foundTargetId);

  RefPtr<DeleteIndexOp> op = new DeleteIndexOp(
      this, aObjectStoreId, aIndexId,
      foundIndexMetadata->mCommonMetadata.unique(), isLastIndex);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvRenameIndex(
    const int64_t& aObjectStoreId, const int64_t& aIndexId,
    const nsString& aName) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!aIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const RefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);
  MOZ_ASSERT(dbMetadata->mNextObjectStoreId > 0);
  MOZ_ASSERT(dbMetadata->mNextIndexId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aIndexId >= dbMetadata->mNextIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullIndexMetadata> foundIndexMetadata =
      GetMetadataForIndexId(foundObjectStoreMetadata, aIndexId);

  if (NS_WARN_IF(!foundIndexMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundIndexMetadata->mCommonMetadata.name() = aName;

  RefPtr<RenameIndexOp> renameOp =
      new RenameIndexOp(this, foundIndexMetadata, aObjectStoreId);

  if (NS_WARN_IF(!renameOp->Init(this))) {
    renameOp->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  renameOp->DispatchToConnectionPool();

  return IPC_OK();
}

PBackgroundIDBRequestParent*
VersionChangeTransaction::AllocPBackgroundIDBRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return AllocRequest(aParams, IsSameProcessActor());
}

mozilla::ipc::IPCResult
VersionChangeTransaction::RecvPBackgroundIDBRequestConstructor(
    PBackgroundIDBRequestParent* aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (!StartRequest(aActor)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool VersionChangeTransaction::DeallocPBackgroundIDBRequestParent(
    PBackgroundIDBRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocRequest(aActor);
}

PBackgroundIDBCursorParent*
VersionChangeTransaction::AllocPBackgroundIDBCursorParent(
    const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();

  return AllocCursor(aParams, IsSameProcessActor());
}

mozilla::ipc::IPCResult
VersionChangeTransaction::RecvPBackgroundIDBCursorConstructor(
    PBackgroundIDBCursorParent* aActor, const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  if (!StartCursor(aActor, aParams)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool VersionChangeTransaction::DeallocPBackgroundIDBCursorParent(
    PBackgroundIDBCursorParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocCursor(aActor);
}

/*******************************************************************************
 * Cursor
 ******************************************************************************/

Cursor::Cursor(TransactionBase* aTransaction, Type aType,
               FullObjectStoreMetadata* aObjectStoreMetadata,
               FullIndexMetadata* aIndexMetadata, Direction aDirection)
    : mTransaction(aTransaction),
      mBackgroundParent(nullptr),
      mObjectStoreMetadata(aObjectStoreMetadata),
      mIndexMetadata(aIndexMetadata),
      mObjectStoreId(aObjectStoreMetadata->mCommonMetadata.id()),
      mIndexId(aIndexMetadata ? aIndexMetadata->mCommonMetadata.id() : 0),
      mCurrentlyRunningOp(nullptr),
      mType(aType),
      mDirection(aDirection),
      mUniqueIndex(aIndexMetadata ? aIndexMetadata->mCommonMetadata.unique()
                                  : false),
      mIsSameProcessActor(!BackgroundParent::IsOtherProcessActor(
          aTransaction->GetBackgroundParent())),
      mActorDestroyed(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(aType != OpenCursorParams::T__None);
  MOZ_ASSERT(aObjectStoreMetadata);
  MOZ_ASSERT_IF(aType == OpenCursorParams::TIndexOpenCursorParams ||
                    aType == OpenCursorParams::TIndexOpenKeyCursorParams,
                aIndexMetadata);

  if (mType == OpenCursorParams::TObjectStoreOpenCursorParams ||
      mType == OpenCursorParams::TIndexOpenCursorParams) {
    mDatabase = aTransaction->GetDatabase();
    MOZ_ASSERT(mDatabase);

    mFileManager = mDatabase->GetFileManager();
    MOZ_ASSERT(mFileManager);

    mBackgroundParent = aTransaction->GetBackgroundParent();
    MOZ_ASSERT(mBackgroundParent);
  }

  if (aIndexMetadata) {
    mLocale = aIndexMetadata->mCommonMetadata.locale();
  }

  static_assert(
      OpenCursorParams::T__None == 0 && OpenCursorParams::T__Last == 4,
      "Lots of code here assumes only four types of cursors!");
}

bool Cursor::VerifyRequestParams(const CursorRequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);
  MOZ_ASSERT(mObjectStoreMetadata);
  MOZ_ASSERT_IF(mType == OpenCursorParams::TIndexOpenCursorParams ||
                    mType == OpenCursorParams::TIndexOpenKeyCursorParams,
                mIndexMetadata);

#ifdef DEBUG
  {
    RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        mTransaction->GetMetadataForObjectStoreId(mObjectStoreId);
    if (objectStoreMetadata) {
      MOZ_ASSERT(objectStoreMetadata == mObjectStoreMetadata);
    } else {
      MOZ_ASSERT(mObjectStoreMetadata->mDeleted);
    }

    if (objectStoreMetadata &&
        (mType == OpenCursorParams::TIndexOpenCursorParams ||
         mType == OpenCursorParams::TIndexOpenKeyCursorParams)) {
      RefPtr<FullIndexMetadata> indexMetadata =
          mTransaction->GetMetadataForIndexId(objectStoreMetadata, mIndexId);
      if (indexMetadata) {
        MOZ_ASSERT(indexMetadata == mIndexMetadata);
      } else {
        MOZ_ASSERT(mIndexMetadata->mDeleted);
      }
    }
  }
#endif

  if (NS_WARN_IF(mObjectStoreMetadata->mDeleted) ||
      (mIndexMetadata && NS_WARN_IF(mIndexMetadata->mDeleted))) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const Key& sortKey = IsLocaleAware() ? mSortKey : mKey;

  switch (aParams.type()) {
    case CursorRequestParams::TContinueParams: {
      const Key& key = aParams.get_ContinueParams().key();
      if (!key.IsUnset()) {
        switch (mDirection) {
          case IDBCursor::NEXT:
          case IDBCursor::NEXT_UNIQUE:
            if (NS_WARN_IF(key <= sortKey)) {
              ASSERT_UNLESS_FUZZING();
              return false;
            }
            break;

          case IDBCursor::PREV:
          case IDBCursor::PREV_UNIQUE:
            if (NS_WARN_IF(key >= sortKey)) {
              ASSERT_UNLESS_FUZZING();
              return false;
            }
            break;

          default:
            MOZ_CRASH("Should never get here!");
        }
      }
      break;
    }

    case CursorRequestParams::TContinuePrimaryKeyParams: {
      const Key& key = aParams.get_ContinuePrimaryKeyParams().key();
      const Key& primaryKey =
          aParams.get_ContinuePrimaryKeyParams().primaryKey();
      MOZ_ASSERT(!key.IsUnset());
      MOZ_ASSERT(!primaryKey.IsUnset());
      switch (mDirection) {
        case IDBCursor::NEXT:
          if (NS_WARN_IF(key < sortKey ||
                         (key == sortKey && primaryKey <= mObjectKey))) {
            ASSERT_UNLESS_FUZZING();
            return false;
          }
          break;

        case IDBCursor::PREV:
          if (NS_WARN_IF(key > sortKey ||
                         (key == sortKey && primaryKey >= mObjectKey))) {
            ASSERT_UNLESS_FUZZING();
            return false;
          }
          break;

        default:
          MOZ_CRASH("Should never get here!");
      }
      break;
    }

    case CursorRequestParams::TAdvanceParams:
      if (NS_WARN_IF(!aParams.get_AdvanceParams().count())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

bool Cursor::Start(const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() == mType);
  MOZ_ASSERT(!mActorDestroyed);

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const Maybe<SerializedKeyRange>& optionalKeyRange =
      mType == OpenCursorParams::TObjectStoreOpenCursorParams
          ? aParams.get_ObjectStoreOpenCursorParams().optionalKeyRange()
          : mType == OpenCursorParams::TObjectStoreOpenKeyCursorParams
                ? aParams.get_ObjectStoreOpenKeyCursorParams()
                      .optionalKeyRange()
                : mType == OpenCursorParams::TIndexOpenCursorParams
                      ? aParams.get_IndexOpenCursorParams().optionalKeyRange()
                      : aParams.get_IndexOpenKeyCursorParams()
                            .optionalKeyRange();

  RefPtr<OpenOp> openOp = new OpenOp(this, optionalKeyRange);

  if (NS_WARN_IF(!openOp->Init(mTransaction))) {
    openOp->Cleanup();
    return false;
  }

  openOp->DispatchToConnectionPool();
  mCurrentlyRunningOp = openOp;

  return true;
}

void Cursor::SendResponseInternal(
    CursorResponse& aResponse,
    const nsTArray<FallibleTArray<StructuredCloneFile>>& aFiles) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tnsresult,
                NS_FAILED(aResponse.get_nsresult()));
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tnsresult,
                NS_ERROR_GET_MODULE(aResponse.get_nsresult()) ==
                    NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tvoid_t, mKey.IsUnset());
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tvoid_t,
                mRangeKey.IsUnset());
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tvoid_t,
                mObjectKey.IsUnset());
  MOZ_ASSERT_IF(
      aResponse.type() == CursorResponse::Tnsresult ||
          aResponse.type() == CursorResponse::Tvoid_t ||
          aResponse.type() == CursorResponse::TObjectStoreKeyCursorResponse ||
          aResponse.type() == CursorResponse::TIndexKeyCursorResponse,
      aFiles.IsEmpty());
  MOZ_ASSERT(!mActorDestroyed);
  MOZ_ASSERT(mCurrentlyRunningOp);

  for (size_t i = 0; i < aFiles.Length(); ++i) {
    const auto& files = aFiles[i];
    if (!files.IsEmpty()) {
      MOZ_ASSERT(aResponse.type() ==
                     CursorResponse::TArrayOfObjectStoreCursorResponse ||
                 aResponse.type() == CursorResponse::TIndexCursorResponse);
      MOZ_ASSERT(mDatabase);
      MOZ_ASSERT(mBackgroundParent);

      FallibleTArray<SerializedStructuredCloneFile> serializedFiles;
      nsresult rv = SerializeStructuredCloneFiles(
          mBackgroundParent, mDatabase, files,
          /* aForPreprocess */ false, serializedFiles);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aResponse = ClampResultCode(rv);
        break;
      }

      SerializedStructuredCloneReadInfo* serializedInfo = nullptr;
      switch (aResponse.type()) {
        case CursorResponse::TArrayOfObjectStoreCursorResponse: {
          auto& responses = aResponse.get_ArrayOfObjectStoreCursorResponse();
          MOZ_ASSERT(i < responses.Length());
          serializedInfo = &responses[i].cloneInfo();
          break;
        }

        case CursorResponse::TIndexCursorResponse:
          MOZ_ASSERT(i == 0);
          serializedInfo = &aResponse.get_IndexCursorResponse().cloneInfo();
          break;

        default:
          MOZ_CRASH("Should never get here!");
      }

      MOZ_ASSERT(serializedInfo);
      MOZ_ASSERT(serializedInfo->files().IsEmpty());

      serializedInfo->files().SwapElements(serializedFiles);
    }
  }

  // Work around the deleted function by casting to the base class.
  auto* base = static_cast<PBackgroundIDBCursorParent*>(this);
  if (!base->SendResponse(aResponse)) {
    NS_WARNING("Failed to send response!");
  }

  mCurrentlyRunningOp = nullptr;
}

void Cursor::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (mCurrentlyRunningOp) {
    mCurrentlyRunningOp->NoteActorDestroyed();
  }

  mBackgroundParent = nullptr;

  mObjectStoreMetadata = nullptr;
  mIndexMetadata = nullptr;
}

mozilla::ipc::IPCResult Cursor::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBCursorParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Cursor::RecvContinue(
    const CursorRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);
  MOZ_ASSERT(!mActorDestroyed);
  MOZ_ASSERT(mObjectStoreMetadata);
  MOZ_ASSERT_IF(mType == OpenCursorParams::TIndexOpenCursorParams ||
                    mType == OpenCursorParams::TIndexOpenKeyCursorParams,
                mIndexMetadata);

  const bool trustParams =
#ifdef DEBUG
      // Always verify parameters in DEBUG builds!
      false
#else
      mIsSameProcessActor
#endif
      ;

  if (!trustParams && !VerifyRequestParams(aParams)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mTransaction->mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<ContinueOp> continueOp = new ContinueOp(this, aParams);
  if (NS_WARN_IF(!continueOp->Init(mTransaction))) {
    continueOp->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  continueOp->DispatchToConnectionPool();
  mCurrentlyRunningOp = continueOp;

  return IPC_OK();
}

/*******************************************************************************
 * FileManager
 ******************************************************************************/

FileManager::FileManager(PersistenceType aPersistenceType,
                         const nsACString& aGroup, const nsACString& aOrigin,
                         const nsAString& aDatabaseName, bool aEnforcingQuota)
    : mPersistenceType(aPersistenceType),
      mGroup(aGroup),
      mOrigin(aOrigin),
      mDatabaseName(aDatabaseName),
      mLastFileId(0),
      mEnforcingQuota(aEnforcingQuota),
      mInvalidated(false) {}

nsresult FileManager::Init(nsIFile* aDirectory,
                           mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aConnection);

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = aDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aDirectory->GetPath(mDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> journalDirectory;
  rv = aDirectory->Clone(getter_AddRefs(journalDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = journalDirectory->Append(NS_LITERAL_STRING(JOURNAL_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = journalDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = journalDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return NS_ERROR_FAILURE;
    }
  }

  rv = journalDirectory->GetPath(mJournalDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING("SELECT id, refcount "
                                                       "FROM file"),
                                    getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    int64_t id;
    rv = stmt->GetInt64(0, &id);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int32_t refcount;
    rv = stmt->GetInt32(1, &refcount);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(refcount > 0);

    RefPtr<FileInfo> fileInfo = FileInfo::Create(this, id);
    fileInfo->mDBRefCnt = static_cast<nsrefcnt>(refcount);

    mFileInfos.Put(id, fileInfo);

    mLastFileId = std::max(id, mLastFileId);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult FileManager::Invalidate() {
  if (IndexedDatabaseManager::IsClosed()) {
    MOZ_ASSERT(false, "Shouldn't be called after shutdown!");
    return NS_ERROR_UNEXPECTED;
  }

  MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

  MOZ_ASSERT(!mInvalidated);
  mInvalidated = true;

  for (auto iter = mFileInfos.Iter(); !iter.Done(); iter.Next()) {
    FileInfo* info = iter.Data();
    MOZ_ASSERT(info);

    if (!info->LockedClearDBRefs()) {
      iter.Remove();
    }
  }

  return NS_OK;
}

already_AddRefed<nsIFile> FileManager::GetDirectory() {
  return GetFileForPath(mDirectoryPath);
}

already_AddRefed<nsIFile> FileManager::GetCheckedDirectory() {
  nsCOMPtr<nsIFile> directory = GetDirectory();
  if (NS_WARN_IF(!directory)) {
    return nullptr;
  }

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(directory->Exists(&exists)));
  MOZ_ASSERT(exists);

  DebugOnly<bool> isDirectory;
  MOZ_ASSERT(NS_SUCCEEDED(directory->IsDirectory(&isDirectory)));
  MOZ_ASSERT(isDirectory);

  return directory.forget();
}

already_AddRefed<nsIFile> FileManager::GetJournalDirectory() {
  return GetFileForPath(mJournalDirectoryPath);
}

already_AddRefed<nsIFile> FileManager::EnsureJournalDirectory() {
  // This can happen on the IO or on a transaction thread.
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIFile> journalDirectory = GetFileForPath(mJournalDirectoryPath);
  if (NS_WARN_IF(!journalDirectory)) {
    return nullptr;
  }

  bool exists;
  nsresult rv = journalDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  if (exists) {
    bool isDirectory;
    rv = journalDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return nullptr;
    }
  } else {
    rv = journalDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  return journalDirectory.forget();
}

already_AddRefed<FileInfo> FileManager::GetFileInfo(int64_t aId) {
  if (IndexedDatabaseManager::IsClosed()) {
    MOZ_ASSERT(false, "Shouldn't be called after shutdown!");
    return nullptr;
  }

  FileInfo* fileInfo;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());
    fileInfo = mFileInfos.Get(aId);
  }

  RefPtr<FileInfo> result = fileInfo;
  return result.forget();
}

already_AddRefed<FileInfo> FileManager::GetNewFileInfo() {
  MOZ_ASSERT(!IndexedDatabaseManager::IsClosed());

  FileInfo* fileInfo;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    int64_t id = mLastFileId + 1;

    fileInfo = FileInfo::Create(this, id);

    mFileInfos.Put(id, fileInfo);

    mLastFileId = id;
  }

  RefPtr<FileInfo> result = fileInfo;
  return result.forget();
}

// static
already_AddRefed<nsIFile> FileManager::GetFileForId(nsIFile* aDirectory,
                                                    int64_t aId) {
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aId > 0);

  nsAutoString id;
  id.AppendInt(aId);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  rv = file->Append(id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return file.forget();
}

// static
already_AddRefed<nsIFile> FileManager::GetCheckedFileForId(nsIFile* aDirectory,
                                                           int64_t aId) {
  nsCOMPtr<nsIFile> file = GetFileForId(aDirectory, aId);
  if (NS_WARN_IF(!file)) {
    return nullptr;
  }

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(file->Exists(&exists)));
  MOZ_ASSERT(exists);

  DebugOnly<bool> isFile;
  MOZ_ASSERT(NS_SUCCEEDED(file->IsFile(&isFile)));
  MOZ_ASSERT(isFile);

  return file.forget();
}

// static
nsresult FileManager::InitDirectory(nsIFile* aDirectory, nsIFile* aDatabaseFile,
                                    PersistenceType aPersistenceType,
                                    const nsACString& aGroup,
                                    const nsACString& aOrigin,
                                    uint32_t aTelemetryId) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aDatabaseFile);

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  bool isDirectory;
  rv = aDirectory->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!isDirectory)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> journalDirectory;
  rv = aDirectory->Clone(getter_AddRefs(journalDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = journalDirectory->Append(NS_LITERAL_STRING(JOURNAL_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = journalDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = journalDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDirectoryEnumerator> entries;
    rv = journalDirectory->GetDirectoryEntries(getter_AddRefs(entries));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasElements;
    rv = entries->HasMoreElements(&hasElements);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (hasElements) {
      nsCOMPtr<mozIStorageConnection> connection;
      rv = CreateStorageConnection(aDatabaseFile, aDirectory, VoidString(),
                                   aPersistenceType, aGroup, aOrigin,
                                   aTelemetryId, getter_AddRefs(connection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mozStorageTransaction transaction(connection, false);

      rv = connection->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("CREATE VIRTUAL TABLE fs USING filesystem;"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<mozIStorageStatement> stmt;
      rv = connection->CreateStatement(
          NS_LITERAL_CSTRING("SELECT name, (name IN (SELECT id FROM file)) "
                             "FROM fs "
                             "WHERE path = :path"),
          getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsString path;
      rv = journalDirectory->GetPath(path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      bool hasResult;
      while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
        nsString name;
        rv = stmt->GetString(0, name);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        int32_t flag = stmt->AsInt32(1);

        if (!flag) {
          nsCOMPtr<nsIFile> file;
          rv = aDirectory->Clone(getter_AddRefs(file));
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          rv = file->Append(name);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          if (NS_FAILED(file->Remove(false))) {
            NS_WARNING("Failed to remove orphaned file!");
          }
        }

        nsCOMPtr<nsIFile> journalFile;
        rv = journalDirectory->Clone(getter_AddRefs(journalFile));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = journalFile->Append(name);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (NS_FAILED(journalFile->Remove(false))) {
          NS_WARNING("Failed to remove journal file!");
        }
      }

      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DROP TABLE fs;"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = transaction.Commit();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

// static
nsresult FileManager::GetUsage(nsIFile* aDirectory, uint64_t* aUsage) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aUsage);

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    *aUsage = 0;
    return NS_OK;
  }

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t usage = 0;

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
         file) {
    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (leafName.EqualsLiteral(JOURNAL_DIRECTORY_NAME)) {
      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    UsageInfo::IncrementUsage(&usage, uint64_t(fileSize));
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aUsage = usage;
  return NS_OK;
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;

QuotaClient::QuotaClient()
    : mDeleteTimer(NS_NewTimer()), mShutdownRequested(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!sInstance, "We expect this to be a singleton!");
  MOZ_ASSERT(!gTelemetryIdMutex);

  // Always create this so that later access to gTelemetryIdHashtable can be
  // properly synchronized.
  gTelemetryIdMutex = new Mutex("IndexedDB gTelemetryIdMutex");

  sInstance = this;
}

QuotaClient::~QuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this, "We expect this to be a singleton!");
  MOZ_ASSERT(gTelemetryIdMutex);
  MOZ_ASSERT(!mMaintenanceThreadPool);

  // No one else should be able to touch gTelemetryIdHashtable now that the
  // QuotaClient has gone away.
  gTelemetryIdHashtable = nullptr;
  gTelemetryIdMutex = nullptr;

  sInstance = nullptr;
}

nsresult QuotaClient::AsyncDeleteFile(FileManager* aFileManager,
                                      int64_t aFileId) {
  AssertIsOnBackgroundThread();

  if (mShutdownRequested) {
    // Whoops! We want to delete an IndexedDB disk-backed File but it's too late
    // to actually delete the file! This means we're going to "leak" the file
    // and leave it around when we shouldn't! (The file will stay around until
    // next storage initialization is triggered when the app is started again).
    // Fixing this is tracked by bug 1539377.

    return NS_OK;
  }

  MOZ_ASSERT(mDeleteTimer);
  MOZ_ALWAYS_SUCCEEDS(mDeleteTimer->Cancel());

  nsresult rv = mDeleteTimer->InitWithNamedFuncCallback(
      DeleteTimerCallback, this, kDeleteTimeoutMs, nsITimer::TYPE_ONE_SHOT,
      "dom::indexeddb::QuotaClient::AsyncDeleteFile");
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<int64_t>* array;
  if (!mPendingDeleteInfos.Get(aFileManager, &array)) {
    array = new nsTArray<int64_t>();
    mPendingDeleteInfos.Put(aFileManager, array);
  }

  array->AppendElement(aFileId);

  return NS_OK;
}

nsresult QuotaClient::FlushPendingFileDeletions() {
  AssertIsOnBackgroundThread();

  nsresult rv = mDeleteTimer->Cancel();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DeleteTimerCallback(mDeleteTimer, this);

  return NS_OK;
}

nsThreadPool* QuotaClient::GetOrCreateThreadPool() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  if (!mMaintenanceThreadPool) {
    RefPtr<nsThreadPool> threadPool = new nsThreadPool();

    // PR_GetNumberOfProcessors() can return -1 on error, so make sure we
    // don't set some huge number here. We add 2 in case some threads block on
    // the disk I/O.
    const uint32_t threadCount =
        std::max(int32_t(PR_GetNumberOfProcessors()), int32_t(1)) + 2;

    MOZ_ALWAYS_SUCCEEDS(threadPool->SetThreadLimit(threadCount));

    // Don't keep more than one idle thread.
    MOZ_ALWAYS_SUCCEEDS(threadPool->SetIdleThreadLimit(1));

    // Don't keep idle threads alive very long.
    MOZ_ALWAYS_SUCCEEDS(threadPool->SetIdleThreadTimeout(5 * PR_MSEC_PER_SEC));

    MOZ_ALWAYS_SUCCEEDS(
        threadPool->SetName(NS_LITERAL_CSTRING("IndexedDB Mnt")));

    mMaintenanceThreadPool = std::move(threadPool);
  }

  return mMaintenanceThreadPool;
}

mozilla::dom::quota::Client::Type QuotaClient::GetType() {
  return QuotaClient::IDB;
}

nsresult QuotaClient::UpgradeStorageFrom1_0To2_0(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  AtomicBool dummy(false);
  AutoTArray<nsString, 20> subdirsToProcess;
  nsTHashtable<nsStringHashKey> databaseFilenames(20);
  nsresult rv = GetDatabaseFilenames(aDirectory,
                                     /* aCanceled */ dummy,
                                     /* aForUpgrade */ true, subdirsToProcess,
                                     databaseFilenames);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const NS_ConvertASCIItoUTF16 filesSuffix(
      kFileManagerDirectoryNameSuffix,
      LiteralStringLength(kFileManagerDirectoryNameSuffix));

  for (uint32_t count = subdirsToProcess.Length(), i = 0; i < count; i++) {
    const nsString& subdirName = subdirsToProcess[i];

    // If the directory has the correct suffix then it should exist in
    // databaseFilenames.
    nsDependentSubstring subdirNameBase;
    if (GetBaseFilename(subdirName, filesSuffix, subdirNameBase)) {
      Unused << NS_WARN_IF(!databaseFilenames.GetEntry(subdirNameBase));

      continue;
    }

    // The directory didn't have the right suffix but we might need to rename
    // it. Check to see if we have a database that references this directory.
    nsString subdirNameWithSuffix;
    if (databaseFilenames.GetEntry(subdirName)) {
      subdirNameWithSuffix = subdirName + filesSuffix;
    } else {
      // Windows doesn't allow a directory to end with a dot ('.'), so we have
      // to check that possibility here too.
      // We do this on all platforms, because the origin directory may have
      // been created on Windows and now accessed on different OS.
      nsString subdirNameWithDot = subdirName + NS_LITERAL_STRING(".");
      if (NS_WARN_IF(!databaseFilenames.GetEntry(subdirNameWithDot))) {
        continue;
      }
      subdirNameWithSuffix = subdirNameWithDot + filesSuffix;
    }

    // We do have a database that uses this directory so we should rename it
    // now. However, first check to make sure that we're not overwriting
    // something else.
    nsCOMPtr<nsIFile> subdir;
    rv = aDirectory->Clone(getter_AddRefs(subdir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = subdir->Append(subdirNameWithSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool exists;
    rv = subdir->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (exists) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    rv = aDirectory->Clone(getter_AddRefs(subdir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = subdir->Append(subdirName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    DebugOnly<bool> isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(subdir->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory);

    rv = subdir->RenameTo(nullptr, subdirNameWithSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult QuotaClient::InitOrigin(PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 const AtomicBool& aCanceled,
                                 UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> directory;
  nsresult rv =
      GetDirectory(aPersistenceType, aOrigin, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_GetDirectory);
    return rv;
  }

  // We need to see if there are any files in the directory already. If they
  // are database files then we need to cleanup stored files (if it's needed)
  // and also get the usage.

  AutoTArray<nsString, 20> subdirsToProcess;
  nsTHashtable<nsStringHashKey> databaseFilenames(20);
  nsTHashtable<nsStringHashKey> obsoleteFilenames;
  rv = GetDatabaseFilenames(directory, aCanceled,
                            /* aForUpgrade */ false, subdirsToProcess,
                            databaseFilenames, &obsoleteFilenames);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_GetDBFilenames);
    return rv;
  }

  const NS_ConvertASCIItoUTF16 filesSuffix(
      kFileManagerDirectoryNameSuffix,
      LiteralStringLength(kFileManagerDirectoryNameSuffix));

  for (uint32_t count = subdirsToProcess.Length(), i = 0; i < count; i++) {
    const nsString& subdirName = subdirsToProcess[i];

    // The directory must have the correct suffix.
    nsDependentSubstring subdirNameBase;
    if (NS_WARN_IF(!GetBaseFilename(subdirName, filesSuffix, subdirNameBase))) {
      // If there is an unexpected directory in the idb directory, trying to
      // delete at first instead of breaking the whole initialization.
      if (NS_WARN_IF(NS_FAILED(DeleteFilesNoQuota(directory, subdirName)))) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_GetBaseFilename);
        return NS_ERROR_UNEXPECTED;
      }

      continue;
    }

    if (obsoleteFilenames.Contains(subdirNameBase)) {
      rv = RemoveDatabaseFilesAndDirectory(directory, subdirNameBase, nullptr,
                                           aPersistenceType, aGroup, aOrigin,
                                           EmptyString());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // If we somehow running into here, it probably means we are in a
        // serious situation. e.g. Filesystem corruption.
        // Will handle this in bug 1521541.
        REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_RemoveDBFiles);
        return NS_ERROR_UNEXPECTED;
      }

      databaseFilenames.RemoveEntry(subdirNameBase);
      continue;
    }

    // The directory base must exist in databaseFilenames.
    // If there is an unexpected directory in the idb directory, trying to
    // delete at first instead of breaking the whole initialization.
    if (NS_WARN_IF(!databaseFilenames.GetEntry(subdirNameBase)) &&
        NS_WARN_IF(NS_FAILED(DeleteFilesNoQuota(directory, subdirName)))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_GetEntry);
      return NS_ERROR_UNEXPECTED;
    }
  }

  const NS_ConvertASCIItoUTF16 sqliteSuffix(kSQLiteSuffix,
                                            LiteralStringLength(kSQLiteSuffix));
  const NS_ConvertASCIItoUTF16 walSuffix(kSQLiteWALSuffix,
                                         LiteralStringLength(kSQLiteWALSuffix));

  for (auto iter = databaseFilenames.ConstIter(); !iter.Done() && !aCanceled;
       iter.Next()) {
    auto& databaseFilename = iter.Get()->GetKey();

    nsCOMPtr<nsIFile> fmDirectory;
    rv = directory->Clone(getter_AddRefs(fmDirectory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_Clone);
      return rv;
    }

    rv = fmDirectory->Append(databaseFilename + filesSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_Append);
      return rv;
    }

    nsCOMPtr<nsIFile> databaseFile;
    rv = directory->Clone(getter_AddRefs(databaseFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_Clone2);
      return rv;
    }

    rv = databaseFile->Append(databaseFilename + sqliteSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_Append2);
      return rv;
    }

    nsCOMPtr<nsIFile> walFile;
    if (aUsageInfo) {
      rv = directory->Clone(getter_AddRefs(walFile));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_Clone3);
        return rv;
      }

      rv = walFile->Append(databaseFilename + walSuffix);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_Append3);
        return rv;
      }
    }

    rv = FileManager::InitDirectory(fmDirectory, databaseFile, aPersistenceType,
                                    aGroup, aOrigin,
                                    TelemetryIdForFile(databaseFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kInternalError, IDB_InitDirectory);
      return rv;
    }

    if (aUsageInfo) {
      int64_t fileSize;
      rv = databaseFile->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_GetFileSize);
        return rv;
      }

      MOZ_ASSERT(fileSize >= 0);

      aUsageInfo->AppendToDatabaseUsage(uint64_t(fileSize));

      rv = walFile->GetFileSize(&fileSize);
      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(fileSize >= 0);
        aUsageInfo->AppendToDatabaseUsage(uint64_t(fileSize));
      } else if (NS_WARN_IF(rv != NS_ERROR_FILE_NOT_FOUND &&
                            rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_GetWalFileSize);
        return rv;
      }

      uint64_t usage;
      rv = FileManager::GetUsage(fmDirectory, &usage);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, IDB_GetUsage);
        return rv;
      }

      aUsageInfo->AppendToFileUsage(usage);
    }
  }

  return NS_OK;
}

nsresult QuotaClient::GetUsageForOrigin(PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        const AtomicBool& aCanceled,
                                        UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aUsageInfo);

  nsCOMPtr<nsIFile> directory;
  nsresult rv =
      GetDirectory(aPersistenceType, aOrigin, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = GetUsageForDirectoryInternal(directory, aCanceled, aUsageInfo, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                         const nsACString& aOrigin) {
  AssertIsOnIOThread();

  if (IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get()) {
    mgr->InvalidateFileManagers(aPersistenceType, aOrigin);
  }
}

void QuotaClient::ReleaseIOThreadObjects() {
  AssertIsOnIOThread();

  if (IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get()) {
    mgr->InvalidateAllFileManagers();
  }
}

void QuotaClient::AbortOperations(const nsACString& aOrigin) {
  AssertIsOnBackgroundThread();

  if (!gLiveDatabaseHashtable) {
    return;
  }

  nsTArray<RefPtr<Database>> databases;

  for (auto iter = gLiveDatabaseHashtable->ConstIter(); !iter.Done();
       iter.Next()) {
    for (Database* database : iter.Data()->mLiveDatabases) {
      if (aOrigin.IsVoid() || database->Origin() == aOrigin) {
        databases.AppendElement(database);
      }
    }
  }

  for (Database* database : databases) {
    database->Invalidate();
  }

  databases.Clear();
}

void QuotaClient::AbortOperationsForProcess(ContentParentId aContentParentId) {
  AssertIsOnBackgroundThread();

  if (!gLiveDatabaseHashtable) {
    return;
  }

  nsTArray<RefPtr<Database>> databases;

  for (auto iter = gLiveDatabaseHashtable->ConstIter(); !iter.Done();
       iter.Next()) {
    for (Database* database : iter.Data()->mLiveDatabases) {
      if (database->IsOwnedByProcess(aContentParentId)) {
        databases.AppendElement(database);
      }
    }
  }

  for (Database* database : databases) {
    database->Invalidate();
  }

  databases.Clear();
}

void QuotaClient::StartIdleMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  mBackgroundThread = GetCurrentThreadEventTarget();

  RefPtr<Maintenance> maintenance = new Maintenance(this);

  mMaintenanceQueue.AppendElement(maintenance.forget());
  ProcessMaintenanceQueue();
}

void QuotaClient::StopIdleMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  if (mCurrentMaintenance) {
    mCurrentMaintenance->Abort();
  }

  for (RefPtr<Maintenance>& maintenance : mMaintenanceQueue) {
    maintenance->Abort();
  }
}

void QuotaClient::ShutdownWorkThreads() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  mShutdownRequested = true;

  AbortOperations(VoidCString());

  // This should release any IDB related quota objects or directory locks.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() {
    return (!gFactoryOps || gFactoryOps->IsEmpty()) &&
           (!gLiveDatabaseHashtable || !gLiveDatabaseHashtable->Count()) &&
           !mCurrentMaintenance;
  }));

  // And finally, shutdown all threads.
  RefPtr<ConnectionPool> connectionPool = gConnectionPool.get();
  if (connectionPool) {
    connectionPool->Shutdown();

    gConnectionPool = nullptr;
  }

  RefPtr<FileHandleThreadPool> fileHandleThreadPool =
      gFileHandleThreadPool.get();
  if (fileHandleThreadPool) {
    fileHandleThreadPool->Shutdown();

    gFileHandleThreadPool = nullptr;
  }

  if (mMaintenanceThreadPool) {
    mMaintenanceThreadPool->Shutdown();
    mMaintenanceThreadPool = nullptr;
  }

  if (mDeleteTimer) {
    MOZ_ALWAYS_SUCCEEDS(mDeleteTimer->Cancel());
    mDeleteTimer = nullptr;
  }
}

void QuotaClient::DeleteTimerCallback(nsITimer* aTimer, void* aClosure) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTimer);

  auto* self = static_cast<QuotaClient*>(aClosure);
  MOZ_ASSERT(self);
  MOZ_ASSERT(self->mDeleteTimer);
  MOZ_ASSERT(SameCOMIdentity(self->mDeleteTimer, aTimer));

  for (auto iter = self->mPendingDeleteInfos.ConstIter(); !iter.Done();
       iter.Next()) {
    auto key = iter.Key();
    auto value = iter.Data();
    MOZ_ASSERT(!value->IsEmpty());

    RefPtr<DeleteFilesRunnable> runnable =
        new DeleteFilesRunnable(key, std::move(*value));

    MOZ_ASSERT(value->IsEmpty());

    runnable->RunImmediately();
  }

  self->mPendingDeleteInfos.Clear();
}

nsresult QuotaClient::GetDirectory(PersistenceType aPersistenceType,
                                   const nsACString& aOrigin,
                                   nsIFile** aDirectory) {
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                                    getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(directory);

  rv = directory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult QuotaClient::GetDatabaseFilenames(
    nsIFile* aDirectory, const AtomicBool& aCanceled, bool aForUpgrade,
    nsTArray<nsString>& aSubdirsToProcess,
    nsTHashtable<nsStringHashKey>& aDatabaseFilenames,
    nsTHashtable<nsStringHashKey>* aObsoleteFilenames) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const NS_ConvertASCIItoUTF16 sqliteSuffix(kSQLiteSuffix,
                                            LiteralStringLength(kSQLiteSuffix));
  const NS_ConvertASCIItoUTF16 journalSuffix(
      kSQLiteJournalSuffix, LiteralStringLength(kSQLiteJournalSuffix));
  const NS_ConvertASCIItoUTF16 shmSuffix(kSQLiteSHMSuffix,
                                         LiteralStringLength(kSQLiteSHMSuffix));
  const NS_ConvertASCIItoUTF16 walSuffix(kSQLiteWALSuffix,
                                         LiteralStringLength(kSQLiteWALSuffix));

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
         file && !aCanceled) {
    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isDirectory) {
      aSubdirsToProcess.AppendElement(leafName);
      continue;
    }

    if (aObsoleteFilenames &&
        StringBeginsWith(leafName,
                         NS_LITERAL_STRING(IDB_DELETION_MARKER_FILE_PREFIX))) {
      const uint32_t start = 13;
      MOZ_ASSERT(start == LiteralStringLength(IDB_DELETION_MARKER_FILE_PREFIX));
      aObsoleteFilenames->PutEntry(Substring(leafName, start));
      continue;
    }

    // Skip OS metadata files. These files are only used in different platforms,
    // but the profile can be shared across different operating systems, so we
    // check it on all platforms.
    if (QuotaManager::IsOSMetadata(leafName)) {
      continue;
    }

    // Skip files starting with ".".
    if (QuotaManager::IsDotFile(leafName)) {
      continue;
    }

    // Skip SQLite temporary files. These files take up space on disk but will
    // be deleted as soon as the database is opened, so we don't count them
    // towards quota.
    if (StringEndsWith(leafName, journalSuffix) ||
        StringEndsWith(leafName, shmSuffix)) {
      continue;
    }

    // The SQLite WAL file does count towards quota, but it is handled below
    // once we find the actual database file.
    if (StringEndsWith(leafName, walSuffix)) {
      continue;
    }

    nsDependentSubstring leafNameBase;
    if (!GetBaseFilename(leafName, sqliteSuffix, leafNameBase)) {
      nsString path;
      MOZ_ALWAYS_SUCCEEDS(file->GetPath(path));
      MOZ_ASSERT(!path.IsEmpty());

      nsPrintfCString warning(R"(An unexpected file exists in the storage )"
                              R"(area: "%s")",
                              NS_ConvertUTF16toUTF8(path).get());
      NS_WARNING(warning.get());
      if (!aForUpgrade) {
        return NS_ERROR_UNEXPECTED;
      }
      continue;
    }

    aDatabaseFilenames.PutEntry(leafNameBase);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaClient::GetUsageForDirectoryInternal(nsIFile* aDirectory,
                                                   const AtomicBool& aCanceled,
                                                   UsageInfo* aUsageInfo,
                                                   bool aDatabaseFiles) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aUsageInfo);

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!entries) {
    return NS_OK;
  }

  const NS_ConvertASCIItoUTF16 journalSuffix(
      kSQLiteJournalSuffix, LiteralStringLength(kSQLiteJournalSuffix));
  const NS_ConvertASCIItoUTF16 shmSuffix(kSQLiteSHMSuffix,
                                         LiteralStringLength(kSQLiteSHMSuffix));

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
         file && !aCanceled) {
    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Ignore the markerfiles. Note: the unremoved files can still be calculated
    // here as long as its size matches the size recorded in QuotaManager
    // in-memory objects.
    if (StringBeginsWith(leafName,
                         NS_LITERAL_STRING(IDB_DELETION_MARKER_FILE_PREFIX))) {
      continue;
    }

    if (QuotaManager::IsOSMetadata(leafName) ||
        QuotaManager::IsDotFile(leafName)) {
      continue;
    }

    // Journal files and sqlite-shm files don't count towards usage.
    if (StringEndsWith(leafName, journalSuffix) ||
        StringEndsWith(leafName, shmSuffix)) {
      continue;
    }

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (rv == NS_ERROR_FILE_NOT_FOUND ||
        rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      continue;
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isDirectory) {
      if (aDatabaseFiles) {
        rv = GetUsageForDirectoryInternal(file, aCanceled, aUsageInfo, false);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        nsString leafName;
        rv = file->GetLeafName(leafName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (!leafName.EqualsLiteral(JOURNAL_DIRECTORY_NAME)) {
          NS_WARNING("Unknown directory found!");
        }
      }

      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(fileSize >= 0);

    if (aDatabaseFiles) {
      aUsageInfo->AppendToDatabaseUsage(uint64_t(fileSize));
    } else {
      aUsageInfo->AppendToFileUsage(uint64_t(fileSize));
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void QuotaClient::ProcessMaintenanceQueue() {
  AssertIsOnBackgroundThread();

  if (mCurrentMaintenance || mMaintenanceQueue.IsEmpty()) {
    return;
  }

  mCurrentMaintenance = mMaintenanceQueue[0];
  mMaintenanceQueue.RemoveElementAt(0);

  mCurrentMaintenance->RunImmediately();
}

/*******************************************************************************
 * DeleteFilesRunnable
 ******************************************************************************/

DeleteFilesRunnable::DeleteFilesRunnable(FileManager* aFileManager,
                                         nsTArray<int64_t>&& aFileIds)
    : Runnable("dom::indexeddb::DeleteFilesRunnable"),
      mOwningEventTarget(GetCurrentThreadEventTarget()),
      mFileManager(aFileManager),
      mFileIds(std::move(aFileIds)),
      mState(State_Initial) {}

void DeleteFilesRunnable::RunImmediately() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_Initial);

  Unused << this->Run();
}

nsresult DeleteFilesRunnable::Open() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_Initial);

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return NS_ERROR_FAILURE;
  }

  mState = State_DirectoryOpenPending;

  quotaManager->OpenDirectory(mFileManager->Type(), mFileManager->Group(),
                              mFileManager->Origin(), quota::Client::IDB,
                              /* aExclusive */ false, this);

  return NS_OK;
}

nsresult DeleteFilesRunnable::DeleteFile(int64_t aFileId) {
  MOZ_ASSERT(mDirectory);
  MOZ_ASSERT(mJournalDirectory);

  nsCOMPtr<nsIFile> file = mFileManager->GetFileForId(mDirectory, aFileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  nsresult rv;
  int64_t fileSize;

  if (mFileManager->EnforcingQuota()) {
    rv = file->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  }

  rv = file->Remove(false);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  if (mFileManager->EnforcingQuota()) {
    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "Shouldn't be null!");

    quotaManager->DecreaseUsageForOrigin(mFileManager->Type(),
                                         mFileManager->Group(),
                                         mFileManager->Origin(), fileSize);
  }

  file = mFileManager->GetFileForId(mJournalDirectory, aFileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  rv = file->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult DeleteFilesRunnable::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DatabaseWorkOpen);

  if (!mFileManager->Invalidated()) {
    mDirectory = mFileManager->GetDirectory();
    if (NS_WARN_IF(!mDirectory)) {
      return NS_ERROR_FAILURE;
    }

    mJournalDirectory = mFileManager->GetJournalDirectory();
    if (NS_WARN_IF(!mJournalDirectory)) {
      return NS_ERROR_FAILURE;
    }

    for (int64_t fileId : mFileIds) {
      if (NS_FAILED(DeleteFile(fileId))) {
        NS_WARNING("Failed to delete file!");
      }
    }
  }

  Finish();

  return NS_OK;
}

void DeleteFilesRunnable::Finish() {
  // Must set mState before dispatching otherwise we will race with the main
  // thread.
  mState = State_UnblockingOpen;

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
}

void DeleteFilesRunnable::UnblockOpen() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_UnblockingOpen);

  mDirectoryLock = nullptr;

  mState = State_Completed;
}

NS_IMPL_ISUPPORTS_INHERITED0(DeleteFilesRunnable, Runnable)

NS_IMETHODIMP
DeleteFilesRunnable::Run() {
  nsresult rv;

  switch (mState) {
    case State_Initial:
      rv = Open();
      break;

    case State_DatabaseWorkOpen:
      rv = DoDatabaseWork();
      break;

    case State_UnblockingOpen:
      UnblockOpen();
      return NS_OK;

    case State_DirectoryOpenPending:
    default:
      MOZ_CRASH("Should never get here!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State_UnblockingOpen) {
    Finish();
  }

  return NS_OK;
}

void DeleteFilesRunnable::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread
  mState = State_DatabaseWorkOpen;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish();
    return;
  }
}

void DeleteFilesRunnable::DirectoryLockFailed() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  Finish();
}

void Maintenance::RegisterDatabaseMaintenance(
    DatabaseMaintenance* aDatabaseMaintenance) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseMaintenance);
  MOZ_ASSERT(mState == State::BeginDatabaseMaintenance);
  MOZ_ASSERT(!mDatabaseMaintenances.Get(aDatabaseMaintenance->DatabasePath()));

  mDatabaseMaintenances.Put(aDatabaseMaintenance->DatabasePath(),
                            aDatabaseMaintenance);
}

void Maintenance::UnregisterDatabaseMaintenance(
    DatabaseMaintenance* aDatabaseMaintenance) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseMaintenance);
  MOZ_ASSERT(mState == State::WaitingForDatabaseMaintenancesToComplete);
  MOZ_ASSERT(mDatabaseMaintenances.Get(aDatabaseMaintenance->DatabasePath()));

  mDatabaseMaintenances.Remove(aDatabaseMaintenance->DatabasePath());

  if (mDatabaseMaintenances.Count()) {
    return;
  }

  mState = State::Finishing;
  Finish();
}

nsresult Maintenance::Start() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  // Make sure that the IndexedDatabaseManager is running so that we can check
  // for low disk space mode.

  if (IndexedDatabaseManager::Get()) {
    OpenDirectory();
    return NS_OK;
  }

  mState = State::CreateIndexedDatabaseManager;
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

  return NS_OK;
}

nsresult Maintenance::CreateIndexedDatabaseManager() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::CreateIndexedDatabaseManager);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::GetOrCreate();
  if (NS_WARN_IF(!mgr)) {
    return NS_ERROR_FAILURE;
  }

  mState = State::IndexedDatabaseManagerOpen;
  MOZ_ALWAYS_SUCCEEDS(
      mQuotaClient->BackgroundThread()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult Maintenance::OpenDirectory() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial ||
             mState == State::IndexedDatabaseManagerOpen);
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(QuotaManager::Get());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  // Get a shared lock for <profile>/storage/*/*/idb

  mState = State::DirectoryOpenPending;
  QuotaManager::Get()->OpenDirectoryInternal(
      Nullable<PersistenceType>(), OriginScope::FromNull(),
      Nullable<Client::Type>(Client::IDB),
      /* aExclusive */ false, this);

  return NS_OK;
}

nsresult Maintenance::DirectoryOpen() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(mDirectoryLock);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  mState = State::DirectoryWorkOpen;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult Maintenance::DirectoryWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DirectoryWorkOpen);

  // The storage directory is structured like this:
  //
  //   <profile>/storage/<persistence>/<origin>/idb/*.sqlite
  //
  // We have to find all database files that match any persistence type and any
  // origin. We ignore anything out of the ordinary for now.

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsresult rv = quotaManager->EnsureStorageIsInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> storageDir = GetFileForPath(quotaManager->GetStoragePath());
  if (NS_WARN_IF(!storageDir)) {
    return NS_ERROR_FAILURE;
  }

  bool exists;
  rv = storageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool isDirectory;
  rv = storageDir->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!isDirectory)) {
    return NS_ERROR_FAILURE;
  }

  // There are currently only 3 persistence types, and we want to iterate them
  // in this order:
  static const PersistenceType kPersistenceTypes[] = {
      PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_DEFAULT,
      PERSISTENCE_TYPE_TEMPORARY};

  static_assert((sizeof(kPersistenceTypes) / sizeof(kPersistenceTypes[0])) ==
                    size_t(PERSISTENCE_TYPE_INVALID),
                "Something changed with available persistence types!");

  NS_NAMED_LITERAL_STRING(idbDirName, IDB_DIRECTORY_NAME);
  NS_NAMED_LITERAL_STRING(sqliteExtension, ".sqlite");

  for (const PersistenceType persistenceType : kPersistenceTypes) {
    // Loop over "<persistence>" directories.
    if (IsAborted()) {
      return NS_ERROR_ABORT;
    }

    nsAutoCString persistenceTypeString;
    if (persistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      // XXX This shouldn't be a special case...
      persistenceTypeString.AssignLiteral("permanent");
    } else {
      PersistenceTypeToText(persistenceType, persistenceTypeString);
    }

    nsCOMPtr<nsIFile> persistenceDir;
    rv = storageDir->Clone(getter_AddRefs(persistenceDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = persistenceDir->Append(NS_ConvertASCIItoUTF16(persistenceTypeString));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = persistenceDir->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!exists) {
      continue;
    }

    rv = persistenceDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      continue;
    }

    nsCOMPtr<nsIDirectoryEnumerator> persistenceDirEntries;
    rv = persistenceDir->GetDirectoryEntries(
        getter_AddRefs(persistenceDirEntries));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!persistenceDirEntries) {
      continue;
    }

    while (true) {
      // Loop over "<origin>/idb" directories.
      if (IsAborted()) {
        return NS_ERROR_ABORT;
      }

      nsCOMPtr<nsIFile> originDir;
      rv = persistenceDirEntries->GetNextFile(getter_AddRefs(originDir));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!originDir) {
        break;
      }

      rv = originDir->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(exists);

      rv = originDir->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!isDirectory) {
        continue;
      }

      nsCOMPtr<nsIFile> idbDir;
      rv = originDir->Clone(getter_AddRefs(idbDir));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = idbDir->Append(idbDirName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = idbDir->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!exists) {
        continue;
      }

      rv = idbDir->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (NS_WARN_IF(!isDirectory)) {
        continue;
      }

      nsCOMPtr<nsIDirectoryEnumerator> idbDirEntries;
      rv = idbDir->GetDirectoryEntries(getter_AddRefs(idbDirEntries));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!idbDirEntries) {
        continue;
      }

      nsCString suffix;
      nsCString group;
      nsCString origin;
      nsTArray<nsString> databasePaths;

      while (true) {
        // Loop over files in the "idb" directory.
        if (IsAborted()) {
          return NS_ERROR_ABORT;
        }

        nsCOMPtr<nsIFile> idbDirFile;
        rv = idbDirEntries->GetNextFile(getter_AddRefs(idbDirFile));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (!idbDirFile) {
          break;
        }

        nsString idbFilePath;
        rv = idbDirFile->GetPath(idbFilePath);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (!StringEndsWith(idbFilePath, sqliteExtension)) {
          continue;
        }

        rv = idbDirFile->Exists(&exists);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        MOZ_ASSERT(exists);

        rv = idbDirFile->IsDirectory(&isDirectory);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (isDirectory) {
          continue;
        }

        // Found a database.
        if (databasePaths.IsEmpty()) {
          MOZ_ASSERT(suffix.IsEmpty());
          MOZ_ASSERT(group.IsEmpty());
          MOZ_ASSERT(origin.IsEmpty());

          int64_t dummyTimeStamp;
          bool dummyPersisted;
          if (NS_WARN_IF(NS_FAILED(quotaManager->GetDirectoryMetadata2(
                  originDir, &dummyTimeStamp, &dummyPersisted, suffix, group,
                  origin)))) {
            // Not much we can do here...
            continue;
          }
        }

        MOZ_ASSERT(!databasePaths.Contains(idbFilePath));

        databasePaths.AppendElement(idbFilePath);
      }

      if (!databasePaths.IsEmpty()) {
        mDirectoryInfos.AppendElement(DirectoryInfo(
            persistenceType, group, origin, std::move(databasePaths)));

        nsCOMPtr<nsIFile> directory;

        // Idle maintenance may occur before origin is initailized.
        // Ensure origin is initialized first. It will initialize all origins
        // for temporary storage including IDB origins.
        rv = quotaManager->EnsureOriginIsInitialized(
            persistenceType, suffix, group, origin,
            /* aCreateIfNotExists */ true, getter_AddRefs(directory));

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  mState = State::BeginDatabaseMaintenance;

  MOZ_ALWAYS_SUCCEEDS(
      mQuotaClient->BackgroundThread()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult Maintenance::BeginDatabaseMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::BeginDatabaseMaintenance);

  class MOZ_STACK_CLASS Helper final {
   public:
    static bool IsSafeToRunMaintenance(const nsAString& aDatabasePath) {
      if (gFactoryOps) {
        for (uint32_t index = gFactoryOps->Length(); index > 0; index--) {
          RefPtr<FactoryOp>& existingOp = (*gFactoryOps)[index - 1];

          if (!existingOp->DatabaseFilePathIsKnown()) {
            continue;
          }

          if (existingOp->DatabaseFilePath() == aDatabasePath) {
            return false;
          }
        }
      }

      if (gLiveDatabaseHashtable) {
        for (auto iter = gLiveDatabaseHashtable->ConstIter(); !iter.Done();
             iter.Next()) {
          for (Database* database : iter.Data()->mLiveDatabases) {
            if (database->FilePath() == aDatabasePath) {
              return false;
            }
          }
        }
      }

      return true;
    }
  };

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  RefPtr<nsThreadPool> threadPool;

  for (DirectoryInfo& directoryInfo : mDirectoryInfos) {
    for (const nsString& databasePath : directoryInfo.mDatabasePaths) {
      if (Helper::IsSafeToRunMaintenance(databasePath)) {
        RefPtr<DatabaseMaintenance> databaseMaintenance =
            new DatabaseMaintenance(this, directoryInfo.mPersistenceType,
                                    directoryInfo.mGroup, directoryInfo.mOrigin,
                                    databasePath);

        if (!threadPool) {
          threadPool = mQuotaClient->GetOrCreateThreadPool();
          MOZ_ASSERT(threadPool);
        }

        MOZ_ALWAYS_SUCCEEDS(
            threadPool->Dispatch(databaseMaintenance, NS_DISPATCH_NORMAL));

        RegisterDatabaseMaintenance(databaseMaintenance);
      }
    }
  }

  mDirectoryInfos.Clear();

  if (mDatabaseMaintenances.Count()) {
    mState = State::WaitingForDatabaseMaintenancesToComplete;
  } else {
    mState = State::Finishing;
    Finish();
  }

  return NS_OK;
}

void Maintenance::Finish() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Finishing);

  if (NS_FAILED(mResultCode)) {
    nsCString errorName;
    GetErrorName(mResultCode, errorName);

    IDB_WARNING("Maintenance finished with error: %s", errorName.get());
  }

  mDirectoryLock = nullptr;

  // It can happen that we are only referenced by mCurrentMaintenance which is
  // cleared in NoteFinishedMaintenance()
  RefPtr<Maintenance> kungFuDeathGrip = this;

  mQuotaClient->NoteFinishedMaintenance(this);

  mState = State::Complete;
}

NS_IMPL_ISUPPORTS_INHERITED0(Maintenance, Runnable)

NS_IMETHODIMP
Maintenance::Run() {
  MOZ_ASSERT(mState != State::Complete);

  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = Start();
      break;

    case State::CreateIndexedDatabaseManager:
      rv = CreateIndexedDatabaseManager();
      break;

    case State::IndexedDatabaseManagerOpen:
      rv = OpenDirectory();
      break;

    case State::DirectoryWorkOpen:
      rv = DirectoryWork();
      break;

    case State::BeginDatabaseMaintenance:
      rv = BeginDatabaseMaintenance();
      break;

    case State::Finishing:
      Finish();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::Finishing) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::Finishing;

    if (IsOnBackgroundThread()) {
      Finish();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          mQuotaClient->BackgroundThread()->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void Maintenance::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  nsresult rv = DirectoryOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State::Finishing;
    Finish();

    return;
  }
}

void Maintenance::DirectoryLockFailed() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = NS_ERROR_FAILURE;
  }

  mState = State::Finishing;
  Finish();
}

void DatabaseMaintenance::PerformMaintenanceOnDatabase() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mMaintenance);
  MOZ_ASSERT(mMaintenance->StartTime());
  MOZ_ASSERT(!mDatabasePath.IsEmpty());
  MOZ_ASSERT(!mGroup.IsEmpty());
  MOZ_ASSERT(!mOrigin.IsEmpty());

  class MOZ_STACK_CLASS AutoClose final {
    nsCOMPtr<mozIStorageConnection> mConnection;

   public:
    explicit AutoClose(mozIStorageConnection* aConnection)
        : mConnection(aConnection) {
      MOZ_ASSERT(aConnection);
    }

    ~AutoClose() {
      MOZ_ASSERT(mConnection);

      MOZ_ALWAYS_SUCCEEDS(mConnection->Close());
    }
  };

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return;
  }

  nsCOMPtr<nsIFile> databaseFile = GetFileForPath(mDatabasePath);
  MOZ_ASSERT(databaseFile);

  nsCOMPtr<mozIStorageConnection> connection;
  nsresult rv = GetStorageConnection(databaseFile, mPersistenceType, mGroup,
                                     mOrigin, TelemetryIdForFile(databaseFile),
                                     getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  AutoClose autoClose(connection);

  AutoProgressHandler progressHandler(mMaintenance);
  if (NS_WARN_IF(NS_FAILED(progressHandler.Register(connection)))) {
    return;
  }

  bool databaseIsOk;
  rv = CheckIntegrity(connection, &databaseIsOk);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_WARN_IF(!databaseIsOk)) {
    // XXX Handle this somehow! Probably need to clear all storage for the
    //     origin. Needs followup.
    MOZ_ASSERT(false, "Database corruption detected!");
    return;
  }

  MaintenanceAction maintenanceAction;
  rv = DetermineMaintenanceAction(connection, databaseFile, &maintenanceAction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  switch (maintenanceAction) {
    case MaintenanceAction::Nothing:
      break;

    case MaintenanceAction::IncrementalVacuum:
      IncrementalVacuum(connection);
      break;

    case MaintenanceAction::FullVacuum:
      FullVacuum(connection, databaseFile);
      break;

    default:
      MOZ_CRASH("Unknown MaintenanceAction!");
  }
}

nsresult DatabaseMaintenance::CheckIntegrity(mozIStorageConnection* aConnection,
                                             bool* aOk) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aOk);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return NS_ERROR_ABORT;
  }

  nsresult rv;

  // First do a full integrity_check. Scope statements tightly here because
  // later operations require zero live statements.
  {
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = aConnection->CreateStatement(
        NS_LITERAL_CSTRING("PRAGMA integrity_check(1);"), getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(hasResult);

    nsString result;
    rv = stmt->GetString(0, result);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!result.EqualsLiteral("ok"))) {
      *aOk = false;
      return NS_OK;
    }
  }

  // Now enable and check for foreign key constraints.
  {
    int32_t foreignKeysWereEnabled;
    {
      nsCOMPtr<mozIStorageStatement> stmt;
      rv = aConnection->CreateStatement(
          NS_LITERAL_CSTRING("PRAGMA foreign_keys;"), getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      bool hasResult;
      rv = stmt->ExecuteStep(&hasResult);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(hasResult);

      rv = stmt->GetInt32(0, &foreignKeysWereEnabled);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (!foreignKeysWereEnabled) {
      rv = aConnection->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("PRAGMA foreign_keys = ON;"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    bool foreignKeyError;
    {
      nsCOMPtr<mozIStorageStatement> stmt;
      rv = aConnection->CreateStatement(
          NS_LITERAL_CSTRING("PRAGMA foreign_key_check;"),
          getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->ExecuteStep(&foreignKeyError);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (!foreignKeysWereEnabled) {
      nsAutoCString stmtSQL;
      stmtSQL.AssignLiteral("PRAGMA foreign_keys = ");
      stmtSQL.AppendLiteral("OFF");
      stmtSQL.Append(';');

      rv = aConnection->ExecuteSimpleSQL(stmtSQL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (foreignKeyError) {
      *aOk = false;
      return NS_OK;
    }
  }

  *aOk = true;
  return NS_OK;
}

nsresult DatabaseMaintenance::DetermineMaintenanceAction(
    mozIStorageConnection* aConnection, nsIFile* aDatabaseFile,
    MaintenanceAction* aMaintenanceAction) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aDatabaseFile);
  MOZ_ASSERT(aMaintenanceAction);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return NS_ERROR_ABORT;
  }

  int32_t schemaVersion;
  nsresult rv = aConnection->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Don't do anything if the schema version is less than 18; before that
  // version no databases had |auto_vacuum == INCREMENTAL| set and we didn't
  // track the values needed for the heuristics below.
  if (schemaVersion < MakeSchemaVersion(18, 0)) {
    *aMaintenanceAction = MaintenanceAction::Nothing;
    return NS_OK;
  }

  // This method shouldn't make any permanent changes to the database, so make
  // sure everything gets rolled back when we leave.
  mozStorageTransaction transaction(aConnection,
                                    /* aCommitOnComplete */ false);

  // Check to see when we last vacuumed this database.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT last_vacuum_time, last_vacuum_size "
                         "FROM database;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  PRTime lastVacuumTime;
  rv = stmt->GetInt64(0, &lastVacuumTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t lastVacuumSize;
  rv = stmt->GetInt64(1, &lastVacuumSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ASSERTION(lastVacuumSize > 0,
               "Thy last vacuum size shall be greater than zero, less than "
               "zero shall thy last vacuum size not be. Zero is right out.");

  PRTime startTime = mMaintenance->StartTime();

  // This shouldn't really be possible...
  if (NS_WARN_IF(startTime <= lastVacuumTime)) {
    *aMaintenanceAction = MaintenanceAction::Nothing;
    return NS_OK;
  }

  if (startTime - lastVacuumTime < kMinVacuumAge) {
    *aMaintenanceAction = MaintenanceAction::IncrementalVacuum;
    return NS_OK;
  }

  // It has been more than a week since the database was vacuumed, so gather
  // statistics on its usage to see if vacuuming is worthwhile.

  // Create a temporary copy of the dbstat table to speed up the queries that
  // come later.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE VIRTUAL TABLE __stats__ USING dbstat;"
      "CREATE TEMP TABLE __temp_stats__ AS SELECT * FROM __stats__;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Calculate the percentage of the database pages that are not in contiguous
  // order.
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING(
          "SELECT SUM(__ts1__.pageno != __ts2__.pageno + 1) * 100.0 / COUNT(*) "
          "FROM __temp_stats__ AS __ts1__, __temp_stats__ AS __ts2__ "
          "WHERE __ts1__.name = __ts2__.name "
          "AND __ts1__.rowid = __ts2__.rowid + 1;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  int32_t percentUnordered;
  rv = stmt->GetInt32(0, &percentUnordered);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(percentUnordered >= 0);
  MOZ_ASSERT(percentUnordered <= 100);

  if (percentUnordered >= kPercentUnorderedThreshold) {
    *aMaintenanceAction = MaintenanceAction::FullVacuum;
    return NS_OK;
  }

  // Don't try a full vacuum if the file hasn't grown by 10%.
  int64_t currentFileSize;
  rv = aDatabaseFile->GetFileSize(&currentFileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (currentFileSize <= lastVacuumSize ||
      (((currentFileSize - lastVacuumSize) * 100 / currentFileSize) <
       kPercentFileSizeGrowthThreshold)) {
    *aMaintenanceAction = MaintenanceAction::IncrementalVacuum;
    return NS_OK;
  }

  // See if there are any free pages that we can reclaim.
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("PRAGMA freelist_count;"), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  int32_t freelistCount;
  rv = stmt->GetInt32(0, &freelistCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(freelistCount >= 0);

  // If we have too many free pages then we should try an incremental vacuum. If
  // that causes too much fragmentation then we'll try a full vacuum later.
  if (freelistCount > kMaxFreelistThreshold) {
    *aMaintenanceAction = MaintenanceAction::IncrementalVacuum;
    return NS_OK;
  }

  // Calculate the percentage of unused bytes on pages in the database.
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT SUM(unused) * 100.0 / SUM(pgsize) "
                         "FROM __temp_stats__;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  int32_t percentUnused;
  rv = stmt->GetInt32(0, &percentUnused);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(percentUnused >= 0);
  MOZ_ASSERT(percentUnused <= 100);

  *aMaintenanceAction = percentUnused >= kPercentUnusedThreshold
                            ? MaintenanceAction::FullVacuum
                            : MaintenanceAction::IncrementalVacuum;
  return NS_OK;
}

void DatabaseMaintenance::IncrementalVacuum(
    mozIStorageConnection* aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return;
  }

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("PRAGMA incremental_vacuum;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void DatabaseMaintenance::FullVacuum(mozIStorageConnection* aConnection,
                                     nsIFile* aDatabaseFile) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aDatabaseFile);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return;
  }

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  PRTime vacuumTime = PR_Now();
  MOZ_ASSERT(vacuumTime > 0);

  int64_t fileSize;
  rv = aDatabaseFile->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MOZ_ASSERT(fileSize > 0);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("UPDATE database "
                         "SET last_vacuum_time = :time"
                         ", last_vacuum_size = :size;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("time"), vacuumTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("size"), fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void DatabaseMaintenance::RunOnOwningThread() {
  AssertIsOnBackgroundThread();

  if (mCompleteCallback) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mCompleteCallback.forget()));
  }

  mMaintenance->UnregisterDatabaseMaintenance(this);
}

void DatabaseMaintenance::RunOnConnectionThread() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  PerformMaintenanceOnDatabase();

  MOZ_ALWAYS_SUCCEEDS(
      mMaintenance->BackgroundThread()->Dispatch(this, NS_DISPATCH_NORMAL));
}

NS_IMETHODIMP
DatabaseMaintenance::Run() {
  if (IsOnBackgroundThread()) {
    RunOnOwningThread();
  } else {
    RunOnConnectionThread();
  }

  return NS_OK;
}

nsresult DatabaseMaintenance::AutoProgressHandler::Register(
    mozIStorageConnection* aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);

  // We want to quickly bail out of any operation if the user becomes active, so
  // use a small granularity here since database performance isn't critical.
  static const int32_t kProgressGranularity = 50;

  nsCOMPtr<mozIStorageProgressHandler> oldHandler;
  nsresult rv = aConnection->SetProgressHandler(kProgressGranularity, this,
                                                getter_AddRefs(oldHandler));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!oldHandler);
  mConnection = aConnection;

  return NS_OK;
}

void DatabaseMaintenance::AutoProgressHandler::Unregister() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mConnection);

  nsCOMPtr<mozIStorageProgressHandler> oldHandler;
  nsresult rv = mConnection->RemoveProgressHandler(getter_AddRefs(oldHandler));
  Unused << NS_WARN_IF(NS_FAILED(rv));

  MOZ_ASSERT_IF(NS_SUCCEEDED(rv), oldHandler == this);
}

NS_IMETHODIMP_(MozExternalRefCountType)
DatabaseMaintenance::AutoProgressHandler::AddRef() {
  NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);

#ifdef DEBUG
  mDEBUGRefCnt++;
#endif
  return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
DatabaseMaintenance::AutoProgressHandler::Release() {
  NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);

#ifdef DEBUG
  mDEBUGRefCnt--;
#endif
  return 1;
}

NS_IMPL_QUERY_INTERFACE(DatabaseMaintenance::AutoProgressHandler,
                        mozIStorageProgressHandler)

NS_IMETHODIMP
DatabaseMaintenance::AutoProgressHandler::OnProgress(
    mozIStorageConnection* aConnection, bool* _retval) {
  NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(mConnection == aConnection);
  MOZ_ASSERT(_retval);

  *_retval = mMaintenance->IsAborted();

  return NS_OK;
}

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

NS_IMPL_ISUPPORTS(CompressDataBlobsFunction, mozIStorageFunction)
NS_IMPL_ISUPPORTS(EncodeKeysFunction, mozIStorageFunction)
NS_IMPL_ISUPPORTS(StripObsoleteOriginAttributesFunction, mozIStorageFunction);

nsresult UpgradeFileIdsFunction::Init(nsIFile* aFMDirectory,
                                      mozIStorageConnection* aConnection) {
  // This file manager doesn't need real origin info, etc. The only purpose is
  // to store file ids without adding more complexity or code duplication.
  RefPtr<FileManager> fileManager =
      new FileManager(PERSISTENCE_TYPE_INVALID, EmptyCString(), EmptyCString(),
                      EmptyString(), false);

  nsresult rv = fileManager->Init(aFMDirectory, aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mFileManager.swap(fileManager);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(UpgradeFileIdsFunction, mozIStorageFunction)

NS_IMETHODIMP
UpgradeFileIdsFunction::OnFunctionCall(mozIStorageValueArray* aArguments,
                                       nsIVariant** aResult) {
  MOZ_ASSERT(aArguments);
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(mFileManager);

  AUTO_PROFILER_LABEL("UpgradeFileIdsFunction::OnFunctionCall", DOM);

  uint32_t argc;
  nsresult rv = aArguments->GetNumEntries(&argc);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (argc != 2) {
    NS_WARNING("Don't call me with the wrong number of arguments!");
    return NS_ERROR_UNEXPECTED;
  }

  StructuredCloneReadInfo cloneInfo(JS::StructuredCloneScope::DifferentProcess);
  DatabaseOperationBase::GetStructuredCloneReadInfoFromValueArray(
      aArguments, 1, 0, mFileManager, &cloneInfo);

  nsAutoString fileIds;
  rv = IDBObjectStore::DeserializeUpgradeValueToFileIds(cloneInfo, fileIds);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  nsCOMPtr<nsIVariant> result = new mozilla::storage::TextVariant(fileIds);

  result.forget(aResult);
  return NS_OK;
}

// static
void DatabaseOperationBase::GetBindingClauseForKeyRange(
    const SerializedKeyRange& aKeyRange, const nsACString& aKeyColumnName,
    nsAutoCString& aBindingClause) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aKeyColumnName.IsEmpty());

  NS_NAMED_LITERAL_CSTRING(andStr, " AND ");
  NS_NAMED_LITERAL_CSTRING(spacecolon, " :");
  NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

  if (aKeyRange.isOnly()) {
    // Both keys equal.
    aBindingClause = andStr + aKeyColumnName + NS_LITERAL_CSTRING(" =") +
                     spacecolon + lowerKey;
    return;
  }

  aBindingClause.Truncate();

  if (!aKeyRange.lower().IsUnset()) {
    // Lower key is set.
    aBindingClause.Append(andStr + aKeyColumnName);
    aBindingClause.AppendLiteral(" >");
    if (!aKeyRange.lowerOpen()) {
      aBindingClause.AppendLiteral("=");
    }
    aBindingClause.Append(spacecolon + lowerKey);
  }

  if (!aKeyRange.upper().IsUnset()) {
    // Upper key is set.
    aBindingClause.Append(andStr + aKeyColumnName);
    aBindingClause.AppendLiteral(" <");
    if (!aKeyRange.upperOpen()) {
      aBindingClause.AppendLiteral("=");
    }
    aBindingClause.Append(spacecolon + NS_LITERAL_CSTRING("upper_key"));
  }

  MOZ_ASSERT(!aBindingClause.IsEmpty());
}

// static
uint64_t DatabaseOperationBase::ReinterpretDoubleAsUInt64(double aDouble) {
  // This is a duplicate of the js engine's byte munging in StructuredClone.cpp
  return BitwiseCast<uint64_t>(aDouble);
}

// static
template <typename T>
nsresult DatabaseOperationBase::GetStructuredCloneReadInfoFromSource(
    T* aSource, uint32_t aDataIndex, uint32_t aFileIdsIndex,
    FileManager* aFileManager, StructuredCloneReadInfo* aInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aSource);
  MOZ_ASSERT(aFileManager);
  MOZ_ASSERT(aInfo);

  int32_t columnType;
  nsresult rv = aSource->GetTypeOfIndex(aDataIndex, &columnType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(columnType == mozIStorageStatement::VALUE_TYPE_BLOB ||
             columnType == mozIStorageStatement::VALUE_TYPE_INTEGER);

  bool isNull;
  rv = aSource->GetIsNull(aFileIdsIndex, &isNull);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString fileIds;

  if (isNull) {
    fileIds.SetIsVoid(true);
  } else {
    rv = aSource->GetString(aFileIdsIndex, fileIds);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (columnType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
    uint64_t intData;
    rv = aSource->GetInt64(aDataIndex, reinterpret_cast<int64_t*>(&intData));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetStructuredCloneReadInfoFromExternalBlob(intData, aFileManager,
                                                    fileIds, aInfo);
  } else {
    const uint8_t* blobData;
    uint32_t blobDataLength;
    rv = aSource->GetSharedBlob(aDataIndex, &blobDataLength, &blobData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetStructuredCloneReadInfoFromBlob(blobData, blobDataLength,
                                            aFileManager, fileIds, aInfo);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::GetStructuredCloneReadInfoFromBlob(
    const uint8_t* aBlobData, uint32_t aBlobDataLength,
    FileManager* aFileManager, const nsAString& aFileIds,
    StructuredCloneReadInfo* aInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aFileManager);
  MOZ_ASSERT(aInfo);

  AUTO_PROFILER_LABEL(
      "DatabaseOperationBase::GetStructuredCloneReadInfoFromBlob", DOM);

  const char* compressed = reinterpret_cast<const char*>(aBlobData);
  size_t compressedLength = size_t(aBlobDataLength);

  size_t uncompressedLength;
  if (NS_WARN_IF(!snappy::GetUncompressedLength(compressed, compressedLength,
                                                &uncompressedLength))) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  AutoTArray<uint8_t, 512> uncompressed;
  if (NS_WARN_IF(!uncompressed.SetLength(uncompressedLength, fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* uncompressedBuffer = reinterpret_cast<char*>(uncompressed.Elements());

  if (NS_WARN_IF(!snappy::RawUncompress(compressed, compressedLength,
                                        uncompressedBuffer))) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  if (!aInfo->mData.AppendBytes(uncompressedBuffer, uncompressed.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!aFileIds.IsVoid()) {
    nsresult rv = DeserializeStructuredCloneFiles(
        aFileManager, aFileIds, aInfo->mFiles, &aInfo->mHasPreprocessInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::GetStructuredCloneReadInfoFromExternalBlob(
    uint64_t aIntData, FileManager* aFileManager, const nsAString& aFileIds,
    StructuredCloneReadInfo* aInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aFileManager);
  MOZ_ASSERT(aInfo);

  AUTO_PROFILER_LABEL(
      "DatabaseOperationBase::GetStructuredCloneReadInfoFromExternalBlob", DOM);

  nsresult rv;

  if (!aFileIds.IsVoid()) {
    rv = DeserializeStructuredCloneFiles(aFileManager, aFileIds, aInfo->mFiles,
                                         &aInfo->mHasPreprocessInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Higher and lower 32 bits described
  // in ObjectStoreAddOrPutRequestOp::DoDatabaseWork.
  uint32_t index = uint32_t(aIntData & 0xFFFFFFFF);

  if (index >= aInfo->mFiles.Length()) {
    MOZ_ASSERT(false, "Bad index value!");
    return NS_ERROR_UNEXPECTED;
  }

  StructuredCloneFile& file = aInfo->mFiles[index];
  MOZ_ASSERT(file.mFileInfo);
  MOZ_ASSERT(file.mType == StructuredCloneFile::eStructuredClone);

  nsCOMPtr<nsIFile> nativeFile = FileInfo::GetFileForFileInfo(file.mFileInfo);
  if (NS_WARN_IF(!nativeFile)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), nativeFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<SnappyUncompressInputStream> snappyInputStream =
      new SnappyUncompressInputStream(fileInputStream);

  do {
    char buffer[kFileCopyBufferSize];

    uint32_t numRead;
    rv = snappyInputStream->Read(buffer, sizeof(buffer), &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (!numRead) {
      break;
    }

    if (NS_WARN_IF(!aInfo->mData.AppendBytes(buffer, numRead))) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
  } while (true);

  return rv;
}

// static
nsresult DatabaseOperationBase::BindKeyRangeToStatement(
    const SerializedKeyRange& aKeyRange, mozIStorageStatement* aStatement) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aStatement);

  nsresult rv = NS_OK;

  if (!aKeyRange.lower().IsUnset()) {
    rv = aKeyRange.lower().BindToStatement(aStatement,
                                           NS_LITERAL_CSTRING("lower_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aKeyRange.isOnly()) {
    return rv;
  }

  if (!aKeyRange.upper().IsUnset()) {
    rv = aKeyRange.upper().BindToStatement(aStatement,
                                           NS_LITERAL_CSTRING("upper_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::BindKeyRangeToStatement(
    const SerializedKeyRange& aKeyRange, mozIStorageStatement* aStatement,
    const nsCString& aLocale) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aStatement);
  MOZ_ASSERT(!aLocale.IsEmpty());

  nsresult rv = NS_OK;

  if (!aKeyRange.lower().IsUnset()) {
    Key lower;
    rv = aKeyRange.lower().ToLocaleBasedKey(lower, aLocale);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = lower.BindToStatement(aStatement, NS_LITERAL_CSTRING("lower_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aKeyRange.isOnly()) {
    return rv;
  }

  if (!aKeyRange.upper().IsUnset()) {
    Key upper;
    rv = aKeyRange.upper().ToLocaleBasedKey(upper, aLocale);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = upper.BindToStatement(aStatement, NS_LITERAL_CSTRING("upper_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

// static
void DatabaseOperationBase::AppendConditionClause(const nsACString& aColumnName,
                                                  const nsACString& aArgName,
                                                  bool aLessThan, bool aEquals,
                                                  nsAutoCString& aResult) {
  aResult +=
      NS_LITERAL_CSTRING(" AND ") + aColumnName + NS_LITERAL_CSTRING(" ");

  if (aLessThan) {
    aResult.Append('<');
  } else {
    aResult.Append('>');
  }

  if (aEquals) {
    aResult.Append('=');
  }

  aResult += NS_LITERAL_CSTRING(" :") + aArgName;
}

// static
nsresult DatabaseOperationBase::GetUniqueIndexTableForObjectStore(
    TransactionBase* aTransaction, int64_t aObjectStoreId,
    Maybe<UniqueIndexTable>& aMaybeUniqueIndexTable) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aMaybeUniqueIndexTable.isNothing());

  const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
      aTransaction->GetMetadataForObjectStoreId(aObjectStoreId);
  MOZ_ASSERT(objectStoreMetadata);

  if (!objectStoreMetadata->mIndexes.Count()) {
    return NS_OK;
  }

  const uint32_t indexCount = objectStoreMetadata->mIndexes.Count();
  MOZ_ASSERT(indexCount > 0);

  aMaybeUniqueIndexTable.emplace();
  UniqueIndexTable* uniqueIndexTable = aMaybeUniqueIndexTable.ptr();
  MOZ_ASSERT(uniqueIndexTable);

  for (auto iter = objectStoreMetadata->mIndexes.Iter(); !iter.Done();
       iter.Next()) {
    FullIndexMetadata* value = iter.UserData();
    MOZ_ASSERT(!uniqueIndexTable->Get(value->mCommonMetadata.id()));

    if (NS_WARN_IF(!uniqueIndexTable->Put(value->mCommonMetadata.id(),
                                          value->mCommonMetadata.unique(),
                                          fallible))) {
      break;
    }
  }

  if (NS_WARN_IF(aMaybeUniqueIndexTable.ref().Count() != indexCount)) {
    IDB_REPORT_INTERNAL_ERR();
    aMaybeUniqueIndexTable.reset();
    NS_WARNING("out of memory");
    return NS_ERROR_OUT_OF_MEMORY;
  }

#ifdef DEBUG
  aMaybeUniqueIndexTable.ref().MarkImmutable();
#endif

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::IndexDataValuesFromUpdateInfos(
    const nsTArray<IndexUpdateInfo>& aUpdateInfos,
    const UniqueIndexTable& aUniqueIndexTable,
    nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(aIndexValues.IsEmpty());
  MOZ_ASSERT_IF(!aUpdateInfos.IsEmpty(), aUniqueIndexTable.Count());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::IndexDataValuesFromUpdateInfos",
                      DOM);

  const uint32_t count = aUpdateInfos.Length();

  if (!count) {
    return NS_OK;
  }

  if (NS_WARN_IF(!aIndexValues.SetCapacity(count, fallible))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t idxIndex = 0; idxIndex < count; idxIndex++) {
    const IndexUpdateInfo& updateInfo = aUpdateInfos[idxIndex];
    const int64_t& indexId = updateInfo.indexId();
    const Key& key = updateInfo.value();
    const Key& sortKey = updateInfo.localizedValue();

    bool unique = false;
    MOZ_ALWAYS_TRUE(aUniqueIndexTable.Get(indexId, &unique));

    IndexDataValue idv(indexId, unique, key, sortKey);

    MOZ_ALWAYS_TRUE(aIndexValues.InsertElementSorted(idv, fallible));
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::InsertIndexTableRows(
    DatabaseConnection* aConnection, const int64_t aObjectStoreId,
    const Key& aObjectStoreKey,
    const FallibleTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::InsertIndexTableRows", DOM);

  const uint32_t count = aIndexValues.Length();
  if (!count) {
    return NS_OK;
  }

  NS_NAMED_LITERAL_CSTRING(objectStoreIdString, "object_store_id");
  NS_NAMED_LITERAL_CSTRING(objectDataKeyString, "object_data_key");
  NS_NAMED_LITERAL_CSTRING(indexIdString, "index_id");
  NS_NAMED_LITERAL_CSTRING(valueString, "value");
  NS_NAMED_LITERAL_CSTRING(valueLocaleString, "value_locale");

  DatabaseConnection::CachedStatement insertUniqueStmt;
  DatabaseConnection::CachedStatement insertStmt;

  nsresult rv;

  for (uint32_t index = 0; index < count; index++) {
    const IndexDataValue& info = aIndexValues[index];

    DatabaseConnection::CachedStatement& stmt =
        info.mUnique ? insertUniqueStmt : insertStmt;

    if (stmt) {
      stmt.Reset();
    } else if (info.mUnique) {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("INSERT INTO unique_index_data "
                             "(index_id, value, object_store_id, "
                             "object_data_key, value_locale) "
                             "VALUES (:index_id, :value, :object_store_id, "
                             ":object_data_key, :value_locale);"),
          &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("INSERT OR IGNORE INTO index_data "
                             "(index_id, value, object_data_key, "
                             "object_store_id, value_locale) "
                             "VALUES (:index_id, :value, :object_data_key, "
                             ":object_store_id, :value_locale);"),
          &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = stmt->BindInt64ByName(indexIdString, info.mIndexId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = info.mKey.BindToStatement(stmt, valueString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = info.mSortKey.BindToStatement(stmt, valueLocaleString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(objectStoreIdString, aObjectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aObjectStoreKey.BindToStatement(stmt, objectDataKeyString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (rv == NS_ERROR_STORAGE_CONSTRAINT && info.mUnique) {
      // If we're inserting multiple entries for the same unique index, then
      // we might have failed to insert due to colliding with another entry for
      // the same index in which case we should ignore it.
      for (int32_t index2 = int32_t(index) - 1;
           index2 >= 0 && aIndexValues[index2].mIndexId == info.mIndexId;
           --index2) {
        if (info.mKey == aIndexValues[index2].mKey) {
          // We found a key with the same value for the same index. So we
          // must have had a collision with a value we just inserted.
          rv = NS_OK;
          break;
        }
      }
    }

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::DeleteIndexDataTableRows(
    DatabaseConnection* aConnection, const Key& aObjectStoreKey,
    const FallibleTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::DeleteIndexDataTableRows", DOM);

  const uint32_t count = aIndexValues.Length();
  if (!count) {
    return NS_OK;
  }

  NS_NAMED_LITERAL_CSTRING(indexIdString, "index_id");
  NS_NAMED_LITERAL_CSTRING(valueString, "value");
  NS_NAMED_LITERAL_CSTRING(objectDataKeyString, "object_data_key");

  DatabaseConnection::CachedStatement deleteUniqueStmt;
  DatabaseConnection::CachedStatement deleteStmt;

  nsresult rv;

  for (uint32_t index = 0; index < count; index++) {
    const IndexDataValue& indexValue = aIndexValues[index];

    DatabaseConnection::CachedStatement& stmt =
        indexValue.mUnique ? deleteUniqueStmt : deleteStmt;

    if (stmt) {
      stmt.Reset();
    } else if (indexValue.mUnique) {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("DELETE FROM unique_index_data "
                             "WHERE index_id = :index_id "
                             "AND value = :value;"),
          &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("DELETE FROM index_data "
                             "WHERE index_id = :index_id "
                             "AND value = :value "
                             "AND object_data_key = :object_data_key;"),
          &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = stmt->BindInt64ByName(indexIdString, indexValue.mIndexId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = indexValue.mKey.BindToStatement(stmt, valueString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!indexValue.mUnique) {
      rv = aObjectStoreKey.BindToStatement(stmt, objectDataKeyString);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::DeleteObjectStoreDataTableRowsWithIndexes(
    DatabaseConnection* aConnection, const int64_t aObjectStoreId,
    const Maybe<SerializedKeyRange>& aKeyRange) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aObjectStoreId);

#ifdef DEBUG
  {
    bool hasIndexes = false;
    MOZ_ASSERT(NS_SUCCEEDED(
        ObjectStoreHasIndexes(aConnection, aObjectStoreId, &hasIndexes)));
    MOZ_ASSERT(hasIndexes,
               "Don't use this slow method if there are no indexes!");
  }
#endif

  AUTO_PROFILER_LABEL(
      "DatabaseOperationBase::DeleteObjectStoreDataTableRowsWithIndexes", DOM);

  const bool singleRowOnly = aKeyRange.isSome() && aKeyRange.ref().isOnly();

  NS_NAMED_LITERAL_CSTRING(objectStoreIdString, "object_store_id");
  NS_NAMED_LITERAL_CSTRING(keyString, "key");

  nsresult rv;
  Key objectStoreKey;
  DatabaseConnection::CachedStatement selectStmt;

  if (singleRowOnly) {
    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT index_data_values "
                           "FROM object_data "
                           "WHERE object_store_id = :object_store_id "
                           "AND key = :key;"),
        &selectStmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    objectStoreKey = aKeyRange.ref().lower();

    rv = objectStoreKey.BindToStatement(selectStmt, keyString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    nsAutoCString keyRangeClause;
    if (aKeyRange.isSome()) {
      GetBindingClauseForKeyRange(aKeyRange.ref(), keyString, keyRangeClause);
    }

    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT index_data_values, key "
                           "FROM object_data "
                           "WHERE object_store_id = :") +
            objectStoreIdString + keyRangeClause + NS_LITERAL_CSTRING(";"),
        &selectStmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (aKeyRange.isSome()) {
      rv = BindKeyRangeToStatement(aKeyRange.ref(), selectStmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  rv = selectStmt->BindInt64ByName(objectStoreIdString, aObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DatabaseConnection::CachedStatement deleteStmt;
  AutoTArray<IndexDataValue, 32> indexValues;

  DebugOnly<uint32_t> resultCountDEBUG = 0;

  bool hasResult;
  while (NS_SUCCEEDED(rv = selectStmt->ExecuteStep(&hasResult)) && hasResult) {
    if (!singleRowOnly) {
      rv = objectStoreKey.SetFromStatement(selectStmt, 1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      indexValues.ClearAndRetainStorage();
    }

    rv = ReadCompressedIndexDataValues(selectStmt, 0, indexValues);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = DeleteIndexDataTableRows(aConnection, objectStoreKey, indexValues);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (deleteStmt) {
      MOZ_ALWAYS_SUCCEEDS(deleteStmt->Reset());
    } else {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("DELETE FROM object_data "
                             "WHERE object_store_id = :object_store_id "
                             "AND key = :key;"),
          &deleteStmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = deleteStmt->BindInt64ByName(objectStoreIdString, aObjectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = objectStoreKey.BindToStatement(deleteStmt, keyString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = deleteStmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    resultCountDEBUG++;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(singleRowOnly, resultCountDEBUG <= 1);

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::UpdateIndexValues(
    DatabaseConnection* aConnection, const int64_t aObjectStoreId,
    const Key& aObjectStoreKey,
    const FallibleTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::UpdateIndexValues", DOM);

  UniqueFreePtr<uint8_t> indexDataValues;
  uint32_t indexDataValuesLength;
  nsresult rv = MakeCompressedIndexDataValues(aIndexValues, indexDataValues,
                                              &indexDataValuesLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!indexDataValuesLength == !(indexDataValues.get()));

  DatabaseConnection::CachedStatement updateStmt;
  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("UPDATE object_data "
                         "SET index_data_values = :index_data_values "
                         "WHERE object_store_id = :object_store_id "
                         "AND key = :key;"),
      &updateStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_NAMED_LITERAL_CSTRING(indexDataValuesString, "index_data_values");

  if (indexDataValues) {
    rv = updateStmt->BindAdoptedBlobByName(indexDataValuesString,
                                           indexDataValues.release(),
                                           indexDataValuesLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = updateStmt->BindNullByName(indexDataValuesString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("object_store_id"),
                                   aObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aObjectStoreKey.BindToStatement(updateStmt, NS_LITERAL_CSTRING("key"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = updateStmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::ObjectStoreHasIndexes(
    DatabaseConnection* aConnection, const int64_t aObjectStoreId,
    bool* aHasIndexes) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aHasIndexes);

  DatabaseConnection::CachedStatement stmt;

  nsresult rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT id "
                         "FROM object_store_index "
                         "WHERE object_store_id = :object_store_id "
                         "LIMIT 1;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("object_store_id"),
                             aObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aHasIndexes = hasResult;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(DatabaseOperationBase, Runnable,
                            mozIStorageProgressHandler)

NS_IMETHODIMP
DatabaseOperationBase::OnProgress(mozIStorageConnection* aConnection,
                                  bool* _retval) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(_retval);

  // This is intentionally racy.
  *_retval = !OperationMayProceed();
  return NS_OK;
}

DatabaseOperationBase::AutoSetProgressHandler::AutoSetProgressHandler()
    : mConnection(nullptr)
#ifdef DEBUG
      ,
      mDEBUGDatabaseOp(nullptr)
#endif
{
  MOZ_ASSERT(!IsOnBackgroundThread());
}

DatabaseOperationBase::AutoSetProgressHandler::~AutoSetProgressHandler() {
  MOZ_ASSERT(!IsOnBackgroundThread());

  if (mConnection) {
    Unregister();
  }
}

nsresult DatabaseOperationBase::AutoSetProgressHandler::Register(
    mozIStorageConnection* aConnection, DatabaseOperationBase* aDatabaseOp) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aDatabaseOp);
  MOZ_ASSERT(!mConnection);

  nsCOMPtr<mozIStorageProgressHandler> oldProgressHandler;

  nsresult rv =
      aConnection->SetProgressHandler(kStorageProgressGranularity, aDatabaseOp,
                                      getter_AddRefs(oldProgressHandler));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!oldProgressHandler);

  mConnection = aConnection;
#ifdef DEBUG
  mDEBUGDatabaseOp = aDatabaseOp;
#endif

  return NS_OK;
}

void DatabaseOperationBase::AutoSetProgressHandler::Unregister() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mConnection);

  nsCOMPtr<mozIStorageProgressHandler> oldHandler;
  MOZ_ALWAYS_SUCCEEDS(
      mConnection->RemoveProgressHandler(getter_AddRefs(oldHandler)));
  MOZ_ASSERT(oldHandler == mDEBUGDatabaseOp);

  mConnection = nullptr;
}

MutableFile::MutableFile(nsIFile* aFile, Database* aDatabase,
                         FileInfo* aFileInfo)
    : BackgroundMutableFileParentBase(FILE_HANDLE_STORAGE_IDB, aDatabase->Id(),
                                      IntString(aFileInfo->Id()), aFile),
      mDatabase(aDatabase),
      mFileInfo(aFileInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(aFileInfo);
}

MutableFile::~MutableFile() { mDatabase->UnregisterMutableFile(this); }

already_AddRefed<MutableFile> MutableFile::Create(nsIFile* aFile,
                                                  Database* aDatabase,
                                                  FileInfo* aFileInfo) {
  AssertIsOnBackgroundThread();

  RefPtr<MutableFile> newMutableFile =
      new MutableFile(aFile, aDatabase, aFileInfo);

  if (!aDatabase->RegisterMutableFile(newMutableFile)) {
    return nullptr;
  }

  return newMutableFile.forget();
}

void MutableFile::NoteActiveState() {
  AssertIsOnBackgroundThread();

  mDatabase->NoteActiveMutableFile();
}

void MutableFile::NoteInactiveState() {
  AssertIsOnBackgroundThread();

  mDatabase->NoteInactiveMutableFile();
}

PBackgroundParent* MutableFile::GetBackgroundParent() const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  return GetDatabase()->GetBackgroundParent();
}

already_AddRefed<nsISupports> MutableFile::CreateStream(bool aReadOnly) {
  AssertIsOnBackgroundThread();

  PersistenceType persistenceType = mDatabase->Type();
  const nsACString& group = mDatabase->Group();
  const nsACString& origin = mDatabase->Origin();

  nsCOMPtr<nsISupports> result;

  if (aReadOnly) {
    RefPtr<FileInputStream> stream =
        CreateFileInputStream(persistenceType, group, origin, mFile, -1, -1,
                              nsIFileInputStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileInputStream*, stream);
  } else {
    RefPtr<FileStream> stream =
        CreateFileStream(persistenceType, group, origin, mFile, -1, -1,
                         nsIFileStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileStream*, stream);
  }
  if (NS_WARN_IF(!result)) {
    return nullptr;
  }

  return result.forget();
}

already_AddRefed<BlobImpl> MutableFile::CreateBlobImpl() {
  AssertIsOnBackgroundThread();

  RefPtr<FileBlobImpl> blobImpl = new FileBlobImpl(mFile);
  blobImpl->SetFileId(mFileInfo->Id());

  return blobImpl.forget();
}

PBackgroundFileHandleParent* MutableFile::AllocPBackgroundFileHandleParent(
    const FileMode& aMode) {
  AssertIsOnBackgroundThread();

  // Once a database is closed it must not try to open new file handles.
  if (NS_WARN_IF(mDatabase->IsClosed())) {
    if (!mDatabase->IsInvalidated()) {
      ASSERT_UNLESS_FUZZING();
    }
    return nullptr;
  }

  if (!gFileHandleThreadPool) {
    RefPtr<FileHandleThreadPool> fileHandleThreadPool =
        FileHandleThreadPool::Create();
    if (NS_WARN_IF(!fileHandleThreadPool)) {
      return nullptr;
    }

    gFileHandleThreadPool = fileHandleThreadPool;
  }

  return BackgroundMutableFileParentBase::AllocPBackgroundFileHandleParent(
      aMode);
}

mozilla::ipc::IPCResult MutableFile::RecvPBackgroundFileHandleConstructor(
    PBackgroundFileHandleParent* aActor, const FileMode& aMode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mDatabase->IsClosed());

  if (NS_WARN_IF(mDatabase->IsInvalidated())) {
    // This is an expected race. We don't want the child to die here, just don't
    // actually do any work.
    return IPC_OK();
  }

  return BackgroundMutableFileParentBase::RecvPBackgroundFileHandleConstructor(
      aActor, aMode);
}

mozilla::ipc::IPCResult MutableFile::RecvGetFileId(int64_t* aFileId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mFileInfo);

  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  *aFileId = mFileInfo->Id();
  return IPC_OK();
}

FactoryOp::FactoryOp(Factory* aFactory,
                     already_AddRefed<ContentParent> aContentParent,
                     const CommonFactoryRequestParams& aCommonParams,
                     bool aDeleting)
    : DatabaseOperationBase(aFactory->GetLoggingInfo()->Id(),
                            aFactory->GetLoggingInfo()->NextRequestSN()),
      mFactory(aFactory),
      mContentParent(std::move(aContentParent)),
      mCommonParams(aCommonParams),
      mState(State::Initial),
      mWaitingForPermissionRetry(false),
      mEnforcingQuota(true),
      mDeleting(aDeleting),
      mChromeWriteAccessAllowed(false),
      mFileHandleDisabled(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
}

nsresult FactoryOp::Open() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Initial);

  // Swap this to the stack now to ensure that we release it on this thread.
  RefPtr<ContentParent> contentParent;
  mContentParent.swap(contentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  PermissionRequestBase::PermissionValue permission;
  nsresult rv = CheckPermission(contentParent, &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
             permission == PermissionRequestBase::kPermissionDenied ||
             permission == PermissionRequestBase::kPermissionPrompt);

  if (permission == PermissionRequestBase::kPermissionDenied) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  {
    // These services have to be started on the main thread currently.

    IndexedDatabaseManager* mgr;
    if (NS_WARN_IF(!(mgr = IndexedDatabaseManager::GetOrCreate()))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    nsCOMPtr<mozIStorageService> ss;
    if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  const DatabaseMetadata& metadata = mCommonParams.metadata();

  QuotaManager::GetStorageId(metadata.persistenceType(), mOrigin, Client::IDB,
                             mDatabaseId);

  mDatabaseId.Append('*');
  mDatabaseId.Append(NS_ConvertUTF16toUTF8(metadata.name()));

  if (permission == PermissionRequestBase::kPermissionPrompt) {
    mState = State::PermissionChallenge;
    MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
    return NS_OK;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed);

  mState = State::FinishOpen;
  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult FactoryOp::ChallengePermission() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::PermissionChallenge);

  const PrincipalInfo& principalInfo = mCommonParams.principalInfo();
  MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

  if (NS_WARN_IF(!SendPermissionChallenge(principalInfo))) {
    return NS_ERROR_FAILURE;
  }

  mWaitingForPermissionRetry = true;

  return NS_OK;
}

void FactoryOp::PermissionRetry() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::PermissionChallenge);

  mContentParent = BackgroundParent::GetContentParent(Manager()->Manager());

  mState = State::PermissionRetry;

  mWaitingForPermissionRetry = false;

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
}

nsresult FactoryOp::RetryCheckPermission() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::PermissionRetry);
  MOZ_ASSERT(mCommonParams.principalInfo().type() ==
             PrincipalInfo::TContentPrincipalInfo);

  // Swap this to the stack now to ensure that we release it on this thread.
  RefPtr<ContentParent> contentParent;
  mContentParent.swap(contentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  PermissionRequestBase::PermissionValue permission;
  nsresult rv = CheckPermission(contentParent, &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
             permission == PermissionRequestBase::kPermissionDenied ||
             permission == PermissionRequestBase::kPermissionPrompt);

  if (permission == PermissionRequestBase::kPermissionDenied ||
      permission == PermissionRequestBase::kPermissionPrompt) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed);

  mState = State::FinishOpen;
  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult FactoryOp::DirectoryOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(gFactoryOps);

  // See if this FactoryOp needs to wait.
  bool delayed = false;
  bool foundThis = false;
  for (uint32_t index = gFactoryOps->Length(); index > 0; index--) {
    RefPtr<FactoryOp>& existingOp = (*gFactoryOps)[index - 1];

    if (existingOp == this) {
      foundThis = true;
      continue;
    }

    if (foundThis && MustWaitFor(*existingOp)) {
      // Only one op can be delayed.
      MOZ_ASSERT(!existingOp->mDelayedOp);
      existingOp->mDelayedOp = this;
      delayed = true;
      break;
    }
  }

  if (!delayed) {
    QuotaClient* quotaClient = QuotaClient::GetInstance();
    MOZ_ASSERT(quotaClient);

    if (RefPtr<Maintenance> currentMaintenance =
            quotaClient->GetCurrentMaintenance()) {
      if (RefPtr<DatabaseMaintenance> databaseMaintenance =
              currentMaintenance->GetDatabaseMaintenance(mDatabaseFilePath)) {
        databaseMaintenance->WaitForCompletion(this);
        delayed = true;
      }
    }
  }

  mState = State::DatabaseOpenPending;
  if (!delayed) {
    nsresult rv = DatabaseOpen();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult FactoryOp::SendToIOThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseOpenPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread.
  mState = State::DatabaseWorkOpen;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void FactoryOp::WaitForTransactions() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange ||
             mState == State::WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mDatabaseId.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  mState = State::WaitingForTransactionsToComplete;

  RefPtr<WaitForTransactionsHelper> helper =
      new WaitForTransactionsHelper(mDatabaseId, this);
  helper->WaitForTransactions();
}

void FactoryOp::CleanupMetadata() {
  AssertIsOnOwningThread();

  if (mDelayedOp) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mDelayedOp.forget()));
  }

  MOZ_ASSERT(gFactoryOps);
  gFactoryOps->RemoveElement(this);

  // Match the IncreaseBusyCount in AllocPBackgroundIDBFactoryRequestParent().
  DecreaseBusyCount();
}

void FactoryOp::FinishSendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(mFactory);

  // Make sure to release the factory on this thread.
  RefPtr<Factory> factory;
  mFactory.swap(factory);

  mState = State::Completed;
}

nsresult FactoryOp::CheckPermission(
    ContentParent* aContentParent,
    PermissionRequestBase::PermissionValue* aPermission) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Initial || mState == State::PermissionRetry);

  const PrincipalInfo& principalInfo = mCommonParams.principalInfo();
  if (principalInfo.type() != PrincipalInfo::TSystemPrincipalInfo) {
    if (principalInfo.type() != PrincipalInfo::TContentPrincipalInfo) {
      if (aContentParent) {
        // We just want ContentPrincipalInfo or SystemPrincipalInfo.
        aContentParent->KillHard("IndexedDB CheckPermission 0");
      }

      return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
    }

    if (NS_WARN_IF(!Preferences::GetBool(kPrefIndexedDBEnabled, false))) {
      if (aContentParent) {
        // The DOM in the other process should have kept us from receiving any
        // indexedDB messages so assume that the child is misbehaving.
        aContentParent->KillHard("IndexedDB CheckPermission 1");
      }

      return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
    }

    const ContentPrincipalInfo& contentPrincipalInfo =
        principalInfo.get_ContentPrincipalInfo();
    if (contentPrincipalInfo.attrs().mPrivateBrowsingId != 0) {
      // IndexedDB is currently disabled in privateBrowsing.
      return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
    }
  }

  mFileHandleDisabled = !Preferences::GetBool(kPrefFileHandleEnabled);

  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  MOZ_ASSERT(principalInfo.type() != PrincipalInfo::TNullPrincipalInfo);

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    MOZ_ASSERT(mState == State::Initial);
    MOZ_ASSERT(persistenceType == PERSISTENCE_TYPE_PERSISTENT);

    if (aContentParent) {
      // Check to make sure that the child process has access to the database it
      // is accessing.
      NS_NAMED_LITERAL_CSTRING(permissionStringBase,
                               PERMISSION_STRING_CHROME_BASE);
      NS_ConvertUTF16toUTF8 databaseName(mCommonParams.metadata().name());
      NS_NAMED_LITERAL_CSTRING(readSuffix,
                               PERMISSION_STRING_CHROME_READ_SUFFIX);
      NS_NAMED_LITERAL_CSTRING(writeSuffix,
                               PERMISSION_STRING_CHROME_WRITE_SUFFIX);

      const nsAutoCString permissionStringWrite =
          permissionStringBase + databaseName + writeSuffix;
      const nsAutoCString permissionStringRead =
          permissionStringBase + databaseName + readSuffix;

      bool canWrite = CheckAtLeastOneAppHasPermission(aContentParent,
                                                      permissionStringWrite);

      bool canRead;
      if (canWrite) {
        MOZ_ASSERT(CheckAtLeastOneAppHasPermission(aContentParent,
                                                   permissionStringRead));
        canRead = true;
      } else {
        canRead = CheckAtLeastOneAppHasPermission(aContentParent,
                                                  permissionStringRead);
      }

      // Deleting a database requires write permissions.
      if (mDeleting && !canWrite) {
        aContentParent->KillHard("IndexedDB CheckPermission 2");
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      // Opening or deleting requires read permissions.
      if (!canRead) {
        aContentParent->KillHard("IndexedDB CheckPermission 3");
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      mChromeWriteAccessAllowed = canWrite;
    } else {
      mChromeWriteAccessAllowed = true;
    }

    if (State::Initial == mState) {
      QuotaManager::GetInfoForChrome(&mSuffix, &mGroup, &mOrigin);

      MOZ_ASSERT(QuotaManager::IsOriginInternal(mOrigin));

      mEnforcingQuota = false;
    }

    *aPermission = PermissionRequestBase::kPermissionAllowed;
    return NS_OK;
  }

  MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString suffix;
  nsCString group;
  nsCString origin;
  rv = QuotaManager::GetInfoFromPrincipal(principal, &suffix, &group, &origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PermissionRequestBase::PermissionValue permission;

  if (persistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    if (QuotaManager::IsOriginInternal(origin)) {
      permission = PermissionRequestBase::kPermissionAllowed;
    } else {
#ifdef IDB_MOBILE
      return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
#else
      rv = PermissionRequestBase::GetCurrentPermission(principal, &permission);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
#endif
    }
  } else {
    permission = PermissionRequestBase::kPermissionAllowed;
  }

  if (permission != PermissionRequestBase::kPermissionDenied &&
      State::Initial == mState) {
    mSuffix = suffix;
    mGroup = group;
    mOrigin = origin;

    mEnforcingQuota = persistenceType != PERSISTENCE_TYPE_PERSISTENT;
  }

  *aPermission = permission;
  return NS_OK;
}

nsresult FactoryOp::SendVersionChangeMessages(
    DatabaseActorInfo* aDatabaseActorInfo, Database* aOpeningDatabase,
    uint64_t aOldVersion, const Maybe<uint64_t>& aNewVersion) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseActorInfo);
  MOZ_ASSERT(mState == State::BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  const uint32_t expectedCount = mDeleting ? 0 : 1;
  const uint32_t liveCount = aDatabaseActorInfo->mLiveDatabases.Length();
  if (liveCount > expectedCount) {
    FallibleTArray<MaybeBlockedDatabaseInfo> maybeBlockedDatabases;
    for (uint32_t index = 0; index < liveCount; index++) {
      Database* database = aDatabaseActorInfo->mLiveDatabases[index];
      if ((!aOpeningDatabase || database != aOpeningDatabase) &&
          !database->IsClosed() &&
          NS_WARN_IF(
              !maybeBlockedDatabases.AppendElement(database, fallible))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    if (!maybeBlockedDatabases.IsEmpty()) {
      mMaybeBlockedDatabases.SwapElements(maybeBlockedDatabases);
    }
  }

  if (!mMaybeBlockedDatabases.IsEmpty()) {
    for (uint32_t count = mMaybeBlockedDatabases.Length(), index = 0;
         index < count;
         /* incremented conditionally */) {
      if (mMaybeBlockedDatabases[index]->SendVersionChange(aOldVersion,
                                                           aNewVersion)) {
        index++;
      } else {
        // We don't want to wait forever if we were not able to send the
        // message.
        mMaybeBlockedDatabases.RemoveElementAt(index);
        count--;
      }
    }
  }

  return NS_OK;
}

// static
bool FactoryOp::CheckAtLeastOneAppHasPermission(
    ContentParent* aContentParent, const nsACString& aPermissionString) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContentParent);
  MOZ_ASSERT(!aPermissionString.IsEmpty());

  return true;
}

nsresult FactoryOp::FinishOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::FinishOpen);
  MOZ_ASSERT(!mContentParent);

  if (QuotaManager::Get()) {
    nsresult rv = OpenDirectory();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  mState = State::QuotaManagerPending;
  QuotaManager::GetOrCreate(this);

  return NS_OK;
}

nsresult FactoryOp::QuotaManagerOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::QuotaManagerPending);

  if (NS_WARN_IF(!QuotaManager::Get())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = OpenDirectory();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult FactoryOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::FinishOpen ||
             mState == State::QuotaManagerPending);
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(QuotaManager::Get());

  // Need to get database file path in advance.
  const nsString& databaseName = mCommonParams.metadata().name();
  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = quotaManager->GetDirectoryForOrigin(persistenceType, mOrigin,
                                                    getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString filename;
  GetDatabaseFilename(databaseName, filename);

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->GetPath(mDatabaseFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mState = State::DirectoryOpenPending;

  quotaManager->OpenDirectory(persistenceType, mGroup, mOrigin, Client::IDB,
                              /* aExclusive */ false, this);

  return NS_OK;
}

bool FactoryOp::MustWaitFor(const FactoryOp& aExistingOp) {
  AssertIsOnOwningThread();

  // Things for the same persistence type, the same origin and the same
  // database must wait.
  return aExistingOp.mCommonParams.metadata().persistenceType() ==
             mCommonParams.metadata().persistenceType() &&
         aExistingOp.mOrigin == mOrigin &&
         aExistingOp.mDatabaseId == mDatabaseId;
}

void FactoryOp::NoteDatabaseBlocked(Database* aDatabase) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(mMaybeBlockedDatabases.Contains(aDatabase));

  // Only send the blocked event if all databases have reported back. If the
  // database was closed then it will have been removed from the array.
  // Otherwise if it was blocked its |mBlocked| flag will be true.
  bool sendBlockedEvent = true;

  for (uint32_t count = mMaybeBlockedDatabases.Length(), index = 0;
       index < count; index++) {
    MaybeBlockedDatabaseInfo& info = mMaybeBlockedDatabases[index];
    if (info == aDatabase) {
      // This database was blocked, mark accordingly.
      info.mBlocked = true;
    } else if (!info.mBlocked) {
      // A database has not yet reported back yet, don't send the event yet.
      sendBlockedEvent = false;
    }
  }

  if (sendBlockedEvent) {
    SendBlockedNotification();
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(FactoryOp, DatabaseOperationBase)

// Run() assumes that the caller holds a strong reference to the object that
// can't be cleared while Run() is being executed.
// So if you call Run() directly (as opposed to dispatching to an event queue)
// you need to make sure there's such a reference.
// See bug 1356824 for more details.
NS_IMETHODIMP
FactoryOp::Run() {
  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = Open();
      break;

    case State::PermissionChallenge:
      rv = ChallengePermission();
      break;

    case State::PermissionRetry:
      rv = RetryCheckPermission();
      break;

    case State::FinishOpen:
      rv = FinishOpen();
      break;

    case State::QuotaManagerPending:
      rv = QuotaManagerOpen();
      break;

    case State::DatabaseOpenPending:
      rv = DatabaseOpen();
      break;

    case State::DatabaseWorkOpen:
      rv = DoDatabaseWork();
      break;

    case State::BeginVersionChange:
      rv = BeginVersionChange();
      break;

    case State::WaitingForTransactionsToComplete:
      rv = DispatchToWorkThread();
      break;

    case State::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::SendingResults) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::SendingResults;

    if (IsOnOwningThread()) {
      SendResults();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void FactoryOp::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  nsresult rv = DirectoryOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // The caller holds a strong reference to us, no need for a self reference
    // before calling Run().

    mState = State::SendingResults;
    MOZ_ALWAYS_SUCCEEDS(Run());

    return;
  }
}

void FactoryOp::DirectoryLockFailed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  if (NS_SUCCEEDED(mResultCode)) {
    IDB_REPORT_INTERNAL_ERR();
    mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  // The caller holds a strong reference to us, no need for a self reference
  // before calling Run().

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(Run());
}

void FactoryOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();

  // Assume ActorDestroy can happen at any time, so we can't probe the current
  // state since mState can be modified on any thread (only one thread at a time
  // based on the state machine).  However we can use mWaitingForPermissionRetry
  // which is only touched on the owning thread.  If mWaitingForPermissionRetry
  // is true, we can also modify mState since we are guaranteed that there are
  // no pending runnables which would probe mState to decide what code needs to
  // run (there shouldn't be any running runnables on other threads either).

  if (mWaitingForPermissionRetry) {
    PermissionRetry();
  }

  // We don't have to handle the case when mWaitingForPermissionRetry is not
  // true since it means that either nothing has been initialized yet, so
  // nothing to cleanup or there are pending runnables that will detect that the
  // actor has been destroyed and cleanup accordingly.
}

mozilla::ipc::IPCResult FactoryOp::RecvPermissionRetry() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!IsActorDestroyed());

  PermissionRetry();

  return IPC_OK();
}

OpenDatabaseOp::OpenDatabaseOp(Factory* aFactory,
                               already_AddRefed<ContentParent> aContentParent,
                               const CommonFactoryRequestParams& aParams)
    : FactoryOp(aFactory, std::move(aContentParent), aParams,
                /* aDeleting */ false),
      mMetadata(new FullDatabaseMetadata(aParams.metadata())),
      mRequestedVersion(aParams.metadata().version()),
      mVersionChangeOp(nullptr),
      mTelemetryId(0) {
  if (mContentParent) {
    // This is a little scary but it looks safe to call this off the main thread
    // for now.
    mOptionalContentParentId = Some(mContentParent->ChildID());
  }
}

void OpenDatabaseOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  FactoryOp::ActorDestroy(aWhy);

  if (mVersionChangeOp) {
    mVersionChangeOp->NoteActorDestroyed();
  }
}

nsresult OpenDatabaseOp::DatabaseOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseOpenPending);

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult OpenDatabaseOp::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DatabaseWorkOpen);

  AUTO_PROFILER_LABEL("OpenDatabaseOp::DoDatabaseWork", DOM);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const nsString& databaseName = mCommonParams.metadata().name();
  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> dbDirectory;

  nsresult rv = quotaManager->EnsureOriginIsInitialized(
      persistenceType, mSuffix, mGroup, mOrigin,
      /* aCreateIfNotExists */ true, getter_AddRefs(dbDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbDirectory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = dbDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    rv = dbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#ifdef DEBUG
  else {
    bool isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(dbDirectory->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory);
  }
#endif

  nsAutoString filename;
  GetDatabaseFilename(databaseName, filename);

  nsCOMPtr<nsIFile> markerFile;
  rv = dbDirectory->Clone(getter_AddRefs(markerFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = markerFile->Append(NS_LITERAL_STRING(IDB_DELETION_MARKER_FILE_PREFIX) +
                          filename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  markerFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    // Delete the database and directroy since they should be deleted in
    // previous operation.
    // Note: only update usage to the QuotaManager when mEnforcingQuota == true
    rv = RemoveDatabaseFilesAndDirectory(
        dbDirectory, filename, mEnforcingQuota ? quotaManager : nullptr,
        persistenceType, mGroup, mOrigin, databaseName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIFile> dbFile;
  rv = dbDirectory->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mTelemetryId = TelemetryIdForFile(dbFile);

#ifdef DEBUG
  nsString databaseFilePath;
  rv = dbFile->GetPath(databaseFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(databaseFilePath == mDatabaseFilePath);
#endif

  nsCOMPtr<nsIFile> fmDirectory;
  rv = dbDirectory->Clone(getter_AddRefs(fmDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const NS_ConvertASCIItoUTF16 filesSuffix(kFileManagerDirectoryNameSuffix);

  rv = fmDirectory->Append(filename + filesSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateStorageConnection(dbFile, fmDirectory, databaseName,
                               persistenceType, mGroup, mOrigin, mTelemetryId,
                               getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoSetProgressHandler asph;
  rv = asph.Register(connection, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = LoadDatabaseInformation(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(mMetadata->mNextObjectStoreId > mMetadata->mObjectStores.Count());
  MOZ_ASSERT(mMetadata->mNextIndexId > 0);

  // See if we need to do a versionchange transaction

  // Optional version semantics.
  if (!mRequestedVersion) {
    // If the requested version was not specified and the database was created,
    // treat it as if version 1 were requested.
    if (mMetadata->mCommonMetadata.version() == 0) {
      mRequestedVersion = 1;
    } else {
      // Otherwise, treat it as if the current version were requested.
      mRequestedVersion = mMetadata->mCommonMetadata.version();
    }
  }

  if (NS_WARN_IF(mMetadata->mCommonMetadata.version() > mRequestedVersion)) {
    return NS_ERROR_DOM_INDEXEDDB_VERSION_ERR;
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  MOZ_ASSERT(mgr);

  RefPtr<FileManager> fileManager =
      mgr->GetFileManager(persistenceType, mOrigin, databaseName);
  if (!fileManager) {
    fileManager = new FileManager(persistenceType, mGroup, mOrigin,
                                  databaseName, mEnforcingQuota);

    rv = fileManager->Init(fmDirectory, connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mgr->AddFileManager(fileManager);
  }

  mFileManager = fileManager.forget();

  // Must close connection before dispatching otherwise we might race with the
  // connection thread which needs to open the same database.
  asph.Unregister();

  MOZ_ALWAYS_SUCCEEDS(connection->Close());

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = (mMetadata->mCommonMetadata.version() == mRequestedVersion)
               ? State::SendingResults
               : State::BeginVersionChange;

  rv = mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult OpenDatabaseOp::LoadDatabaseInformation(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(mMetadata);

  // Load version information.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT name, origin, version "
                         "FROM database"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  nsString databaseName;
  rv = stmt->GetString(0, databaseName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(mCommonParams.metadata().name() != databaseName)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  nsCString origin;
  rv = stmt->GetUTF8String(1, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We can't just compare these strings directly. See bug 1339081 comment 69.
  if (NS_WARN_IF(!QuotaManager::AreOriginsEqualOnDisk(mOrigin, origin))) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  int64_t version;
  rv = stmt->GetInt64(2, &version);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mMetadata->mCommonMetadata.version() = uint64_t(version);

  ObjectStoreTable& objectStores = mMetadata->mObjectStores;

  // Load object store names and ids.
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT id, auto_increment, name, key_path "
                         "FROM object_store"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Maybe<nsTHashtable<nsUint64HashKey>> usedIds;
  Maybe<nsTHashtable<nsStringHashKey>> usedNames;

  int64_t lastObjectStoreId = 0;

  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    int64_t objectStoreId;
    rv = stmt->GetInt64(0, &objectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!usedIds) {
      usedIds.emplace();
    }

    if (NS_WARN_IF(objectStoreId <= 0) ||
        NS_WARN_IF(usedIds.ref().Contains(objectStoreId))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedIds.ref().PutEntry(objectStoreId, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsString name;
    rv = stmt->GetString(2, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!usedNames) {
      usedNames.emplace();
    }

    if (NS_WARN_IF(usedNames.ref().Contains(name))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedNames.ref().PutEntry(name, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    RefPtr<FullObjectStoreMetadata> metadata = new FullObjectStoreMetadata();
    metadata->mCommonMetadata.id() = objectStoreId;
    metadata->mCommonMetadata.name() = name;

    int32_t columnType;
    rv = stmt->GetTypeOfIndex(3, &columnType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (columnType == mozIStorageStatement::VALUE_TYPE_NULL) {
      metadata->mCommonMetadata.keyPath() = KeyPath(0);
    } else {
      MOZ_ASSERT(columnType == mozIStorageStatement::VALUE_TYPE_TEXT);

      nsString keyPathSerialization;
      rv = stmt->GetString(3, keyPathSerialization);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      metadata->mCommonMetadata.keyPath() =
          KeyPath::DeserializeFromString(keyPathSerialization);
      if (NS_WARN_IF(!metadata->mCommonMetadata.keyPath().IsValid())) {
        return NS_ERROR_FILE_CORRUPTED;
      }
    }

    int64_t nextAutoIncrementId;
    rv = stmt->GetInt64(1, &nextAutoIncrementId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    metadata->mCommonMetadata.autoIncrement() = !!nextAutoIncrementId;
    metadata->mNextAutoIncrementId = nextAutoIncrementId;
    metadata->mCommittedAutoIncrementId = nextAutoIncrementId;

    if (NS_WARN_IF(!objectStores.Put(objectStoreId, metadata, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    lastObjectStoreId = std::max(lastObjectStoreId, objectStoreId);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  usedIds.reset();
  usedNames.reset();

  // Load index information
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING(
          "SELECT "
          "id, object_store_id, name, key_path, unique_index, multientry, "
          "locale, is_auto_locale "
          "FROM object_store_index"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t lastIndexId = 0;

  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    int64_t objectStoreId;
    rv = stmt->GetInt64(1, &objectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<FullObjectStoreMetadata> objectStoreMetadata;
    if (NS_WARN_IF(!objectStores.Get(objectStoreId,
                                     getter_AddRefs(objectStoreMetadata)))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    MOZ_ASSERT(objectStoreMetadata->mCommonMetadata.id() == objectStoreId);

    int64_t indexId;
    rv = stmt->GetInt64(0, &indexId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!usedIds) {
      usedIds.emplace();
    }

    if (NS_WARN_IF(indexId <= 0) ||
        NS_WARN_IF(usedIds.ref().Contains(indexId))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedIds.ref().PutEntry(indexId, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsString name;
    rv = stmt->GetString(2, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoString hashName;
    hashName.AppendInt(indexId);
    hashName.Append(':');
    hashName.Append(name);

    if (!usedNames) {
      usedNames.emplace();
    }

    if (NS_WARN_IF(usedNames.ref().Contains(hashName))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedNames.ref().PutEntry(hashName, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    RefPtr<FullIndexMetadata> indexMetadata = new FullIndexMetadata();
    indexMetadata->mCommonMetadata.id() = indexId;
    indexMetadata->mCommonMetadata.name() = name;

#ifdef DEBUG
    {
      int32_t columnType;
      rv = stmt->GetTypeOfIndex(3, &columnType);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      MOZ_ASSERT(columnType != mozIStorageStatement::VALUE_TYPE_NULL);
    }
#endif

    nsString keyPathSerialization;
    rv = stmt->GetString(3, keyPathSerialization);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    indexMetadata->mCommonMetadata.keyPath() =
        KeyPath::DeserializeFromString(keyPathSerialization);
    if (NS_WARN_IF(!indexMetadata->mCommonMetadata.keyPath().IsValid())) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    int32_t scratch;
    rv = stmt->GetInt32(4, &scratch);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    indexMetadata->mCommonMetadata.unique() = !!scratch;

    rv = stmt->GetInt32(5, &scratch);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    indexMetadata->mCommonMetadata.multiEntry() = !!scratch;

    const bool localeAware = !stmt->IsNull(6);
    if (localeAware) {
      rv = stmt->GetUTF8String(6, indexMetadata->mCommonMetadata.locale());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->GetInt32(7, &scratch);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      indexMetadata->mCommonMetadata.autoLocale() = !!scratch;

      // Update locale-aware indexes if necessary
      const nsCString& indexedLocale = indexMetadata->mCommonMetadata.locale();
      const bool& isAutoLocale = indexMetadata->mCommonMetadata.autoLocale();
      nsCString systemLocale = IndexedDatabaseManager::GetLocale();
      if (!systemLocale.IsEmpty() && isAutoLocale &&
          !indexedLocale.EqualsASCII(systemLocale.get())) {
        rv = UpdateLocaleAwareIndex(aConnection, indexMetadata->mCommonMetadata,
                                    systemLocale);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    if (NS_WARN_IF(!objectStoreMetadata->mIndexes.Put(indexId, indexMetadata,
                                                      fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    lastIndexId = std::max(lastIndexId, indexId);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(lastObjectStoreId == INT64_MAX) ||
      NS_WARN_IF(lastIndexId == INT64_MAX)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mMetadata->mNextObjectStoreId = lastObjectStoreId + 1;
  mMetadata->mNextIndexId = lastIndexId + 1;

  return NS_OK;
}

/* static */
nsresult OpenDatabaseOp::UpdateLocaleAwareIndex(
    mozIStorageConnection* aConnection, const IndexMetadata& aIndexMetadata,
    const nsCString& aLocale) {
  nsresult rv;

  nsCString indexTable;
  if (aIndexMetadata.unique()) {
    indexTable.AssignLiteral("unique_index_data");
  } else {
    indexTable.AssignLiteral("index_data");
  }

  nsCString readQuery =
      NS_LITERAL_CSTRING("SELECT value, object_data_key FROM ") + indexTable +
      NS_LITERAL_CSTRING(" WHERE index_id = :index_id");
  nsCOMPtr<mozIStorageStatement> readStmt;
  rv = aConnection->CreateStatement(readQuery, getter_AddRefs(readStmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = readStmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                 aIndexMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> writeStmt;
  bool needCreateWriteQuery = true;
  bool hasResult;
  while (NS_SUCCEEDED((rv = readStmt->ExecuteStep(&hasResult))) && hasResult) {
    if (needCreateWriteQuery) {
      needCreateWriteQuery = false;
      nsCString writeQuery = NS_LITERAL_CSTRING("UPDATE ") + indexTable +
                             NS_LITERAL_CSTRING(
                                 "SET value_locale = :value_locale "
                                 "WHERE index_id = :index_id AND "
                                 "value = :value AND "
                                 "object_data_key = :object_data_key");
      rv = aConnection->CreateStatement(writeQuery, getter_AddRefs(writeStmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    mozStorageStatementScoper scoper(writeStmt);
    rv = writeStmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                    aIndexMetadata.id());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    Key oldKey, newSortKey, objectKey;
    rv = oldKey.SetFromStatement(readStmt, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = oldKey.BindToStatement(writeStmt, NS_LITERAL_CSTRING("value"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = oldKey.ToLocaleBasedKey(newSortKey, aLocale);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = newSortKey.BindToStatement(writeStmt,
                                    NS_LITERAL_CSTRING("value_locale"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = objectKey.SetFromStatement(readStmt, 1);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = objectKey.BindToStatement(writeStmt,
                                   NS_LITERAL_CSTRING("object_data_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = writeStmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCString metaQuery = NS_LITERAL_CSTRING(
      "UPDATE object_store_index SET "
      "locale = :locale WHERE id = :id");
  nsCOMPtr<mozIStorageStatement> metaStmt;
  rv = aConnection->CreateStatement(metaQuery, getter_AddRefs(metaStmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString locale;
  CopyASCIItoUTF16(aLocale, locale);
  rv = metaStmt->BindStringByName(NS_LITERAL_CSTRING("locale"), locale);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = metaStmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), aIndexMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = metaStmt->Execute();
  return rv;
}

nsresult OpenDatabaseOp::BeginVersionChange() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(mMetadata->mCommonMetadata.version() <= mRequestedVersion);
  MOZ_ASSERT(!mDatabase);
  MOZ_ASSERT(!mVersionChangeTransaction);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  EnsureDatabaseActor();

  if (mDatabase->IsInvalidated()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  MOZ_ASSERT(!mDatabase->IsClosed());

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(mDatabaseId, &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(mDatabase));
  MOZ_ASSERT(!info->mWaitingFactoryOp);
  MOZ_ASSERT(info->mMetadata == mMetadata);

  RefPtr<VersionChangeTransaction> transaction =
      new VersionChangeTransaction(this);

  if (NS_WARN_IF(!transaction->CopyDatabaseMetadata())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ASSERT(info->mMetadata != mMetadata);
  mMetadata = info->mMetadata;

  Maybe<uint64_t> newVersion = Some(mRequestedVersion);

  nsresult rv = SendVersionChangeMessages(
      info, mDatabase, mMetadata->mCommonMetadata.version(), newVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mVersionChangeTransaction.swap(transaction);

  if (mMaybeBlockedDatabases.IsEmpty()) {
    // We don't need to wait on any databases, just jump to the transaction
    // pool.
    WaitForTransactions();
    return NS_OK;
  }

  info->mWaitingFactoryOp = this;

  mState = State::WaitingForOtherDatabasesToClose;
  return NS_OK;
}

void OpenDatabaseOp::NoteDatabaseClosed(Database* aDatabase) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose ||
             mState == State::WaitingForTransactionsToComplete ||
             mState == State::DatabaseWorkVersionChange);

  if (mState != State::WaitingForOtherDatabasesToClose) {
    MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
    MOZ_ASSERT(
        mRequestedVersion > aDatabase->Metadata()->mCommonMetadata.version(),
        "Must only be closing databases for a previous version!");
    return;
  }

  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());

  bool actorDestroyed = IsActorDestroyed() || mDatabase->IsActorDestroyed();

  nsresult rv;
  if (actorDestroyed) {
    IDB_REPORT_INTERNAL_ERR();
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  } else {
    rv = NS_OK;
  }

  // We are being called with an assumption that mWaitingFactoryOp holds
  // a strong reference to us.
  RefPtr<OpenDatabaseOp> kungFuDeathGrip;

  if (mMaybeBlockedDatabases.RemoveElement(aDatabase) &&
      mMaybeBlockedDatabases.IsEmpty()) {
    if (actorDestroyed) {
      DatabaseActorInfo* info;
      MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(mDatabaseId, &info));
      MOZ_ASSERT(info->mWaitingFactoryOp == this);
      kungFuDeathGrip =
          static_cast<OpenDatabaseOp*>(info->mWaitingFactoryOp.get());
      info->mWaitingFactoryOp = nullptr;
    } else {
      WaitForTransactions();
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // A strong reference is held in kungFuDeathGrip, so it's safe to call Run()
    // directly.

    mState = State::SendingResults;
    MOZ_ALWAYS_SUCCEEDS(Run());
  }
}

void OpenDatabaseOp::SendBlockedNotification() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);

  if (!IsActorDestroyed()) {
    Unused << SendBlocked(mMetadata->mCommonMetadata.version());
  }
}

nsresult OpenDatabaseOp::DispatchToWorkThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForTransactionsToComplete);
  MOZ_ASSERT(mVersionChangeTransaction);
  MOZ_ASSERT(mVersionChangeTransaction->GetMode() ==
             IDBTransaction::VERSION_CHANGE);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed() || mDatabase->IsInvalidated()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mState = State::DatabaseWorkVersionChange;

  // Intentionally empty.
  nsTArray<nsString> objectStoreNames;

  const int64_t loggingSerialNumber =
      mVersionChangeTransaction->LoggingSerialNumber();
  const nsID& backgroundChildLoggingId =
      mVersionChangeTransaction->GetLoggingInfo()->Id();

  if (NS_WARN_IF(!mDatabase->RegisterTransaction(mVersionChangeTransaction))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!gConnectionPool) {
    gConnectionPool = new ConnectionPool();
  }

  RefPtr<VersionChangeOp> versionChangeOp = new VersionChangeOp(this);

  uint64_t transactionId = versionChangeOp->StartOnConnectionPool(
      backgroundChildLoggingId, mVersionChangeTransaction->DatabaseId(),
      loggingSerialNumber, objectStoreNames,
      /* aIsWriteTransaction */ true);

  mVersionChangeOp = versionChangeOp;

  mVersionChangeTransaction->NoteActiveRequest();
  mVersionChangeTransaction->SetActive(transactionId);

  return NS_OK;
}

nsresult OpenDatabaseOp::SendUpgradeNeeded() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseWorkVersionChange);
  MOZ_ASSERT(mVersionChangeTransaction);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
  MOZ_ASSERT_IF(!IsActorDestroyed(), mDatabase);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  RefPtr<VersionChangeTransaction> transaction;
  mVersionChangeTransaction.swap(transaction);

  nsresult rv = EnsureDatabaseActorIsAlive();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Transfer ownership to IPDL.
  transaction->SetActorAlive();

  if (!mDatabase->SendPBackgroundIDBVersionChangeTransactionConstructor(
          transaction, mMetadata->mCommonMetadata.version(), mRequestedVersion,
          mMetadata->mNextObjectStoreId, mMetadata->mNextIndexId)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void OpenDatabaseOp::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT_IF(NS_SUCCEEDED(mResultCode), mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT_IF(NS_SUCCEEDED(mResultCode), !mVersionChangeTransaction);

  mMaybeBlockedDatabases.Clear();

  DatabaseActorInfo* info;
  if (gLiveDatabaseHashtable &&
      gLiveDatabaseHashtable->Get(mDatabaseId, &info) &&
      info->mWaitingFactoryOp) {
    MOZ_ASSERT(info->mWaitingFactoryOp == this);
    // SendResults() should only be called by Run() and Run() should only be
    // called if there's a strong reference to the object that can't be cleared
    // here, so it's safe to clear mWaitingFactoryOp without adding additional
    // strong reference.
    info->mWaitingFactoryOp = nullptr;
  }

  if (mVersionChangeTransaction) {
    MOZ_ASSERT(NS_FAILED(mResultCode));

    mVersionChangeTransaction->Abort(mResultCode, /* aForce */ true);
    mVersionChangeTransaction = nullptr;
  }

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  } else {
    FactoryRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      // If we just successfully completed a versionchange operation then we
      // need to update the version in our metadata.
      mMetadata->mCommonMetadata.version() = mRequestedVersion;

      nsresult rv = EnsureDatabaseActorIsAlive();
      if (NS_SUCCEEDED(rv)) {
        // We successfully opened a database so use its actor as the success
        // result for this request.
        OpenDatabaseRequestResponse openResponse;
        openResponse.databaseParent() = mDatabase;
        response = openResponse;
      } else {
        response = ClampResultCode(rv);
#ifdef DEBUG
        mResultCode = response.get_nsresult();
#endif
      }
    } else {
#ifdef DEBUG
      // If something failed then our metadata pointer is now bad. No one should
      // ever touch it again though so just null it out in DEBUG builds to make
      // sure we find such cases.
      mMetadata = nullptr;
#endif
      response = ClampResultCode(mResultCode);
    }

    Unused << PBackgroundIDBFactoryRequestParent::Send__delete__(this,
                                                                 response);
  }

  if (mDatabase) {
    MOZ_ASSERT(!mDirectoryLock);

    if (NS_FAILED(mResultCode)) {
      mDatabase->Invalidate();
    }

    // Make sure to release the database on this thread.
    mDatabase = nullptr;

    CleanupMetadata();
  } else if (mDirectoryLock) {
    // ConnectionClosedCallback will call CleanupMetadata().
    nsCOMPtr<nsIRunnable> callback = NewRunnableMethod(
        "dom::indexedDB::OpenDatabaseOp::ConnectionClosedCallback", this,
        &OpenDatabaseOp::ConnectionClosedCallback);

    RefPtr<WaitForTransactionsHelper> helper =
        new WaitForTransactionsHelper(mDatabaseId, callback);
    helper->WaitForTransactions();
  } else {
    CleanupMetadata();
  }

  FinishSendResults();
}

void OpenDatabaseOp::ConnectionClosedCallback() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(mResultCode));
  MOZ_ASSERT(mDirectoryLock);

  mDirectoryLock = nullptr;

  CleanupMetadata();
}

void OpenDatabaseOp::EnsureDatabaseActor() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange ||
             mState == State::DatabaseWorkVersionChange ||
             mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
  MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  if (mDatabase) {
    return;
  }

  MOZ_ASSERT(mMetadata->mDatabaseId.IsEmpty());
  mMetadata->mDatabaseId = mDatabaseId;

  MOZ_ASSERT(mMetadata->mFilePath.IsEmpty());
  mMetadata->mFilePath = mDatabaseFilePath;

  DatabaseActorInfo* info;
  if (gLiveDatabaseHashtable->Get(mDatabaseId, &info)) {
    AssertMetadataConsistency(info->mMetadata);
    mMetadata = info->mMetadata;
  }

  auto factory = static_cast<Factory*>(Manager());

  mDatabase = new Database(
      factory, mCommonParams.principalInfo(), mOptionalContentParentId, mGroup,
      mOrigin, mTelemetryId, mMetadata, mFileManager, mDirectoryLock.forget(),
      mFileHandleDisabled, mChromeWriteAccessAllowed);

  if (info) {
    info->mLiveDatabases.AppendElement(mDatabase);
  } else {
    info = new DatabaseActorInfo(mMetadata, mDatabase);
    gLiveDatabaseHashtable->Put(mDatabaseId, info);
  }

  // Balanced in Database::CleanupMetadata().
  IncreaseBusyCount();
}

nsresult OpenDatabaseOp::EnsureDatabaseActorIsAlive() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseWorkVersionChange ||
             mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
  MOZ_ASSERT(!IsActorDestroyed());

  EnsureDatabaseActor();

  if (mDatabase->IsActorAlive()) {
    return NS_OK;
  }

  auto factory = static_cast<Factory*>(Manager());

  DatabaseSpec spec;
  MetadataToSpec(spec);

  // Transfer ownership to IPDL.
  mDatabase->SetActorAlive();

  if (!factory->SendPBackgroundIDBDatabaseConstructor(mDatabase, spec, this)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void OpenDatabaseOp::MetadataToSpec(DatabaseSpec& aSpec) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  aSpec.metadata() = mMetadata->mCommonMetadata;

  for (auto objectStoreIter = mMetadata->mObjectStores.ConstIter();
       !objectStoreIter.Done(); objectStoreIter.Next()) {
    FullObjectStoreMetadata* metadata = objectStoreIter.UserData();
    MOZ_ASSERT(objectStoreIter.Key());
    MOZ_ASSERT(metadata);

    // XXX This should really be fallible...
    ObjectStoreSpec* objectStoreSpec = aSpec.objectStores().AppendElement();
    objectStoreSpec->metadata() = metadata->mCommonMetadata;

    for (auto indexIter = metadata->mIndexes.Iter(); !indexIter.Done();
         indexIter.Next()) {
      FullIndexMetadata* indexMetadata = indexIter.UserData();
      MOZ_ASSERT(indexIter.Key());
      MOZ_ASSERT(indexMetadata);

      // XXX This should really be fallible...
      IndexMetadata* metadata = objectStoreSpec->indexes().AppendElement();
      *metadata = indexMetadata->mCommonMetadata;
    }
  }
}

#ifdef DEBUG

void OpenDatabaseOp::AssertMetadataConsistency(
    const FullDatabaseMetadata* aMetadata) {
  AssertIsOnBackgroundThread();

  const FullDatabaseMetadata* thisDB = mMetadata;
  const FullDatabaseMetadata* otherDB = aMetadata;

  MOZ_ASSERT(thisDB);
  MOZ_ASSERT(otherDB);
  MOZ_ASSERT(thisDB != otherDB);

  MOZ_ASSERT(thisDB->mCommonMetadata.name() == otherDB->mCommonMetadata.name());
  MOZ_ASSERT(thisDB->mCommonMetadata.version() ==
             otherDB->mCommonMetadata.version());
  MOZ_ASSERT(thisDB->mCommonMetadata.persistenceType() ==
             otherDB->mCommonMetadata.persistenceType());
  MOZ_ASSERT(thisDB->mDatabaseId == otherDB->mDatabaseId);
  MOZ_ASSERT(thisDB->mFilePath == otherDB->mFilePath);

  // |thisDB| reflects the latest objectStore and index ids that have committed
  // to disk. The in-memory metadata |otherDB| keeps track of objectStores and
  // indexes that were created and then removed as well, so the next ids for
  // |otherDB| may be higher than for |thisDB|.
  MOZ_ASSERT(thisDB->mNextObjectStoreId <= otherDB->mNextObjectStoreId);
  MOZ_ASSERT(thisDB->mNextIndexId <= otherDB->mNextIndexId);

  MOZ_ASSERT(thisDB->mObjectStores.Count() == otherDB->mObjectStores.Count());

  for (auto objectStoreIter = thisDB->mObjectStores.ConstIter();
       !objectStoreIter.Done(); objectStoreIter.Next()) {
    FullObjectStoreMetadata* thisObjectStore = objectStoreIter.UserData();
    MOZ_ASSERT(thisObjectStore);
    MOZ_ASSERT(!thisObjectStore->mDeleted);

    auto* otherObjectStore =
        MetadataNameOrIdMatcher<FullObjectStoreMetadata>::Match(
            otherDB->mObjectStores, thisObjectStore->mCommonMetadata.id());
    MOZ_ASSERT(otherObjectStore);

    MOZ_ASSERT(thisObjectStore != otherObjectStore);

    MOZ_ASSERT(thisObjectStore->mCommonMetadata.id() ==
               otherObjectStore->mCommonMetadata.id());
    MOZ_ASSERT(thisObjectStore->mCommonMetadata.name() ==
               otherObjectStore->mCommonMetadata.name());
    MOZ_ASSERT(thisObjectStore->mCommonMetadata.autoIncrement() ==
               otherObjectStore->mCommonMetadata.autoIncrement());
    MOZ_ASSERT(thisObjectStore->mCommonMetadata.keyPath() ==
               otherObjectStore->mCommonMetadata.keyPath());
    // mNextAutoIncrementId and mCommittedAutoIncrementId may be modified
    // concurrently with this OpenOp, so it is not possible to assert equality
    // here. It's also possible that we've written the new ids to disk but not
    // yet updated the in-memory count.
    MOZ_ASSERT(thisObjectStore->mNextAutoIncrementId <=
               otherObjectStore->mNextAutoIncrementId);
    MOZ_ASSERT(thisObjectStore->mCommittedAutoIncrementId <=
                   otherObjectStore->mCommittedAutoIncrementId ||
               thisObjectStore->mCommittedAutoIncrementId ==
                   otherObjectStore->mNextAutoIncrementId);
    MOZ_ASSERT(!otherObjectStore->mDeleted);

    MOZ_ASSERT(thisObjectStore->mIndexes.Count() ==
               otherObjectStore->mIndexes.Count());

    for (auto indexIter = thisObjectStore->mIndexes.Iter(); !indexIter.Done();
         indexIter.Next()) {
      FullIndexMetadata* thisIndex = indexIter.UserData();
      MOZ_ASSERT(thisIndex);
      MOZ_ASSERT(!thisIndex->mDeleted);

      auto* otherIndex = MetadataNameOrIdMatcher<FullIndexMetadata>::Match(
          otherObjectStore->mIndexes, thisIndex->mCommonMetadata.id());
      MOZ_ASSERT(otherIndex);

      MOZ_ASSERT(thisIndex != otherIndex);

      MOZ_ASSERT(thisIndex->mCommonMetadata.id() ==
                 otherIndex->mCommonMetadata.id());
      MOZ_ASSERT(thisIndex->mCommonMetadata.name() ==
                 otherIndex->mCommonMetadata.name());
      MOZ_ASSERT(thisIndex->mCommonMetadata.keyPath() ==
                 otherIndex->mCommonMetadata.keyPath());
      MOZ_ASSERT(thisIndex->mCommonMetadata.unique() ==
                 otherIndex->mCommonMetadata.unique());
      MOZ_ASSERT(thisIndex->mCommonMetadata.multiEntry() ==
                 otherIndex->mCommonMetadata.multiEntry());
      MOZ_ASSERT(!otherIndex->mDeleted);
    }
  }
}

#endif  // DEBUG

nsresult OpenDatabaseOp::VersionChangeOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State::DatabaseWorkVersionChange);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  AUTO_PROFILER_LABEL("OpenDatabaseOp::VersionChangeOp::DoDatabaseWork", DOM);

  IDB_LOG_MARK(
      "IndexedDB %s: Parent Transaction[%lld]: "
      "Beginning database work",
      "IndexedDB %s: P T[%lld]: DB Start",
      IDB_LOG_ID_STRING(mBackgroundChildLoggingId), mLoggingSerialNumber);

  Transaction()->SetActiveOnConnectionThread();

  nsresult rv = aConnection->BeginWriteTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DatabaseConnection::CachedStatement updateStmt;
  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("UPDATE database "
                         "SET version = :version;"),
      &updateStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("version"),
                                   int64_t(mRequestedVersion));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = updateStmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult OpenDatabaseOp::VersionChangeOp::SendSuccessResult() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State::DatabaseWorkVersionChange);
  MOZ_ASSERT(mOpenDatabaseOp->mVersionChangeOp == this);

  nsresult rv = mOpenDatabaseOp->SendUpgradeNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool OpenDatabaseOp::VersionChangeOp::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State::DatabaseWorkVersionChange);
  MOZ_ASSERT(mOpenDatabaseOp->mVersionChangeOp == this);

  mOpenDatabaseOp->SetFailureCode(aResultCode);
  mOpenDatabaseOp->mState = State::SendingResults;

  MOZ_ALWAYS_SUCCEEDS(mOpenDatabaseOp->Run());

  return false;
}

void OpenDatabaseOp::VersionChangeOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mVersionChangeOp == this);

  mOpenDatabaseOp->mVersionChangeOp = nullptr;
  mOpenDatabaseOp = nullptr;

#ifdef DEBUG
  // A bit hacky but the VersionChangeOp is not generated in response to a
  // child request like most other database operations. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

void DeleteDatabaseOp::LoadPreviousVersion(nsIFile* aDatabaseFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDatabaseFile);
  MOZ_ASSERT(mState == State::DatabaseWorkOpen);
  MOZ_ASSERT(!mPreviousVersion);

  AUTO_PROFILER_LABEL("DeleteDatabaseOp::LoadPreviousVersion", DOM);

  nsresult rv;

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = OpenDatabaseAndHandleBusy(ss, aDatabaseFile, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

#ifdef DEBUG
  {
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(NS_LITERAL_CSTRING("SELECT name "
                                                        "FROM database"),
                                     getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsString databaseName;
    rv = stmt->GetString(0, databaseName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    MOZ_ASSERT(mCommonParams.metadata().name() == databaseName);
  }
#endif

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(NS_LITERAL_CSTRING("SELECT version "
                                                      "FROM database"),
                                   getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_WARN_IF(!hasResult)) {
    return;
  }

  int64_t version;
  rv = stmt->GetInt64(0, &version);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mPreviousVersion = uint64_t(version);
}

nsresult DeleteDatabaseOp::DatabaseOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseOpenPending);

  // Swap this to the stack now to ensure that we release it on this thread.
  RefPtr<ContentParent> contentParent;
  mContentParent.swap(contentParent);

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DeleteDatabaseOp::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DatabaseWorkOpen);

  AUTO_PROFILER_LABEL("DeleteDatabaseOp::DoDatabaseWork", DOM);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const nsString& databaseName = mCommonParams.metadata().name();
  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(persistenceType, mOrigin,
                                                    getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directory->GetPath(mDatabaseDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString filename;
  GetDatabaseFilename(databaseName, filename);

  mDatabaseFilenameBase = filename;

  nsCOMPtr<nsIFile> dbFile;
  rv = directory->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  nsString databaseFilePath;
  rv = dbFile->GetPath(databaseFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(databaseFilePath == mDatabaseFilePath);
#endif

  bool exists;
  rv = dbFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    // Parts of this function may fail but that shouldn't prevent us from
    // deleting the file eventually.
    LoadPreviousVersion(dbFile);

    mState = State::BeginVersionChange;
  } else {
    mState = State::SendingResults;
  }

  rv = mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DeleteDatabaseOp::BeginVersionChange() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  DatabaseActorInfo* info;
  if (gLiveDatabaseHashtable->Get(mDatabaseId, &info)) {
    MOZ_ASSERT(!info->mWaitingFactoryOp);

    nsresult rv =
        SendVersionChangeMessages(info, nullptr, mPreviousVersion, Nothing());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mMaybeBlockedDatabases.IsEmpty()) {
      info->mWaitingFactoryOp = this;

      mState = State::WaitingForOtherDatabasesToClose;
      return NS_OK;
    }
  }

  // No other databases need to be notified, just make sure that all
  // transactions are complete.
  WaitForTransactions();
  return NS_OK;
}

nsresult DeleteDatabaseOp::DispatchToWorkThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForTransactionsToComplete);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mState = State::DatabaseWorkVersionChange;

  RefPtr<VersionChangeOp> versionChangeOp = new VersionChangeOp(this);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsresult rv = quotaManager->IOThread()->Dispatch(versionChangeOp.forget(),
                                                   NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void DeleteDatabaseOp::NoteDatabaseClosed(Database* aDatabase) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());

  bool actorDestroyed = IsActorDestroyed();

  nsresult rv;
  if (actorDestroyed) {
    IDB_REPORT_INTERNAL_ERR();
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  } else {
    rv = NS_OK;
  }

  // We are being called with an assumption that mWaitingFactoryOp holds
  // a strong reference to us.
  RefPtr<OpenDatabaseOp> kungFuDeathGrip;

  if (mMaybeBlockedDatabases.RemoveElement(aDatabase) &&
      mMaybeBlockedDatabases.IsEmpty()) {
    if (actorDestroyed) {
      DatabaseActorInfo* info;
      MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(mDatabaseId, &info));
      MOZ_ASSERT(info->mWaitingFactoryOp == this);
      kungFuDeathGrip =
          static_cast<OpenDatabaseOp*>(info->mWaitingFactoryOp.get());
      info->mWaitingFactoryOp = nullptr;
    } else {
      WaitForTransactions();
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // A strong reference is held in kungFuDeathGrip, so it's safe to call Run()
    // directly.

    mState = State::SendingResults;
    MOZ_ALWAYS_SUCCEEDS(Run());
  }
}

void DeleteDatabaseOp::SendBlockedNotification() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);

  if (!IsActorDestroyed()) {
    Unused << SendBlocked(mPreviousVersion);
  }
}

void DeleteDatabaseOp::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (!IsActorDestroyed()) {
    FactoryRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      response = DeleteDatabaseRequestResponse(mPreviousVersion);
    } else {
      response = ClampResultCode(mResultCode);
    }

    Unused << PBackgroundIDBFactoryRequestParent::Send__delete__(this,
                                                                 response);
  }

  mDirectoryLock = nullptr;

  CleanupMetadata();

  FinishSendResults();
}

nsresult DeleteDatabaseOp::VersionChangeOp::RunOnIOThread() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mDeleteDatabaseOp->mState == State::DatabaseWorkVersionChange);

  AUTO_PROFILER_LABEL("DeleteDatabaseOp::VersionChangeOp::RunOnIOThread", DOM);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const PersistenceType& persistenceType =
      mDeleteDatabaseOp->mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager =
      mDeleteDatabaseOp->mEnforcingQuota ? QuotaManager::Get() : nullptr;

  MOZ_ASSERT_IF(mDeleteDatabaseOp->mEnforcingQuota, quotaManager);

  nsCOMPtr<nsIFile> directory =
      GetFileForPath(mDeleteDatabaseOp->mDatabaseDirectoryPath);
  if (NS_WARN_IF(!directory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv = RemoveDatabaseFilesAndDirectory(
      directory, mDeleteDatabaseOp->mDatabaseFilenameBase, quotaManager,
      persistenceType, mDeleteDatabaseOp->mGroup, mDeleteDatabaseOp->mOrigin,
      mDeleteDatabaseOp->mCommonParams.metadata().name());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void DeleteDatabaseOp::VersionChangeOp::RunOnOwningThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDeleteDatabaseOp->mState == State::DatabaseWorkVersionChange);

  RefPtr<DeleteDatabaseOp> deleteOp;
  mDeleteDatabaseOp.swap(deleteOp);

  if (deleteOp->IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    deleteOp->SetFailureCode(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  } else {
    DatabaseActorInfo* info;
    if (gLiveDatabaseHashtable->Get(deleteOp->mDatabaseId, &info) &&
        info->mWaitingFactoryOp) {
      MOZ_ASSERT(info->mWaitingFactoryOp == deleteOp);
      info->mWaitingFactoryOp = nullptr;
    }

    if (NS_FAILED(mResultCode)) {
      if (NS_SUCCEEDED(deleteOp->ResultCode())) {
        deleteOp->SetFailureCode(mResultCode);
      }
    } else {
      // Inform all the other databases that they are now invalidated. That
      // should remove the previous metadata from our table.
      if (info) {
        MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());

        FallibleTArray<Database*> liveDatabases;
        if (NS_WARN_IF(!liveDatabases.AppendElements(info->mLiveDatabases,
                                                     fallible))) {
          deleteOp->SetFailureCode(NS_ERROR_OUT_OF_MEMORY);
        } else {
#ifdef DEBUG
          // The code below should result in the deletion of |info|. Set to null
          // here to make sure we find invalid uses later.
          info = nullptr;
#endif
          for (uint32_t count = liveDatabases.Length(), index = 0;
               index < count; index++) {
            RefPtr<Database> database = liveDatabases[index];
            database->Invalidate();
          }

          MOZ_ASSERT(!gLiveDatabaseHashtable->Get(deleteOp->mDatabaseId));
        }
      }
    }
  }

  // We hold a strong ref to the deleteOp, so it's safe to call Run() directly.

  deleteOp->mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(deleteOp->Run());

#ifdef DEBUG
  // A bit hacky but the DeleteDatabaseOp::VersionChangeOp is not really a
  // normal database operation that is tied to an actor. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif
}

nsresult DeleteDatabaseOp::VersionChangeOp::Run() {
  nsresult rv;

  if (IsOnIOThread()) {
    rv = RunOnIOThread();
  } else {
    RunOnOwningThread();
    rv = NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
  }

  return NS_OK;
}

TransactionDatabaseOperationBase::TransactionDatabaseOperationBase(
    TransactionBase* aTransaction)
    : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                            aTransaction->GetLoggingInfo()->NextRequestSN()),
      mTransaction(aTransaction),
      mTransactionLoggingSerialNumber(aTransaction->LoggingSerialNumber()),
      mInternalState(InternalState::Initial),
      mTransactionIsAborted(aTransaction->IsAborted()) {
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(LoggingSerialNumber());
}

TransactionDatabaseOperationBase::TransactionDatabaseOperationBase(
    TransactionBase* aTransaction, uint64_t aLoggingSerialNumber)
    : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                            aLoggingSerialNumber),
      mTransaction(aTransaction),
      mTransactionLoggingSerialNumber(aTransaction->LoggingSerialNumber()),
      mInternalState(InternalState::Initial),
      mTransactionIsAborted(aTransaction->IsAborted()) {
  MOZ_ASSERT(aTransaction);
}

TransactionDatabaseOperationBase::~TransactionDatabaseOperationBase() {
  MOZ_ASSERT(mInternalState == InternalState::Completed);
  MOZ_ASSERT(!mTransaction,
             "TransactionDatabaseOperationBase::Cleanup() was not called by a "
             "subclass!");
}

#ifdef DEBUG

void TransactionDatabaseOperationBase::AssertIsOnConnectionThread() const {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();
}

#endif  // DEBUG

uint64_t TransactionDatabaseOperationBase::StartOnConnectionPool(
    const nsID& aBackgroundChildLoggingId, const nsACString& aDatabaseId,
    int64_t aLoggingSerialNumber, const nsTArray<nsString>& aObjectStoreNames,
    bool aIsWriteTransaction) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);

  // Must set mInternalState before dispatching otherwise we will race with the
  // connection thread.
  mInternalState = InternalState::DatabaseWork;

  return gConnectionPool->Start(aBackgroundChildLoggingId, aDatabaseId,
                                aLoggingSerialNumber, aObjectStoreNames,
                                aIsWriteTransaction, this);
}

void TransactionDatabaseOperationBase::DispatchToConnectionPool() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);

  Unused << this->Run();
}

void TransactionDatabaseOperationBase::RunOnConnectionThread() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mInternalState == InternalState::DatabaseWork);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));

  AUTO_PROFILER_LABEL("TransactionDatabaseOperationBase::RunOnConnectionThread",
                      DOM);

  // There are several cases where we don't actually have to to any work here.

  if (mTransactionIsAborted || mTransaction->IsInvalidatedOnAnyThread()) {
    // This transaction is already set to be aborted or invalidated.
    mResultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  } else if (!OperationMayProceed()) {
    // The operation was canceled in some way, likely because the child process
    // has crashed.
    IDB_REPORT_INTERNAL_ERR();
    mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  } else {
    Database* database = mTransaction->GetDatabase();
    MOZ_ASSERT(database);

    // Here we're actually going to perform the database operation.
    nsresult rv = database->EnsureConnection();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mResultCode = rv;
    } else {
      DatabaseConnection* connection = database->GetConnection();
      MOZ_ASSERT(connection);
      MOZ_ASSERT(connection->GetStorageConnection());

      AutoSetProgressHandler autoProgress;
      if (mLoggingSerialNumber) {
        rv = autoProgress.Register(connection->GetStorageConnection(), this);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          mResultCode = rv;
        }
      }

      if (NS_SUCCEEDED(rv)) {
        if (mLoggingSerialNumber) {
          IDB_LOG_MARK(
              "IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
              "Beginning database work",
              "IndexedDB %s: P T[%lld] R[%llu]: DB Start",
              IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
              mTransactionLoggingSerialNumber, mLoggingSerialNumber);
        }

        rv = DoDatabaseWork(connection);

        if (mLoggingSerialNumber) {
          IDB_LOG_MARK(
              "IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
              "Finished database work",
              "IndexedDB %s: P T[%lld] R[%llu]: DB End",
              IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
              mTransactionLoggingSerialNumber, mLoggingSerialNumber);
        }

        if (NS_FAILED(rv)) {
          mResultCode = rv;
        }
      }
    }
  }

  // Must set mInternalState before dispatching otherwise we will race with the
  // owning thread.
  if (HasPreprocessInfo()) {
    mInternalState = InternalState::SendingPreprocess;
  } else {
    mInternalState = InternalState::SendingResults;
  }

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
}

bool TransactionDatabaseOperationBase::HasPreprocessInfo() { return false; }

nsresult TransactionDatabaseOperationBase::SendPreprocessInfo() {
  return NS_OK;
}

void TransactionDatabaseOperationBase::NoteContinueReceived() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::WaitingForContinue);

  mInternalState = InternalState::SendingResults;

  // This TransactionDatabaseOperationBase can only be held alive by the IPDL.
  // Run() can end up with clearing that last reference. So we need to add
  // a self reference here.
  RefPtr<TransactionDatabaseOperationBase> kungFuDeathGrip = this;

  Unused << this->Run();
}

void TransactionDatabaseOperationBase::SendToConnectionPool() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);

  // Must set mInternalState before dispatching otherwise we will race with the
  // connection thread.
  mInternalState = InternalState::DatabaseWork;

  gConnectionPool->Dispatch(mTransaction->TransactionId(), this);

  mTransaction->NoteActiveRequest();
}

void TransactionDatabaseOperationBase::SendPreprocess() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingPreprocess);

  SendPreprocessInfoOrResults(/* aSendPreprocessInfo */ true);
}

void TransactionDatabaseOperationBase::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingResults);

  SendPreprocessInfoOrResults(/* aSendPreprocessInfo */ false);
}

void TransactionDatabaseOperationBase::SendPreprocessInfoOrResults(
    bool aSendPreprocessInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingPreprocess ||
             mInternalState == InternalState::SendingResults);
  MOZ_ASSERT(mTransaction);

  if (NS_WARN_IF(IsActorDestroyed())) {
    // Normally we wouldn't need to send any notifications if the actor was
    // already destroyed, but this can be a VersionChangeOp which needs to
    // notify its parent operation (OpenDatabaseOp) about the failure.
    // So SendFailureResult needs to be called even when the actor was
    // destroyed.  Normal operations redundantly check if the actor was
    // destroyed in SendSuccessResult and SendFailureResult, therefore it's
    // ok to call it in all cases here.
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  } else if (mTransaction->IsInvalidated() || mTransaction->IsAborted()) {
    // Aborted transactions always see their requests fail with ABORT_ERR,
    // even if the request succeeded or failed with another error.
    mResultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  if (NS_SUCCEEDED(mResultCode)) {
    if (aSendPreprocessInfo) {
      // This should not release the IPDL reference.
      mResultCode = SendPreprocessInfo();
    } else {
      // This may release the IPDL reference.
      mResultCode = SendSuccessResult();
    }
  }

  if (NS_FAILED(mResultCode)) {
    // This should definitely release the IPDL reference.
    if (!SendFailureResult(mResultCode)) {
      // Abort the transaction.
      mTransaction->Abort(mResultCode, /* aForce */ false);
    }
  }

  if (aSendPreprocessInfo && NS_SUCCEEDED(mResultCode)) {
    mInternalState = InternalState::WaitingForContinue;
  } else {
    if (mLoggingSerialNumber) {
      mTransaction->NoteFinishedRequest();
    }

    Cleanup();

    mInternalState = InternalState::Completed;
  }
}

bool TransactionDatabaseOperationBase::Init(TransactionBase* aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);
  MOZ_ASSERT(aTransaction);

  return true;
}

void TransactionDatabaseOperationBase::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingResults);
  MOZ_ASSERT(mTransaction);

  mTransaction = nullptr;
}

NS_IMETHODIMP
TransactionDatabaseOperationBase::Run() {
  switch (mInternalState) {
    case InternalState::Initial:
      SendToConnectionPool();
      return NS_OK;

    case InternalState::DatabaseWork:
      RunOnConnectionThread();
      return NS_OK;

    case InternalState::SendingPreprocess:
      SendPreprocess();
      return NS_OK;

    case InternalState::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }
}

TransactionBase::CommitOp::CommitOp(TransactionBase* aTransaction,
                                    nsresult aResultCode)
    : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                            aTransaction->GetLoggingInfo()->NextRequestSN()),
      mTransaction(aTransaction),
      mResultCode(aResultCode) {
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(LoggingSerialNumber());
}

nsresult TransactionBase::CommitOp::WriteAutoIncrementCounts() {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();
  MOZ_ASSERT(mTransaction->GetMode() == IDBTransaction::READ_WRITE ||
             mTransaction->GetMode() == IDBTransaction::READ_WRITE_FLUSH ||
             mTransaction->GetMode() == IDBTransaction::CLEANUP ||
             mTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);

  const nsTArray<RefPtr<FullObjectStoreMetadata>>& metadataArray =
      mTransaction->mModifiedAutoIncrementObjectStoreMetadataArray;

  if (!metadataArray.IsEmpty()) {
    NS_NAMED_LITERAL_CSTRING(osid, "osid");
    NS_NAMED_LITERAL_CSTRING(ai, "ai");

    Database* database = mTransaction->GetDatabase();
    MOZ_ASSERT(database);

    DatabaseConnection* connection = database->GetConnection();
    MOZ_ASSERT(connection);

    DatabaseConnection::CachedStatement stmt;
    nsresult rv;

    for (uint32_t count = metadataArray.Length(), index = 0; index < count;
         index++) {
      const RefPtr<FullObjectStoreMetadata>& metadata = metadataArray[index];
      MOZ_ASSERT(!metadata->mDeleted);
      MOZ_ASSERT(metadata->mNextAutoIncrementId > 1);

      if (stmt) {
        MOZ_ALWAYS_SUCCEEDS(stmt->Reset());
      } else {
        rv = connection->GetCachedStatement(
            NS_LITERAL_CSTRING("UPDATE object_store "
                               "SET auto_increment = :") +
                ai + NS_LITERAL_CSTRING(" WHERE id = :") + osid +
                NS_LITERAL_CSTRING(";"),
            &stmt);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      rv = stmt->BindInt64ByName(osid, metadata->mCommonMetadata.id());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindInt64ByName(ai, metadata->mNextAutoIncrementId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

void TransactionBase::CommitOp::CommitOrRollbackAutoIncrementCounts() {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();
  MOZ_ASSERT(mTransaction->GetMode() == IDBTransaction::READ_WRITE ||
             mTransaction->GetMode() == IDBTransaction::READ_WRITE_FLUSH ||
             mTransaction->GetMode() == IDBTransaction::CLEANUP ||
             mTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);

  nsTArray<RefPtr<FullObjectStoreMetadata>>& metadataArray =
      mTransaction->mModifiedAutoIncrementObjectStoreMetadataArray;

  if (!metadataArray.IsEmpty()) {
    bool committed = NS_SUCCEEDED(mResultCode);

    for (uint32_t count = metadataArray.Length(), index = 0; index < count;
         index++) {
      RefPtr<FullObjectStoreMetadata>& metadata = metadataArray[index];

      if (committed) {
        metadata->mCommittedAutoIncrementId = metadata->mNextAutoIncrementId;
      } else {
        metadata->mNextAutoIncrementId = metadata->mCommittedAutoIncrementId;
      }
    }
  }
}

#ifdef DEBUG

void TransactionBase::CommitOp::AssertForeignKeyConsistency(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();
  MOZ_ASSERT(mTransaction->GetMode() != IDBTransaction::READ_ONLY);

  DatabaseConnection::CachedStatement pragmaStmt;
  MOZ_ALWAYS_SUCCEEDS(aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("PRAGMA foreign_keys;"), &pragmaStmt));

  bool hasResult;
  MOZ_ALWAYS_SUCCEEDS(pragmaStmt->ExecuteStep(&hasResult));

  MOZ_ASSERT(hasResult);

  int32_t foreignKeysEnabled;
  MOZ_ALWAYS_SUCCEEDS(pragmaStmt->GetInt32(0, &foreignKeysEnabled));

  MOZ_ASSERT(foreignKeysEnabled, "Database doesn't have foreign keys enabled!");

  DatabaseConnection::CachedStatement checkStmt;
  MOZ_ALWAYS_SUCCEEDS(aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("PRAGMA foreign_key_check;"), &checkStmt));

  MOZ_ALWAYS_SUCCEEDS(checkStmt->ExecuteStep(&hasResult));

  MOZ_ASSERT(!hasResult, "Database has inconsisistent foreign keys!");
}

#endif  // DEBUG

NS_IMPL_ISUPPORTS_INHERITED0(TransactionBase::CommitOp, DatabaseOperationBase)

NS_IMETHODIMP
TransactionBase::CommitOp::Run() {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("TransactionBase::CommitOp::Run", DOM);

  IDB_LOG_MARK(
      "IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
      "Beginning database work",
      "IndexedDB %s: P T[%lld] R[%llu]: DB Start",
      IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
      mTransaction->LoggingSerialNumber(), mLoggingSerialNumber);

  if (mTransaction->GetMode() != IDBTransaction::READ_ONLY &&
      mTransaction->mHasBeenActiveOnConnectionThread) {
    Database* database = mTransaction->GetDatabase();
    MOZ_ASSERT(database);

    if (DatabaseConnection* connection = database->GetConnection()) {
      // May be null if the VersionChangeOp was canceled.
      DatabaseConnection::UpdateRefcountFunction* fileRefcountFunction =
          connection->GetUpdateRefcountFunction();

      if (NS_SUCCEEDED(mResultCode)) {
        if (fileRefcountFunction) {
          mResultCode = fileRefcountFunction->WillCommit();
          NS_WARNING_ASSERTION(NS_SUCCEEDED(mResultCode),
                               "WillCommit() failed!");
        }

        if (NS_SUCCEEDED(mResultCode)) {
          mResultCode = WriteAutoIncrementCounts();
          NS_WARNING_ASSERTION(NS_SUCCEEDED(mResultCode),
                               "WriteAutoIncrementCounts() failed!");

          if (NS_SUCCEEDED(mResultCode)) {
            AssertForeignKeyConsistency(connection);

            mResultCode = connection->CommitWriteTransaction();
            NS_WARNING_ASSERTION(NS_SUCCEEDED(mResultCode), "Commit failed!");

            if (NS_SUCCEEDED(mResultCode) &&
                mTransaction->GetMode() == IDBTransaction::READ_WRITE_FLUSH) {
              mResultCode = connection->Checkpoint();
            }

            if (NS_SUCCEEDED(mResultCode) && fileRefcountFunction) {
              fileRefcountFunction->DidCommit();
            }
          }
        }
      }

      if (NS_FAILED(mResultCode)) {
        if (fileRefcountFunction) {
          fileRefcountFunction->DidAbort();
        }

        connection->RollbackWriteTransaction();
      }

      CommitOrRollbackAutoIncrementCounts();

      connection->FinishWriteTransaction();

      if (mTransaction->GetMode() == IDBTransaction::CLEANUP) {
        connection->DoIdleProcessing(/* aNeedsCheckpoint */ true);

        connection->EnableQuotaChecks();
      }
    }
  }

  IDB_LOG_MARK(
      "IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
      "Finished database work",
      "IndexedDB %s: P T[%lld] R[%llu]: DB End",
      IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
      mTransaction->LoggingSerialNumber(), mLoggingSerialNumber);

  IDB_LOG_MARK(
      "IndexedDB %s: Parent Transaction[%lld]: "
      "Finished database work",
      "IndexedDB %s: P T[%lld]: DB End",
      IDB_LOG_ID_STRING(mBackgroundChildLoggingId), mLoggingSerialNumber);

  return NS_OK;
}

void TransactionBase::CommitOp::TransactionFinishedBeforeUnblock() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction);

  AUTO_PROFILER_LABEL("CommitOp::TransactionFinishedBeforeUnblock", DOM);

  if (!IsActorDestroyed()) {
    mTransaction->UpdateMetadata(mResultCode);
  }
}

void TransactionBase::CommitOp::TransactionFinishedAfterUnblock() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction);

  IDB_LOG_MARK(
      "IndexedDB %s: Parent Transaction[%lld]: "
      "Finished with result 0x%x",
      "IndexedDB %s: P T[%lld]: Transaction finished (0x%x)",
      IDB_LOG_ID_STRING(mTransaction->GetLoggingInfo()->Id()),
      mTransaction->LoggingSerialNumber(), mResultCode);

  mTransaction->SendCompleteNotification(ClampResultCode(mResultCode));

  Database* database = mTransaction->GetDatabase();
  MOZ_ASSERT(database);

  database->UnregisterTransaction(mTransaction);

  mTransaction = nullptr;

#ifdef DEBUG
  // A bit hacky but the CommitOp is not really a normal database operation
  // that is tied to an actor. Do this to make our assertions happy.
  NoteActorDestroyed();
#endif
}

DatabaseOp::DatabaseOp(Database* aDatabase)
    : DatabaseOperationBase(aDatabase->GetLoggingInfo()->Id(),
                            aDatabase->GetLoggingInfo()->NextRequestSN()),
      mDatabase(aDatabase),
      mState(State::Initial) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
}

nsresult DatabaseOp::SendToIOThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Initial);

  if (!OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  // Must set this before dispatching otherwise we will race with the IO thread.
  mState = State::DatabaseWork;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

NS_IMETHODIMP
DatabaseOp::Run() {
  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = SendToIOThread();
      break;

    case State::DatabaseWork:
      rv = DoDatabaseWork();
      break;

    case State::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::SendingResults) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::SendingResults;

    MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
  }

  return NS_OK;
}

void DatabaseOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();
}

CreateFileOp::CreateFileOp(Database* aDatabase,
                           const DatabaseRequestParams& aParams)
    : DatabaseOp(aDatabase), mParams(aParams.get_CreateFileParams()) {
  MOZ_ASSERT(aParams.type() == DatabaseRequestParams::TCreateFileParams);
}

nsresult CreateFileOp::CreateMutableFile(MutableFile** aMutableFile) {
  nsCOMPtr<nsIFile> file = FileInfo::GetFileForFileInfo(mFileInfo);
  if (NS_WARN_IF(!file)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  RefPtr<MutableFile> mutableFile =
      MutableFile::Create(file, mDatabase, mFileInfo);
  if (NS_WARN_IF(!mutableFile)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  // Transfer ownership to IPDL.
  mutableFile->SetActorAlive();

  if (!mDatabase->SendPBackgroundMutableFileConstructor(
          mutableFile, mParams.name(), mParams.type())) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mutableFile.forget(aMutableFile);
  return NS_OK;
}

nsresult CreateFileOp::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DatabaseWork);

  AUTO_PROFILER_LABEL("CreateFileOp::DoDatabaseWork", DOM);

  if (NS_WARN_IF(QuotaManager::IsShuttingDown()) || !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  FileManager* fileManager = mDatabase->GetFileManager();

  mFileInfo = fileManager->GetNewFileInfo();
  if (NS_WARN_IF(!mFileInfo)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const int64_t fileId = mFileInfo->Id();

  nsCOMPtr<nsIFile> journalDirectory = fileManager->EnsureJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFile> journalFile =
      fileManager->GetFileForId(journalDirectory, fileId);
  if (NS_WARN_IF(!journalFile)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv = journalFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> fileDirectory = fileManager->GetDirectory();
  if (NS_WARN_IF(!fileDirectory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFile> file = fileManager->GetFileForId(fileDirectory, fileId);
  if (NS_WARN_IF(!file)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = State::SendingResults;

  rv = mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void CreateFileOp::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (!IsActorDestroyed() && !mDatabase->IsInvalidated()) {
    DatabaseRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      RefPtr<MutableFile> mutableFile;
      nsresult rv = CreateMutableFile(getter_AddRefs(mutableFile));
      if (NS_SUCCEEDED(rv)) {
        // We successfully created a mutable file so use its actor as the
        // success result for this request.
        CreateFileRequestResponse createResponse;
        createResponse.mutableFileParent() = mutableFile;
        response = createResponse;
      } else {
        response = ClampResultCode(rv);
#ifdef DEBUG
        mResultCode = response.get_nsresult();
#endif
      }
    } else {
      response = ClampResultCode(mResultCode);
    }

    Unused << PBackgroundIDBDatabaseRequestParent::Send__delete__(this,
                                                                  response);
  }

  mState = State::Completed;
}

nsresult VersionChangeTransactionOp::SendSuccessResult() {
  AssertIsOnOwningThread();

  // Nothing to send here, the API assumes that this request always succeeds.
  return NS_OK;
}

bool VersionChangeTransactionOp::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();

  // The only option here is to cause the transaction to abort.
  return false;
}

void VersionChangeTransactionOp::Cleanup() {
  AssertIsOnOwningThread();

#ifdef DEBUG
  // A bit hacky but the VersionChangeTransactionOp is not generated in response
  // to a child request like most other database operations. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

nsresult CreateObjectStoreOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("CreateObjectStoreOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not creating an object store with the same name as
    // another that already exists. This should be impossible because we should
    // have thrown an error long before now...
    DatabaseConnection::CachedStatement stmt;
    MOZ_ALWAYS_SUCCEEDS(aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT name "
                           "FROM object_store "
                           "WHERE name = :name;"),
        &stmt));

    MOZ_ALWAYS_SUCCEEDS(
        stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mMetadata.name()));

    bool hasResult;
    MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));
    MOZ_ASSERT(!hasResult);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DatabaseConnection::CachedStatement stmt;
  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING(
          "INSERT INTO object_store (id, auto_increment, name, key_path) "
          "VALUES (:id, :auto_increment, :name, :key_path);"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mMetadata.autoIncrement() ? 1 : 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mMetadata.name());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_NAMED_LITERAL_CSTRING(keyPath, "key_path");

  if (mMetadata.keyPath().IsValid()) {
    nsAutoString keyPathSerialization;
    mMetadata.keyPath().SerializeToString(keyPathSerialization);

    rv = stmt->BindStringByName(keyPath, keyPathSerialization);
  } else {
    rv = stmt->BindNullByName(keyPath);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  {
    int64_t id;
    MOZ_ALWAYS_SUCCEEDS(
        aConnection->GetStorageConnection()->GetLastInsertRowID(&id));
    MOZ_ASSERT(mMetadata.id() == id);
  }
#endif

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DeleteObjectStoreOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DeleteObjectStoreOp::DoDatabaseWork", DOM);

  NS_NAMED_LITERAL_CSTRING(objectStoreIdString, "object_store_id");

#ifdef DEBUG
  {
    // Make sure |mIsLastObjectStore| is telling the truth.
    DatabaseConnection::CachedStatement stmt;
    MOZ_ALWAYS_SUCCEEDS(aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT id "
                           "FROM object_store;"),
        &stmt));

    bool foundThisObjectStore = false;
    bool foundOtherObjectStore = false;

    while (true) {
      bool hasResult;
      MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));

      if (!hasResult) {
        break;
      }

      int64_t id;
      MOZ_ALWAYS_SUCCEEDS(stmt->GetInt64(0, &id));

      if (id == mMetadata->mCommonMetadata.id()) {
        foundThisObjectStore = true;
      } else {
        foundOtherObjectStore = true;
      }
    }

    MOZ_ASSERT_IF(mIsLastObjectStore,
                  foundThisObjectStore && !foundOtherObjectStore);
    MOZ_ASSERT_IF(!mIsLastObjectStore,
                  foundThisObjectStore && foundOtherObjectStore);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mIsLastObjectStore) {
    // We can just delete everything if this is the last object store.
    DatabaseConnection::CachedStatement stmt;
    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM index_data;"), &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM unique_index_data;"), &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM object_data;"), &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM object_store_index;"), &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM object_store;"), &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    bool hasIndexes;
    rv = ObjectStoreHasIndexes(aConnection, mMetadata->mCommonMetadata.id(),
                               &hasIndexes);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (hasIndexes) {
      rv = DeleteObjectStoreDataTableRowsWithIndexes(
          aConnection, mMetadata->mCommonMetadata.id(), Nothing());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Now clean up the object store index table.
      DatabaseConnection::CachedStatement stmt;
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("DELETE FROM object_store_index "
                             "WHERE object_store_id = :object_store_id;"),
          &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindInt64ByName(objectStoreIdString,
                                 mMetadata->mCommonMetadata.id());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // We only have to worry about object data if this object store has no
      // indexes.
      DatabaseConnection::CachedStatement stmt;
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("DELETE FROM object_data "
                             "WHERE object_store_id = :object_store_id;"),
          &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindInt64ByName(objectStoreIdString,
                                 mMetadata->mCommonMetadata.id());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    DatabaseConnection::CachedStatement stmt;
    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM object_store "
                           "WHERE id = :object_store_id;"),
        &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(objectStoreIdString,
                               mMetadata->mCommonMetadata.id());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

#ifdef DEBUG
    {
      int32_t deletedRowCount;
      MOZ_ALWAYS_SUCCEEDS(aConnection->GetStorageConnection()->GetAffectedRows(
          &deletedRowCount));
      MOZ_ASSERT(deletedRowCount == 1);
    }
#endif
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mMetadata->mCommonMetadata.autoIncrement()) {
    Transaction()->ForgetModifiedAutoIncrementObjectStore(mMetadata);
  }

  return NS_OK;
}

nsresult RenameObjectStoreOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("RenameObjectStoreOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not renaming an object store with the same name as
    // another that already exists. This should be impossible because we should
    // have thrown an error long before now...
    DatabaseConnection::CachedStatement stmt;
    MOZ_ALWAYS_SUCCEEDS(
        aConnection->GetCachedStatement(NS_LITERAL_CSTRING("SELECT name "
                                                           "FROM object_store "
                                                           "WHERE name = :name "
                                                           "AND id != :id;"),
                                        &stmt));

    MOZ_ALWAYS_SUCCEEDS(
        stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mNewName));

    MOZ_ALWAYS_SUCCEEDS(stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mId));

    bool hasResult;
    MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));
    MOZ_ASSERT(!hasResult);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DatabaseConnection::CachedStatement stmt;
  rv = aConnection->GetCachedStatement(NS_LITERAL_CSTRING("UPDATE object_store "
                                                          "SET name = :name "
                                                          "WHERE id = :id;"),
                                       &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mNewName);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mId);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

CreateIndexOp::CreateIndexOp(VersionChangeTransaction* aTransaction,
                             const int64_t aObjectStoreId,
                             const IndexMetadata& aMetadata)
    : VersionChangeTransactionOp(aTransaction),
      mMetadata(aMetadata),
      mFileManager(aTransaction->GetDatabase()->GetFileManager()),
      mDatabaseId(aTransaction->DatabaseId()),
      mObjectStoreId(aObjectStoreId) {
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aMetadata.id());
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT(!mDatabaseId.IsEmpty());
}

nsresult CreateIndexOp::InsertDataFromObjectStore(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mMaybeUniqueIndexTable);

  AUTO_PROFILER_LABEL("CreateIndexOp::InsertDataFromObjectStore", DOM);

  nsCOMPtr<mozIStorageConnection> storageConnection =
      aConnection->GetStorageConnection();
  MOZ_ASSERT(storageConnection);

  RefPtr<UpdateIndexDataValuesFunction> updateFunction =
      new UpdateIndexDataValuesFunction(this, aConnection);

  NS_NAMED_LITERAL_CSTRING(updateFunctionName, "update_index_data_values");

  nsresult rv =
      storageConnection->CreateFunction(updateFunctionName, 4, updateFunction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InsertDataFromObjectStoreInternal(aConnection);

  MOZ_ALWAYS_SUCCEEDS(storageConnection->RemoveFunction(updateFunctionName));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult CreateIndexOp::InsertDataFromObjectStoreInternal(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mMaybeUniqueIndexTable);

  DebugOnly<void*> storageConnection = aConnection->GetStorageConnection();
  MOZ_ASSERT(storageConnection);

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("UPDATE object_data "
                         "SET index_data_values = update_index_data_values "
                         "(key, index_data_values, file_ids, data) "
                         "WHERE object_store_id = :object_store_id;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("object_store_id"),
                             mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool CreateIndexOp::Init(TransactionBase* aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);

  nsresult rv = GetUniqueIndexTableForObjectStore(aTransaction, mObjectStoreId,
                                                  mMaybeUniqueIndexTable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

nsresult CreateIndexOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("CreateIndexOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not creating an index with the same name and object
    // store as another that already exists. This should be impossible because
    // we should have thrown an error long before now...
    DatabaseConnection::CachedStatement stmt;
    MOZ_ALWAYS_SUCCEEDS(aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT name "
                           "FROM object_store_index "
                           "WHERE object_store_id = :osid "
                           "AND name = :name;"),
        &stmt));
    MOZ_ALWAYS_SUCCEEDS(
        stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStoreId));
    MOZ_ALWAYS_SUCCEEDS(
        stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mMetadata.name()));

    bool hasResult;
    MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));

    MOZ_ASSERT(!hasResult);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DatabaseConnection::CachedStatement stmt;
  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING(
          "INSERT INTO object_store_index (id, name, key_path, unique_index, "
          "multientry, object_store_id, locale, "
          "is_auto_locale) "
          "VALUES (:id, :name, :key_path, :unique, :multientry, :osid, "
          ":locale, "
          ":is_auto_locale)"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mMetadata.name());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString keyPathSerialization;
  mMetadata.keyPath().SerializeToString(keyPathSerialization);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"),
                              keyPathSerialization);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("unique"),
                             mMetadata.unique() ? 1 : 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("multientry"),
                             mMetadata.multiEntry() ? 1 : 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mMetadata.locale().IsEmpty()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("locale"));
  } else {
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("locale"),
                                    mMetadata.locale());
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("is_auto_locale"),
                             mMetadata.autoLocale());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  {
    int64_t id;
    MOZ_ALWAYS_SUCCEEDS(
        aConnection->GetStorageConnection()->GetLastInsertRowID(&id));
    MOZ_ASSERT(mMetadata.id() == id);
  }
#endif

  rv = InsertDataFromObjectStore(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(CreateIndexOp::UpdateIndexDataValuesFunction,
                  mozIStorageFunction);

NS_IMETHODIMP
CreateIndexOp::UpdateIndexDataValuesFunction::OnFunctionCall(
    mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mOp);

  AUTO_PROFILER_LABEL(
      "CreateIndexOp::UpdateIndexDataValuesFunction::OnFunctionCall", DOM);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 4);  // key, index_data_values, file_ids, data

    int32_t valueType;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(0, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(1, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(2, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_TEXT);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(3, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB ||
               valueType == mozIStorageValueArray::VALUE_TYPE_INTEGER);
  }
#endif

  StructuredCloneReadInfo cloneInfo(JS::StructuredCloneScope::DifferentProcess);
  nsresult rv = GetStructuredCloneReadInfoFromValueArray(
      aValues,
      /* aDataIndex */ 3,
      /* aFileIdsIndex */ 2, mOp->mFileManager, &cloneInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const IndexMetadata& metadata = mOp->mMetadata;
  const int64_t& objectStoreId = mOp->mObjectStoreId;

  AutoTArray<IndexUpdateInfo, 32> updateInfos;
  rv = IDBObjectStore::DeserializeIndexValueToUpdateInfos(
      metadata.id(), metadata.keyPath(), metadata.unique(),
      metadata.multiEntry(), metadata.locale(), cloneInfo, updateInfos);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (updateInfos.IsEmpty()) {
    // XXX See if we can do this without copying...

    nsCOMPtr<nsIVariant> unmodifiedValue;

    // No changes needed, just return the original value.
    int32_t valueType;
    rv = aValues->GetTypeOfIndex(1, &valueType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    if (valueType == mozIStorageValueArray::VALUE_TYPE_NULL) {
      unmodifiedValue = new storage::NullVariant();
      unmodifiedValue.forget(_retval);
      return NS_OK;
    }

    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    const uint8_t* blobData;
    uint32_t blobDataLength;
    rv = aValues->GetSharedBlob(1, &blobDataLength, &blobData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    std::pair<uint8_t*, int> copiedBlobDataPair(
        static_cast<uint8_t*>(malloc(blobDataLength)), blobDataLength);

    if (!copiedBlobDataPair.first) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_OUT_OF_MEMORY;
    }

    memcpy(copiedBlobDataPair.first, blobData, blobDataLength);

    unmodifiedValue = new storage::AdoptedBlobVariant(copiedBlobDataPair);
    unmodifiedValue.forget(_retval);

    return NS_OK;
  }

  Key key;
  rv = key.SetFromValueArray(aValues, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoTArray<IndexDataValue, 32> indexValues;
  rv = ReadCompressedIndexDataValues(aValues, 1, indexValues);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const bool hadPreviousIndexValues = !indexValues.IsEmpty();

  const uint32_t updateInfoCount = updateInfos.Length();

  if (NS_WARN_IF(!indexValues.SetCapacity(
          indexValues.Length() + updateInfoCount, fallible))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // First construct the full list to update the index_data_values row.
  for (uint32_t index = 0; index < updateInfoCount; index++) {
    const IndexUpdateInfo& info = updateInfos[index];

    MOZ_ALWAYS_TRUE(indexValues.InsertElementSorted(
        IndexDataValue(metadata.id(), metadata.unique(), info.value(),
                       info.localizedValue()),
        fallible));
  }

  UniqueFreePtr<uint8_t> indexValuesBlob;
  uint32_t indexValuesBlobLength;
  rv = MakeCompressedIndexDataValues(indexValues, indexValuesBlob,
                                     &indexValuesBlobLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!indexValuesBlobLength == !(indexValuesBlob.get()));

  nsCOMPtr<nsIVariant> value;

  if (!indexValuesBlob) {
    value = new storage::NullVariant();

    value.forget(_retval);
    return NS_OK;
  }

  // Now insert the new table rows. We only need to construct a new list if
  // the full list is different.
  if (hadPreviousIndexValues) {
    indexValues.ClearAndRetainStorage();

    MOZ_ASSERT(indexValues.Capacity() >= updateInfoCount);

    for (uint32_t index = 0; index < updateInfoCount; index++) {
      const IndexUpdateInfo& info = updateInfos[index];

      MOZ_ALWAYS_TRUE(indexValues.InsertElementSorted(
          IndexDataValue(metadata.id(), metadata.unique(), info.value(),
                         info.localizedValue()),
          fallible));
    }
  }

  rv = InsertIndexTableRows(mConnection, objectStoreId, key, indexValues);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  std::pair<uint8_t*, int> copiedBlobDataPair(indexValuesBlob.release(),
                                              indexValuesBlobLength);

  value = new storage::AdoptedBlobVariant(copiedBlobDataPair);

  value.forget(_retval);
  return NS_OK;
}

DeleteIndexOp::DeleteIndexOp(VersionChangeTransaction* aTransaction,
                             const int64_t aObjectStoreId,
                             const int64_t aIndexId, const bool aUnique,
                             const bool aIsLastIndex)
    : VersionChangeTransactionOp(aTransaction),
      mObjectStoreId(aObjectStoreId),
      mIndexId(aIndexId),
      mUnique(aUnique),
      mIsLastIndex(aIsLastIndex) {
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aIndexId);
}

nsresult DeleteIndexOp::RemoveReferencesToIndex(
    DatabaseConnection* aConnection, const Key& aObjectStoreKey,
    nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());
  MOZ_ASSERT_IF(!mIsLastIndex, !aIndexValues.IsEmpty());

  struct MOZ_STACK_CLASS IndexIdComparator final {
    bool Equals(const IndexDataValue& aA, const IndexDataValue& aB) const {
      // Ignore everything but the index id.
      return aA.mIndexId == aB.mIndexId;
    };

    bool LessThan(const IndexDataValue& aA, const IndexDataValue& aB) const {
      return aA.mIndexId < aB.mIndexId;
    };
  };

  AUTO_PROFILER_LABEL("DeleteIndexOp::RemoveReferencesToIndex", DOM);

  if (mIsLastIndex) {
    // There is no need to parse the previous entry in the index_data_values
    // column if this is the last index. Simply set it to NULL.
    DatabaseConnection::CachedStatement stmt;
    nsresult rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("UPDATE object_data "
                           "SET index_data_values = NULL "
                           "WHERE object_store_id = :object_store_id "
                           "AND key = :key;"),
        &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("object_store_id"),
                               mObjectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aObjectStoreKey.BindToStatement(stmt, NS_LITERAL_CSTRING("key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  IndexDataValue search;
  search.mIndexId = mIndexId;

  // This returns the first element that matches our index id found during a
  // binary search. However, there could still be other elements before that.
  size_t firstElementIndex =
      aIndexValues.BinaryIndexOf(search, IndexIdComparator());
  if (NS_WARN_IF(firstElementIndex == aIndexValues.NoIndex) ||
      NS_WARN_IF(aIndexValues[firstElementIndex].mIndexId != mIndexId)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  MOZ_ASSERT(aIndexValues[firstElementIndex].mIndexId == mIndexId);

  // Walk backwards to find the real first index.
  while (firstElementIndex) {
    if (aIndexValues[firstElementIndex - 1].mIndexId == mIndexId) {
      firstElementIndex--;
    } else {
      break;
    }
  }

  MOZ_ASSERT(aIndexValues[firstElementIndex].mIndexId == mIndexId);

  const size_t indexValuesLength = aIndexValues.Length();

  // Find the last element with the same index id.
  size_t lastElementIndex = firstElementIndex;

  while (lastElementIndex < indexValuesLength) {
    if (aIndexValues[lastElementIndex].mIndexId == mIndexId) {
      lastElementIndex++;
    } else {
      break;
    }
  }

  MOZ_ASSERT(lastElementIndex > firstElementIndex);
  MOZ_ASSERT_IF(lastElementIndex < indexValuesLength,
                aIndexValues[lastElementIndex].mIndexId != mIndexId);
  MOZ_ASSERT(aIndexValues[lastElementIndex - 1].mIndexId == mIndexId);

  aIndexValues.RemoveElementsAt(firstElementIndex,
                                lastElementIndex - firstElementIndex);

  nsresult rv = UpdateIndexValues(aConnection, mObjectStoreId, aObjectStoreKey,
                                  aIndexValues);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DeleteIndexOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

#ifdef DEBUG
  {
    // Make sure |mIsLastIndex| is telling the truth.
    DatabaseConnection::CachedStatement stmt;
    MOZ_ALWAYS_SUCCEEDS(aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT id "
                           "FROM object_store_index "
                           "WHERE object_store_id = :object_store_id;"),
        &stmt));

    MOZ_ALWAYS_SUCCEEDS(stmt->BindInt64ByName(
        NS_LITERAL_CSTRING("object_store_id"), mObjectStoreId));

    bool foundThisIndex = false;
    bool foundOtherIndex = false;

    while (true) {
      bool hasResult;
      MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));

      if (!hasResult) {
        break;
      }

      int64_t id;
      MOZ_ALWAYS_SUCCEEDS(stmt->GetInt64(0, &id));

      if (id == mIndexId) {
        foundThisIndex = true;
      } else {
        foundOtherIndex = true;
      }
    }

    MOZ_ASSERT_IF(mIsLastIndex, foundThisIndex && !foundOtherIndex);
    MOZ_ASSERT_IF(!mIsLastIndex, foundThisIndex && foundOtherIndex);
  }
#endif

  AUTO_PROFILER_LABEL("DeleteIndexOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DatabaseConnection::CachedStatement selectStmt;

  // mozStorage warns that these statements trigger a sort operation but we
  // don't care because this is a very rare call and we expect it to be slow.
  // The cost of having an index on this field is too high.
  if (mUnique) {
    if (mIsLastIndex) {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("/* do not warn (bug someone else) */ "
                             "SELECT value, object_data_key "
                             "FROM unique_index_data "
                             "WHERE index_id = :index_id "
                             "ORDER BY object_data_key ASC;"),
          &selectStmt);
    } else {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING(
              "/* do not warn (bug out) */ "
              "SELECT unique_index_data.value, "
              "unique_index_data.object_data_key, "
              "object_data.index_data_values "
              "FROM unique_index_data "
              "JOIN object_data "
              "ON unique_index_data.object_data_key = object_data.key "
              "WHERE unique_index_data.index_id = :index_id "
              "AND object_data.object_store_id = :object_store_id "
              "ORDER BY unique_index_data.object_data_key ASC;"),
          &selectStmt);
    }
  } else {
    if (mIsLastIndex) {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING("/* do not warn (bug me not) */ "
                             "SELECT value, object_data_key "
                             "FROM index_data "
                             "WHERE index_id = :index_id "
                             "AND object_store_id = :object_store_id "
                             "ORDER BY object_data_key ASC;"),
          &selectStmt);
    } else {
      rv = aConnection->GetCachedStatement(
          NS_LITERAL_CSTRING(
              "/* do not warn (bug off) */ "
              "SELECT index_data.value, "
              "index_data.object_data_key, "
              "object_data.index_data_values "
              "FROM index_data "
              "JOIN object_data "
              "ON index_data.object_data_key = object_data.key "
              "WHERE index_data.index_id = :index_id "
              "AND object_data.object_store_id = :object_store_id "
              "ORDER BY index_data.object_data_key ASC;"),
          &selectStmt);
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_NAMED_LITERAL_CSTRING(indexIdString, "index_id");

  rv = selectStmt->BindInt64ByName(indexIdString, mIndexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!mUnique || !mIsLastIndex) {
    rv = selectStmt->BindInt64ByName(NS_LITERAL_CSTRING("object_store_id"),
                                     mObjectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  NS_NAMED_LITERAL_CSTRING(valueString, "value");
  NS_NAMED_LITERAL_CSTRING(objectDataKeyString, "object_data_key");

  DatabaseConnection::CachedStatement deleteIndexRowStmt;
  DatabaseConnection::CachedStatement nullIndexDataValuesStmt;

  Key lastObjectStoreKey;
  AutoTArray<IndexDataValue, 32> lastIndexValues;

  bool hasResult;
  while (NS_SUCCEEDED(rv = selectStmt->ExecuteStep(&hasResult)) && hasResult) {
    // We always need the index key to delete the index row.
    Key indexKey;
    rv = indexKey.SetFromStatement(selectStmt, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(indexKey.IsUnset())) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    // Don't call |lastObjectStoreKey.BindToStatement()| directly because we
    // don't want to copy the same key multiple times.
    const uint8_t* objectStoreKeyData;
    uint32_t objectStoreKeyDataLength;
    rv = selectStmt->GetSharedBlob(1, &objectStoreKeyDataLength,
                                   &objectStoreKeyData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!objectStoreKeyDataLength)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    nsDependentCString currentObjectStoreKeyBuffer(
        reinterpret_cast<const char*>(objectStoreKeyData),
        objectStoreKeyDataLength);
    if (currentObjectStoreKeyBuffer != lastObjectStoreKey.GetBuffer()) {
      // We just walked to the next object store key.
      if (!lastObjectStoreKey.IsUnset()) {
        // Before we move on to the next key we need to update the previous
        // key's index_data_values column.
        rv = RemoveReferencesToIndex(aConnection, lastObjectStoreKey,
                                     lastIndexValues);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // Save the object store key.
      lastObjectStoreKey = Key(currentObjectStoreKeyBuffer);

      // And the |index_data_values| row if this isn't the only index.
      if (!mIsLastIndex) {
        lastIndexValues.ClearAndRetainStorage();
        rv = ReadCompressedIndexDataValues(selectStmt, 2, lastIndexValues);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (NS_WARN_IF(lastIndexValues.IsEmpty())) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_FILE_CORRUPTED;
        }
      }
    }

    // Now delete the index row.
    if (deleteIndexRowStmt) {
      MOZ_ALWAYS_SUCCEEDS(deleteIndexRowStmt->Reset());
    } else {
      if (mUnique) {
        rv = aConnection->GetCachedStatement(
            NS_LITERAL_CSTRING("DELETE FROM unique_index_data "
                               "WHERE index_id = :index_id "
                               "AND value = :value;"),
            &deleteIndexRowStmt);
      } else {
        rv = aConnection->GetCachedStatement(
            NS_LITERAL_CSTRING("DELETE FROM index_data "
                               "WHERE index_id = :index_id "
                               "AND value = :value "
                               "AND object_data_key = :object_data_key;"),
            &deleteIndexRowStmt);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = deleteIndexRowStmt->BindInt64ByName(indexIdString, mIndexId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = indexKey.BindToStatement(deleteIndexRowStmt, valueString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mUnique) {
      rv = lastObjectStoreKey.BindToStatement(deleteIndexRowStmt,
                                              objectDataKeyString);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = deleteIndexRowStmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Take care of the last key.
  if (!lastObjectStoreKey.IsUnset()) {
    MOZ_ASSERT_IF(!mIsLastIndex, !lastIndexValues.IsEmpty());

    rv = RemoveReferencesToIndex(aConnection, lastObjectStoreKey,
                                 lastIndexValues);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  DatabaseConnection::CachedStatement deleteStmt;
  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM object_store_index "
                         "WHERE id = :index_id;"),
      &deleteStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = deleteStmt->BindInt64ByName(indexIdString, mIndexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = deleteStmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  {
    int32_t deletedRowCount;
    MOZ_ALWAYS_SUCCEEDS(
        aConnection->GetStorageConnection()->GetAffectedRows(&deletedRowCount));
    MOZ_ASSERT(deletedRowCount == 1);
  }
#endif

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult RenameIndexOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("RenameIndexOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not renaming an index with the same name as another
    // that already exists. This should be impossible because we should have
    // thrown an error long before now...
    DatabaseConnection::CachedStatement stmt;
    MOZ_ALWAYS_SUCCEEDS(aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT name "
                           "FROM object_store_index "
                           "WHERE object_store_id = :object_store_id "
                           "AND name = :name "
                           "AND id != :id;"),
        &stmt));

    MOZ_ALWAYS_SUCCEEDS(stmt->BindInt64ByName(
        NS_LITERAL_CSTRING("object_store_id"), mObjectStoreId));

    MOZ_ALWAYS_SUCCEEDS(
        stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mNewName));

    MOZ_ALWAYS_SUCCEEDS(
        stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mIndexId));

    bool hasResult;
    MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));
    MOZ_ASSERT(!hasResult);
  }
#else
  Unused << mObjectStoreId;
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DatabaseConnection::CachedStatement stmt;
  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("UPDATE object_store_index "
                         "SET name = :name "
                         "WHERE id = :id;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mNewName);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mIndexId);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
nsresult NormalTransactionOp::ObjectStoreHasIndexes(
    NormalTransactionOp* aOp, DatabaseConnection* aConnection,
    const int64_t aObjectStoreId, const bool aMayHaveIndexes,
    bool* aHasIndexes) {
  MOZ_ASSERT(aOp);
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aHasIndexes);

  bool hasIndexes;
  if (aOp->Transaction()->GetMode() == IDBTransaction::VERSION_CHANGE &&
      aMayHaveIndexes) {
    // If this is a version change transaction then mObjectStoreMayHaveIndexes
    // could be wrong (e.g. if a unique index failed to be created due to a
    // constraint error). We have to check on this thread by asking the database
    // directly.
    nsresult rv = DatabaseOperationBase::ObjectStoreHasIndexes(
        aConnection, aObjectStoreId, &hasIndexes);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    MOZ_ASSERT(NS_SUCCEEDED(DatabaseOperationBase::ObjectStoreHasIndexes(
        aConnection, aObjectStoreId, &hasIndexes)));
    MOZ_ASSERT(aMayHaveIndexes == hasIndexes);

    hasIndexes = aMayHaveIndexes;
  }

  *aHasIndexes = hasIndexes;
  return NS_OK;
}

nsresult NormalTransactionOp::GetPreprocessParams(PreprocessParams& aParams) {
  return NS_OK;
}

nsresult NormalTransactionOp::SendPreprocessInfo() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!IsActorDestroyed());

  PreprocessParams params;
  nsresult rv = GetPreprocessParams(params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(params.type() != PreprocessParams::T__None);

  if (NS_WARN_IF(!PBackgroundIDBRequestParent::SendPreprocess(params))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

nsresult NormalTransactionOp::SendSuccessResult() {
  AssertIsOnOwningThread();

  if (!IsActorDestroyed()) {
    static const size_t kMaxIDBMsgOverhead = 1024 * 1024 * 10;  // 10MB
    const uint32_t maximalSizeFromPref =
        IndexedDatabaseManager::MaxSerializedMsgSize();
    MOZ_ASSERT(maximalSizeFromPref > kMaxIDBMsgOverhead);
    const size_t kMaxMessageSize = maximalSizeFromPref - kMaxIDBMsgOverhead;

    RequestResponse response;
    size_t responseSize = kMaxMessageSize;
    GetResponse(response, &responseSize);

    if (responseSize >= kMaxMessageSize) {
      nsPrintfCString warning(
          "The serialized value is too large"
          " (size=%zu bytes, max=%zu bytes).",
          responseSize, kMaxMessageSize);
      NS_WARNING(warning.get());
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    MOZ_ASSERT(response.type() != RequestResponse::T__None);

    if (response.type() == RequestResponse::Tnsresult) {
      MOZ_ASSERT(NS_FAILED(response.get_nsresult()));

      return response.get_nsresult();
    }

    if (NS_WARN_IF(
            !PBackgroundIDBRequestParent::Send__delete__(this, response))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

#ifdef DEBUG
  mResponseSent = true;
#endif

  return NS_OK;
}

bool NormalTransactionOp::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));

  bool result = false;

  if (!IsActorDestroyed()) {
    result = PBackgroundIDBRequestParent::Send__delete__(
        this, ClampResultCode(aResultCode));
  }

#ifdef DEBUG
  mResponseSent = true;
#endif

  return result;
}

void NormalTransactionOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(!IsActorDestroyed(), mResponseSent);

  TransactionDatabaseOperationBase::Cleanup();
}

void NormalTransactionOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult NormalTransactionOp::RecvContinue(
    const PreprocessResponse& aResponse) {
  AssertIsOnOwningThread();

  switch (aResponse.type()) {
    case PreprocessResponse::Tnsresult:
      mResultCode = aResponse.get_nsresult();
      break;

    case PreprocessResponse::TObjectStoreGetPreprocessResponse:
      break;

    case PreprocessResponse::TObjectStoreGetAllPreprocessResponse:
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  NoteContinueReceived();

  return IPC_OK();
}

ObjectStoreAddOrPutRequestOp::ObjectStoreAddOrPutRequestOp(
    TransactionBase* aTransaction, const RequestParams& aParams)
    : NormalTransactionOp(aTransaction),
      mParams(aParams.type() == RequestParams::TObjectStoreAddParams
                  ? aParams.get_ObjectStoreAddParams().commonParams()
                  : aParams.get_ObjectStorePutParams().commonParams()),
      mGroup(aTransaction->GetDatabase()->Group()),
      mOrigin(aTransaction->GetDatabase()->Origin()),
      mPersistenceType(aTransaction->GetDatabase()->Type()),
      mOverwrite(aParams.type() == RequestParams::TObjectStorePutParams),
      mObjectStoreMayHaveIndexes(false) {
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreAddParams ||
             aParams.type() == RequestParams::TObjectStorePutParams);

  mMetadata =
      aTransaction->GetMetadataForObjectStoreId(mParams.objectStoreId());
  MOZ_ASSERT(mMetadata);

  mObjectStoreMayHaveIndexes = mMetadata->HasLiveIndexes();

  mDataOverThreshold =
      snappy::MaxCompressedLength(mParams.cloneInfo().data().data.Size()) >
      IndexedDatabaseManager::DataThreshold();
}

nsresult ObjectStoreAddOrPutRequestOp::RemoveOldIndexDataValues(
    DatabaseConnection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(mOverwrite);
  MOZ_ASSERT(!mResponse.IsUnset());

#ifdef DEBUG
  {
    bool hasIndexes = false;
    MOZ_ASSERT(NS_SUCCEEDED(DatabaseOperationBase::ObjectStoreHasIndexes(
        aConnection, mParams.objectStoreId(), &hasIndexes)));
    MOZ_ASSERT(hasIndexes,
               "Don't use this slow method if there are no indexes!");
  }
#endif

  DatabaseConnection::CachedStatement indexValuesStmt;
  nsresult rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT index_data_values "
                         "FROM object_data "
                         "WHERE object_store_id = :object_store_id "
                         "AND key = :key;"),
      &indexValuesStmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = indexValuesStmt->BindInt64ByName(NS_LITERAL_CSTRING("object_store_id"),
                                        mParams.objectStoreId());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mResponse.BindToStatement(indexValuesStmt, NS_LITERAL_CSTRING("key"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = indexValuesStmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasResult) {
    AutoTArray<IndexDataValue, 32> existingIndexValues;
    rv = ReadCompressedIndexDataValues(indexValuesStmt, 0, existingIndexValues);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = DeleteIndexDataTableRows(aConnection, mResponse, existingIndexValues);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

bool ObjectStoreAddOrPutRequestOp::Init(TransactionBase* aTransaction) {
  AssertIsOnOwningThread();

  const nsTArray<IndexUpdateInfo>& indexUpdateInfos =
      mParams.indexUpdateInfos();

  if (!indexUpdateInfos.IsEmpty()) {
    const uint32_t count = indexUpdateInfos.Length();

    mUniqueIndexTable.emplace();

    for (uint32_t index = 0; index < count; index++) {
      const IndexUpdateInfo& updateInfo = indexUpdateInfos[index];

      RefPtr<FullIndexMetadata> indexMetadata;
      MOZ_ALWAYS_TRUE(mMetadata->mIndexes.Get(updateInfo.indexId(),
                                              getter_AddRefs(indexMetadata)));

      MOZ_ASSERT(!indexMetadata->mDeleted);

      const int64_t& indexId = indexMetadata->mCommonMetadata.id();
      const bool& unique = indexMetadata->mCommonMetadata.unique();

      MOZ_ASSERT(indexId == updateInfo.indexId());
      MOZ_ASSERT_IF(!indexMetadata->mCommonMetadata.multiEntry(),
                    !mUniqueIndexTable.ref().Get(indexId));

      if (NS_WARN_IF(!mUniqueIndexTable.ref().Put(indexId, unique, fallible))) {
        return false;
      }
    }
  } else if (mOverwrite) {
    mUniqueIndexTable.emplace();
  }

#ifdef DEBUG
  if (mUniqueIndexTable.isSome()) {
    mUniqueIndexTable.ref().MarkImmutable();
  }
#endif

  const nsTArray<FileAddInfo>& fileAddInfos = mParams.fileAddInfos();

  if (!fileAddInfos.IsEmpty()) {
    const uint32_t count = fileAddInfos.Length();

    if (NS_WARN_IF(!mStoredFileInfos.SetCapacity(count, fallible))) {
      return false;
    }

    for (uint32_t index = 0; index < count; index++) {
      const FileAddInfo& fileAddInfo = fileAddInfos[index];

      MOZ_ASSERT(fileAddInfo.type() == StructuredCloneFile::eBlob ||
                 fileAddInfo.type() == StructuredCloneFile::eMutableFile ||
                 fileAddInfo.type() == StructuredCloneFile::eWasmBytecode ||
                 fileAddInfo.type() == StructuredCloneFile::eWasmCompiled);

      const DatabaseOrMutableFile& file = fileAddInfo.file();

      StoredFileInfo* storedFileInfo = mStoredFileInfos.AppendElement(fallible);
      MOZ_ASSERT(storedFileInfo);

      switch (fileAddInfo.type()) {
        case StructuredCloneFile::eBlob: {
          MOZ_ASSERT(file.type() ==
                     DatabaseOrMutableFile::TPBackgroundIDBDatabaseFileParent);

          storedFileInfo->mFileActor = static_cast<DatabaseFile*>(
              file.get_PBackgroundIDBDatabaseFileParent());
          MOZ_ASSERT(storedFileInfo->mFileActor);

          storedFileInfo->mFileInfo = storedFileInfo->mFileActor->GetFileInfo();
          MOZ_ASSERT(storedFileInfo->mFileInfo);

          storedFileInfo->mType = StructuredCloneFile::eBlob;
          break;
        }

        case StructuredCloneFile::eMutableFile: {
          MOZ_ASSERT(file.type() ==
                     DatabaseOrMutableFile::TPBackgroundMutableFileParent);

          auto mutableFileActor = static_cast<MutableFile*>(
              file.get_PBackgroundMutableFileParent());
          MOZ_ASSERT(mutableFileActor);

          storedFileInfo->mFileInfo = mutableFileActor->GetFileInfo();
          MOZ_ASSERT(storedFileInfo->mFileInfo);

          storedFileInfo->mType = StructuredCloneFile::eMutableFile;
          break;
        }

        case StructuredCloneFile::eWasmBytecode:
        case StructuredCloneFile::eWasmCompiled: {
          MOZ_ASSERT(file.type() ==
                     DatabaseOrMutableFile::TPBackgroundIDBDatabaseFileParent);

          storedFileInfo->mFileActor = static_cast<DatabaseFile*>(
              file.get_PBackgroundIDBDatabaseFileParent());
          MOZ_ASSERT(storedFileInfo->mFileActor);

          storedFileInfo->mFileInfo = storedFileInfo->mFileActor->GetFileInfo();
          MOZ_ASSERT(storedFileInfo->mFileInfo);

          storedFileInfo->mType = fileAddInfo.type();
          break;
        }

        default:
          MOZ_CRASH("Should never get here!");
      }
    }
  }

  if (mDataOverThreshold) {
    StoredFileInfo* storedFileInfo = mStoredFileInfos.AppendElement(fallible);
    MOZ_ASSERT(storedFileInfo);

    RefPtr<FileManager> fileManager =
        aTransaction->GetDatabase()->GetFileManager();
    MOZ_ASSERT(fileManager);

    storedFileInfo->mFileInfo = fileManager->GetNewFileInfo();

    storedFileInfo->mInputStream =
        new SCInputStream(mParams.cloneInfo().data().data);

    storedFileInfo->mType = StructuredCloneFile::eStructuredClone;
  }

  return true;
}

nsresult ObjectStoreAddOrPutRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection->GetStorageConnection());

  AUTO_PROFILER_LABEL("ObjectStoreAddOrPutRequestOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool objectStoreHasIndexes;
  rv =
      ObjectStoreHasIndexes(this, aConnection, mParams.objectStoreId(),
                            mObjectStoreMayHaveIndexes, &objectStoreHasIndexes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This will be the final key we use.
  Key& key = mResponse;
  key = mParams.key();

  const bool keyUnset = key.IsUnset();
  const int64_t osid = mParams.objectStoreId();

  // First delete old index_data_values if we're overwriting something and we
  // have indexes.
  if (mOverwrite && !keyUnset && objectStoreHasIndexes) {
    rv = RemoveOldIndexDataValues(aConnection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // The "|| keyUnset" here is mostly a debugging tool. If a key isn't
  // specified we should never have a collision and so it shouldn't matter
  // if we allow overwrite or not. By not allowing overwrite we raise
  // detectable errors rather than corrupting data.
  DatabaseConnection::CachedStatement stmt;
  if (!mOverwrite || keyUnset) {
    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO object_data "
                           "(object_store_id, key, file_ids, data) "
                           "VALUES (:osid, :key, :file_ids, :data);"),
        &stmt);
  } else {
    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT OR REPLACE INTO object_data "
                           "(object_store_id, key, file_ids, data) "
                           "VALUES (:osid, :key, :file_ids, :data);"),
        &stmt);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), osid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const SerializedStructuredCloneWriteInfo& cloneInfo = mParams.cloneInfo();
  const JSStructuredCloneData& cloneData = cloneInfo.data().data;
  size_t cloneDataSize = cloneData.Size();

  MOZ_ASSERT(!keyUnset || mMetadata->mCommonMetadata.autoIncrement(),
             "Should have key unless autoIncrement");

  int64_t autoIncrementNum = 0;

  if (mMetadata->mCommonMetadata.autoIncrement()) {
    if (keyUnset) {
      autoIncrementNum = mMetadata->mNextAutoIncrementId;

      MOZ_ASSERT(autoIncrementNum > 0);

      if (autoIncrementNum > (1LL << 53)) {
        return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
      }

      key.SetFromInteger(autoIncrementNum);
    } else if (key.IsFloat()) {
      double numericKey = key.ToFloat();
      numericKey = std::min(numericKey, double(1LL << 53));
      numericKey = floor(numericKey);
      if (numericKey >= mMetadata->mNextAutoIncrementId) {
        autoIncrementNum = numericKey;
      }
    }

    if (keyUnset && mMetadata->mCommonMetadata.keyPath().IsValid()) {
      const SerializedStructuredCloneWriteInfo& cloneInfo = mParams.cloneInfo();
      MOZ_ASSERT(cloneInfo.offsetToKeyProp());
      MOZ_ASSERT(cloneDataSize > sizeof(uint64_t));
      MOZ_ASSERT(cloneInfo.offsetToKeyProp() <=
                 (cloneDataSize - sizeof(uint64_t)));

      // Special case where someone put an object into an autoIncrement'ing
      // objectStore with no key in its keyPath set. We needed to figure out
      // which row id we would get above before we could set that properly.
      uint64_t keyPropValue =
          ReinterpretDoubleAsUInt64(static_cast<double>(autoIncrementNum));

      static const size_t keyPropSize = sizeof(uint64_t);

      char keyPropBuffer[keyPropSize];
      LittleEndian::writeUint64(keyPropBuffer, keyPropValue);

      auto iter = cloneData.Start();
      MOZ_ALWAYS_TRUE(cloneData.Advance(iter, cloneInfo.offsetToKeyProp()));
      MOZ_ALWAYS_TRUE(cloneData.UpdateBytes(iter, keyPropBuffer, keyPropSize));
    }
  }

  key.BindToStatement(stmt, NS_LITERAL_CSTRING("key"));

  if (mDataOverThreshold) {
    // The data we store in the SQLite database is a (signed) 64-bit integer.
    // The flags are left-shifted 32 bits so the max value is 0xFFFFFFFF.
    // The file_ids index occupies the lower 32 bits and its max is 0xFFFFFFFF.
    static const uint32_t kCompressedFlag = (1 << 0);

    uint32_t flags = 0;
    flags |= kCompressedFlag;

    uint32_t index = mStoredFileInfos.Length() - 1;

    int64_t data = (uint64_t(flags) << 32) | index;

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("data"), data);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    nsCString flatCloneData;
    flatCloneData.SetLength(cloneDataSize);
    auto iter = cloneData.Start();
    cloneData.ReadBytes(iter, flatCloneData.BeginWriting(), cloneDataSize);

    // Compress the bytes before adding into the database.
    const char* uncompressed = flatCloneData.BeginReading();
    size_t uncompressedLength = cloneDataSize;

    size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);

    UniqueFreePtr<char> compressed(
        static_cast<char*>(malloc(compressedLength)));
    if (NS_WARN_IF(!compressed)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    snappy::RawCompress(uncompressed, uncompressedLength, compressed.get(),
                        &compressedLength);

    uint8_t* dataBuffer = reinterpret_cast<uint8_t*>(compressed.release());
    size_t dataBufferLength = compressedLength;

    rv = stmt->BindAdoptedBlobByName(NS_LITERAL_CSTRING("data"), dataBuffer,
                                     dataBufferLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (!mStoredFileInfos.IsEmpty()) {
    // Moved outside the loop to allow it to be cached when demanded by the
    // first write.  (We may have mStoredFileInfos without any required writes.)
    Maybe<FileHelper> fileHelper;
    nsAutoString fileIds;

    for (uint32_t count = mStoredFileInfos.Length(), index = 0; index < count;
         index++) {
      StoredFileInfo& storedFileInfo = mStoredFileInfos[index];
      MOZ_ASSERT(storedFileInfo.mFileInfo);

      // If there is a StoredFileInfo, then one of the following is true:
      // - This was an overflow structured clone and storedFileInfo.mInputStream
      //   MUST be non-null.
      // - This is a reference to a Blob that may or may not have already been
      //   written to disk.  storedFileInfo.mFileActor MUST be non-null, but
      //   its GetInputStream may return null (so don't assert on them).
      // - It's a mutable file.  No writing will be performed.
      MOZ_ASSERT(storedFileInfo.mInputStream || storedFileInfo.mFileActor ||
                 storedFileInfo.mType == StructuredCloneFile::eMutableFile);

      nsCOMPtr<nsIInputStream> inputStream;
      // Check for an explicit stream, like a structured clone stream.
      storedFileInfo.mInputStream.swap(inputStream);
      // Check for a blob-backed stream otherwise.
      if (!inputStream && storedFileInfo.mFileActor) {
        ErrorResult streamRv;
        inputStream = storedFileInfo.mFileActor->GetInputStream(streamRv);
        if (NS_WARN_IF(streamRv.Failed())) {
          return streamRv.StealNSResult();
        }
      }

      if (inputStream) {
        if (fileHelper.isNothing()) {
          RefPtr<FileManager> fileManager =
              Transaction()->GetDatabase()->GetFileManager();
          MOZ_ASSERT(fileManager);

          fileHelper.emplace(fileManager);
          rv = fileHelper->Init();
          if (NS_WARN_IF(NS_FAILED(rv))) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }
        }

        RefPtr<FileInfo>& fileInfo = storedFileInfo.mFileInfo;

        nsCOMPtr<nsIFile> file = fileHelper->GetFile(fileInfo);
        if (NS_WARN_IF(!file)) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        nsCOMPtr<nsIFile> journalFile = fileHelper->GetJournalFile(fileInfo);
        if (NS_WARN_IF(!journalFile)) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        bool compress =
            storedFileInfo.mType == StructuredCloneFile::eStructuredClone;

        rv = fileHelper->CreateFileFromStream(file, journalFile, inputStream,
                                              compress);
        if (NS_FAILED(rv) &&
            NS_ERROR_GET_MODULE(rv) != NS_ERROR_MODULE_DOM_INDEXEDDB) {
          IDB_REPORT_INTERNAL_ERR();
          rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          // Try to remove the file if the copy failed.
          nsresult rv2 = fileHelper->RemoveFile(file, journalFile);
          if (NS_WARN_IF(NS_FAILED(rv2))) {
            return rv;
          }
          return rv;
        }

        if (storedFileInfo.mFileActor) {
          storedFileInfo.mFileActor->WriteSucceededClearBlobImpl();
        }
      }

      if (index) {
        fileIds.Append(' ');
      }
      storedFileInfo.Serialize(fileIds);
    }

    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("file_ids"), fileIds);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("file_ids"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = stmt->Execute();
  if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
    MOZ_ASSERT(!keyUnset, "Generated key had a collision!");
    return rv;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update our indexes if needed.
  if (!mParams.indexUpdateInfos().IsEmpty()) {
    MOZ_ASSERT(mUniqueIndexTable.isSome());

    // Write the index_data_values column.
    AutoTArray<IndexDataValue, 32> indexValues;
    rv = IndexDataValuesFromUpdateInfos(mParams.indexUpdateInfos(),
                                        mUniqueIndexTable.ref(), indexValues);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = UpdateIndexValues(aConnection, osid, key, indexValues);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = InsertIndexTableRows(aConnection, osid, key, indexValues);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (autoIncrementNum) {
    mMetadata->mNextAutoIncrementId = autoIncrementNum + 1;
    Transaction()->NoteModifiedAutoIncrementObjectStore(mMetadata);
  }

  return NS_OK;
}

void ObjectStoreAddOrPutRequestOp::GetResponse(RequestResponse& aResponse,
                                               size_t* aResponseSize) {
  AssertIsOnOwningThread();

  if (mOverwrite) {
    aResponse = ObjectStorePutResponse(mResponse);
    *aResponseSize = mResponse.GetBuffer().Length();
  } else {
    aResponse = ObjectStoreAddResponse(mResponse);
    *aResponseSize = mResponse.GetBuffer().Length();
  }
}

void ObjectStoreAddOrPutRequestOp::Cleanup() {
  AssertIsOnOwningThread();

  mStoredFileInfos.Clear();

  NormalTransactionOp::Cleanup();
}

NS_IMPL_ISUPPORTS(ObjectStoreAddOrPutRequestOp::SCInputStream, nsIInputStream)

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::Close() { return NS_OK; }

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::Available(uint64_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::Read(char* aBuf, uint32_t aCount,
                                                  uint32_t* _retval) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::ReadSegments(
    nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
    uint32_t* _retval) {
  *_retval = 0;

  while (aCount) {
    uint32_t count = std::min(uint32_t(mIter.RemainingInSegment()), aCount);
    if (!count) {
      // We've run out of data in the last segment.
      break;
    }

    uint32_t written;
    nsresult rv =
        aWriter(this, aClosure, mIter.Data(), *_retval, count, &written);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // InputStreams do not propagate errors to caller.
      return NS_OK;
    }

    // Writer should write what we asked it to write.
    MOZ_ASSERT(written == count);

    *_retval += count;
    aCount -= count;

    mData.Advance(mIter, count);
  }

  return NS_OK;
}

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::IsNonBlocking(bool* _retval) {
  *_retval = false;
  return NS_OK;
}

ObjectStoreGetRequestOp::ObjectStoreGetRequestOp(TransactionBase* aTransaction,
                                                 const RequestParams& aParams,
                                                 bool aGetAll)
    : NormalTransactionOp(aTransaction),
      mObjectStoreId(aGetAll
                         ? aParams.get_ObjectStoreGetAllParams().objectStoreId()
                         : aParams.get_ObjectStoreGetParams().objectStoreId()),
      mDatabase(aTransaction->GetDatabase()),
      mOptionalKeyRange(
          aGetAll ? aParams.get_ObjectStoreGetAllParams().optionalKeyRange()
                  : Some(aParams.get_ObjectStoreGetParams().keyRange())),
      mBackgroundParent(aTransaction->GetBackgroundParent()),
      mPreprocessInfoCount(0),
      mLimit(aGetAll ? aParams.get_ObjectStoreGetAllParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreGetParams ||
             aParams.type() == RequestParams::TObjectStoreGetAllParams);
  MOZ_ASSERT(mObjectStoreId);
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT(mBackgroundParent);
}

template <typename T>
void MoveData(StructuredCloneReadInfo& aInfo, T& aResult);

template <>
void MoveData<SerializedStructuredCloneReadInfo>(
    StructuredCloneReadInfo& aInfo,
    SerializedStructuredCloneReadInfo& aResult) {
  aResult.data().data = std::move(aInfo.mData);
  aResult.hasPreprocessInfo() = aInfo.mHasPreprocessInfo;
}

template <>
void MoveData<WasmModulePreprocessInfo>(StructuredCloneReadInfo& aInfo,
                                        WasmModulePreprocessInfo& aResult) {}

template <bool aForPreprocess, typename T>
nsresult ObjectStoreGetRequestOp::ConvertResponse(
    StructuredCloneReadInfo& aInfo, T& aResult) {
  MoveData(aInfo, aResult);

  FallibleTArray<SerializedStructuredCloneFile> serializedFiles;
  nsresult rv =
      SerializeStructuredCloneFiles(mBackgroundParent, mDatabase, aInfo.mFiles,
                                    aForPreprocess, serializedFiles);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(aResult.files().IsEmpty());

  aResult.files().SwapElements(serializedFiles);

  return NS_OK;
}

nsresult ObjectStoreGetRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(!mGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  AUTO_PROFILER_LABEL("ObjectStoreGetRequestOp::DoDatabaseWork", DOM);

  const bool hasKeyRange = mOptionalKeyRange.isSome();

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(),
                                NS_LITERAL_CSTRING("key"), keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING(
                        "SELECT file_ids, data "
                        "FROM object_data "
                        "WHERE object_store_id = :osid") +
                    keyRangeClause + NS_LITERAL_CSTRING(" ORDER BY key ASC") +
                    limitClause;

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    StructuredCloneReadInfo* cloneInfo = mResponse.AppendElement(fallible);
    if (NS_WARN_IF(!cloneInfo)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = GetStructuredCloneReadInfoFromStatement(
        stmt, 1, 0, mDatabase->GetFileManager(), cloneInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (cloneInfo->mHasPreprocessInfo) {
      mPreprocessInfoCount++;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

bool ObjectStoreGetRequestOp::HasPreprocessInfo() {
  return mPreprocessInfoCount > 0;
}

nsresult ObjectStoreGetRequestOp::GetPreprocessParams(
    PreprocessParams& aParams) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mResponse.IsEmpty());

  if (mGetAll) {
    aParams = ObjectStoreGetAllPreprocessParams();

    FallibleTArray<WasmModulePreprocessInfo> falliblePreprocessInfos;
    if (NS_WARN_IF(!falliblePreprocessInfos.SetLength(mPreprocessInfoCount,
                                                      fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    uint32_t fallibleIndex = 0;
    for (uint32_t count = mResponse.Length(), index = 0; index < count;
         index++) {
      StructuredCloneReadInfo& info = mResponse[index];

      if (info.mHasPreprocessInfo) {
        nsresult rv = ConvertResponse<true>(
            info, falliblePreprocessInfos[fallibleIndex++]);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    nsTArray<WasmModulePreprocessInfo>& preprocessInfos =
        aParams.get_ObjectStoreGetAllPreprocessParams().preprocessInfos();

    falliblePreprocessInfos.SwapElements(preprocessInfos);

    return NS_OK;
  }

  aParams = ObjectStoreGetPreprocessParams();

  WasmModulePreprocessInfo& preprocessInfo =
      aParams.get_ObjectStoreGetPreprocessParams().preprocessInfo();

  nsresult rv = ConvertResponse<true>(mResponse[0], preprocessInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void ObjectStoreGetRequestOp::GetResponse(RequestResponse& aResponse,
                                          size_t* aResponseSize) {
  MOZ_ASSERT_IF(mLimit, mResponse.Length() <= mLimit);

  if (mGetAll) {
    aResponse = ObjectStoreGetAllResponse();
    *aResponseSize = 0;

    if (!mResponse.IsEmpty()) {
      FallibleTArray<SerializedStructuredCloneReadInfo> fallibleCloneInfos;
      if (NS_WARN_IF(
              !fallibleCloneInfos.SetLength(mResponse.Length(), fallible))) {
        aResponse = NS_ERROR_OUT_OF_MEMORY;
        return;
      }

      for (uint32_t count = mResponse.Length(), index = 0; index < count;
           index++) {
        *aResponseSize += mResponse[index].Size();
        nsresult rv =
            ConvertResponse<false>(mResponse[index], fallibleCloneInfos[index]);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          aResponse = rv;
          return;
        }
      }

      nsTArray<SerializedStructuredCloneReadInfo>& cloneInfos =
          aResponse.get_ObjectStoreGetAllResponse().cloneInfos();

      fallibleCloneInfos.SwapElements(cloneInfos);
    }

    return;
  }

  aResponse = ObjectStoreGetResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    SerializedStructuredCloneReadInfo& serializedInfo =
        aResponse.get_ObjectStoreGetResponse().cloneInfo();

    *aResponseSize += mResponse[0].Size();
    nsresult rv = ConvertResponse<false>(mResponse[0], serializedInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResponse = rv;
    }
  }
}

ObjectStoreGetKeyRequestOp::ObjectStoreGetKeyRequestOp(
    TransactionBase* aTransaction, const RequestParams& aParams, bool aGetAll)
    : NormalTransactionOp(aTransaction),
      mObjectStoreId(
          aGetAll ? aParams.get_ObjectStoreGetAllKeysParams().objectStoreId()
                  : aParams.get_ObjectStoreGetKeyParams().objectStoreId()),
      mOptionalKeyRange(
          aGetAll ? aParams.get_ObjectStoreGetAllKeysParams().optionalKeyRange()
                  : Some(aParams.get_ObjectStoreGetKeyParams().keyRange())),
      mLimit(aGetAll ? aParams.get_ObjectStoreGetAllKeysParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreGetKeyParams ||
             aParams.type() == RequestParams::TObjectStoreGetAllKeysParams);
  MOZ_ASSERT(mObjectStoreId);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
}

nsresult ObjectStoreGetKeyRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("ObjectStoreGetKeyRequestOp::DoDatabaseWork", DOM);

  const bool hasKeyRange = mOptionalKeyRange.isSome();

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(),
                                NS_LITERAL_CSTRING("key"), keyRangeClause);
  }

  nsAutoCString limitClause;
  if (mLimit) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING(
                        "SELECT key "
                        "FROM object_data "
                        "WHERE object_store_id = :osid") +
                    keyRangeClause + NS_LITERAL_CSTRING(" ORDER BY key ASC") +
                    limitClause;

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    Key* key = mResponse.AppendElement(fallible);
    if (NS_WARN_IF(!key)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = key->SetFromStatement(stmt, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void ObjectStoreGetKeyRequestOp::GetResponse(RequestResponse& aResponse,
                                             size_t* aResponseSize) {
  MOZ_ASSERT_IF(mLimit, mResponse.Length() <= mLimit);

  if (mGetAll) {
    aResponse = ObjectStoreGetAllKeysResponse();
    *aResponseSize = 0;

    if (!mResponse.IsEmpty()) {
      nsTArray<Key>& response =
          aResponse.get_ObjectStoreGetAllKeysResponse().keys();

      mResponse.SwapElements(response);
      for (uint32_t i = 0; i < mResponse.Length(); ++i) {
        *aResponseSize += mResponse[i].GetBuffer().Length();
      }
    }

    return;
  }

  aResponse = ObjectStoreGetKeyResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    *aResponseSize = mResponse[0].GetBuffer().Length();
    aResponse.get_ObjectStoreGetKeyResponse().key() = std::move(mResponse[0]);
  }
}

ObjectStoreDeleteRequestOp::ObjectStoreDeleteRequestOp(
    TransactionBase* aTransaction, const ObjectStoreDeleteParams& aParams)
    : NormalTransactionOp(aTransaction),
      mParams(aParams),
      mObjectStoreMayHaveIndexes(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);

  RefPtr<FullObjectStoreMetadata> metadata =
      aTransaction->GetMetadataForObjectStoreId(mParams.objectStoreId());
  MOZ_ASSERT(metadata);

  mObjectStoreMayHaveIndexes = metadata->HasLiveIndexes();
}

nsresult ObjectStoreDeleteRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  AUTO_PROFILER_LABEL("ObjectStoreDeleteRequestOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool objectStoreHasIndexes;
  rv =
      ObjectStoreHasIndexes(this, aConnection, mParams.objectStoreId(),
                            mObjectStoreMayHaveIndexes, &objectStoreHasIndexes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (objectStoreHasIndexes) {
    rv = DeleteObjectStoreDataTableRowsWithIndexes(
        aConnection, mParams.objectStoreId(), Some(mParams.keyRange()));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    NS_NAMED_LITERAL_CSTRING(objectStoreIdString, "object_store_id");

    nsAutoCString keyRangeClause;
    GetBindingClauseForKeyRange(mParams.keyRange(), NS_LITERAL_CSTRING("key"),
                                keyRangeClause);

    DatabaseConnection::CachedStatement stmt;
    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM object_data "
                           "WHERE object_store_id = :") +
            objectStoreIdString + keyRangeClause + NS_LITERAL_CSTRING(";"),
        &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(objectStoreIdString, mParams.objectStoreId());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = BindKeyRangeToStatement(mParams.keyRange(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

ObjectStoreClearRequestOp::ObjectStoreClearRequestOp(
    TransactionBase* aTransaction, const ObjectStoreClearParams& aParams)
    : NormalTransactionOp(aTransaction),
      mParams(aParams),
      mObjectStoreMayHaveIndexes(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);

  RefPtr<FullObjectStoreMetadata> metadata =
      aTransaction->GetMetadataForObjectStoreId(mParams.objectStoreId());
  MOZ_ASSERT(metadata);

  mObjectStoreMayHaveIndexes = metadata->HasLiveIndexes();
}

nsresult ObjectStoreClearRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("ObjectStoreClearRequestOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(Transaction());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool objectStoreHasIndexes;
  rv =
      ObjectStoreHasIndexes(this, aConnection, mParams.objectStoreId(),
                            mObjectStoreMayHaveIndexes, &objectStoreHasIndexes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (objectStoreHasIndexes) {
    rv = DeleteObjectStoreDataTableRowsWithIndexes(
        aConnection, mParams.objectStoreId(), Nothing());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    DatabaseConnection::CachedStatement stmt;
    rv = aConnection->GetCachedStatement(
        NS_LITERAL_CSTRING("DELETE FROM object_data "
                           "WHERE object_store_id = :object_store_id;"),
        &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("object_store_id"),
                               mParams.objectStoreId());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult ObjectStoreCountRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("ObjectStoreCountRequestOp::DoDatabaseWork", DOM);

  const bool hasKeyRange = mParams.optionalKeyRange().isSome();

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mParams.optionalKeyRange().ref(),
                                NS_LITERAL_CSTRING("key"), keyRangeClause);
  }

  nsCString query = NS_LITERAL_CSTRING(
                        "SELECT count(*) "
                        "FROM object_data "
                        "WHERE object_store_id = :osid") +
                    keyRangeClause;

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                             mParams.objectStoreId());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mParams.optionalKeyRange().ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  int64_t count = stmt->AsInt64(0);
  if (NS_WARN_IF(count < 0)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mResponse.count() = count;

  return NS_OK;
}

// static
already_AddRefed<FullIndexMetadata> IndexRequestOpBase::IndexMetadataForParams(
    TransactionBase* aTransaction, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetParams ||
             aParams.type() == RequestParams::TIndexGetKeyParams ||
             aParams.type() == RequestParams::TIndexGetAllParams ||
             aParams.type() == RequestParams::TIndexGetAllKeysParams ||
             aParams.type() == RequestParams::TIndexCountParams);

  uint64_t objectStoreId;
  uint64_t indexId;

  switch (aParams.type()) {
    case RequestParams::TIndexGetParams: {
      const IndexGetParams& params = aParams.get_IndexGetParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexGetKeyParams: {
      const IndexGetKeyParams& params = aParams.get_IndexGetKeyParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexGetAllParams: {
      const IndexGetAllParams& params = aParams.get_IndexGetAllParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexGetAllKeysParams: {
      const IndexGetAllKeysParams& params = aParams.get_IndexGetAllKeysParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexCountParams: {
      const IndexCountParams& params = aParams.get_IndexCountParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
      aTransaction->GetMetadataForObjectStoreId(objectStoreId);
  MOZ_ASSERT(objectStoreMetadata);

  RefPtr<FullIndexMetadata> indexMetadata =
      aTransaction->GetMetadataForIndexId(objectStoreMetadata, indexId);
  MOZ_ASSERT(indexMetadata);

  return indexMetadata.forget();
}

IndexGetRequestOp::IndexGetRequestOp(TransactionBase* aTransaction,
                                     const RequestParams& aParams, bool aGetAll)
    : IndexRequestOpBase(aTransaction, aParams),
      mDatabase(aTransaction->GetDatabase()),
      mOptionalKeyRange(aGetAll
                            ? aParams.get_IndexGetAllParams().optionalKeyRange()
                            : Some(aParams.get_IndexGetParams().keyRange())),
      mBackgroundParent(aTransaction->GetBackgroundParent()),
      mLimit(aGetAll ? aParams.get_IndexGetAllParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetParams ||
             aParams.type() == RequestParams::TIndexGetAllParams);
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT(mBackgroundParent);
}

nsresult IndexGetRequestOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(!mGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  AUTO_PROFILER_LABEL("IndexGetRequestOp::DoDatabaseWork", DOM);

  const bool hasKeyRange = mOptionalKeyRange.isSome();

  nsCString indexTable;
  if (mMetadata->mCommonMetadata.unique()) {
    indexTable.AssignLiteral("unique_index_data ");
  } else {
    indexTable.AssignLiteral("index_data ");
  }

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(),
                                NS_LITERAL_CSTRING("value"), keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING(
                        "SELECT file_ids, data "
                        "FROM object_data "
                        "INNER JOIN ") +
                    indexTable +
                    NS_LITERAL_CSTRING(
                        "AS index_table "
                        "ON object_data.object_store_id = "
                        "index_table.object_store_id "
                        "AND object_data.key = "
                        "index_table.object_data_key "
                        "WHERE index_id = :index_id") +
                    keyRangeClause + limitClause;

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                             mMetadata->mCommonMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    StructuredCloneReadInfo* cloneInfo = mResponse.AppendElement(fallible);
    if (NS_WARN_IF(!cloneInfo)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = GetStructuredCloneReadInfoFromStatement(
        stmt, 1, 0, mDatabase->GetFileManager(), cloneInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (cloneInfo->mHasPreprocessInfo) {
      IDB_WARNING("Preprocessing for indexes not yet implemented!");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void IndexGetRequestOp::GetResponse(RequestResponse& aResponse,
                                    size_t* aResponseSize) {
  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  if (mGetAll) {
    aResponse = IndexGetAllResponse();
    *aResponseSize = 0;

    if (!mResponse.IsEmpty()) {
      FallibleTArray<SerializedStructuredCloneReadInfo> fallibleCloneInfos;
      if (NS_WARN_IF(
              !fallibleCloneInfos.SetLength(mResponse.Length(), fallible))) {
        aResponse = NS_ERROR_OUT_OF_MEMORY;
        return;
      }

      for (uint32_t count = mResponse.Length(), index = 0; index < count;
           index++) {
        StructuredCloneReadInfo& info = mResponse[index];
        *aResponseSize += info.Size();

        SerializedStructuredCloneReadInfo& serializedInfo =
            fallibleCloneInfos[index];

        serializedInfo.data().data = std::move(info.mData);

        FallibleTArray<SerializedStructuredCloneFile> serializedFiles;
        nsresult rv = SerializeStructuredCloneFiles(
            mBackgroundParent, mDatabase, info.mFiles,
            /* aForPreprocess */ false, serializedFiles);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          aResponse = rv;
          return;
        }

        MOZ_ASSERT(serializedInfo.files().IsEmpty());

        serializedInfo.files().SwapElements(serializedFiles);
      }

      nsTArray<SerializedStructuredCloneReadInfo>& cloneInfos =
          aResponse.get_IndexGetAllResponse().cloneInfos();

      fallibleCloneInfos.SwapElements(cloneInfos);
    }

    return;
  }

  aResponse = IndexGetResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    StructuredCloneReadInfo& info = mResponse[0];
    *aResponseSize += info.Size();

    SerializedStructuredCloneReadInfo& serializedInfo =
        aResponse.get_IndexGetResponse().cloneInfo();

    serializedInfo.data().data = std::move(info.mData);

    FallibleTArray<SerializedStructuredCloneFile> serializedFiles;
    nsresult rv = SerializeStructuredCloneFiles(
        mBackgroundParent, mDatabase, info.mFiles,
        /* aForPreprocess */ false, serializedFiles);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResponse = rv;
      return;
    }

    MOZ_ASSERT(serializedInfo.files().IsEmpty());

    serializedInfo.files().SwapElements(serializedFiles);
  }
}

IndexGetKeyRequestOp::IndexGetKeyRequestOp(TransactionBase* aTransaction,
                                           const RequestParams& aParams,
                                           bool aGetAll)
    : IndexRequestOpBase(aTransaction, aParams),
      mOptionalKeyRange(
          aGetAll ? aParams.get_IndexGetAllKeysParams().optionalKeyRange()
                  : Some(aParams.get_IndexGetKeyParams().keyRange())),
      mLimit(aGetAll ? aParams.get_IndexGetAllKeysParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetKeyParams ||
             aParams.type() == RequestParams::TIndexGetAllKeysParams);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
}

nsresult IndexGetKeyRequestOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(!mGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  AUTO_PROFILER_LABEL("IndexGetKeyRequestOp::DoDatabaseWork", DOM);

  const bool hasKeyRange = mOptionalKeyRange.isSome();

  nsCString indexTable;
  if (mMetadata->mCommonMetadata.unique()) {
    indexTable.AssignLiteral("unique_index_data ");
  } else {
    indexTable.AssignLiteral("index_data ");
  }

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(),
                                NS_LITERAL_CSTRING("value"), keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING(
                        "SELECT object_data_key "
                        "FROM ") +
                    indexTable +
                    NS_LITERAL_CSTRING("WHERE index_id = :index_id") +
                    keyRangeClause + limitClause;

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                             mMetadata->mCommonMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    Key* key = mResponse.AppendElement(fallible);
    if (NS_WARN_IF(!key)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = key->SetFromStatement(stmt, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void IndexGetKeyRequestOp::GetResponse(RequestResponse& aResponse,
                                       size_t* aResponseSize) {
  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  if (mGetAll) {
    aResponse = IndexGetAllKeysResponse();
    *aResponseSize = 0;

    if (!mResponse.IsEmpty()) {
      mResponse.SwapElements(aResponse.get_IndexGetAllKeysResponse().keys());
      for (uint32_t i = 0; i < mResponse.Length(); ++i) {
        *aResponseSize += mResponse[i].GetBuffer().Length();
      }
    }

    return;
  }

  aResponse = IndexGetKeyResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    *aResponseSize = mResponse[0].GetBuffer().Length();
    aResponse.get_IndexGetKeyResponse().key() = std::move(mResponse[0]);
  }
}

nsresult IndexCountRequestOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("IndexCountRequestOp::DoDatabaseWork", DOM);

  const bool hasKeyRange = mParams.optionalKeyRange().isSome();

  nsCString indexTable;
  if (mMetadata->mCommonMetadata.unique()) {
    indexTable.AssignLiteral("unique_index_data ");
  } else {
    indexTable.AssignLiteral("index_data ");
  }

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mParams.optionalKeyRange().ref(),
                                NS_LITERAL_CSTRING("value"), keyRangeClause);
  }

  nsCString query = NS_LITERAL_CSTRING(
                        "SELECT count(*) "
                        "FROM ") +
                    indexTable +
                    NS_LITERAL_CSTRING("WHERE index_id = :index_id") +
                    keyRangeClause;

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                             mMetadata->mCommonMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mParams.optionalKeyRange().ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  int64_t count = stmt->AsInt64(0);
  if (NS_WARN_IF(count < 0)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mResponse.count() = count;

  return NS_OK;
}

bool Cursor::CursorOpBase::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT(!mResponseSent);

  if (!IsActorDestroyed()) {
    mResponse = ClampResultCode(aResultCode);

    // This is an expected race when the transaction is invalidated after
    // data is retrieved from database. We clear the retrieved files to prevent
    // the assertion failure in SendResponseInternal when mResponse.type() is
    // CursorResponse::Tnsresult.
    if (Transaction()->IsInvalidated() && !mFiles.IsEmpty()) {
      mFiles.Clear();
    }

    mCursor->SendResponseInternal(mResponse, mFiles);
  }

#ifdef DEBUG
  mResponseSent = true;
#endif
  return false;
}

void Cursor::CursorOpBase::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT_IF(!IsActorDestroyed(), mResponseSent);

  mCursor = nullptr;

#ifdef DEBUG
  // A bit hacky but the CursorOp request is not generated in response to a
  // child request like most other database operations. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

nsresult Cursor::CursorOpBase::PopulateResponseFromStatement(
    DatabaseConnection::CachedStatement& aStmt, bool aInitializeResponse) {
  Transaction()->AssertIsOnConnectionThread();
  MOZ_ASSERT(mResponse.type() == CursorResponse::T__None);
  MOZ_ASSERT_IF(mFiles.IsEmpty(), aInitializeResponse);

  nsresult rv = mCursor->mKey.SetFromStatement(aStmt, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  switch (mCursor->mType) {
    case OpenCursorParams::TObjectStoreOpenCursorParams: {
      StructuredCloneReadInfo cloneInfo(
          JS::StructuredCloneScope::DifferentProcess);
      rv = GetStructuredCloneReadInfoFromStatement(
          aStmt, 2, 1, mCursor->mFileManager, &cloneInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (cloneInfo.mHasPreprocessInfo) {
        IDB_WARNING("Preprocessing for cursors not yet implemented!");
        return NS_ERROR_NOT_IMPLEMENTED;
      }

      if (aInitializeResponse) {
        mResponse = nsTArray<ObjectStoreCursorResponse>();
      } else {
        MOZ_ASSERT(mResponse.type() ==
                   CursorResponse::TArrayOfObjectStoreCursorResponse);
      }

      auto& responses = mResponse.get_ArrayOfObjectStoreCursorResponse();
      auto& response = *responses.AppendElement();
      response.cloneInfo().data().data = std::move(cloneInfo.mData);
      response.key() = mCursor->mKey;

      mFiles.AppendElement(std::move(cloneInfo.mFiles));
      break;
    }

    case OpenCursorParams::TObjectStoreOpenKeyCursorParams: {
      MOZ_ASSERT(aInitializeResponse);
      mResponse = ObjectStoreKeyCursorResponse(mCursor->mKey);
      break;
    }

    case OpenCursorParams::TIndexOpenCursorParams: {
      rv = mCursor->mSortKey.SetFromStatement(aStmt, 1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = mCursor->mObjectKey.SetFromStatement(aStmt, 2);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      StructuredCloneReadInfo cloneInfo(
          JS::StructuredCloneScope::DifferentProcess);
      rv = GetStructuredCloneReadInfoFromStatement(
          aStmt, 4, 3, mCursor->mFileManager, &cloneInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (cloneInfo.mHasPreprocessInfo) {
        IDB_WARNING("Preprocessing for cursors not yet implemented!");
        return NS_ERROR_NOT_IMPLEMENTED;
      }

      MOZ_ASSERT(aInitializeResponse);
      mResponse = IndexCursorResponse();

      auto& response = mResponse.get_IndexCursorResponse();
      response.cloneInfo().data().data = std::move(cloneInfo.mData);
      response.key() = mCursor->mKey;
      response.sortKey() = mCursor->mSortKey;
      response.objectKey() = mCursor->mObjectKey;

      mFiles.AppendElement(std::move(cloneInfo.mFiles));
      break;
    }

    case OpenCursorParams::TIndexOpenKeyCursorParams: {
      rv = mCursor->mSortKey.SetFromStatement(aStmt, 1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = mCursor->mObjectKey.SetFromStatement(aStmt, 2);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(aInitializeResponse);
      mResponse = IndexKeyCursorResponse(mCursor->mKey, mCursor->mSortKey,
                                         mCursor->mObjectKey);
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

void Cursor::OpenOp::GetRangeKeyInfo(bool aLowerBound, Key* aKey, bool* aOpen) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aKey);
  MOZ_ASSERT(aKey->IsUnset());
  MOZ_ASSERT(aOpen);

  if (mOptionalKeyRange.isSome()) {
    const SerializedKeyRange& range = mOptionalKeyRange.ref();
    if (range.isOnly()) {
      *aKey = range.lower();
      *aOpen = false;
      if (mCursor->IsLocaleAware()) {
        range.lower().ToLocaleBasedKey(*aKey, mCursor->mLocale);
      }
    } else {
      *aKey = aLowerBound ? range.lower() : range.upper();
      *aOpen = aLowerBound ? range.lowerOpen() : range.upperOpen();
      if (mCursor->IsLocaleAware()) {
        if (aLowerBound) {
          range.lower().ToLocaleBasedKey(*aKey, mCursor->mLocale);
        } else {
          range.upper().ToLocaleBasedKey(*aKey, mCursor->mLocale);
        }
      }
    }
  } else {
    *aOpen = false;
  }
}

nsresult Cursor::OpenOp::DoObjectStoreDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType == OpenCursorParams::TObjectStoreOpenCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(mCursor->mFileManager);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoObjectStoreDatabaseWork", DOM);

  const bool usingKeyRange = mOptionalKeyRange.isSome();

  NS_NAMED_LITERAL_CSTRING(keyString, "key");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsCString queryStart = NS_LITERAL_CSTRING("SELECT ") + keyString +
                         NS_LITERAL_CSTRING(
                             ", file_ids, data "
                             "FROM object_data "
                             "WHERE object_store_id = :") +
                         id;

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(), keyString,
                                keyRangeClause);
  }

  nsAutoCString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + keyString;
  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC");
      break;

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  nsCString firstQuery = queryStart + keyRangeClause + directionClause +
                         openLimit + NS_LITERAL_CSTRING("1");

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = PopulateResponseFromStatement(stmt, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  keyRangeClause.Truncate();
  nsAutoCString continueToKeyRangeClause;

  NS_NAMED_LITERAL_CSTRING(currentKey, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      AppendConditionClause(keyString, currentKey, false, false,
                            keyRangeClause);
      AppendConditionClause(keyString, currentKey, false, true,
                            continueToKeyRangeClause);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(keyString, rangeKey, true, !open, keyRangeClause);
        AppendConditionClause(keyString, rangeKey, true, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = upper;
      }
      break;
    }

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      AppendConditionClause(keyString, currentKey, true, false, keyRangeClause);
      AppendConditionClause(keyString, currentKey, true, true,
                            continueToKeyRangeClause);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(keyString, rangeKey, false, !open,
                              keyRangeClause);
        AppendConditionClause(keyString, rangeKey, false, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = lower;
      }
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  mCursor->mContinueQuery =
      queryStart + keyRangeClause + directionClause + openLimit;

  mCursor->mContinueToQuery =
      queryStart + continueToKeyRangeClause + directionClause + openLimit;

  return NS_OK;
}

nsresult Cursor::OpenOp::DoObjectStoreKeyDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType ==
             OpenCursorParams::TObjectStoreOpenKeyCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoObjectStoreKeyDatabaseWork", DOM);

  const bool usingKeyRange = mOptionalKeyRange.isSome();

  NS_NAMED_LITERAL_CSTRING(keyString, "key");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsCString queryStart = NS_LITERAL_CSTRING("SELECT ") + keyString +
                         NS_LITERAL_CSTRING(
                             " FROM object_data "
                             "WHERE object_store_id = :") +
                         id;

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(), keyString,
                                keyRangeClause);
  }

  nsAutoCString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + keyString;
  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC");
      break;

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  nsCString firstQuery = queryStart + keyRangeClause + directionClause +
                         openLimit + NS_LITERAL_CSTRING("1");

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = PopulateResponseFromStatement(stmt, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  keyRangeClause.Truncate();
  nsAutoCString continueToKeyRangeClause;

  NS_NAMED_LITERAL_CSTRING(currentKey, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      AppendConditionClause(keyString, currentKey, false, false,
                            keyRangeClause);
      AppendConditionClause(keyString, currentKey, false, true,
                            continueToKeyRangeClause);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(keyString, rangeKey, true, !open, keyRangeClause);
        AppendConditionClause(keyString, rangeKey, true, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = upper;
      }
      break;
    }

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      AppendConditionClause(keyString, currentKey, true, false, keyRangeClause);
      AppendConditionClause(keyString, currentKey, true, true,
                            continueToKeyRangeClause);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(keyString, rangeKey, false, !open,
                              keyRangeClause);
        AppendConditionClause(keyString, rangeKey, false, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = lower;
      }
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  mCursor->mContinueQuery =
      queryStart + keyRangeClause + directionClause + openLimit;
  mCursor->mContinueToQuery =
      queryStart + continueToKeyRangeClause + directionClause + openLimit;

  return NS_OK;
}

nsresult Cursor::OpenOp::DoIndexDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType == OpenCursorParams::TIndexOpenCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(mCursor->mIndexId);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoIndexDatabaseWork", DOM);

  const bool usingKeyRange = mOptionalKeyRange.isSome();

  nsCString indexTable = mCursor->mUniqueIndex
                             ? NS_LITERAL_CSTRING("unique_index_data")
                             : NS_LITERAL_CSTRING("index_data");

  NS_NAMED_LITERAL_CSTRING(sortColumn, "sort_column");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsAutoCString sortColumnAlias;
  if (mCursor->IsLocaleAware()) {
    sortColumnAlias =
        "SELECT index_table.value, "
        "index_table.value_locale as sort_column, ";
  } else {
    sortColumnAlias =
        "SELECT index_table.value as sort_column, "
        "index_table.value_locale, ";
  }

  nsAutoCString queryStart = sortColumnAlias +
                             NS_LITERAL_CSTRING(
                                 "index_table.object_data_key, "
                                 "object_data.file_ids, "
                                 "object_data.data "
                                 "FROM ") +
                             indexTable +
                             NS_LITERAL_CSTRING(
                                 " AS index_table "
                                 "JOIN object_data "
                                 "ON index_table.object_store_id = "
                                 "object_data.object_store_id "
                                 "AND index_table.object_data_key = "
                                 "object_data.key "
                                 "WHERE index_table.index_id = :") +
                             id;

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(), sortColumn,
                                keyRangeClause);
  }

  nsAutoCString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + sortColumn;

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC, index_table.object_data_key ASC");
      break;

    case IDBCursor::PREV:
      directionClause.AppendLiteral(" DESC, index_table.object_data_key DESC");
      break;

    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC, index_table.object_data_key ASC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  nsCString firstQuery = queryStart + keyRangeClause + directionClause +
                         openLimit + NS_LITERAL_CSTRING("1");

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mIndexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    if (mCursor->IsLocaleAware()) {
      rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt,
                                   mCursor->mLocale);
    } else {
      rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = PopulateResponseFromStatement(stmt, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery =
          queryStart +
          NS_LITERAL_CSTRING(
              " AND sort_column >= :current_key "
              "AND ( sort_column > :current_key OR "
              "index_table.object_data_key > :object_key ) ") +
          directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column >= :current_key") +
          directionClause + openLimit;
      mCursor->mContinuePrimaryKeyQuery =
          queryStart +
          NS_LITERAL_CSTRING(
              " AND ("
              "(sort_column == :current_key AND "
              "index_table.object_data_key >= :object_key) OR "
              "sort_column > :current_key"
              ")") +
          directionClause + openLimit;
      break;
    }

    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column > :current_key") +
          directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column >= :current_key") +
          directionClause + openLimit;
      break;
    }

    case IDBCursor::PREV: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }
      mCursor->mContinueQuery =
          queryStart +
          NS_LITERAL_CSTRING(
              " AND sort_column <= :current_key "
              "AND ( sort_column < :current_key OR "
              "index_table.object_data_key < :object_key ) ") +
          directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column <= :current_key") +
          directionClause + openLimit;
      mCursor->mContinuePrimaryKeyQuery =
          queryStart +
          NS_LITERAL_CSTRING(
              " AND ("
              "(sort_column == :current_key AND "
              "index_table.object_data_key <= :object_key) OR "
              "sort_column < :current_key"
              ")") +
          directionClause + openLimit;
      break;
    }

    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }
      mCursor->mContinueQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column < :current_key") +
          directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column <= :current_key") +
          directionClause + openLimit;
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

nsresult Cursor::OpenOp::DoIndexKeyDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType == OpenCursorParams::TIndexOpenKeyCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(mCursor->mIndexId);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoIndexKeyDatabaseWork", DOM);

  const bool usingKeyRange = mOptionalKeyRange.isSome();

  nsCString table = mCursor->mUniqueIndex
                        ? NS_LITERAL_CSTRING("unique_index_data")
                        : NS_LITERAL_CSTRING("index_data");

  NS_NAMED_LITERAL_CSTRING(sortColumn, "sort_column");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsAutoCString sortColumnAlias;
  if (mCursor->IsLocaleAware()) {
    sortColumnAlias =
        "SELECT value, "
        "value_locale as sort_column, ";
  } else {
    sortColumnAlias =
        "SELECT value as sort_column, "
        "value_locale, ";
  }

  nsAutoCString queryStart = sortColumnAlias +
                             NS_LITERAL_CSTRING(
                                 "object_data_key "
                                 " FROM ") +
                             table + NS_LITERAL_CSTRING(" WHERE index_id = :") +
                             id;

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.ref(), sortColumn,
                                keyRangeClause);
  }

  nsAutoCString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + sortColumn;

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC, object_data_key ASC");
      break;

    case IDBCursor::PREV:
      directionClause.AppendLiteral(" DESC, object_data_key DESC");
      break;

    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC, object_data_key ASC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  nsCString firstQuery = queryStart + keyRangeClause + directionClause +
                         openLimit + NS_LITERAL_CSTRING("1");

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mIndexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    if (mCursor->IsLocaleAware()) {
      rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt,
                                   mCursor->mLocale);
    } else {
      rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), stmt);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = PopulateResponseFromStatement(stmt, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery = queryStart +
                                NS_LITERAL_CSTRING(
                                    " AND sort_column >= :current_key "
                                    "AND ( sort_column > :current_key OR "
                                    "object_data_key > :object_key )") +
                                directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column >= :current_key ") +
          directionClause + openLimit;
      mCursor->mContinuePrimaryKeyQuery =
          queryStart +
          NS_LITERAL_CSTRING(
              " AND ("
              "(sort_column == :current_key AND "
              "object_data_key >= :object_key) OR "
              "sort_column > :current_key"
              ")") +
          directionClause + openLimit;
      break;
    }

    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column > :current_key") +
          directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column >= :current_key") +
          directionClause + openLimit;
      break;
    }

    case IDBCursor::PREV: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }

      mCursor->mContinueQuery = queryStart +
                                NS_LITERAL_CSTRING(
                                    " AND sort_column <= :current_key "
                                    "AND ( sort_column < :current_key OR "
                                    "object_data_key < :object_key )") +
                                directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column <= :current_key ") +
          directionClause + openLimit;
      mCursor->mContinuePrimaryKeyQuery =
          queryStart +
          NS_LITERAL_CSTRING(
              " AND ("
              "(sort_column == :current_key AND "
              "object_data_key <= :object_key) OR "
              "sort_column < :current_key"
              ")") +
          directionClause + openLimit;
      break;
    }

    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(sortColumn, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }
      mCursor->mContinueQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column < :current_key") +
          directionClause + openLimit;
      mCursor->mContinueToQuery =
          queryStart + NS_LITERAL_CSTRING(" AND sort_column <= :current_key") +
          directionClause + openLimit;
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

nsresult Cursor::OpenOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mContinueQuery.IsEmpty());
  MOZ_ASSERT(mCursor->mContinueToQuery.IsEmpty());
  MOZ_ASSERT(mCursor->mContinuePrimaryKeyQuery.IsEmpty());
  MOZ_ASSERT(mCursor->mKey.IsUnset());
  MOZ_ASSERT(mCursor->mRangeKey.IsUnset());

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoDatabaseWork", DOM);

  nsresult rv;

  switch (mCursor->mType) {
    case OpenCursorParams::TObjectStoreOpenCursorParams:
      rv = DoObjectStoreDatabaseWork(aConnection);
      break;

    case OpenCursorParams::TObjectStoreOpenKeyCursorParams:
      rv = DoObjectStoreKeyDatabaseWork(aConnection);
      break;

    case OpenCursorParams::TIndexOpenCursorParams:
      rv = DoIndexDatabaseWork(aConnection);
      break;

    case OpenCursorParams::TIndexOpenKeyCursorParams:
      rv = DoIndexKeyDatabaseWork(aConnection);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult Cursor::OpenOp::SendSuccessResult() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT(mResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mSortKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mRangeKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mObjectKey.IsUnset());

  if (IsActorDestroyed()) {
    return NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  mCursor->SendResponseInternal(mResponse, mFiles);

#ifdef DEBUG
  mResponseSent = true;
#endif
  return NS_OK;
}

nsresult Cursor::ContinueOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(!mCursor->mContinueQuery.IsEmpty());
  MOZ_ASSERT(!mCursor->mContinueToQuery.IsEmpty());
  MOZ_ASSERT(!mCursor->mKey.IsUnset());

  const bool isIndex =
      mCursor->mType == OpenCursorParams::TIndexOpenCursorParams ||
      mCursor->mType == OpenCursorParams::TIndexOpenKeyCursorParams;

  MOZ_ASSERT_IF(isIndex && (mCursor->mDirection == IDBCursor::NEXT ||
                            mCursor->mDirection == IDBCursor::PREV),
                !mCursor->mContinuePrimaryKeyQuery.IsEmpty());
  MOZ_ASSERT_IF(isIndex, mCursor->mIndexId);
  MOZ_ASSERT_IF(isIndex, !mCursor->mObjectKey.IsUnset());

  AUTO_PROFILER_LABEL("Cursor::ContinueOp::DoDatabaseWork", DOM);

  // We need to pick a query based on whether or not a key was passed to the
  // continue function. If not we'll grab the the next item in the database that
  // is greater than (or less than, if we're running a PREV cursor) the current
  // key. If a key was passed we'll grab the next item in the database that is
  // greater than (or less than, if we're running a PREV cursor) or equal to the
  // key that was specified.

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  bool hasContinueKey = false;
  bool hasContinuePrimaryKey = false;
  uint32_t advanceCount = 1;
  Key& currentKey =
      mCursor->IsLocaleAware() ? mCursor->mSortKey : mCursor->mKey;

  switch (mParams.type()) {
    case CursorRequestParams::TContinueParams:
      if (!mParams.get_ContinueParams().key().IsUnset()) {
        hasContinueKey = true;
        currentKey = mParams.get_ContinueParams().key();
      }
      break;
    case CursorRequestParams::TContinuePrimaryKeyParams:
      MOZ_ASSERT(!mParams.get_ContinuePrimaryKeyParams().key().IsUnset());
      MOZ_ASSERT(
          !mParams.get_ContinuePrimaryKeyParams().primaryKey().IsUnset());
      MOZ_ASSERT(mCursor->mDirection == IDBCursor::NEXT ||
                 mCursor->mDirection == IDBCursor::PREV);
      hasContinueKey = true;
      hasContinuePrimaryKey = true;
      currentKey = mParams.get_ContinuePrimaryKeyParams().key();
      break;
    case CursorRequestParams::TAdvanceParams:
      advanceCount = mParams.get_AdvanceParams().count();
      break;
    default:
      MOZ_CRASH("Should never get here!");
  }

  const nsCString& continueQuery =
      hasContinuePrimaryKey ? mCursor->mContinuePrimaryKeyQuery
                            : hasContinueKey ? mCursor->mContinueToQuery
                                             : mCursor->mContinueQuery;

  MOZ_ASSERT(advanceCount > 0);
  nsAutoCString countString;
  countString.AppendInt(advanceCount);

  nsCString query = continueQuery + countString;

  NS_NAMED_LITERAL_CSTRING(currentKeyName, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKeyName, "range_key");
  NS_NAMED_LITERAL_CSTRING(objectKeyName, "object_key");

  const bool usingRangeKey = !mCursor->mRangeKey.IsUnset();

  DatabaseConnection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t id = isIndex ? mCursor->mIndexId : mCursor->mObjectStoreId;

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Bind current key.
  rv = currentKey.BindToStatement(stmt, currentKeyName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Bind range key if it is specified.
  if (usingRangeKey) {
    rv = mCursor->mRangeKey.BindToStatement(stmt, rangeKeyName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Bind object key if duplicates are allowed and we're not continuing to a
  // specific key.
  if (isIndex && !hasContinueKey &&
      (mCursor->mDirection == IDBCursor::NEXT ||
       mCursor->mDirection == IDBCursor::PREV)) {
    rv = mCursor->mObjectKey.BindToStatement(stmt, objectKeyName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Bind object key if primaryKey is specified.
  if (hasContinuePrimaryKey) {
    rv = mParams.get_ContinuePrimaryKeyParams().primaryKey().BindToStatement(
        stmt, objectKeyName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  for (uint32_t index = 0; index < advanceCount; index++) {
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!hasResult) {
      mCursor->mKey.Unset();
      mCursor->mSortKey.Unset();
      mCursor->mRangeKey.Unset();
      mCursor->mObjectKey.Unset();
      mResponse = void_t();
      return NS_OK;
    }
  }

  rv = PopulateResponseFromStatement(stmt, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult Cursor::ContinueOp::SendSuccessResult() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mRangeKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mObjectKey.IsUnset());

  if (IsActorDestroyed()) {
    return NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  mCursor->SendResponseInternal(mResponse, mFiles);

#ifdef DEBUG
  mResponseSent = true;
#endif
  return NS_OK;
}

Utils::Utils()
#ifdef DEBUG
    : mActorDestroyed(false)
#endif
{
  AssertIsOnBackgroundThread();
}

Utils::~Utils() { MOZ_ASSERT(mActorDestroyed); }

void Utils::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorDestroyed = true;
#endif
}

mozilla::ipc::IPCResult Utils::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundIndexedDBUtilsParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Utils::RecvGetFileReferences(
    const PersistenceType& aPersistenceType, const nsCString& aOrigin,
    const nsString& aDatabaseName, const int64_t& aFileId, int32_t* aRefCnt,
    int32_t* aDBRefCnt, int32_t* aSliceRefCnt, bool* aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aRefCnt);
  MOZ_ASSERT(aDBRefCnt);
  MOZ_ASSERT(aSliceRefCnt);
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(!mActorDestroyed);

  if (NS_WARN_IF(!IndexedDatabaseManager::Get() || !QuotaManager::Get())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aPersistenceType != quota::PERSISTENCE_TYPE_PERSISTENT &&
                 aPersistenceType != quota::PERSISTENCE_TYPE_TEMPORARY &&
                 aPersistenceType != quota::PERSISTENCE_TYPE_DEFAULT)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aOrigin.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aDatabaseName.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aFileId == 0)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<GetFileReferencesHelper> helper = new GetFileReferencesHelper(
      aPersistenceType, aOrigin, aDatabaseName, aFileId);

  nsresult rv = helper->DispatchAndReturnFileReferences(aRefCnt, aDBRefCnt,
                                                        aSliceRefCnt, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

nsresult GetFileReferencesHelper::DispatchAndReturnFileReferences(
    int32_t* aMemRefCnt, int32_t* aDBRefCnt, int32_t* aSliceRefCnt,
    bool* aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMemRefCnt);
  MOZ_ASSERT(aDBRefCnt);
  MOZ_ASSERT(aSliceRefCnt);
  MOZ_ASSERT(aResult);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mozilla::MutexAutoLock autolock(mMutex);
  while (mWaiting) {
    mCondVar.Wait();
  }

  *aMemRefCnt = mMemRefCnt;
  *aDBRefCnt = mDBRefCnt;
  *aSliceRefCnt = mSliceRefCnt;
  *aResult = mResult;

  return NS_OK;
}

NS_IMETHODIMP
GetFileReferencesHelper::Run() {
  AssertIsOnIOThread();

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  MOZ_ASSERT(mgr);

  RefPtr<FileManager> fileManager =
      mgr->GetFileManager(mPersistenceType, mOrigin, mDatabaseName);

  if (fileManager) {
    RefPtr<FileInfo> fileInfo = fileManager->GetFileInfo(mFileId);

    if (fileInfo) {
      fileInfo->GetReferences(&mMemRefCnt, &mDBRefCnt, &mSliceRefCnt);

      if (mMemRefCnt != -1) {
        // We added an extra temp ref, so account for that accordingly.
        mMemRefCnt--;
      }

      mResult = true;
    }
  }

  mozilla::MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mWaiting);

  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
}

void PermissionRequestHelper::OnPromptComplete(
    PermissionValue aPermissionValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mActorDestroyed) {
    Unused << PIndexedDBPermissionRequestParent::Send__delete__(
        this, aPermissionValue);
  }
}

void PermissionRequestHelper::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;
}

#ifdef DEBUG

NS_IMPL_ISUPPORTS(DEBUGThreadSlower, nsIThreadObserver)

NS_IMETHODIMP
DEBUGThreadSlower::OnDispatchedEvent() { MOZ_CRASH("Should never be called!"); }

NS_IMETHODIMP
DEBUGThreadSlower::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                      bool /* aMayWait */) {
  return NS_OK;
}

NS_IMETHODIMP
DEBUGThreadSlower::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                         bool /* aEventWasProcessed */) {
  MOZ_ASSERT(kDEBUGThreadSleepMS);

  MOZ_ALWAYS_TRUE(PR_Sleep(PR_MillisecondsToInterval(kDEBUGThreadSleepMS)) ==
                  PR_SUCCESS);
  return NS_OK;
}

#endif  // DEBUG

nsresult FileHelper::Init() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mFileManager);

  nsCOMPtr<nsIFile> fileDirectory = mFileManager->GetCheckedDirectory();
  if (NS_WARN_IF(!fileDirectory)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> journalDirectory = mFileManager->EnsureJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    return NS_ERROR_FAILURE;
  }

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(journalDirectory->Exists(&exists)));
  MOZ_ASSERT(exists);

  DebugOnly<bool> isDirectory;
  MOZ_ASSERT(NS_SUCCEEDED(journalDirectory->IsDirectory(&isDirectory)));
  MOZ_ASSERT(isDirectory);

  mFileDirectory = std::move(fileDirectory);
  mJournalDirectory = std::move(journalDirectory);

  return NS_OK;
}

already_AddRefed<nsIFile> FileHelper::GetFile(FileInfo* aFileInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aFileInfo);
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT(mFileDirectory);

  const int64_t fileId = aFileInfo->Id();
  MOZ_ASSERT(fileId > 0);

  nsCOMPtr<nsIFile> file = mFileManager->GetFileForId(mFileDirectory, fileId);
  return file.forget();
}

already_AddRefed<nsIFile> FileHelper::GetJournalFile(FileInfo* aFileInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aFileInfo);
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT(mJournalDirectory);

  const int64_t fileId = aFileInfo->Id();
  MOZ_ASSERT(fileId > 0);

  nsCOMPtr<nsIFile> file =
      mFileManager->GetFileForId(mJournalDirectory, fileId);
  return file.forget();
}

nsresult FileHelper::CreateFileFromStream(nsIFile* aFile, nsIFile* aJournalFile,
                                          nsIInputStream* aInputStream,
                                          bool aCompress) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aFile);
  MOZ_ASSERT(aJournalFile);
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT(mFileDirectory);
  MOZ_ASSERT(mJournalDirectory);

  bool exists;
  nsresult rv = aFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // DOM blobs that are being stored in IDB are cached by calling
  // IDBDatabase::GetOrCreateFileActorForBlob. So if the same DOM blob is stored
  // again under a different key or in a different object store, we just add
  // a new reference instead of creating a new copy (all such stored blobs share
  // the same id).
  // However, it can happen that CreateFileFromStream failed due to quota
  // exceeded error and for some reason the orphaned file couldn't be deleted
  // immediately. Now, if the operation is being repeated, the DOM blob is
  // already cached, so it has the same file id which clashes with the orphaned
  // file. We could do some tricks to restore previous copy loop, but it's safer
  // to just delete the orphaned file and start from scratch.
  // This corner case is partially simulated in test_file_copy_failure.js
  if (exists) {
    bool isFile;
    rv = aFile->IsFile(&isFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isFile)) {
      return NS_ERROR_FAILURE;
    }

    rv = aJournalFile->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!exists)) {
      return NS_ERROR_FAILURE;
    }

    rv = aJournalFile->IsFile(&isFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isFile)) {
      return NS_ERROR_FAILURE;
    }

    IDB_WARNING("Deleting orphaned file!");

    rv = RemoveFile(aFile, aJournalFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Create a journal file first.
  rv = aJournalFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now try to copy the stream.
  RefPtr<FileOutputStream> fileOutputStream =
      CreateFileOutputStream(mFileManager->Type(), mFileManager->Group(),
                             mFileManager->Origin(), aFile);
  if (NS_WARN_IF(!fileOutputStream)) {
    return NS_ERROR_FAILURE;
  }

  if (aCompress) {
    RefPtr<SnappyCompressOutputStream> snappyOutputStream =
        new SnappyCompressOutputStream(fileOutputStream);

    UniquePtr<char[]> buffer(new char[snappyOutputStream->BlockSize()]);

    rv = SyncCopy(aInputStream, snappyOutputStream, buffer.get(),
                  snappyOutputStream->BlockSize());
  } else {
    char buffer[kFileCopyBufferSize];

    rv = SyncCopy(aInputStream, fileOutputStream, buffer, kFileCopyBufferSize);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult FileHelper::RemoveFile(nsIFile* aFile, nsIFile* aJournalFile) {
  nsresult rv;

  int64_t fileSize;

  if (mFileManager->EnforcingQuota()) {
    rv = aFile->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aFile->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mFileManager->EnforcingQuota()) {
    QuotaManager* quotaManager = QuotaManager::Get();
    MOZ_ASSERT(quotaManager);

    quotaManager->DecreaseUsageForOrigin(mFileManager->Type(),
                                         mFileManager->Group(),
                                         mFileManager->Origin(), fileSize);
  }

  rv = aJournalFile->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class FileHelper::ReadCallback final : public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ReadCallback()
      : mMutex("ReadCallback::mMutex"),
        mCondVar(mMutex, "ReadCallback::mCondVar"),
        mInputAvailable(false) {}

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override {
    mozilla::MutexAutoLock autolock(mMutex);

    mInputAvailable = true;
    mCondVar.Notify();

    return NS_OK;
  }

  nsresult AsyncWait(nsIAsyncInputStream* aStream, uint32_t aBufferSize,
                     nsIEventTarget* aTarget) {
    MOZ_ASSERT(aStream);
    mozilla::MutexAutoLock autolock(mMutex);

    nsresult rv = aStream->AsyncWait(this, 0, aBufferSize, aTarget);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mInputAvailable = false;
    while (!mInputAvailable) {
      mCondVar.Wait();
    }

    return NS_OK;
  }

 private:
  ~ReadCallback() = default;

  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  bool mInputAvailable;
};

NS_IMPL_ADDREF(FileHelper::ReadCallback);
NS_IMPL_RELEASE(FileHelper::ReadCallback);

NS_INTERFACE_MAP_BEGIN(FileHelper::ReadCallback)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStreamCallback)
NS_INTERFACE_MAP_END

nsresult FileHelper::SyncRead(nsIInputStream* aInputStream, char* aBuffer,
                              uint32_t aBufferSize, uint32_t* aRead) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aInputStream);

  // Let's try to read, directly.
  nsresult rv = aInputStream->Read(aBuffer, aBufferSize, aRead);
  if (NS_SUCCEEDED(rv) || rv != NS_BASE_STREAM_WOULD_BLOCK) {
    return rv;
  }

  // We need to proceed async.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(aInputStream);
  if (!asyncStream) {
    return rv;
  }

  if (!mReadCallback) {
    mReadCallback = new ReadCallback();
  }

  // We just need any thread with an event loop for receiving the
  // OnInputStreamReady callback. Let's use the I/O thread.
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  rv = mReadCallback->AsyncWait(asyncStream, aBufferSize, target);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SyncRead(aInputStream, aBuffer, aBufferSize, aRead);
}

nsresult FileHelper::SyncCopy(nsIInputStream* aInputStream,
                              nsIOutputStream* aOutputStream, char* aBuffer,
                              uint32_t aBufferSize) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aOutputStream);

  AUTO_PROFILER_LABEL("FileHelper::SyncCopy", DOM);

  nsresult rv;

  do {
    uint32_t numRead;
    rv = SyncRead(aInputStream, aBuffer, aBufferSize, &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (!numRead) {
      break;
    }

    uint32_t numWrite;
    rv = aOutputStream->Write(aBuffer, numRead, &numWrite);
    if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      rv = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (NS_WARN_IF(numWrite != numRead)) {
      rv = NS_ERROR_FAILURE;
      break;
    }
  } while (true);

  if (NS_SUCCEEDED(rv)) {
    rv = aOutputStream->Flush();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv2 = aOutputStream->Close();
  if (NS_WARN_IF(NS_FAILED(rv2))) {
    return NS_SUCCEEDED(rv) ? rv2 : rv;
  }

  return rv;
}

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#undef IDB_MOBILE
#undef IDB_DEBUG_LOG
#undef ASSERT_UNLESS_FUZZING
#undef DISABLE_ASSERTS_FOR_FUZZING
