/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddatabasemanager_h__
#define mozilla_dom_indexeddatabasemanager_h__

#include "nsIObserver.h"

#include "js/TypeDecls.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/Mutex.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsITimer.h"

class nsIEventTarget;

namespace mozilla {

class EventChainPostVisitor;

namespace dom {

class IDBFactory;

namespace quota {

class QuotaManager;

} // namespace quota

namespace indexedDB {

class BackgroundUtilsChild;
class FileManager;
class FileManagerInfo;

} // namespace indexedDB

class IndexedDatabaseManager final
  : public nsIObserver
  , public nsITimerCallback
{
  typedef mozilla::dom::quota::PersistenceType PersistenceType;
  typedef mozilla::dom::quota::QuotaManager QuotaManager;
  typedef mozilla::dom::indexedDB::FileManager FileManager;
  typedef mozilla::dom::indexedDB::FileManagerInfo FileManagerInfo;

public:
  enum LoggingMode
  {
    Logging_Disabled = 0,
    Logging_Concise,
    Logging_Detailed,
    Logging_ConciseProfilerMarks,
    Logging_DetailedProfilerMarks
  };

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  // Returns a non-owning reference.
  static IndexedDatabaseManager*
  GetOrCreate();

  // Returns a non-owning reference.
  static IndexedDatabaseManager*
  Get();

  static bool
  IsClosed();

  static bool
  IsMainProcess()
#ifdef DEBUG
  ;
#else
  {
    return sIsMainProcess;
  }
#endif

  static bool
  InLowDiskSpaceMode()
#ifdef DEBUG
  ;
#else
  {
    return !!sLowDiskSpaceMode;
  }
#endif

  static bool
  InTestingMode();

  static bool
  FullSynchronous();

  static LoggingMode
  GetLoggingMode()
#ifdef DEBUG
  ;
#else
  {
    return sLoggingMode;
  }
#endif

  static mozilla::LogModule*
  GetLoggingModule()
#ifdef DEBUG
  ;
#else
  {
    return sLoggingModule;
  }
#endif

  static bool
  ExperimentalFeaturesEnabled();

  static bool
  ExperimentalFeaturesEnabled(JSContext* aCx, JSObject* aGlobal);

  static bool
  IsFileHandleEnabled();

  static uint32_t
  DataThreshold();

  static uint32_t
  MaxSerializedMsgSize();

  void
  ClearBackgroundActor();

  void
  NoteLiveQuotaManager(QuotaManager* aQuotaManager);

  void
  NoteShuttingDownQuotaManager();

  already_AddRefed<FileManager>
  GetFileManager(PersistenceType aPersistenceType,
                 const nsACString& aOrigin,
                 const nsAString& aDatabaseName);

  void
  AddFileManager(FileManager* aFileManager);

  void
  InvalidateAllFileManagers();

  void
  InvalidateFileManagers(PersistenceType aPersistenceType,
                         const nsACString& aOrigin);

  void
  InvalidateFileManager(PersistenceType aPersistenceType,
                        const nsACString& aOrigin,
                        const nsAString& aDatabaseName);

  nsresult
  AsyncDeleteFile(FileManager* aFileManager,
                  int64_t aFileId);

  // Don't call this method in real code, it blocks the main thread!
  // It is intended to be used by mochitests to test correctness of the special
  // reference counting of stored blobs/files.
  nsresult
  BlockAndGetFileReferences(PersistenceType aPersistenceType,
                            const nsACString& aOrigin,
                            const nsAString& aDatabaseName,
                            int64_t aFileId,
                            int32_t* aRefCnt,
                            int32_t* aDBRefCnt,
                            int32_t* aSliceRefCnt,
                            bool* aResult);

  nsresult
  FlushPendingFileDeletions();

#ifdef ENABLE_INTL_API
  static const nsCString&
  GetLocale();
#endif

  static mozilla::Mutex&
  FileMutex()
  {
    IndexedDatabaseManager* mgr = Get();
    NS_ASSERTION(mgr, "Must have a manager here!");

    return mgr->mFileMutex;
  }

  static nsresult
  CommonPostHandleEvent(EventChainPostVisitor& aVisitor, IDBFactory* aFactory);

  static bool
  ResolveSandboxBinding(JSContext* aCx);

  static bool
  DefineIndexedDB(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

private:
  IndexedDatabaseManager();
  ~IndexedDatabaseManager();

  nsresult
  Init();

  void
  Destroy();

  static void
  LoggingModePrefChangedCallback(const char* aPrefName, void* aClosure);

  nsCOMPtr<nsIEventTarget> mBackgroundThread;

  nsCOMPtr<nsITimer> mDeleteTimer;

  // Maintains a list of all file managers per origin. This list isn't
  // protected by any mutex but it is only ever touched on the IO thread.
  nsClassHashtable<nsCStringHashKey, FileManagerInfo> mFileManagerInfos;

  nsClassHashtable<nsRefPtrHashKey<FileManager>,
                   nsTArray<int64_t>> mPendingDeleteInfos;

  // Lock protecting FileManager.mFileInfos.
  // It's s also used to atomically update FileInfo.mRefCnt, FileInfo.mDBRefCnt
  // and FileInfo.mSliceRefCnt
  mozilla::Mutex mFileMutex;

#ifdef ENABLE_INTL_API
  nsCString mLocale;
#endif

  indexedDB::BackgroundUtilsChild* mBackgroundActor;

  static bool sIsMainProcess;
  static bool sFullSynchronousMode;
  static LazyLogModule sLoggingModule;
  static Atomic<LoggingMode> sLoggingMode;
  static mozilla::Atomic<bool> sLowDiskSpaceMode;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddatabasemanager_h__
