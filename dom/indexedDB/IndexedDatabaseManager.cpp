/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDatabaseManager.h"

#include "chrome/common/ipc_channel.h"  // for IPC::Channel::kMaximumMessageSize
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"

#include "jsapi.h"
#include "js/Object.h"              // JS::GetClass
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/intl/LocaleCanonicalizer.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "mozilla/Logging.h"

#include "ActorsChild.h"
#include "DatabaseFileManager.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"
#include "IndexedDBCommon.h"
#include "ProfilerHelpers.h"
#include "ScriptErrorHelper.h"
#include "nsCharSeparatedTokenizer.h"

// Bindings for ResolveConstructors
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBDatabaseBinding.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/IDBIndexBinding.h"
#include "mozilla/dom/IDBKeyRangeBinding.h"
#include "mozilla/dom/IDBObjectStoreBinding.h"
#include "mozilla/dom/IDBOpenDBRequestBinding.h"
#include "mozilla/dom/IDBRequestBinding.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "mozilla/dom/IDBVersionChangeEventBinding.h"

#define IDB_STR "indexedDB"

namespace mozilla::dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

class FileManagerInfo {
 public:
  [[nodiscard]] SafeRefPtr<DatabaseFileManager> GetFileManager(
      PersistenceType aPersistenceType, const nsAString& aName) const;

  void AddFileManager(SafeRefPtr<DatabaseFileManager> aFileManager);

  bool HasFileManagers() const {
    AssertIsOnIOThread();

    return !mPersistentStorageFileManagers.IsEmpty() ||
           !mTemporaryStorageFileManagers.IsEmpty() ||
           !mDefaultStorageFileManagers.IsEmpty() ||
           !mPrivateStorageFileManagers.IsEmpty();
  }

  void InvalidateAllFileManagers() const;

  void InvalidateAndRemoveFileManagers(PersistenceType aPersistenceType);

  void InvalidateAndRemoveFileManager(PersistenceType aPersistenceType,
                                      const nsAString& aName);

 private:
  nsTArray<SafeRefPtr<DatabaseFileManager> >& GetArray(
      PersistenceType aPersistenceType);

  const nsTArray<SafeRefPtr<DatabaseFileManager> >& GetImmutableArray(
      PersistenceType aPersistenceType) const {
    return const_cast<FileManagerInfo*>(this)->GetArray(aPersistenceType);
  }

  nsTArray<SafeRefPtr<DatabaseFileManager> > mPersistentStorageFileManagers;
  nsTArray<SafeRefPtr<DatabaseFileManager> > mTemporaryStorageFileManagers;
  nsTArray<SafeRefPtr<DatabaseFileManager> > mDefaultStorageFileManagers;
  nsTArray<SafeRefPtr<DatabaseFileManager> > mPrivateStorageFileManagers;
};

}  // namespace indexedDB

using namespace mozilla::dom::indexedDB;

namespace {

// The threshold we use for structured clone data storing.
// Anything smaller than the threshold is compressed and stored in the database.
// Anything larger is compressed and stored outside the database.
const int32_t kDefaultDataThresholdBytes = 1024 * 1024;  // 1MB

// The maximal size of a serialized object to be transfered through IPC.
const int32_t kDefaultMaxSerializedMsgSize = IPC::Channel::kMaximumMessageSize;

// The maximum number of records to preload (in addition to the one requested by
// the child).
//
// TODO: The current number was chosen for no particular reason. Telemetry
// should be added to determine whether this is a reasonable number for an
// overwhelming majority of cases.
const int32_t kDefaultMaxPreloadExtraRecords = 64;

#define IDB_PREF_BRANCH_ROOT "dom.indexedDB."

const char kDataThresholdPref[] = IDB_PREF_BRANCH_ROOT "dataThreshold";
const char kPrefMaxSerilizedMsgSize[] =
    IDB_PREF_BRANCH_ROOT "maxSerializedMsgSize";
const char kPrefMaxPreloadExtraRecords[] =
    IDB_PREF_BRANCH_ROOT "maxPreloadExtraRecords";

#define IDB_PREF_LOGGING_BRANCH_ROOT IDB_PREF_BRANCH_ROOT "logging."

const char kPrefLoggingEnabled[] = IDB_PREF_LOGGING_BRANCH_ROOT "enabled";
const char kPrefLoggingDetails[] = IDB_PREF_LOGGING_BRANCH_ROOT "details";

const char kPrefLoggingProfiler[] =
    IDB_PREF_LOGGING_BRANCH_ROOT "profiler-marks";

#undef IDB_PREF_LOGGING_BRANCH_ROOT
#undef IDB_PREF_BRANCH_ROOT

StaticMutex gDBManagerMutex;
StaticRefPtr<IndexedDatabaseManager> gDBManager MOZ_GUARDED_BY(gDBManagerMutex);
bool gInitialized MOZ_GUARDED_BY(gDBManagerMutex) = false;
bool gClosed MOZ_GUARDED_BY(gDBManagerMutex) = false;

Atomic<int32_t> gDataThresholdBytes(0);
Atomic<int32_t> gMaxSerializedMsgSize(0);
Atomic<int32_t> gMaxPreloadExtraRecords(0);

void DataThresholdPrefChangedCallback(const char* aPrefName, void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kDataThresholdPref));
  MOZ_ASSERT(!aClosure);

  int32_t dataThresholdBytes =
      Preferences::GetInt(aPrefName, kDefaultDataThresholdBytes);

  // The magic -1 is for use only by tests that depend on stable blob file id's.
  if (dataThresholdBytes == -1) {
    dataThresholdBytes = INT32_MAX;
  }

  gDataThresholdBytes = dataThresholdBytes;
}

void MaxSerializedMsgSizePrefChangeCallback(const char* aPrefName,
                                            void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kPrefMaxSerilizedMsgSize));
  MOZ_ASSERT(!aClosure);

  gMaxSerializedMsgSize =
      Preferences::GetInt(aPrefName, kDefaultMaxSerializedMsgSize);
  MOZ_ASSERT(gMaxSerializedMsgSize > 0);
}

void MaxPreloadExtraRecordsPrefChangeCallback(const char* aPrefName,
                                              void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kPrefMaxPreloadExtraRecords));
  MOZ_ASSERT(!aClosure);

  gMaxPreloadExtraRecords =
      Preferences::GetInt(aPrefName, kDefaultMaxPreloadExtraRecords);
  MOZ_ASSERT(gMaxPreloadExtraRecords >= 0);

  // TODO: We could also allow setting a negative value to preload all available
  // records, but this doesn't seem to be too useful in general, and it would
  // require adaptations in ActorsParent.cpp
}

auto DatabaseNameMatchPredicate(const nsAString* const aName) {
  MOZ_ASSERT(aName);
  return [aName](const auto& fileManager) {
    return fileManager->DatabaseName() == *aName;
  };
}

}  // namespace

IndexedDatabaseManager::IndexedDatabaseManager() : mBackgroundActor(nullptr) {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IndexedDatabaseManager::~IndexedDatabaseManager() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

bool IndexedDatabaseManager::sIsMainProcess = false;
bool IndexedDatabaseManager::sFullSynchronousMode = false;

mozilla::LazyLogModule IndexedDatabaseManager::sLoggingModule("IndexedDB");

Atomic<IndexedDatabaseManager::LoggingMode>
    IndexedDatabaseManager::sLoggingMode(
        IndexedDatabaseManager::Logging_Disabled);

// static
IndexedDatabaseManager* IndexedDatabaseManager::GetOrCreate() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  StaticMutexAutoLock lock(gDBManagerMutex);

  if (gClosed) {
    NS_ERROR("Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gDBManager) {
    sIsMainProcess = XRE_IsParentProcess();

    if (gInitialized) {
      NS_ERROR("Initialized more than once?!");
    }

    RefPtr<IndexedDatabaseManager> instance(new IndexedDatabaseManager());

    {
      StaticMutexAutoUnlock unlock(gDBManagerMutex);

      QM_TRY(MOZ_TO_RESULT(instance->Init()), nullptr);
    }

    gDBManager = instance;

    ClearOnShutdown(&gDBManager);

    gInitialized = true;
  }

  return gDBManager;
}

// static
IndexedDatabaseManager* IndexedDatabaseManager::Get() {
  StaticMutexAutoLock lock(gDBManagerMutex);

  // Does not return an owning reference.
  return gDBManager;
}

nsresult IndexedDatabaseManager::Init() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // By default IndexedDB uses SQLite with PRAGMA synchronous = NORMAL. This
  // guarantees (unlike synchronous = OFF) atomicity and consistency, but not
  // necessarily durability in situations such as power loss. This preference
  // allows enabling PRAGMA synchronous = FULL on SQLite, which does guarantee
  // durability, but with an extra fsync() and the corresponding performance
  // hit.
  sFullSynchronousMode = Preferences::GetBool("dom.indexedDB.fullSynchronous");

  Preferences::RegisterCallback(LoggingModePrefChangedCallback,
                                kPrefLoggingDetails);

  Preferences::RegisterCallback(LoggingModePrefChangedCallback,
                                kPrefLoggingProfiler);

  Preferences::RegisterCallbackAndCall(LoggingModePrefChangedCallback,
                                       kPrefLoggingEnabled);

  Preferences::RegisterCallbackAndCall(DataThresholdPrefChangedCallback,
                                       kDataThresholdPref);

  Preferences::RegisterCallbackAndCall(MaxSerializedMsgSizePrefChangeCallback,
                                       kPrefMaxSerilizedMsgSize);

  Preferences::RegisterCallbackAndCall(MaxPreloadExtraRecordsPrefChangeCallback,
                                       kPrefMaxPreloadExtraRecords);

  nsAutoCString acceptLang;
  Preferences::GetLocalizedCString("intl.accept_languages", acceptLang);

  // Split values on commas.
  for (const auto& lang :
       nsCCharSeparatedTokenizer(acceptLang, ',').ToRange()) {
    mozilla::intl::LocaleCanonicalizer::Vector asciiString{};
    auto result = mozilla::intl::LocaleCanonicalizer::CanonicalizeICULevel1(
        PromiseFlatCString(lang).get(), asciiString);
    if (result.isOk()) {
      mLocale.AssignASCII(asciiString);
      break;
    }
  }

  if (mLocale.IsEmpty()) {
    mLocale.AssignLiteral("en_US");
  }

  return NS_OK;
}

void IndexedDatabaseManager::Destroy() {
  {
    StaticMutexAutoLock lock(gDBManagerMutex);

    // Setting the closed flag prevents the service from being recreated.
    // Don't set it though if there's no real instance created.
    if (gInitialized && gClosed) {
      NS_ERROR("Shutdown more than once?!");
    }

    gClosed = true;
  }

  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingDetails);

  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingProfiler);

  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingEnabled);

  Preferences::UnregisterCallback(DataThresholdPrefChangedCallback,
                                  kDataThresholdPref);

  Preferences::UnregisterCallback(MaxSerializedMsgSizePrefChangeCallback,
                                  kPrefMaxSerilizedMsgSize);

  delete this;
}

// static
bool IndexedDatabaseManager::ResolveSandboxBinding(JSContext* aCx) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(
      JS::GetClass(JS::CurrentGlobalOrNull(aCx))->flags & JSCLASS_DOM_GLOBAL,
      "Passed object is not a global object!");

  // We need to ensure that the manager has been created already here so that we
  // load preferences that may control which properties are exposed.
  if (NS_WARN_IF(!GetOrCreate())) {
    return false;
  }

  if (!IDBCursor_Binding::GetConstructorObject(aCx) ||
      !IDBCursorWithValue_Binding::GetConstructorObject(aCx) ||
      !IDBDatabase_Binding::GetConstructorObject(aCx) ||
      !IDBFactory_Binding::GetConstructorObject(aCx) ||
      !IDBIndex_Binding::GetConstructorObject(aCx) ||
      !IDBKeyRange_Binding::GetConstructorObject(aCx) ||
      !IDBObjectStore_Binding::GetConstructorObject(aCx) ||
      !IDBOpenDBRequest_Binding::GetConstructorObject(aCx) ||
      !IDBRequest_Binding::GetConstructorObject(aCx) ||
      !IDBTransaction_Binding::GetConstructorObject(aCx) ||
      !IDBVersionChangeEvent_Binding::GetConstructorObject(aCx)) {
    return false;
  }

  return true;
}

// static
bool IndexedDatabaseManager::DefineIndexedDB(JSContext* aCx,
                                             JS::Handle<JSObject*> aGlobal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(JS::GetClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL,
             "Passed object is not a global object!");

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return false;
  }

  QM_TRY_UNWRAP(auto factory, IDBFactory::CreateForMainThreadJS(global), false);

  MOZ_ASSERT(factory, "This should never fail for chrome!");

  JS::Rooted<JS::Value> indexedDB(aCx);
  js::AssertSameCompartment(aCx, aGlobal);
  if (!GetOrCreateDOMReflector(aCx, factory, &indexedDB)) {
    return false;
  }

  return JS_DefineProperty(aCx, aGlobal, IDB_STR, indexedDB, JSPROP_ENUMERATE);
}

// static
bool IndexedDatabaseManager::IsClosed() {
  StaticMutexAutoLock lock(gDBManagerMutex);

  return gClosed;
}

#ifdef DEBUG
// static
bool IndexedDatabaseManager::IsMainProcess() {
  NS_ASSERTION(Get(),
               "IsMainProcess() called before indexedDB has been initialized!");
  NS_ASSERTION((XRE_IsParentProcess()) == sIsMainProcess,
               "XRE_GetProcessType changed its tune!");
  return sIsMainProcess;
}

// static
IndexedDatabaseManager::LoggingMode IndexedDatabaseManager::GetLoggingMode() {
  MOZ_ASSERT(Get(),
             "GetLoggingMode called before IndexedDatabaseManager has been "
             "initialized!");

  return sLoggingMode;
}

// static
mozilla::LogModule* IndexedDatabaseManager::GetLoggingModule() {
  MOZ_ASSERT(Get(),
             "GetLoggingModule called before IndexedDatabaseManager has been "
             "initialized!");

  return sLoggingModule;
}

#endif  // DEBUG

// static
bool IndexedDatabaseManager::FullSynchronous() {
  MOZ_ASSERT(Get(),
             "FullSynchronous() called before indexedDB has been initialized!");

  return sFullSynchronousMode;
}

// static
uint32_t IndexedDatabaseManager::DataThreshold() {
  MOZ_ASSERT(Get(),
             "DataThreshold() called before indexedDB has been initialized!");

  return gDataThresholdBytes;
}

// static
uint32_t IndexedDatabaseManager::MaxSerializedMsgSize() {
  MOZ_ASSERT(
      Get(),
      "MaxSerializedMsgSize() called before indexedDB has been initialized!");
  MOZ_ASSERT(gMaxSerializedMsgSize > 0);

  return gMaxSerializedMsgSize;
}

// static
int32_t IndexedDatabaseManager::MaxPreloadExtraRecords() {
  MOZ_ASSERT(Get(),
             "MaxPreloadExtraRecords() called before indexedDB has been "
             "initialized!");

  return gMaxPreloadExtraRecords;
}

void IndexedDatabaseManager::ClearBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  mBackgroundActor = nullptr;
}

SafeRefPtr<DatabaseFileManager> IndexedDatabaseManager::GetFileManager(
    PersistenceType aPersistenceType, const nsACString& aOrigin,
    const nsAString& aDatabaseName) {
  AssertIsOnIOThread();

  FileManagerInfo* info;
  if (!mFileManagerInfos.Get(aOrigin, &info)) {
    return nullptr;
  }

  return info->GetFileManager(aPersistenceType, aDatabaseName);
}

void IndexedDatabaseManager::AddFileManager(
    SafeRefPtr<DatabaseFileManager> aFileManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aFileManager);

  const auto& origin = aFileManager->Origin();
  mFileManagerInfos.GetOrInsertNew(origin)->AddFileManager(
      std::move(aFileManager));
}

void IndexedDatabaseManager::InvalidateAllFileManagers() {
  AssertIsOnIOThread();

  for (const auto& fileManagerInfo : mFileManagerInfos.Values()) {
    fileManagerInfo->InvalidateAllFileManagers();
  }

  mFileManagerInfos.Clear();
}

void IndexedDatabaseManager::InvalidateFileManagers(
    PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  for (auto iter = mFileManagerInfos.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->InvalidateAndRemoveFileManagers(aPersistenceType);

    if (!iter.Data()->HasFileManagers()) {
      iter.Remove();
    }
  }
}

void IndexedDatabaseManager::InvalidateFileManagers(
    PersistenceType aPersistenceType, const nsACString& aOrigin) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aOrigin.IsEmpty());

  FileManagerInfo* info;
  if (!mFileManagerInfos.Get(aOrigin, &info)) {
    return;
  }

  info->InvalidateAndRemoveFileManagers(aPersistenceType);

  if (!info->HasFileManagers()) {
    mFileManagerInfos.Remove(aOrigin);
  }
}

void IndexedDatabaseManager::InvalidateFileManager(
    PersistenceType aPersistenceType, const nsACString& aOrigin,
    const nsAString& aDatabaseName) {
  AssertIsOnIOThread();

  FileManagerInfo* info;
  if (!mFileManagerInfos.Get(aOrigin, &info)) {
    return;
  }

  info->InvalidateAndRemoveFileManager(aPersistenceType, aDatabaseName);

  if (!info->HasFileManagers()) {
    mFileManagerInfos.Remove(aOrigin);
  }
}

nsresult IndexedDatabaseManager::BlockAndGetFileReferences(
    PersistenceType aPersistenceType, const nsACString& aOrigin,
    const nsAString& aDatabaseName, int64_t aFileId, int32_t* aRefCnt,
    int32_t* aDBRefCnt, bool* aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!StaticPrefs::dom_indexedDB_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mBackgroundActor) {
    PBackgroundChild* bgActor = BackgroundChild::GetForCurrentThread();
    if (NS_WARN_IF(!bgActor)) {
      return NS_ERROR_FAILURE;
    }

    BackgroundUtilsChild* actor = new BackgroundUtilsChild(this);

    // We don't set event target for BackgroundUtilsChild because:
    // 1. BackgroundUtilsChild is a singleton.
    // 2. SendGetFileReferences is a sync operation to be returned asap if
    // unlabeled.
    // 3. The rest operations like DeleteMe/__delete__ only happens at shutdown.
    // Hence, we should keep it unlabeled.
    mBackgroundActor = static_cast<BackgroundUtilsChild*>(
        bgActor->SendPBackgroundIndexedDBUtilsConstructor(actor));
  }

  if (NS_WARN_IF(!mBackgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  if (!mBackgroundActor->SendGetFileReferences(
          aPersistenceType, nsCString(aOrigin), nsString(aDatabaseName),
          aFileId, aRefCnt, aDBRefCnt, aResult)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult IndexedDatabaseManager::FlushPendingFileDeletions() {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!StaticPrefs::dom_indexedDB_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  PBackgroundChild* bgActor = BackgroundChild::GetForCurrentThread();
  if (NS_WARN_IF(!bgActor)) {
    return NS_ERROR_FAILURE;
  }

  if (!bgActor->SendFlushPendingFileDeletions()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// static
void IndexedDatabaseManager::LoggingModePrefChangedCallback(
    const char* /* aPrefName */, void* /* aClosure */) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!Preferences::GetBool(kPrefLoggingEnabled)) {
    sLoggingMode = Logging_Disabled;
    return;
  }

  bool useProfiler = Preferences::GetBool(kPrefLoggingProfiler);
#if !defined(MOZ_GECKO_PROFILER)
  if (useProfiler) {
    NS_WARNING(
        "IndexedDB cannot create profiler marks because this build does "
        "not have profiler extensions enabled!");
    useProfiler = false;
  }
#endif

  const bool logDetails = Preferences::GetBool(kPrefLoggingDetails);

  if (useProfiler) {
    sLoggingMode = logDetails ? Logging_DetailedProfilerMarks
                              : Logging_ConciseProfilerMarks;
  } else {
    sLoggingMode = logDetails ? Logging_Detailed : Logging_Concise;
  }
}

// static
const nsCString& IndexedDatabaseManager::GetLocale() {
  IndexedDatabaseManager* idbManager = Get();
  MOZ_ASSERT(idbManager, "IDBManager is not ready!");

  return idbManager->mLocale;
}

SafeRefPtr<DatabaseFileManager> FileManagerInfo::GetFileManager(
    PersistenceType aPersistenceType, const nsAString& aName) const {
  AssertIsOnIOThread();

  const auto& managers = GetImmutableArray(aPersistenceType);

  const auto end = managers.cend();
  const auto foundIt =
      std::find_if(managers.cbegin(), end, DatabaseNameMatchPredicate(&aName));

  return foundIt != end ? foundIt->clonePtr() : nullptr;
}

void FileManagerInfo::AddFileManager(
    SafeRefPtr<DatabaseFileManager> aFileManager) {
  AssertIsOnIOThread();

  nsTArray<SafeRefPtr<DatabaseFileManager> >& managers =
      GetArray(aFileManager->Type());

  NS_ASSERTION(!managers.Contains(aFileManager), "Adding more than once?!");

  managers.AppendElement(std::move(aFileManager));
}

void FileManagerInfo::InvalidateAllFileManagers() const {
  AssertIsOnIOThread();

  uint32_t i;

  for (i = 0; i < mPersistentStorageFileManagers.Length(); i++) {
    mPersistentStorageFileManagers[i]->Invalidate();
  }

  for (i = 0; i < mTemporaryStorageFileManagers.Length(); i++) {
    mTemporaryStorageFileManagers[i]->Invalidate();
  }

  for (i = 0; i < mDefaultStorageFileManagers.Length(); i++) {
    mDefaultStorageFileManagers[i]->Invalidate();
  }

  for (i = 0; i < mPrivateStorageFileManagers.Length(); i++) {
    mPrivateStorageFileManagers[i]->Invalidate();
  }
}

void FileManagerInfo::InvalidateAndRemoveFileManagers(
    PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  nsTArray<SafeRefPtr<DatabaseFileManager> >& managers =
      GetArray(aPersistenceType);

  for (uint32_t i = 0; i < managers.Length(); i++) {
    managers[i]->Invalidate();
  }

  managers.Clear();
}

void FileManagerInfo::InvalidateAndRemoveFileManager(
    PersistenceType aPersistenceType, const nsAString& aName) {
  AssertIsOnIOThread();

  auto& managers = GetArray(aPersistenceType);
  const auto end = managers.cend();
  const auto foundIt =
      std::find_if(managers.cbegin(), end, DatabaseNameMatchPredicate(&aName));

  if (foundIt != end) {
    (*foundIt)->Invalidate();
    managers.RemoveElementAt(foundIt.GetIndex());
  }
}

nsTArray<SafeRefPtr<DatabaseFileManager> >& FileManagerInfo::GetArray(
    PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_PERSISTENT:
      return mPersistentStorageFileManagers;
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryStorageFileManagers;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultStorageFileManagers;
    case PERSISTENCE_TYPE_PRIVATE:
      return mPrivateStorageFileManagers;

    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad storage type value!");
  }
}

}  // namespace mozilla::dom
