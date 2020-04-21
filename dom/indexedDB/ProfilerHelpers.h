/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_profilerhelpers_h__
#define mozilla_dom_indexeddb_profilerhelpers_h__

// This file is not exported and is only meant to be included in IndexedDB
// source files.

#include "BackgroundChildImpl.h"
#include "GeckoProfiler.h"
#include "IDBCursorType.h"
#include "IDBDatabase.h"
#include "IDBIndex.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDatabaseManager.h"
#include "Key.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Event.h"
#include "nsDebug.h"
#include "nsID.h"
#include "nsString.h"
#include "mozilla/Logging.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

class MOZ_STACK_CLASS LoggingIdString final
    : public nsAutoCStringN<NSID_LENGTH> {
 public:
  LoggingIdString() {
    using mozilla::ipc::BackgroundChildImpl;

    if (IndexedDatabaseManager::GetLoggingMode() !=
        IndexedDatabaseManager::Logging_Disabled) {
      const BackgroundChildImpl::ThreadLocal* threadLocal =
          BackgroundChildImpl::GetThreadLocalForCurrentThread();
      if (threadLocal) {
        const auto& idbThreadLocal = threadLocal->mIndexedDBThreadLocal;
        if (idbThreadLocal) {
          Assign(idbThreadLocal->IdString());
        }
      }
    }
  }

  explicit LoggingIdString(const nsID& aID) {
    static_assert(NSID_LENGTH > 1, "NSID_LENGTH is set incorrectly!");
    static_assert(NSID_LENGTH <= kStorageSize,
                  "nsID string won't fit in our storage!");
    // Capacity() excludes the null terminator; NSID_LENGTH includes it.
    MOZ_ASSERT(Capacity() + 1 == NSID_LENGTH);

    if (IndexedDatabaseManager::GetLoggingMode() !=
        IndexedDatabaseManager::Logging_Disabled) {
      // NSID_LENGTH counts the null terminator, SetLength() does not.
      SetLength(NSID_LENGTH - 1);

      aID.ToProvidedString(
          *reinterpret_cast<char(*)[NSID_LENGTH]>(BeginWriting()));
    }
  }
};

class MOZ_STACK_CLASS LoggingString final : public nsAutoCString {
  static const char kQuote = '\"';
  static const char kOpenBracket = '[';
  static const char kCloseBracket = ']';
  static const char kOpenParen = '(';
  static const char kCloseParen = ')';

 public:
  explicit LoggingString(IDBDatabase* aDatabase) : nsAutoCString(kQuote) {
    MOZ_ASSERT(aDatabase);

    AppendUTF16toUTF8(aDatabase->Name(), *this);
    Append(kQuote);
  }

  explicit LoggingString(const IDBTransaction& aTransaction)
      : nsAutoCString(kOpenBracket) {
    NS_NAMED_LITERAL_CSTRING(kCommaSpace, ", ");

    const nsTArray<nsString>& stores = aTransaction.ObjectStoreNamesInternal();

    for (uint32_t count = stores.Length(), index = 0; index < count; index++) {
      Append(kQuote);
      AppendUTF16toUTF8(stores[index], *this);
      Append(kQuote);

      if (index != count - 1) {
        Append(kCommaSpace);
      }
    }

    Append(kCloseBracket);
    Append(kCommaSpace);

    switch (aTransaction.GetMode()) {
      case IDBTransaction::Mode::ReadOnly:
        AppendLiteral("\"readonly\"");
        break;
      case IDBTransaction::Mode::ReadWrite:
        AppendLiteral("\"readwrite\"");
        break;
      case IDBTransaction::Mode::ReadWriteFlush:
        AppendLiteral("\"readwriteflush\"");
        break;
      case IDBTransaction::Mode::Cleanup:
        AppendLiteral("\"cleanup\"");
        break;
      case IDBTransaction::Mode::VersionChange:
        AppendLiteral("\"versionchange\"");
        break;
      default:
        MOZ_CRASH("Unknown mode!");
    };
  }

  explicit LoggingString(IDBObjectStore* aObjectStore) : nsAutoCString(kQuote) {
    MOZ_ASSERT(aObjectStore);

    AppendUTF16toUTF8(aObjectStore->Name(), *this);
    Append(kQuote);
  }

  explicit LoggingString(IDBIndex* aIndex) : nsAutoCString(kQuote) {
    MOZ_ASSERT(aIndex);

    AppendUTF16toUTF8(aIndex->Name(), *this);
    Append(kQuote);
  }

  explicit LoggingString(IDBKeyRange* aKeyRange) {
    if (aKeyRange) {
      if (aKeyRange->IsOnly()) {
        Assign(LoggingString(aKeyRange->Lower()));
      } else {
        if (aKeyRange->LowerOpen()) {
          Assign(kOpenParen);
        } else {
          Assign(kOpenBracket);
        }

        Append(LoggingString(aKeyRange->Lower()));
        AppendLiteral(", ");
        Append(LoggingString(aKeyRange->Upper()));

        if (aKeyRange->UpperOpen()) {
          Append(kCloseParen);
        } else {
          Append(kCloseBracket);
        }
      }
    } else {
      AssignLiteral("<undefined>");
    }
  }

  explicit LoggingString(const Key& aKey) {
    if (aKey.IsUnset()) {
      AssignLiteral("<undefined>");
    } else if (aKey.IsFloat()) {
      AppendPrintf("%g", aKey.ToFloat());
    } else if (aKey.IsDate()) {
      AppendPrintf("<Date %g>", aKey.ToDateMsec());
    } else if (aKey.IsString()) {
      nsAutoString str;
      aKey.ToString(str);
      AppendPrintf("\"%s\"", NS_ConvertUTF16toUTF8(str).get());
    } else if (aKey.IsBinary()) {
      AssignLiteral("[object ArrayBuffer]");
    } else {
      MOZ_ASSERT(aKey.IsArray());
      AssignLiteral("[...]");
    }
  }

  explicit LoggingString(const IDBCursorDirection aDirection) {
    switch (aDirection) {
      case IDBCursorDirection::Next:
        AssignLiteral("\"next\"");
        break;
      case IDBCursorDirection::Nextunique:
        AssignLiteral("\"nextunique\"");
        break;
      case IDBCursorDirection::Prev:
        AssignLiteral("\"prev\"");
        break;
      case IDBCursorDirection::Prevunique:
        AssignLiteral("\"prevunique\"");
        break;
      default:
        MOZ_CRASH("Unknown direction!");
    };
  }

  explicit LoggingString(const Optional<uint64_t>& aVersion) {
    if (aVersion.WasPassed()) {
      AppendInt(aVersion.Value());
    } else {
      AssignLiteral("<undefined>");
    }
  }

  explicit LoggingString(const Optional<uint32_t>& aLimit) {
    if (aLimit.WasPassed()) {
      AppendInt(aLimit.Value());
    } else {
      AssignLiteral("<undefined>");
    }
  }

  LoggingString(IDBObjectStore* aObjectStore, const Key& aKey) {
    MOZ_ASSERT(aObjectStore);

    if (!aObjectStore->HasValidKeyPath()) {
      Append(LoggingString(aKey));
    }
  }

  LoggingString(Event* aEvent, const char16_t* aDefault)
      : nsAutoCString(kQuote) {
    MOZ_ASSERT(aDefault);

    nsAutoString eventType;

    if (aEvent) {
      aEvent->GetType(eventType);
    } else {
      eventType = nsDependentString(aDefault);
    }

    AppendUTF16toUTF8(eventType, *this);
    Append(kQuote);
  }
};

inline void MOZ_FORMAT_PRINTF(2, 3)
    LoggingHelper(bool aUseProfiler, const char* aFmt, ...) {
  MOZ_ASSERT(IndexedDatabaseManager::GetLoggingMode() !=
             IndexedDatabaseManager::Logging_Disabled);
  MOZ_ASSERT(aFmt);

  mozilla::LogModule* logModule = IndexedDatabaseManager::GetLoggingModule();
  MOZ_ASSERT(logModule);

  static const mozilla::LogLevel logLevel = LogLevel::Warning;

  if (MOZ_LOG_TEST(logModule, logLevel) ||
#ifdef MOZ_GECKO_PROFILER
      (aUseProfiler && profiler_thread_is_being_profiled())
#else
      false
#endif
  ) {
    nsAutoCString message;

    {
      va_list args;
      va_start(args, aFmt);

      message.AppendPrintf(aFmt, args);

      va_end(args);
    }

    MOZ_LOG(logModule, logLevel, ("%s", message.get()));

    if (aUseProfiler) {
      PROFILER_ADD_MARKER(message.get(), DOM);
    }
  }
}

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#define IDB_LOG_MARK2(_detailedFmt, _conciseFmt, ...)                          \
  do {                                                                         \
    using namespace mozilla::dom::indexedDB;                                   \
                                                                               \
    const IndexedDatabaseManager::LoggingMode mode =                           \
        IndexedDatabaseManager::GetLoggingMode();                              \
                                                                               \
    if (mode != IndexedDatabaseManager::Logging_Disabled) {                    \
      const char* _fmt;                                                        \
      if (mode == IndexedDatabaseManager::Logging_Concise ||                   \
          mode == IndexedDatabaseManager::Logging_ConciseProfilerMarks) {      \
        _fmt = _conciseFmt;                                                    \
      } else {                                                                 \
        MOZ_ASSERT(mode == IndexedDatabaseManager::Logging_Detailed ||         \
                   mode ==                                                     \
                       IndexedDatabaseManager::Logging_DetailedProfilerMarks); \
        _fmt = _detailedFmt;                                                   \
      }                                                                        \
                                                                               \
      const bool _useProfiler =                                                \
          mode == IndexedDatabaseManager::Logging_ConciseProfilerMarks ||      \
          mode == IndexedDatabaseManager::Logging_DetailedProfilerMarks;       \
                                                                               \
      LoggingHelper(_useProfiler, _fmt, ##__VA_ARGS__);                        \
    }                                                                          \
  } while (0)

#define IDB_LOG_MARK(_detailedFmt, _conciseFmt, _loggingId, ...)             \
  IDB_LOG_MARK2("IndexedDB %s: " _detailedFmt, "IndexedDB %s: " _conciseFmt, \
                _loggingId, ##__VA_ARGS__)

#define IDB_LOG_ID_STRING(...) \
  mozilla::dom::indexedDB::LoggingIdString(__VA_ARGS__).get()

#define IDB_LOG_STRINGIFY(...) \
  mozilla::dom::indexedDB::LoggingString(__VA_ARGS__).get()

// IDB_LOG_MARK_DETAILED_PARENT and IDB_LOG_MARK_DETAILED_CHILD should have the
// same width.
#define IDB_LOG_MARK_DETAILED_PARENT "Parent"
#define IDB_LOG_MARK_DETAILED_CHILD "Child "

#define IDB_LOG_MARK_CONCISE_PARENT "P"
#define IDB_LOG_MARK_CONCISE_CHILD "C"

#define IDB_LOG_MARK_DETAILED_TRANSACTION "Transaction[%lld]"
#define IDB_LOG_MARK_DETAILED_REQUEST "Request[%llu]"

#define IDB_LOG_MARK_CONCISE_TRANSACTION "T[%lld]"
#define IDB_LOG_MARK_CONCISE_REQUEST "R[%llu]"

#define IDB_LOG_MARK_TRANSACTION_REQUEST(                                      \
    _detailedPeer, _concisePeer, _detailedFmt, _conciseFmt, _loggingId,        \
    _transactionSerialNumber, _requestSerialNumber, ...)                       \
  IDB_LOG_MARK(_detailedPeer " " IDB_LOG_MARK_DETAILED_TRANSACTION             \
                             " " IDB_LOG_MARK_DETAILED_REQUEST                 \
                             ": " _detailedFmt,                                \
               _concisePeer " " IDB_LOG_MARK_CONCISE_TRANSACTION               \
                            " " IDB_LOG_MARK_CONCISE_REQUEST ": " _conciseFmt, \
               _loggingId, _transactionSerialNumber, _requestSerialNumber,     \
               ##__VA_ARGS__)

#define IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(                               \
    _detailedFmt, _conciseFmt, _loggingId, _transactionSerialNumber,           \
    _requestSerialNumber, ...)                                                 \
  IDB_LOG_MARK_TRANSACTION_REQUEST(                                            \
      IDB_LOG_MARK_DETAILED_PARENT, IDB_LOG_MARK_CONCISE_PARENT, _detailedFmt, \
      _conciseFmt, _loggingId, _transactionSerialNumber, _requestSerialNumber, \
      ##__VA_ARGS__)

#define IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(_detailedFmt, _conciseFmt,    \
                                               _transactionSerialNumber,     \
                                               _requestSerialNumber, ...)    \
  IDB_LOG_MARK_TRANSACTION_REQUEST(                                          \
      IDB_LOG_MARK_DETAILED_CHILD, IDB_LOG_MARK_CONCISE_CHILD, _detailedFmt, \
      _conciseFmt, IDB_LOG_ID_STRING(), _transactionSerialNumber,            \
      _requestSerialNumber, ##__VA_ARGS__)

#define IDB_LOG_MARK_CHILD_REQUEST(_detailedFmt, _conciseFmt,                \
                                   _requestSerialNumber, ...)                \
  IDB_LOG_MARK(IDB_LOG_MARK_DETAILED_CHILD " " IDB_LOG_MARK_DETAILED_REQUEST \
                                           ": " _detailedFmt,                \
               IDB_LOG_MARK_CONCISE_CHILD " " IDB_LOG_MARK_CONCISE_REQUEST   \
                                          ": " _conciseFmt,                  \
               IDB_LOG_ID_STRING(), _requestSerialNumber, ##__VA_ARGS__)

#define IDB_LOG_MARK_TRANSACTION(_detailedPeer, _concisePeer, _detailedFmt,  \
                                 _conciseFmt, _loggingId,                    \
                                 _transactionSerialNumber, ...)              \
  IDB_LOG_MARK(                                                              \
      _detailedPeer " " IDB_LOG_MARK_DETAILED_TRANSACTION ": " _detailedFmt, \
      _concisePeer " " IDB_LOG_MARK_CONCISE_TRANSACTION ": " _conciseFmt,    \
      _loggingId, _transactionSerialNumber, ##__VA_ARGS__)

#define IDB_LOG_MARK_PARENT_TRANSACTION(_detailedFmt, _conciseFmt, _loggingId, \
                                        _transactionSerialNumber, ...)         \
  IDB_LOG_MARK_TRANSACTION(                                                    \
      IDB_LOG_MARK_DETAILED_PARENT, IDB_LOG_MARK_CONCISE_PARENT, _detailedFmt, \
      _conciseFmt, _loggingId, _transactionSerialNumber, ##__VA_ARGS__)

#define IDB_LOG_MARK_CHILD_TRANSACTION(_detailedFmt, _conciseFmt,     \
                                       _transactionSerialNumber, ...) \
  IDB_LOG_MARK_TRANSACTION(IDB_LOG_MARK_DETAILED_CHILD,               \
                           IDB_LOG_MARK_CONCISE_CHILD, _detailedFmt,  \
                           _conciseFmt, IDB_LOG_ID_STRING(),          \
                           _transactionSerialNumber, ##__VA_ARGS__)

#endif  // mozilla_dom_indexeddb_profilerhelpers_h__
