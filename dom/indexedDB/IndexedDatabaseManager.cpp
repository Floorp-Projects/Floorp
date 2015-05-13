/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDatabaseManager.h"

#include "nsIConsoleService.h"
#include "nsIDiskSpaceWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"

#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CondVar.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/PBlobChild.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/Utilities.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsThreadUtils.h"
#include "prlog.h"

#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"
#include "ProfilerHelpers.h"
#include "WorkerScope.h"
#include "WorkerPrivate.h"

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

// The two possible values for the data argument when receiving the disk space
// observer notification.
#define LOW_DISK_SPACE_DATA_FULL "full"
#define LOW_DISK_SPACE_DATA_FREE "free"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::dom::workers;

class FileManagerInfo
{
public:
  already_AddRefed<FileManager>
  GetFileManager(PersistenceType aPersistenceType,
                 const nsAString& aName) const;

  void
  AddFileManager(FileManager* aFileManager);

  bool
  HasFileManagers() const
  {
    AssertIsOnIOThread();

    return !mPersistentStorageFileManagers.IsEmpty() ||
           !mTemporaryStorageFileManagers.IsEmpty() ||
           !mDefaultStorageFileManagers.IsEmpty();
  }

  void
  InvalidateAllFileManagers() const;

  void
  InvalidateAndRemoveFileManagers(PersistenceType aPersistenceType);

  void
  InvalidateAndRemoveFileManager(PersistenceType aPersistenceType,
                                 const nsAString& aName);

private:
  nsTArray<nsRefPtr<FileManager> >&
  GetArray(PersistenceType aPersistenceType);

  const nsTArray<nsRefPtr<FileManager> >&
  GetImmutableArray(PersistenceType aPersistenceType) const
  {
    return const_cast<FileManagerInfo*>(this)->GetArray(aPersistenceType);
  }

  nsTArray<nsRefPtr<FileManager> > mPersistentStorageFileManagers;
  nsTArray<nsRefPtr<FileManager> > mTemporaryStorageFileManagers;
  nsTArray<nsRefPtr<FileManager> > mDefaultStorageFileManagers;
};

namespace {

NS_DEFINE_IID(kIDBRequestIID, PRIVATE_IDBREQUEST_IID);

#define IDB_PREF_BRANCH_ROOT "dom.indexedDB."

const char kTestingPref[] = IDB_PREF_BRANCH_ROOT "testing";
const char kPrefExperimental[] = IDB_PREF_BRANCH_ROOT "experimental";

#define IDB_PREF_LOGGING_BRANCH_ROOT IDB_PREF_BRANCH_ROOT "logging."

const char kPrefLoggingEnabled[] = IDB_PREF_LOGGING_BRANCH_ROOT "enabled";
const char kPrefLoggingDetails[] = IDB_PREF_LOGGING_BRANCH_ROOT "details";

#if defined(DEBUG) || defined(MOZ_ENABLE_PROFILER_SPS)
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

class AsyncDeleteFileRunnable final : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteFileRunnable(FileManager* aFileManager, int64_t aFileId);

private:
  ~AsyncDeleteFileRunnable() {}

  nsRefPtr<FileManager> mFileManager;
  int64_t mFileId;
};

class GetFileReferencesHelper final : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  GetFileReferencesHelper(PersistenceType aPersistenceType,
                          const nsACString& aOrigin,
                          const nsAString& aDatabaseName,
                          int64_t aFileId)
  : mPersistenceType(aPersistenceType),
    mOrigin(aOrigin),
    mDatabaseName(aDatabaseName),
    mFileId(aFileId),
    mMutex(IndexedDatabaseManager::FileMutex()),
    mCondVar(mMutex, "GetFileReferencesHelper::mCondVar"),
    mMemRefCnt(-1),
    mDBRefCnt(-1),
    mSliceRefCnt(-1),
    mResult(false),
    mWaiting(true)
  { }

  nsresult
  DispatchAndReturnFileReferences(int32_t* aMemRefCnt,
                                  int32_t* aDBRefCnt,
                                  int32_t* aSliceRefCnt,
                                  bool* aResult);

private:
  ~GetFileReferencesHelper() {}

  PersistenceType mPersistenceType;
  nsCString mOrigin;
  nsString mDatabaseName;
  int64_t mFileId;

  mozilla::Mutex& mMutex;
  mozilla::CondVar mCondVar;
  int32_t mMemRefCnt;
  int32_t mDBRefCnt;
  int32_t mSliceRefCnt;
  bool mResult;
  bool mWaiting;
};

void
AtomicBoolPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aClosure);

  *static_cast<Atomic<bool>*>(aClosure) = Preferences::GetBool(aPrefName);
}

} // anonymous namespace

IndexedDatabaseManager::IndexedDatabaseManager()
: mFileMutex("IndexedDatabaseManager.mFileMutex")
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IndexedDatabaseManager::~IndexedDatabaseManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

bool IndexedDatabaseManager::sIsMainProcess = false;
bool IndexedDatabaseManager::sFullSynchronousMode = false;

PRLogModuleInfo* IndexedDatabaseManager::sLoggingModule;

Atomic<IndexedDatabaseManager::LoggingMode>
  IndexedDatabaseManager::sLoggingMode(
    IndexedDatabaseManager::Logging_Disabled);

mozilla::Atomic<bool> IndexedDatabaseManager::sLowDiskSpaceMode(false);

// static
IndexedDatabaseManager*
IndexedDatabaseManager::GetOrCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsClosed()) {
    NS_ERROR("Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gDBManager) {
    sIsMainProcess = XRE_GetProcessType() == GeckoProcessType_Default;

    if (!sLoggingModule) {
      sLoggingModule = PR_NewLogModule("IndexedDB");
    }

    if (sIsMainProcess && Preferences::GetBool("disk_space_watcher.enabled", false)) {
      // See if we're starting up in low disk space conditions.
      nsCOMPtr<nsIDiskSpaceWatcher> watcher =
        do_GetService(DISKSPACEWATCHER_CONTRACTID);
      if (watcher) {
        bool isDiskFull;
        if (NS_SUCCEEDED(watcher->GetIsDiskFull(&isDiskFull))) {
          sLowDiskSpaceMode = isDiskFull;
        }
        else {
          NS_WARNING("GetIsDiskFull failed!");
        }
      }
      else {
        NS_WARNING("No disk space watcher component available!");
      }
    }

    nsRefPtr<IndexedDatabaseManager> instance(new IndexedDatabaseManager());

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
IndexedDatabaseManager*
IndexedDatabaseManager::Get()
{
  // Does not return an owning reference.
  return gDBManager;
}

nsresult
IndexedDatabaseManager::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // During Init() we can't yet call IsMainProcess(), just check sIsMainProcess
  // directly.
  if (sIsMainProcess) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    NS_ENSURE_STATE(obs);

    nsresult rv =
      obs->AddObserver(this, DISKSPACEWATCHER_OBSERVER_TOPIC, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kTestingPref,
                                       &gTestingMode);
  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kPrefExperimental,
                                       &gExperimentalFeaturesEnabled);

  // By default IndexedDB uses SQLite with PRAGMA synchronous = NORMAL. This
  // guarantees (unlike synchronous = OFF) atomicity and consistency, but not
  // necessarily durability in situations such as power loss. This preference
  // allows enabling PRAGMA synchronous = FULL on SQLite, which does guarantee
  // durability, but with an extra fsync() and the corresponding performance
  // hit.
  sFullSynchronousMode = Preferences::GetBool("dom.indexedDB.fullSynchronous");

  Preferences::RegisterCallback(LoggingModePrefChangedCallback,
                                kPrefLoggingDetails);
#ifdef MOZ_ENABLE_PROFILER_SPS
  Preferences::RegisterCallback(LoggingModePrefChangedCallback,
                                kPrefLoggingProfiler);
#endif
  Preferences::RegisterCallbackAndCall(LoggingModePrefChangedCallback,
                                       kPrefLoggingEnabled);

  return NS_OK;
}

void
IndexedDatabaseManager::Destroy()
{
  // Setting the closed flag prevents the service from being recreated.
  // Don't set it though if there's no real instance created.
  if (gInitialized && gClosed.exchange(true)) {
    NS_ERROR("Shutdown more than once?!");
  }

  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kTestingPref,
                                  &gTestingMode);
  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kPrefExperimental,
                                  &gExperimentalFeaturesEnabled);

  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingDetails);
#ifdef MOZ_ENABLE_PROFILER_SPS
  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingProfiler);
#endif
  Preferences::UnregisterCallback(LoggingModePrefChangedCallback,
                                  kPrefLoggingEnabled);

  delete this;
}

// static
nsresult
IndexedDatabaseManager::CommonPostHandleEvent(EventChainPostVisitor& aVisitor,
                                              IDBFactory* aFactory)
{
  MOZ_ASSERT(aVisitor.mDOMEvent);
  MOZ_ASSERT(aFactory);

  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  Event* internalEvent = aVisitor.mDOMEvent->InternalDOMEvent();
  MOZ_ASSERT(internalEvent);

  if (!internalEvent->IsTrusted()) {
    return NS_OK;
  }

  nsString type;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(internalEvent->GetType(type)));

  MOZ_ASSERT(nsDependentString(kErrorEventType).EqualsLiteral("error"));
  if (!type.EqualsLiteral("error")) {
    return NS_OK;
  }

  nsCOMPtr<EventTarget> eventTarget = internalEvent->GetTarget();
  MOZ_ASSERT(eventTarget);

  // Only mess with events that were originally targeted to an IDBRequest.
  nsRefPtr<IDBRequest> request;
  if (NS_FAILED(eventTarget->QueryInterface(kIDBRequestIID,
                                            getter_AddRefs(request))) ||
      !request) {
    return NS_OK;
  }

  nsRefPtr<DOMError> error = request->GetErrorAfterResult();

  nsString errorName;
  if (error) {
    error->GetName(errorName);
  }

  ThreadsafeAutoJSContext cx;
  RootedDictionary<ErrorEventInit> init(cx);
  request->GetCallerLocation(init.mFilename, &init.mLineno);

  init.mMessage = errorName;
  init.mCancelable = true;
  init.mBubbles = true;

  nsEventStatus status = nsEventStatus_eIgnore;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(eventTarget->GetOwnerGlobal());
    if (window) {
      nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window);
      MOZ_ASSERT(sgo);

      if (NS_WARN_IF(NS_FAILED(sgo->HandleScriptError(init, &status)))) {
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

    nsRefPtr<WorkerGlobalScope> globalScope = workerPrivate->GlobalScope();
    MOZ_ASSERT(globalScope);

    nsRefPtr<ErrorEvent> errorEvent =
      ErrorEvent::Constructor(globalScope,
                              nsDependentString(kErrorEventType),
                              init);
    MOZ_ASSERT(errorEvent);

    errorEvent->SetTrusted(true);

    auto* target = static_cast<EventTarget*>(globalScope.get());

    if (NS_WARN_IF(NS_FAILED(
      EventDispatcher::DispatchDOMEvent(target,
                                        /* aWidgetEvent */ nullptr,
                                        errorEvent,
                                        /* aPresContext */ nullptr,
                                        &status)))) {
      status = nsEventStatus_eIgnore;
    }
  }

  if (status == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  nsAutoCString category;
  if (aFactory->IsChrome()) {
    category.AssignLiteral("chrome ");
  } else {
    category.AssignLiteral("content ");
  }
  category.AppendLiteral("javascript");

  // Log the error to the error console.
  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  MOZ_ASSERT(consoleService);

  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  MOZ_ASSERT(consoleService);

  if (uint64_t innerWindowID = aFactory->InnerWindowID()) {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      scriptError->InitWithWindowID(errorName,
                                    init.mFilename,
                                    /* aSourceLine */ EmptyString(),
                                    init.mLineno,
                                    /* aColumnNumber */ 0,
                                    nsIScriptError::errorFlag,
                                    category,
                                    innerWindowID)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      scriptError->Init(errorName,
                        init.mFilename,
                        /* aSourceLine */ EmptyString(),
                        init.mLineno,
                        /* aColumnNumber */ 0,
                        nsIScriptError::errorFlag,
                        category.get())));
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(consoleService->LogMessage(scriptError)));

  return NS_OK;
}

// static
bool
IndexedDatabaseManager::TabContextMayAccessOrigin(const TabContext& aContext,
                                                  const nsACString& aOrigin)
{
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");

  // If aContext is for a browser element, it's allowed only to access other
  // browser elements.  But if aContext is not for a browser element, it may
  // access both browser and non-browser elements.
  nsAutoCString pattern;
  QuotaManager::GetOriginPatternStringMaybeIgnoreBrowser(
                                                aContext.OwnOrContainingAppId(),
                                                aContext.IsBrowserElement(),
                                                pattern);

  return PatternMatchesOrigin(pattern, aOrigin);
}

// static
bool
IndexedDatabaseManager::DefineIndexedDB(JSContext* aCx,
                                        JS::Handle<JSObject*> aGlobal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome(), "Only for chrome!");
  MOZ_ASSERT(js::GetObjectClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL,
             "Passed object is not a global object!");

  // We need to ensure that the manager has been created already here so that we
  // load preferences that may control which properties are exposed.
  if (NS_WARN_IF(!GetOrCreate())) {
    return false;
  }

  if (!IDBCursorBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBCursorWithValueBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBDatabaseBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBFactoryBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBIndexBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBKeyRangeBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBMutableFileBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBObjectStoreBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBOpenDBRequestBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBRequestBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBTransactionBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBVersionChangeEventBinding::GetConstructorObject(aCx, aGlobal))
  {
    return false;
  }

  nsRefPtr<IDBFactory> factory;
  if (NS_FAILED(IDBFactory::CreateForChromeJS(aCx,
                                              aGlobal,
                                              getter_AddRefs(factory)))) {
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
bool
IndexedDatabaseManager::IsClosed()
{
  return gClosed;
}

#ifdef DEBUG
// static
bool
IndexedDatabaseManager::IsMainProcess()
{
  NS_ASSERTION(gDBManager,
               "IsMainProcess() called before indexedDB has been initialized!");
  NS_ASSERTION((XRE_GetProcessType() == GeckoProcessType_Default) ==
               sIsMainProcess, "XRE_GetProcessType changed its tune!");
  return sIsMainProcess;
}

//static
bool
IndexedDatabaseManager::InLowDiskSpaceMode()
{
  NS_ASSERTION(gDBManager,
               "InLowDiskSpaceMode() called before indexedDB has been "
               "initialized!");
  return sLowDiskSpaceMode;
}

// static
IndexedDatabaseManager::LoggingMode
IndexedDatabaseManager::GetLoggingMode()
{
  MOZ_ASSERT(gDBManager,
             "GetLoggingMode called before IndexedDatabaseManager has been "
             "initialized!");

  return sLoggingMode;
}

// static
PRLogModuleInfo*
IndexedDatabaseManager::GetLoggingModule()
{
  MOZ_ASSERT(gDBManager,
             "GetLoggingModule called before IndexedDatabaseManager has been "
             "initialized!");

  return sLoggingModule;
}

#endif // DEBUG

// static
bool
IndexedDatabaseManager::InTestingMode()
{
  MOZ_ASSERT(gDBManager,
             "InTestingMode() called before indexedDB has been initialized!");

  return gTestingMode;
}

// static
bool
IndexedDatabaseManager::FullSynchronous()
{
  MOZ_ASSERT(gDBManager,
             "FullSynchronous() called before indexedDB has been initialized!");

  return sFullSynchronousMode;
}

// static
bool
IndexedDatabaseManager::ExperimentalFeaturesEnabled()
{
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

already_AddRefed<FileManager>
IndexedDatabaseManager::GetFileManager(PersistenceType aPersistenceType,
                                       const nsACString& aOrigin,
                                       const nsAString& aDatabaseName)
{
  AssertIsOnIOThread();

  FileManagerInfo* info;
  if (!mFileManagerInfos.Get(aOrigin, &info)) {
    return nullptr;
  }

  nsRefPtr<FileManager> fileManager =
    info->GetFileManager(aPersistenceType, aDatabaseName);

  return fileManager.forget();
}

void
IndexedDatabaseManager::AddFileManager(FileManager* aFileManager)
{
  AssertIsOnIOThread();
  NS_ASSERTION(aFileManager, "Null file manager!");

  FileManagerInfo* info;
  if (!mFileManagerInfos.Get(aFileManager->Origin(), &info)) {
    info = new FileManagerInfo();
    mFileManagerInfos.Put(aFileManager->Origin(), info);
  }

  info->AddFileManager(aFileManager);
}

void
IndexedDatabaseManager::InvalidateAllFileManagers()
{
  AssertIsOnIOThread();

  class MOZ_STACK_CLASS Helper final
  {
  public:
    static PLDHashOperator
    Enumerate(const nsACString& aKey,
              FileManagerInfo* aValue,
              void* aUserArg)
    {
      AssertIsOnIOThread();
      MOZ_ASSERT(!aKey.IsEmpty());
      MOZ_ASSERT(aValue);

      aValue->InvalidateAllFileManagers();
      return PL_DHASH_NEXT;
    }
  };

  mFileManagerInfos.EnumerateRead(Helper::Enumerate, nullptr);
  mFileManagerInfos.Clear();
}

void
IndexedDatabaseManager::InvalidateFileManagers(
                                  PersistenceType aPersistenceType,
                                  const nsACString& aOrigin)
{
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

void
IndexedDatabaseManager::InvalidateFileManager(PersistenceType aPersistenceType,
                                              const nsACString& aOrigin,
                                              const nsAString& aDatabaseName)
{
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

nsresult
IndexedDatabaseManager::AsyncDeleteFile(FileManager* aFileManager,
                                        int64_t aFileId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aFileManager);

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  // See if we're currently clearing the storages for this origin. If so then
  // we pretend that we've already deleted everything.
  if (quotaManager->IsClearOriginPending(
                             aFileManager->Origin(),
                             Nullable<PersistenceType>(aFileManager->Type()))) {
    return NS_OK;
  }

  nsRefPtr<AsyncDeleteFileRunnable> runnable =
    new AsyncDeleteFileRunnable(aFileManager, aFileId);

  nsresult rv =
    quotaManager->IOThread()->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
IndexedDatabaseManager::BlockAndGetFileReferences(
                                               PersistenceType aPersistenceType,
                                               const nsACString& aOrigin,
                                               const nsAString& aDatabaseName,
                                               int64_t aFileId,
                                               int32_t* aRefCnt,
                                               int32_t* aDBRefCnt,
                                               int32_t* aSliceRefCnt,
                                               bool* aResult)
{
  if (NS_WARN_IF(!InTestingMode())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (IsMainProcess()) {
    nsRefPtr<GetFileReferencesHelper> helper =
      new GetFileReferencesHelper(aPersistenceType, aOrigin, aDatabaseName,
                                  aFileId);

    nsresult rv =
      helper->DispatchAndReturnFileReferences(aRefCnt, aDBRefCnt,
                                              aSliceRefCnt, aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    ContentChild* contentChild = ContentChild::GetSingleton();
    if (NS_WARN_IF(!contentChild)) {
      return NS_ERROR_FAILURE;
    }

    if (!contentChild->SendGetFileReferences(aPersistenceType,
                                             nsCString(aOrigin),
                                             nsString(aDatabaseName),
                                             aFileId,
                                             aRefCnt,
                                             aDBRefCnt,
                                             aSliceRefCnt,
                                             aResult)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

// static
void
IndexedDatabaseManager::LoggingModePrefChangedCallback(
                                                    const char* /* aPrefName */,
                                                    void* /* aClosure */)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!Preferences::GetBool(kPrefLoggingEnabled)) {
    sLoggingMode = Logging_Disabled;
    return;
  }

  bool useProfiler = 
#if defined(DEBUG) || defined(MOZ_ENABLE_PROFILER_SPS)
    Preferences::GetBool(kPrefLoggingProfiler);
#if !defined(MOZ_ENABLE_PROFILER_SPS)
  if (useProfiler) {
    NS_WARNING("IndexedDB cannot create profiler marks because this build does "
               "not have profiler extensions enabled!");
    useProfiler = false;
  }
#endif
#else
    false;
#endif

  const bool logDetails = Preferences::GetBool(kPrefLoggingDetails);

  if (useProfiler) {
    sLoggingMode = logDetails ?
                   Logging_DetailedProfilerMarks :
                   Logging_ConciseProfilerMarks;
  } else {
    sLoggingMode = logDetails ? Logging_Detailed : Logging_Concise;
  }
}

NS_IMPL_ADDREF(IndexedDatabaseManager)
NS_IMPL_RELEASE_WITH_DESTROY(IndexedDatabaseManager, Destroy())
NS_IMPL_QUERY_INTERFACE(IndexedDatabaseManager, nsIObserver)

NS_IMETHODIMP
IndexedDatabaseManager::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData)
{
  NS_ASSERTION(IsMainProcess(), "Wrong process!");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, DISKSPACEWATCHER_OBSERVER_TOPIC)) {
    NS_ASSERTION(aData, "No data?!");

    const nsDependentString data(aData);

    if (data.EqualsLiteral(LOW_DISK_SPACE_DATA_FULL)) {
      sLowDiskSpaceMode = true;
    }
    else if (data.EqualsLiteral(LOW_DISK_SPACE_DATA_FREE)) {
      sLowDiskSpaceMode = false;
    }
    else {
      NS_NOTREACHED("Unknown data value!");
    }

    return NS_OK;
  }

   NS_NOTREACHED("Unknown topic!");
   return NS_ERROR_UNEXPECTED;
 }

already_AddRefed<FileManager>
FileManagerInfo::GetFileManager(PersistenceType aPersistenceType,
                                const nsAString& aName) const
{
  AssertIsOnIOThread();

  const nsTArray<nsRefPtr<FileManager> >& managers =
    GetImmutableArray(aPersistenceType);

  for (uint32_t i = 0; i < managers.Length(); i++) {
    const nsRefPtr<FileManager>& fileManager = managers[i];

    if (fileManager->DatabaseName() == aName) {
      nsRefPtr<FileManager> result = fileManager;
      return result.forget();
    }
  }

  return nullptr;
}

void
FileManagerInfo::AddFileManager(FileManager* aFileManager)
{
  AssertIsOnIOThread();

  nsTArray<nsRefPtr<FileManager> >& managers = GetArray(aFileManager->Type());

  NS_ASSERTION(!managers.Contains(aFileManager), "Adding more than once?!");

  managers.AppendElement(aFileManager);
}

void
FileManagerInfo::InvalidateAllFileManagers() const
{
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

void
FileManagerInfo::InvalidateAndRemoveFileManagers(
                                               PersistenceType aPersistenceType)
{
  AssertIsOnIOThread();

  nsTArray<nsRefPtr<FileManager > >& managers = GetArray(aPersistenceType);

  for (uint32_t i = 0; i < managers.Length(); i++) {
    managers[i]->Invalidate();
  }

  managers.Clear();
}

void
FileManagerInfo::InvalidateAndRemoveFileManager(
                                               PersistenceType aPersistenceType,
                                               const nsAString& aName)
{
  AssertIsOnIOThread();

  nsTArray<nsRefPtr<FileManager > >& managers = GetArray(aPersistenceType);

  for (uint32_t i = 0; i < managers.Length(); i++) {
    nsRefPtr<FileManager>& fileManager = managers[i];
    if (fileManager->DatabaseName() == aName) {
      fileManager->Invalidate();
      managers.RemoveElementAt(i);
      return;
    }
  }
}

nsTArray<nsRefPtr<FileManager> >&
FileManagerInfo::GetArray(PersistenceType aPersistenceType)
{
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

AsyncDeleteFileRunnable::AsyncDeleteFileRunnable(FileManager* aFileManager,
                                                 int64_t aFileId)
: mFileManager(aFileManager), mFileId(aFileId)
{
}

NS_IMPL_ISUPPORTS(AsyncDeleteFileRunnable,
                  nsIRunnable)

NS_IMETHODIMP
AsyncDeleteFileRunnable::Run()
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> directory = mFileManager->GetDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFile> file = mFileManager->GetFileForId(directory, mFileId);
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

  directory = mFileManager->GetJournalDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

  file = mFileManager->GetFileForId(directory, mFileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  rv = file->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
GetFileReferencesHelper::DispatchAndReturnFileReferences(int32_t* aMemRefCnt,
                                                         int32_t* aDBRefCnt,
                                                         int32_t* aSliceRefCnt,
                                                         bool* aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  nsresult rv =
    quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

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

NS_IMPL_ISUPPORTS(GetFileReferencesHelper,
                  nsIRunnable)

NS_IMETHODIMP
GetFileReferencesHelper::Run()
{
  AssertIsOnIOThread();

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never fail!");

  nsRefPtr<FileManager> fileManager =
    mgr->GetFileManager(mPersistenceType, mOrigin, mDatabaseName);

  if (fileManager) {
    nsRefPtr<FileInfo> fileInfo = fileManager->GetFileInfo(mFileId);

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
  NS_ASSERTION(mWaiting, "Huh?!");

  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
