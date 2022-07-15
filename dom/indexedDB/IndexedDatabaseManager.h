/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddatabasemanager_h__
#define mozilla_dom_indexeddatabasemanager_h__

#include "js/TypeDecls.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/Logging.h"
#include "mozilla/Mutex.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "SafeRefPtr.h"

namespace mozilla {

class EventChainPostVisitor;

namespace dom {

class IDBFactory;

namespace indexedDB {

class BackgroundUtilsChild;
class DatabaseFileManager;
class FileManagerInfo;

}  // namespace indexedDB

class IndexedDatabaseManager final {
  using PersistenceType = mozilla::dom::quota::PersistenceType;
  using DatabaseFileManager = mozilla::dom::indexedDB::DatabaseFileManager;
  using FileManagerInfo = mozilla::dom::indexedDB::FileManagerInfo;

 public:
  enum LoggingMode {
    Logging_Disabled = 0,
    Logging_Concise,
    Logging_Detailed,
    Logging_ConciseProfilerMarks,
    Logging_DetailedProfilerMarks
  };

  NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(IndexedDatabaseManager, Destroy())

  // Returns a non-owning reference.
  static IndexedDatabaseManager* GetOrCreate();

  // Returns a non-owning reference.
  static IndexedDatabaseManager* Get();

  static bool IsClosed();

  static bool IsMainProcess()
#ifdef DEBUG
      ;
#else
  {
    return sIsMainProcess;
  }
#endif

  static bool FullSynchronous();

  static LoggingMode GetLoggingMode()
#ifdef DEBUG
      ;
#else
  {
    return sLoggingMode;
  }
#endif

  static mozilla::LogModule* GetLoggingModule()
#ifdef DEBUG
      ;
#else
  {
    return sLoggingModule;
  }
#endif

  static uint32_t DataThreshold();

  static uint32_t MaxSerializedMsgSize();

  // The maximum number of extra entries to preload in an Cursor::OpenOp or
  // Cursor::ContinueOp.
  static int32_t MaxPreloadExtraRecords();

  void ClearBackgroundActor();

  [[nodiscard]] SafeRefPtr<DatabaseFileManager> GetFileManager(
      PersistenceType aPersistenceType, const nsACString& aOrigin,
      const nsAString& aDatabaseName);

  void AddFileManager(SafeRefPtr<DatabaseFileManager> aFileManager);

  void InvalidateAllFileManagers();

  void InvalidateFileManagers(PersistenceType aPersistenceType,
                              const nsACString& aOrigin);

  void InvalidateFileManager(PersistenceType aPersistenceType,
                             const nsACString& aOrigin,
                             const nsAString& aDatabaseName);

  // Don't call this method in real code, it blocks the main thread!
  // It is intended to be used by mochitests to test correctness of the special
  // reference counting of stored blobs/files.
  nsresult BlockAndGetFileReferences(PersistenceType aPersistenceType,
                                     const nsACString& aOrigin,
                                     const nsAString& aDatabaseName,
                                     int64_t aFileId, int32_t* aRefCnt,
                                     int32_t* aDBRefCnt, bool* aResult);

  nsresult FlushPendingFileDeletions();

  static const nsCString& GetLocale();

  static bool ResolveSandboxBinding(JSContext* aCx);

  static bool DefineIndexedDB(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

 private:
  IndexedDatabaseManager();
  ~IndexedDatabaseManager();

  nsresult Init();

  void Destroy();

  static void LoggingModePrefChangedCallback(const char* aPrefName,
                                             void* aClosure);

  // Maintains a list of all DatabaseFileManager objects per origin. This list
  // isn't protected by any mutex but it is only ever touched on the IO thread.
  nsClassHashtable<nsCStringHashKey, FileManagerInfo> mFileManagerInfos;

  nsClassHashtable<nsRefPtrHashKey<DatabaseFileManager>, nsTArray<int64_t>>
      mPendingDeleteInfos;

  nsCString mLocale;

  indexedDB::BackgroundUtilsChild* mBackgroundActor;

  static bool sIsMainProcess;
  static bool sFullSynchronousMode;
  static LazyLogModule sLoggingModule;
  static Atomic<LoggingMode> sLoggingMode;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddatabasemanager_h__
