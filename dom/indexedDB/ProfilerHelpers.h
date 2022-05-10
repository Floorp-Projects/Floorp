/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_profilerhelpers_h__
#define mozilla_dom_indexeddb_profilerhelpers_h__

// This file is not exported and is only meant to be included in IndexedDB
// source files.

#include "IndexedDatabaseManager.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "nsID.h"
#include "nsString.h"

namespace mozilla::dom {

class Event;
class IDBDatabase;
class IDBIndex;
class IDBKeyRange;
class IDBObjectStore;
class IDBTransaction;

namespace indexedDB {

class Key;

template <bool CheckLoggingMode>
class LoggingIdString final : public nsAutoCStringN<NSID_LENGTH> {
 public:
  LoggingIdString();

  explicit LoggingIdString(const nsID& aID);
};

class MOZ_STACK_CLASS LoggingString final : public nsAutoCString {
 public:
  explicit LoggingString(IDBDatabase* aDatabase);
  explicit LoggingString(const IDBTransaction& aTransaction);
  explicit LoggingString(IDBObjectStore* aObjectStore);
  explicit LoggingString(IDBIndex* aIndex);
  explicit LoggingString(IDBKeyRange* aKeyRange);
  explicit LoggingString(const Key& aKey);
  explicit LoggingString(const IDBCursorDirection aDirection);
  explicit LoggingString(const Optional<uint64_t>& aVersion);
  explicit LoggingString(const Optional<uint32_t>& aLimit);

  LoggingString(IDBObjectStore* aObjectStore, const Key& aKey);
  LoggingString(Event* aEvent, const char16_t* aDefault);
};

// Both the aDetailedFmt and the aConciseFmt need to match the variable argument
// list, so we use MOZ_FORMAT_PRINTF twice here.
void MOZ_FORMAT_PRINTF(1, 3) MOZ_FORMAT_PRINTF(2, 3)
    LoggingHelper(const char* aDetailedFmt, const char* aConciseFmt, ...);

}  // namespace indexedDB
}  // namespace mozilla::dom

#define IDB_LOG_MARK(_detailedFmt, _conciseFmt, _loggingId, ...)        \
  mozilla::dom::indexedDB::LoggingHelper("IndexedDB %s: " _detailedFmt, \
                                         "IndexedDB %s: " _conciseFmt,  \
                                         _loggingId, ##__VA_ARGS__)

#define IDB_LOG_ID_STRING(...) \
  mozilla::dom::indexedDB::LoggingIdString<true>(__VA_ARGS__).get()

#define IDB_LOG_STRINGIFY(...) \
  mozilla::dom::indexedDB::LoggingString(__VA_ARGS__).get()

// IDB_LOG_MARK_DETAILED_PARENT and IDB_LOG_MARK_DETAILED_CHILD should have the
// same width.
#define IDB_LOG_MARK_DETAILED_PARENT "Parent"
#define IDB_LOG_MARK_DETAILED_CHILD "Child "

#define IDB_LOG_MARK_CONCISE_PARENT "P"
#define IDB_LOG_MARK_CONCISE_CHILD "C"

#define IDB_LOG_MARK_DETAILED_TRANSACTION "Transaction[%" PRIi64 "]"
#define IDB_LOG_MARK_DETAILED_REQUEST "Request[%" PRIu64 "]"

#define IDB_LOG_MARK_CONCISE_TRANSACTION "T[%" PRIi64 "]"
#define IDB_LOG_MARK_CONCISE_REQUEST "R[%" PRIu64 "]"

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
