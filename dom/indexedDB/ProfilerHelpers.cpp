/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerHelpers.h"

#include "BackgroundChildImpl.h"
#include "GeckoProfiler.h"
#include "IDBDatabase.h"
#include "IDBIndex.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "Key.h"
#include "ThreadLocal.h"

#include "mozilla/dom/Event.h"
#include "nsReadableUtils.h"

namespace mozilla::dom::indexedDB {

namespace {
static const char kQuote = '\"';
static const char kOpenBracket = '[';
static const char kCloseBracket = ']';
static const char kOpenParen = '(';
static const char kCloseParen = ')';

void LoggingHelper(bool aUseProfiler, const char* aFmt, va_list args) {
  MOZ_ASSERT(IndexedDatabaseManager::GetLoggingMode() !=
             IndexedDatabaseManager::Logging_Disabled);
  MOZ_ASSERT(aFmt);

  mozilla::LogModule* logModule = IndexedDatabaseManager::GetLoggingModule();
  MOZ_ASSERT(logModule);

  static const mozilla::LogLevel logLevel = LogLevel::Warning;

  if (MOZ_LOG_TEST(logModule, logLevel) ||
      (aUseProfiler && profiler_thread_is_being_profiled_for_markers())) {
    nsAutoCString message;

    message.AppendVprintf(aFmt, args);

    MOZ_LOG(logModule, logLevel, ("%s", message.get()));

    if (aUseProfiler) {
      PROFILER_MARKER_UNTYPED(message, DOM);
    }
  }
}
}  // namespace

template <bool CheckLoggingMode>
LoggingIdString<CheckLoggingMode>::LoggingIdString() {
  using mozilla::ipc::BackgroundChildImpl;

  if (!CheckLoggingMode || IndexedDatabaseManager::GetLoggingMode() !=
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

template <bool CheckLoggingMode>
LoggingIdString<CheckLoggingMode>::LoggingIdString(const nsID& aID) {
  static_assert(NSID_LENGTH > 1, "NSID_LENGTH is set incorrectly!");
  static_assert(NSID_LENGTH <= kStorageSize,
                "nsID string won't fit in our storage!");
  // Capacity() excludes the null terminator; NSID_LENGTH includes it.
  MOZ_ASSERT(Capacity() + 1 == NSID_LENGTH);

  if (!CheckLoggingMode || IndexedDatabaseManager::GetLoggingMode() !=
                               IndexedDatabaseManager::Logging_Disabled) {
    // NSID_LENGTH counts the null terminator, SetLength() does not.
    SetLength(NSID_LENGTH - 1);

    aID.ToProvidedString(
        *reinterpret_cast<char(*)[NSID_LENGTH]>(BeginWriting()));
  }
}

template class LoggingIdString<false>;
template class LoggingIdString<true>;

LoggingString::LoggingString(IDBDatabase* aDatabase) : nsAutoCString(kQuote) {
  MOZ_ASSERT(aDatabase);

  AppendUTF16toUTF8(aDatabase->Name(), *this);
  Append(kQuote);
}

LoggingString::LoggingString(const IDBTransaction& aTransaction)
    : nsAutoCString(kOpenBracket) {
  constexpr auto kCommaSpace = ", "_ns;

  StringJoinAppend(*this, kCommaSpace, aTransaction.ObjectStoreNamesInternal(),
                   [](nsACString& dest, const auto& store) {
                     dest.Append(kQuote);
                     AppendUTF16toUTF8(store, dest);
                     dest.Append(kQuote);
                   });

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

LoggingString::LoggingString(IDBObjectStore* aObjectStore)
    : nsAutoCString(kQuote) {
  MOZ_ASSERT(aObjectStore);

  AppendUTF16toUTF8(aObjectStore->Name(), *this);
  Append(kQuote);
}

LoggingString::LoggingString(IDBIndex* aIndex) : nsAutoCString(kQuote) {
  MOZ_ASSERT(aIndex);

  AppendUTF16toUTF8(aIndex->Name(), *this);
  Append(kQuote);
}

LoggingString::LoggingString(IDBKeyRange* aKeyRange) {
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

LoggingString::LoggingString(const Key& aKey) {
  if (aKey.IsUnset()) {
    AssignLiteral("<undefined>");
  } else if (aKey.IsFloat()) {
    AppendPrintf("%g", aKey.ToFloat());
  } else if (aKey.IsDate()) {
    AppendPrintf("<Date %g>", aKey.ToDateMsec());
  } else if (aKey.IsString()) {
    AppendPrintf("\"%s\"", NS_ConvertUTF16toUTF8(aKey.ToString()).get());
  } else if (aKey.IsBinary()) {
    AssignLiteral("[object ArrayBuffer]");
  } else {
    MOZ_ASSERT(aKey.IsArray());
    AssignLiteral("[...]");
  }
}

LoggingString::LoggingString(const IDBCursorDirection aDirection) {
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

LoggingString::LoggingString(const Optional<uint64_t>& aVersion) {
  if (aVersion.WasPassed()) {
    AppendInt(aVersion.Value());
  } else {
    AssignLiteral("<undefined>");
  }
}

LoggingString::LoggingString(const Optional<uint32_t>& aLimit) {
  if (aLimit.WasPassed()) {
    AppendInt(aLimit.Value());
  } else {
    AssignLiteral("<undefined>");
  }
}

LoggingString::LoggingString(IDBObjectStore* aObjectStore, const Key& aKey) {
  MOZ_ASSERT(aObjectStore);

  if (!aObjectStore->HasValidKeyPath()) {
    Append(LoggingString(aKey));
  }
}

LoggingString::LoggingString(Event* aEvent, const char16_t* aDefault)
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

void LoggingHelper(const char* aDetailedFmt, const char* aConciseFmt, ...) {
  const IndexedDatabaseManager::LoggingMode mode =
      IndexedDatabaseManager::GetLoggingMode();

  if (mode != IndexedDatabaseManager::Logging_Disabled) {
    const char* fmt;
    if (mode == IndexedDatabaseManager::Logging_Concise ||
        mode == IndexedDatabaseManager::Logging_ConciseProfilerMarks) {
      fmt = aConciseFmt;
    } else {
      MOZ_ASSERT(mode == IndexedDatabaseManager::Logging_Detailed ||
                 mode == IndexedDatabaseManager::Logging_DetailedProfilerMarks);
      fmt = aDetailedFmt;
    }

    const bool useProfiler =
        mode == IndexedDatabaseManager::Logging_ConciseProfilerMarks ||
        mode == IndexedDatabaseManager::Logging_DetailedProfilerMarks;

    va_list args;
    va_start(args, aConciseFmt);

    LoggingHelper(useProfiler, fmt, args);

    va_end(args);
  }
}

}  // namespace mozilla::dom::indexedDB
