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
#include "IDBCursor.h"
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
#include "nsDebug.h"
#include "nsID.h"
#include "nsIDOMEvent.h"
#include "nsString.h"
#include "mozilla/Logging.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

class MOZ_STACK_CLASS LoggingIdString final
  : public nsAutoCString
{
public:
  LoggingIdString()
  {
    using mozilla::ipc::BackgroundChildImpl;

    if (IndexedDatabaseManager::GetLoggingMode() !=
          IndexedDatabaseManager::Logging_Disabled) {
      const BackgroundChildImpl::ThreadLocal* threadLocal =
        BackgroundChildImpl::GetThreadLocalForCurrentThread();
      if (threadLocal) {
        const ThreadLocal* idbThreadLocal = threadLocal->mIndexedDBThreadLocal;
        if (idbThreadLocal) {
          Assign(idbThreadLocal->IdString());
        }
      }
    }
  }

  explicit
  LoggingIdString(const nsID& aID)
  {
    static_assert(NSID_LENGTH > 1, "NSID_LENGTH is set incorrectly!");
    static_assert(NSID_LENGTH <= kDefaultStorageSize,
                  "nID string won't fit in our storage!");
    MOZ_ASSERT(Capacity() > NSID_LENGTH);

    if (IndexedDatabaseManager::GetLoggingMode() !=
          IndexedDatabaseManager::Logging_Disabled) {
      // NSID_LENGTH counts the null terminator, SetLength() does not.
      SetLength(NSID_LENGTH - 1);

      aID.ToProvidedString(
        *reinterpret_cast<char(*)[NSID_LENGTH]>(BeginWriting()));
    }
  }
};

class MOZ_STACK_CLASS LoggingString final
  : public nsAutoCString
{
  static const char kQuote = '\"';
  static const char kOpenBracket = '[';
  static const char kCloseBracket = ']';
  static const char kOpenParen = '(';
  static const char kCloseParen = ')';

public:
  explicit
  LoggingString(IDBDatabase* aDatabase)
    : nsAutoCString(kQuote)
  {
    MOZ_ASSERT(aDatabase);

    AppendUTF16toUTF8(aDatabase->Name(), *this);
    Append(kQuote);
  }

  explicit
  LoggingString(IDBTransaction* aTransaction)
    : nsAutoCString(kOpenBracket)
  {
    MOZ_ASSERT(aTransaction);

    NS_NAMED_LITERAL_CSTRING(kCommaSpace, ", ");

    const nsTArray<nsString>& stores = aTransaction->ObjectStoreNamesInternal();

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

    switch (aTransaction->GetMode()) {
      case IDBTransaction::READ_ONLY:
        AppendLiteral("\"readonly\"");
        break;
      case IDBTransaction::READ_WRITE:
        AppendLiteral("\"readwrite\"");
        break;
      case IDBTransaction::READ_WRITE_FLUSH:
        AppendLiteral("\"readwriteflush\"");
        break;
      case IDBTransaction::VERSION_CHANGE:
        AppendLiteral("\"versionchange\"");
        break;
      default:
        MOZ_CRASH("Unknown mode!");
    };
  }

  explicit
  LoggingString(IDBObjectStore* aObjectStore)
    : nsAutoCString(kQuote)
  {
    MOZ_ASSERT(aObjectStore);

    AppendUTF16toUTF8(aObjectStore->Name(), *this);
    Append(kQuote);
  }

  explicit
  LoggingString(IDBIndex* aIndex)
    : nsAutoCString(kQuote)
  {
    MOZ_ASSERT(aIndex);

    AppendUTF16toUTF8(aIndex->Name(), *this);
    Append(kQuote);
  }

  explicit
  LoggingString(IDBKeyRange* aKeyRange)
  {
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

  explicit
  LoggingString(const Key& aKey)
  {
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
    } else {
      MOZ_ASSERT(aKey.IsArray());
      AssignLiteral("[...]");
    }
  }

  explicit
  LoggingString(const IDBCursor::Direction aDirection)
  {
    switch (aDirection) {
      case IDBCursor::NEXT:
        AssignLiteral("\"next\"");
        break;
      case IDBCursor::NEXT_UNIQUE:
        AssignLiteral("\"nextunique\"");
        break;
      case IDBCursor::PREV:
        AssignLiteral("\"prev\"");
        break;
      case IDBCursor::PREV_UNIQUE:
        AssignLiteral("\"prevunique\"");
        break;
      default:
        MOZ_CRASH("Unknown direction!");
    };
  }

  explicit
  LoggingString(const Optional<uint64_t>& aVersion)
  {
    if (aVersion.WasPassed()) {
      AppendInt(aVersion.Value());
    } else {
      AssignLiteral("<undefined>");
    }
  }

  explicit
  LoggingString(const Optional<uint32_t>& aLimit)
  {
    if (aLimit.WasPassed()) {
      AppendInt(aLimit.Value());
    } else {
      AssignLiteral("<undefined>");
    }
  }

  LoggingString(IDBObjectStore* aObjectStore, const Key& aKey)
  {
    MOZ_ASSERT(aObjectStore);

    if (!aObjectStore->HasValidKeyPath()) {
      Append(LoggingString(aKey));
    }
  }

  LoggingString(nsIDOMEvent* aEvent, const char16_t* aDefault)
    : nsAutoCString(kQuote)
  {
    MOZ_ASSERT(aDefault);

    nsString eventType;

    if (aEvent) {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aEvent->GetType(eventType)));
    } else {
      eventType = nsDependentString(aDefault);
    }

    AppendUTF16toUTF8(eventType, *this);
    Append(kQuote);
  }
};

inline void
LoggingHelper(bool aUseProfiler, const char* aFmt, ...)
{
  MOZ_ASSERT(IndexedDatabaseManager::GetLoggingMode() !=
               IndexedDatabaseManager::Logging_Disabled);
  MOZ_ASSERT(aFmt);

  mozilla::LogModule* logModule = IndexedDatabaseManager::GetLoggingModule();
  MOZ_ASSERT(logModule);

  static const mozilla::LogLevel logLevel = LogLevel::Warning;

  if (MOZ_LOG_TEST(logModule, logLevel) ||
      (aUseProfiler && profiler_is_active())) {
    nsAutoCString message;

    {
      va_list args;
      va_start(args, aFmt);

      message.AppendPrintf(aFmt, args);

      va_end(args);
    }

    MOZ_LOG(logModule, logLevel, ("%s", message.get()));

    if (aUseProfiler) {
      PROFILER_MARKER(message.get());
    }
  }
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#define IDB_LOG_MARK(_detailedFmt, _conciseFmt, ...)                           \
  do {                                                                         \
    using namespace mozilla::dom::indexedDB;                                   \
                                                                               \
    const IndexedDatabaseManager::LoggingMode mode =                           \
      IndexedDatabaseManager::GetLoggingMode();                                \
                                                                               \
    if (mode != IndexedDatabaseManager::Logging_Disabled) {                    \
      const char* _fmt;                                                        \
      if (mode == IndexedDatabaseManager::Logging_Concise ||                   \
          mode == IndexedDatabaseManager::Logging_ConciseProfilerMarks) {      \
        _fmt = _conciseFmt;                                                    \
      } else {                                                                 \
        MOZ_ASSERT(                                                            \
          mode == IndexedDatabaseManager::Logging_Detailed ||                  \
          mode == IndexedDatabaseManager::Logging_DetailedProfilerMarks);      \
        _fmt = _detailedFmt;                                                   \
      }                                                                        \
                                                                               \
      const bool _useProfiler =                                                \
        mode == IndexedDatabaseManager::Logging_ConciseProfilerMarks ||        \
        mode == IndexedDatabaseManager::Logging_DetailedProfilerMarks;         \
                                                                               \
      LoggingHelper(_useProfiler, _fmt, ##__VA_ARGS__);                        \
    }                                                                          \
  } while (0)

#define IDB_LOG_ID_STRING(...)                                                 \
  mozilla::dom::indexedDB::LoggingIdString(__VA_ARGS__).get()

#define IDB_LOG_STRINGIFY(...)                                                 \
  mozilla::dom::indexedDB::LoggingString(__VA_ARGS__).get()

#endif // mozilla_dom_indexeddb_profilerhelpers_h__
