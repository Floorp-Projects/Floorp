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
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "mozilla/Logging.h"

#include "FileManager.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"
#include "ProfilerHelpers.h"
#include "ScriptErrorHelper.h"
#include "nsCharSeparatedTokenizer.h"
#include "unicode/locid.h"

// Bindings for ResolveConstructors
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBDatabaseBinding.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/IDBIndexBinding.h"
#include "mozilla/dom/IDBKeyRangeBinding.h"
#include "mozilla/dom/IDBMutableFileBinding.h"
#include "mozilla/dom/IDBObjectStoreBinding.h"
#include "mozilla/dom/IDBOpenDBRequestBinding.h"
#include "mozilla/dom/IDBRequestBinding.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "mozilla/dom/IDBVersionChangeEventBinding.h"

#define IDB_STR "indexedDB"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

class FileManagerInfo {
 public:
  MOZ_MUST_USE SafeRefPtr<FileManager> GetFileManager(
      PersistenceType aPersistenceType, const nsAString& aName) const;

  void AddFileManager(SafeRefPtr<FileManager> aFileManager);

  bool HasFileManagers() const {
    AssertIsOnIOThread();

    return !mPersistentStorageFileManagers.IsEmpty() ||
           !mTemporaryStorageFileManagers.IsEmpty() ||
           !mDefaultStorageFileManagers.IsEmpty();
  }

  void InvalidateAllFileManagers() const;

  void InvalidateAndRemoveFileManagers(PersistenceType aPersistenceType);

  void InvalidateAndRemoveFileManager(PersistenceType aPersistenceType,
                                      const nsAString& aName);

 private:
  nsTArray<SafeRefPtr<FileManager> >& GetArray(
      PersistenceType aPersistenceType);

  const nsTArray<SafeRefPtr<FileManager> >& GetImmutableArray(
      PersistenceType aPersistenceType) const {
    return const_cast<FileManagerInfo*>(this)->GetArray(aPersistenceType);
  }

  nsTArray<SafeRefPtr<FileManager> > mPersistentStorageFileManagers;
  nsTArray<SafeRefPtr<FileManager> > mTemporaryStorageFileManagers;
  nsTArray<SafeRefPtr<FileManager> > mDefaultStorageFileManagers;
};

}  // namespace indexedDB

using namespace mozilla::dom::indexedDB;

namespace {

NS_DEFINE_IID(kIDBPrivateRequestIID, PRIVATE_IDBREQUEST_IID);

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

const char kTestingPref[] = IDB_PREF_BRANCH_ROOT "testing";
const char kPrefExperimental[] = IDB_PREF_BRANCH_ROOT "experimental";
const char kPrefFileHandle[] = "dom.fileHandle.enabled";
const char kDataThresholdPref[] = IDB_PREF_BRANCH_ROOT "dataThreshold";
const char kPrefMaxSerilizedMsgSize[] =
    IDB_PREF_BRANCH_ROOT "maxSerializedMsgSize";
const char kPrefErrorEventToSelfError[] =
    IDB_PREF_BRANCH_ROOT "errorEventToSelfError";
const char kPreprocessingPref[] = IDB_PREF_BRANCH_ROOT "preprocessing";
const char kPrefMaxPreloadExtraRecords[] =
    IDB_PREF_BRANCH_ROOT "maxPreloadExtraRecords";

#define IDB_PREF_LOGGING_BRANCH_ROOT IDB_PREF_BRANCH_ROOT "logging."

const char kPrefLoggingEnabled[] = IDB_PREF_LOGGING_BRANCH_ROOT "enabled";
const char kPrefLoggingDetails[] = IDB_PREF_LOGGING_BRANCH_ROOT "details";

#if defined(DEBUG) || defined(MOZ_GECKO_PROFILER)
const char kPrefLoggingProfiler[] =
    IDB_PREF_LOGGING_BRANCH_ROOT "profiler-marks";
#endif

#undef IDB_PREF_LOGGING_BRANCH_ROOT
#undef IDB_PREF_BRANCH_ROOT

StaticRefPtr<IndexedDatabaseManager> gDBManager;

Atomic<bool> gInitialized(false);
Atomic<bool> gClosed(false);
Atomic<bool> gTestingMode(false);
Atomic<bool> gExperimentalFeaturesEnabled(false);
Atomic<bool> gFileHandleEnabled(false);
Atomic<bool> gPrefErrorEventToSelfError(false);
Atomic<int32_t> gDataThresholdBytes(0);
Atomic<int32_t> gMaxSerializedMsgSize(0);
Atomic<bool> gPreprocessingEnabled(false);
Atomic<int32_t> gMaxPreloadExtraRecords(0);

void AtomicBoolPrefChangedCallback(const char* aPrefName, void* aBool) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBool);

  *static_cast<Atomic<bool>*>(aBool) = Preferences::GetBool(aPrefName);
}

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

  if (IsClosed()) {
    NS_ERROR("Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gDBManager) {
    sIsMainProcess = XRE_IsParentProcess();

    RefPtr<IndexedDatabaseManager> instance(new IndexedDatabaseManager());

    nsresult rv = instance->Init();
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (gInitialized.exchange(true)) {
      NS_ERROR("Initialized more than once?!");
    }

    gDBManager = instance;

    ClearOnShutdown(&gDBManager);
  }

  return gDBManager;
}

// static
IndexedDatabaseManager* IndexedDatabaseManager::Get() {
  // Does not return an owning reference.
  return gDBManager;
}

nsresult IndexedDatabaseManager::Init() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kTestingPref, &gTestingMode);
  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kPrefExperimental,
                                       &gExperimentalFeaturesEnabled);
  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kPrefFileHandle, &gFileHandleEnabled);
  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kPrefErrorEventToSelfError,
                                       &gPrefErrorEventToSelfError);

  // By default IndexedDB uses SQLite with PRAGMA synchronous = NORMAL. This
  // guarantees (unlike synchronous = OFF) atomicity and consistency, but not
  // necessarily durability in situations such as power loss. This preference
  // allows enabling PRAGMA synchronous = FULL on SQLite, which does guarantee
  // durability, but with an extra fsync() and the corresponding performance
  // hit.
  sFullSynchronousMode = Preferences::GetBool("dom.indexedDB.fullSynchronous");

  Preferences::RegisterCallback(LoggingModePrefChangedCallback,
                                kPrefLoggingDetails);
#ifdef MOZ_GECKO_PROFILER
  Preferences::RegisterCallback(LoggingModePrefChangedCallback,
                                kPrefLoggingProfiler);
#endif
  Preferences::RegisterCallbackAndCall(LoggingModePrefChangedCallback,
                                       kPrefLoggingEnabled);

  Preferences::RegisterCallbackAndCall(DataThresholdPrefChangedCallback,
                                       kDataThresholdPref);

  Preferences::RegisterCallbackAndCall(MaxSerializedMsgSizePrefChangeCallback,
                                       kPrefMaxSerilizedMsgSize);

  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kPreprocessingPref,
                                       &gPreprocessingEnabled);

  Preferences::RegisterCallbackAndCall(MaxPreloadExtraRecordsPrefChangeCallback,
                                       kPrefMaxPreloadExtraRecords);

  nsAutoCString acceptLang;
  Preferences::GetLocalizedCString("intl.accept_languages", acceptLang);

  // Split values on commas.
  nsCCharSeparatedTokenizer langTokenizer(acceptLang, ',');
  while (langTokenizer.hasMoreTokens()) {
    nsAutoCString lang(langTokenizer.nextToken());
    icu::Locale locale = icu::Locale::createCanonical(lang.get());
    if (!locale.isBogus()) {
      // icu::Locale::getBaseName is always ASCII as per BCP 47
      mLocale.AssignASCII(locale.getBaseName());
      break;
    }
  }

  if (mLocale.IsEmpty()) {
    mLocale.AssignLiteral("en_US");
  }

  return NS_OK;
}

void IndexedDatabaseManager::Destroy() {
  // Setting the closed flag prevents the service from being recreated.
  // Don't set it though if there's no real instance created.
  if (gInitialized && gClosed.exchange(true)) {
    NS_ERROR("Shutdown more than once?!");
  }

  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback, kTestingPref,
                                  &gTestingMode);
  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kPrefExperimental,
                                  &gExperimentalFeaturesEnabled);
  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kPrefFileHandle, &gFileHandleEnabled);
  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kPrefErrorEventToSelfError,
                                  &gPrefErrorEventToSelfError);

  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingDetails);
#ifdef MOZ_GECKO_PROFILER
  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingProfiler);
#endif
  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingEnabled);

  Preferences::UnregisterCallback(DataThresholdPrefChangedCallback,
                                  kDataThresholdPref);

  Preferences::UnregisterCallback(MaxSerializedMsgSizePrefChangeCallback,
                                  kPrefMaxSerilizedMsgSize);

  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kPreprocessingPref, &gPreprocessingEnabled);

  delete this;
}

// static
nsresult IndexedDatabaseManager::CommonPostHandleEvent(
    EventChainPostVisitor& aVisitor, IDBFactory* aFactory) {
  MOZ_ASSERT(aVisitor.mDOMEvent);
  MOZ_ASSERT(aFactory);

  if (!gPrefErrorEventToSelfError) {
    return NS_OK;
  }

  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  if (!aVisitor.mDOMEvent->IsTrusted()) {
    return NS_OK;
  }

  nsAutoString type;
  aVisitor.mDOMEvent->GetType(type);

  MOZ_ASSERT(nsDependentString(kErrorEventType).EqualsLiteral("error"));
  if (!type.EqualsLiteral("error")) {
    return NS_OK;
  }

  nsCOMPtr<EventTarget> eventTarget = aVisitor.mDOMEvent->GetTarget();
  MOZ_ASSERT(eventTarget);

  // Only mess with events that were originally targeted to an IDBRequest.
  RefPtr<IDBRequest> request;
  if (NS_FAILED(eventTarget->QueryInterface(kIDBPrivateRequestIID,
                                            getter_AddRefs(request))) ||
      !request) {
    return NS_OK;
  }

  RefPtr<DOMException> error = request->GetErrorAfterResult();

  nsString errorName;
  if (error) {
    error->GetName(errorName);
  }

  RootedDictionary<ErrorEventInit> init(RootingCx());
  request->GetCallerLocation(init.mFilename, &init.mLineno, &init.mColno);

  init.mMessage = errorName;
  init.mCancelable = true;
  init.mBubbles = true;

  nsEventStatus status = nsEventStatus_eIgnore;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIDOMWindow> window =
        do_QueryInterface(eventTarget->GetOwnerGlobal());
    if (window) {
      nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window);
      MOZ_ASSERT(sgo);

      if (NS_WARN_IF(!sgo->HandleScriptError(init, &status))) {
        status = nsEventStatus_eIgnore;
      }
    } else {
      // We don't fire error events at any global for non-window JS on the main
      // thread.
    }
  } else {
    // Not on the main thread, must be in a worker.
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    RefPtr<WorkerGlobalScope> globalScope = workerPrivate->GlobalScope();
    MOZ_ASSERT(globalScope);

    RefPtr<ErrorEvent> errorEvent = ErrorEvent::Constructor(
        globalScope, nsDependentString(kErrorEventType), init);
    MOZ_ASSERT(errorEvent);

    errorEvent->SetTrusted(true);

    auto* target = static_cast<EventTarget*>(globalScope.get());

    if (NS_WARN_IF(NS_FAILED(EventDispatcher::DispatchDOMEvent(
            target,
            /* aWidgetEvent */ nullptr, errorEvent,
            /* aPresContext */ nullptr, &status)))) {
      status = nsEventStatus_eIgnore;
    }
  }

  if (status == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  // Log the error to the error console.
  ScriptErrorHelper::Dump(errorName, init.mFilename, init.mLineno, init.mColno,
                          nsIScriptError::errorFlag, aFactory->IsChrome(),
                          aFactory->InnerWindowID());

  return NS_OK;
}

// static
bool IndexedDatabaseManager::ResolveSandboxBinding(JSContext* aCx) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(js::GetObjectClass(JS::CurrentGlobalOrNull(aCx))->flags &
                 JSCLASS_DOM_GLOBAL,
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
      !IDBLocaleAwareKeyRange_Binding::GetConstructorObject(aCx) ||
      !IDBMutableFile_Binding::GetConstructorObject(aCx) ||
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
  MOZ_ASSERT(js::GetObjectClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL,
             "Passed object is not a global object!");

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return false;
  }

  RefPtr<IDBFactory> factory;
  if (NS_FAILED(
          IDBFactory::CreateForMainThreadJS(global, getter_AddRefs(factory)))) {
    return false;
  }

  MOZ_ASSERT(factory, "This should never fail for chrome!");

  JS::Rooted<JS::Value> indexedDB(aCx);
  js::AssertSameCompartment(aCx, aGlobal);
  if (!GetOrCreateDOMReflector(aCx, factory, &indexedDB)) {
    return false;
  }

  return JS_DefineProperty(aCx, aGlobal, IDB_STR, indexedDB, JSPROP_ENUMERATE);
}

// static
bool IndexedDatabaseManager::IsClosed() { return gClosed; }

#ifdef DEBUG
// static
bool IndexedDatabaseManager::IsMainProcess() {
  NS_ASSERTION(gDBManager,
               "IsMainProcess() called before indexedDB has been initialized!");
  NS_ASSERTION((XRE_IsParentProcess()) == sIsMainProcess,
               "XRE_GetProcessType changed its tune!");
  return sIsMainProcess;
}

// static
IndexedDatabaseManager::LoggingMode IndexedDatabaseManager::GetLoggingMode() {
  MOZ_ASSERT(gDBManager,
             "GetLoggingMode called before IndexedDatabaseManager has been "
             "initialized!");

  return sLoggingMode;
}

// static
mozilla::LogModule* IndexedDatabaseManager::GetLoggingModule() {
  MOZ_ASSERT(gDBManager,
             "GetLoggingModule called before IndexedDatabaseManager has been "
             "initialized!");

  return sLoggingModule;
}

#endif  // DEBUG

// static
bool IndexedDatabaseManager::InTestingMode() {
  MOZ_ASSERT(gDBManager,
             "InTestingMode() called before indexedDB has been initialized!");

  return gTestingMode;
}

// static
bool IndexedDatabaseManager::FullSynchronous() {
  MOZ_ASSERT(gDBManager,
             "FullSynchronous() called before indexedDB has been initialized!");

  return sFullSynchronousMode;
}

// static
bool IndexedDatabaseManager::ExperimentalFeaturesEnabled() {
  if (NS_IsMainThread()) {
    if (NS_WARN_IF(!GetOrCreate())) {
      return false;
    }
  } else {
    MOZ_ASSERT(Get(),
               "ExperimentalFeaturesEnabled() called off the main thread "
               "before indexedDB has been initialized!");
  }

  return gExperimentalFeaturesEnabled;
}

// static
bool IndexedDatabaseManager::ExperimentalFeaturesEnabled(JSContext* aCx,
                                                         JSObject* aGlobal) {
  // If, in the child process, properties of the global object are enumerated
  // before the chrome registry (and thus the value of |intl.accept_languages|)
  // is ready, calling IndexedDatabaseManager::Init will permanently break
  // that preference. We can retrieve gExperimentalFeaturesEnabled without
  // actually going through IndexedDatabaseManager.
  // See Bug 1198093 comment 14 for detailed explanation.
  MOZ_DIAGNOSTIC_ASSERT(JS_IsGlobalObject(aGlobal));
  if (IsNonExposedGlobal(aCx, aGlobal, GlobalNames::BackstagePass)) {
    MOZ_ASSERT(NS_IsMainThread());
    static bool featureRetrieved = false;
    if (!featureRetrieved) {
      gExperimentalFeaturesEnabled = Preferences::GetBool(kPrefExperimental);
      featureRetrieved = true;
    }
    return gExperimentalFeaturesEnabled;
  }

  return ExperimentalFeaturesEnabled();
}

// static
bool IndexedDatabaseManager::IsFileHandleEnabled() {
  MOZ_ASSERT(gDBManager,
             "IsFileHandleEnabled() called before indexedDB has been "
             "initialized!");

  return gFileHandleEnabled;
}

// static
uint32_t IndexedDatabaseManager::DataThreshold() {
  MOZ_ASSERT(gDBManager,
             "DataThreshold() called before indexedDB has been initialized!");

  return gDataThresholdBytes;
}

// static
uint32_t IndexedDatabaseManager::MaxSerializedMsgSize() {
  MOZ_ASSERT(
      gDBManager,
      "MaxSerializedMsgSize() called before indexedDB has been initialized!");
  MOZ_ASSERT(gMaxSerializedMsgSize > 0);

  return gMaxSerializedMsgSize;
}

// static
bool IndexedDatabaseManager::PreprocessingEnabled() {
  MOZ_ASSERT(gDBManager,
             "PreprocessingEnabled() called before indexedDB has been "
             "initialized!");

  return gPreprocessingEnabled;
}

// static
int32_t IndexedDatabaseManager::MaxPreloadExtraRecords() {
  MOZ_ASSERT(gDBManager,
             "MaxPreloadExtraRecords() called before indexedDB has been "
             "initialized!");

  return gMaxPreloadExtraRecords;
}

void IndexedDatabaseManager::ClearBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  mBackgroundActor = nullptr;
}

SafeRefPtr<FileManager> IndexedDatabaseManager::GetFileManager(
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
    SafeRefPtr<FileManager> aFileManager) {
  AssertIsOnIOThread();
  NS_ASSERTION(aFileManager, "Null file manager!");

  FileManagerInfo* info;
  if (!mFileManagerInfos.Get(aFileManager->Origin(), &info)) {
    info = new FileManagerInfo();
    mFileManagerInfos.Put(aFileManager->Origin(), info);
  }

  info->AddFileManager(std::move(aFileManager));
}

void IndexedDatabaseManager::InvalidateAllFileManagers() {
  AssertIsOnIOThread();

  for (auto iter = mFileManagerInfos.ConstIter(); !iter.Done(); iter.Next()) {
    auto value = iter.UserData();
    MOZ_ASSERT(value);

    value->InvalidateAllFileManagers();
  }

  mFileManagerInfos.Clear();
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

  if (NS_WARN_IF(!InTestingMode())) {
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

  if (NS_WARN_IF(!InTestingMode())) {
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

  bool useProfiler =
#if defined(DEBUG) || defined(MOZ_GECKO_PROFILER)
      Preferences::GetBool(kPrefLoggingProfiler);
#  if !defined(MOZ_GECKO_PROFILER)
  if (useProfiler) {
    NS_WARNING(
        "IndexedDB cannot create profiler marks because this build does "
        "not have profiler extensions enabled!");
    useProfiler = false;
  }
#  endif
#else
      false;
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

SafeRefPtr<FileManager> FileManagerInfo::GetFileManager(
    PersistenceType aPersistenceType, const nsAString& aName) const {
  AssertIsOnIOThread();

  const auto& managers = GetImmutableArray(aPersistenceType);

  const auto end = managers.cend();
  const auto foundIt =
      std::find_if(managers.cbegin(), end, DatabaseNameMatchPredicate(&aName));

  return foundIt != end ? foundIt->clonePtr() : nullptr;
}

void FileManagerInfo::AddFileManager(SafeRefPtr<FileManager> aFileManager) {
  AssertIsOnIOThread();

  nsTArray<SafeRefPtr<FileManager> >& managers = GetArray(aFileManager->Type());

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
}

void FileManagerInfo::InvalidateAndRemoveFileManagers(
    PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  nsTArray<SafeRefPtr<FileManager> >& managers = GetArray(aPersistenceType);

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

nsTArray<SafeRefPtr<FileManager> >& FileManagerInfo::GetArray(
    PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_PERSISTENT:
      return mPersistentStorageFileManagers;
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryStorageFileManagers;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultStorageFileManagers;

    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad storage type value!");
  }
}

}  // namespace dom
}  // namespace mozilla
