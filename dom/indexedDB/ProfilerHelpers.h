/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_profilerhelpers_h__
#define mozilla_dom_indexeddb_profilerhelpers_h__

#include "GeckoProfiler.h"

// Uncomment this if you want IndexedDB operations to be marked in the profiler.
//#define IDB_PROFILER_USE_MARKS

// Uncomment this if you want extended details to appear in profiler marks.
//#define IDB_PROFILER_MARK_DETAILS 0

// Sanity check the options above.
#if defined(IDB_PROFILER_USE_MARKS) && !defined(MOZ_ENABLE_PROFILER_SPS)
#error Cannot use IDB_PROFILER_USE_MARKS without MOZ_ENABLE_PROFILER_SPS!
#endif

#if defined(IDB_PROFILER_MARK_DETAILS) && !defined(IDB_PROFILER_USE_MARKS)
#error Cannot use IDB_PROFILER_MARK_DETAILS without IDB_PROFILER_USE_MARKS!
#endif

#ifdef IDB_PROFILER_USE_MARKS

#ifdef IDB_PROFILER_MARK_DETAILS

#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBIndex.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "Key.h"

BEGIN_INDEXEDDB_NAMESPACE

class ProfilerString : public nsAutoCString
{
  static const char kQuote = '\"';
  static const char kOpenBracket = '[';
  static const char kCloseBracket = ']';
  static const char kOpenParen = '(';
  static const char kCloseParen = ')';

public:
  explicit
  ProfilerString(IDBDatabase* aDatabase)
  {
    MOZ_ASSERT(aDatabase);

    Append(kQuote);
    AppendUTF16toUTF8(aDatabase->Name(), *this);
    Append(kQuote);
  }

  explicit
  ProfilerString(IDBTransaction* aTransaction)
  {
    MOZ_ASSERT(aTransaction);

    switch (aTransaction->GetMode()) {
      case IDBTransaction::READ_ONLY:
        AppendLiteral("\"readonly\"");
        break;
      case IDBTransaction::READ_WRITE:
        AppendLiteral("\"readwrite\"");
        break;
      case IDBTransaction::VERSION_CHANGE:
        AppendLiteral("\"versionchange\"");
        break;
      default:
        MOZ_NOT_REACHED("Unknown mode!");
    };
  }

  explicit
  ProfilerString(IDBObjectStore* aObjectStore)
  {
    MOZ_ASSERT(aObjectStore);

    Append(kQuote);
    AppendUTF16toUTF8(aObjectStore->Name(), *this);
    Append(kQuote);
  }

  explicit
  ProfilerString(IDBIndex* aIndex)
  {
    MOZ_ASSERT(aIndex);

    Append(kQuote);
    AppendUTF16toUTF8(aIndex->Name(), *this);
    Append(kQuote);
  }

  explicit
  ProfilerString(IDBKeyRange* aKeyRange)
  {
    if (aKeyRange) {
      if (aKeyRange->IsOnly()) {
        Append(ProfilerString(aKeyRange->Lower()));
      }
      else {
        Append(aKeyRange->IsLowerOpen() ? kOpenParen : kOpenBracket);
        Append(ProfilerString(aKeyRange->Lower()));
        AppendLiteral(", ");
        Append(ProfilerString(aKeyRange->Upper()));
        Append(aKeyRange->IsUpperOpen() ? kCloseParen : kCloseBracket);
      }
    }
  }

  explicit
  ProfilerString(const Key& aKey)
  {
    if (aKey.IsUnset()) {
      Assign("null");
    }
    else if (aKey.IsFloat()) {
      AppendPrintf("%g", aKey.ToFloat());
    }
    else if (aKey.IsDate()) {
      AppendPrintf("<Date %g>", aKey.ToDateMsec());
    }
    else if (aKey.IsString()) {
      nsAutoString str;
      aKey.ToString(str);
      AppendPrintf("\"%s\"", NS_ConvertUTF16toUTF8(str).get());
    }
    else {
      MOZ_ASSERT(aKey.IsArray());
      AppendLiteral("<Array>");
    }
  }

  explicit
  ProfilerString(const IDBCursor::Direction aDirection)
  {
    switch (aDirection) {
      case IDBCursor::NEXT:
        AppendLiteral("\"next\"");
        break;
      case IDBCursor::NEXT_UNIQUE:
        AppendLiteral("\"nextunique\"");
        break;
      case IDBCursor::PREV:
        AppendLiteral("\"prev\"");
        break;
      case IDBCursor::PREV_UNIQUE:
        AppendLiteral("\"prevunique\"");
        break;
      default:
        MOZ_NOT_REACHED("Unknown direction!");
    };
  }
};

END_INDEXEDDB_NAMESPACE

#define IDB_PROFILER_MARK(_detailedFmt, _conciseFmt, ...)                      \
  do {                                                                         \
    nsAutoCString _mark;                                                       \
    _mark.AppendPrintf(_detailedFmt, ##__VA_ARGS__);                           \
    PROFILER_MARKER(_mark.get());                                              \
  } while (0)

#define IDB_PROFILER_STRING(_arg)                                              \
  mozilla::dom::indexedDB::ProfilerString((_arg)).get()

#else // IDB_PROFILER_MARK_DETAILS

#define IDB_PROFILER_MARK(_detailedFmt, _conciseFmt, ...)                      \
  do {                                                                         \
    nsAutoCString _mark;                                                       \
    _mark.AppendPrintf(_conciseFmt, ##__VA_ARGS__);                            \
    PROFILER_MARKER(_mark.get());                                              \
  } while (0)

#define IDB_PROFILER_STRING(_arg) ""

#endif // IDB_PROFILER_MARK_DETAILS

#define IDB_PROFILER_MARK_IF(_cond, _detailedFmt, _conciseFmt, ...)            \
  do {                                                                         \
    if (_cond) {                                                               \
      IDB_PROFILER_MARK(_detailedFmt, _conciseFmt, __VA_ARGS__);               \
    }                                                                          \
  } while (0)

#else // IDB_PROFILER_USE_MARKS

#define IDB_PROFILER_MARK(...) do { } while(0)
#define IDB_PROFILER_MARK_IF(_cond, ...) do { } while(0)
#define IDB_PROFILER_MARK2(_detailedFmt, _notdetailedFmt, ...) do { } while(0)
#define IDB_PROFILER_STRING(_arg) ""

#endif // IDB_PROFILER_USE_MARKS

#endif // mozilla_dom_indexeddb_profilerhelpers_h__
