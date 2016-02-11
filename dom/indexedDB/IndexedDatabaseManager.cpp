/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDatabaseManager.h"

#include "nsIConsoleService.h"
#include "nsIDiskSpaceWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIEventTarget.h"
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
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"

#include "FileInfo.h"
#include "FileManager.h"
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

#ifdef ENABLE_INTL_API
#include "nsCharSeparatedTokenizer.h"
#include "unicode/locid.h"
#endif

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
using namespace mozilla::ipc;

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
  nsTArray<RefPtr<FileManager> >&
  GetArray(PersistenceType aPersistenceType);

  const nsTArray<RefPtr<FileManager> >&
  GetImmutableArray(PersistenceType aPersistenceType) const
  {
    return const_cast<FileManagerInfo*>(this)->GetArray(aPersistenceType);
  }

  nsTArray<RefPtr<FileManager> > mPersistentStorageFileManagers;
  nsTArray<RefPtr<FileManager> > mTemporaryStorageFileManagers;
  nsTArray<RefPtr<FileManager> > mDefaultStorageFileManagers;
};

namespace {

NS_DEFINE_IID(kIDBRequestIID, PRIVATE_IDBREQUEST_IID);

const uint32_t kDeleteTimeoutMs = 1000;

#define IDB_PREF_BRANCH_ROOT "dom.indexedDB."

const char kTestingPref[] = IDB_PREF_BRANCH_ROOT "testing";
const char kPrefExperimental[] = IDB_PREF_BRANCH_ROOT "experimental";
const char kPrefFileHandle[] = "dom.fileHandle.enabled";

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
Atomic<bool> gFileHandleEnabled(false);

class DeleteFilesRunnable final
  : public nsIRunnable
  , public OpenDirectoryListener
{
  typedef mozilla::dom::quota::DirectoryLock DirectoryLock;

  enum State
  {
    // Just created on the main thread. Next step is State_DirectoryOpenPending.
    State_Initial,

    // Waiting for directory open allowed on the main thread. The next step is
    // State_DatabaseWorkOpen.
    State_DirectoryOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. The next step is
    // State_UnblockingOpen.
    State_DatabaseWorkOpen,

    // Notifying the QuotaManager that it can proceed to the next operation on
    // the main thread. Next step is State_Completed.
    State_UnblockingOpen,

    // All done.
    State_Completed
  };

  nsCOMPtr<nsIEventTarget> mBackgroundThread;

  RefPtr<FileManager> mFileManager;
  nsTArray<int64_t> mFileIds;

  RefPtr<DirectoryLock> mDirectoryLock;

  nsCOMPtr<nsIFile> mDirectory;
  nsCOMPtr<nsIFile> mJournalDirectory;

  State mState;

public:
  DeleteFilesRunnable(nsIEventTarget* aBackgroundThread,
                      FileManager* aFileManager,
                      nsTArray<int64_t>& aFileIds);

  void
  Dispatch();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  virtual void
  DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void
  DirectoryLockFailed() override;

private:
  ~DeleteFilesRunnable() {}

  nsresult
  Open();

  nsresult
  DeleteFile(int64_t aFileId);

  nsresult
  DoDatabaseWork();

  void
  Finish();

  void
  UnblockOpen();
};

void
AtomicBoolPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aClosure);

  *static_cast<Atomic<bool>*>(aClosure) = Preferences::GetBool(aPrefName);
}

} // namespace

IndexedDatabaseManager::IndexedDatabaseManager()
  : mFileMutex("IndexedDatabaseManager.mFileMutex")
  , mBackgroundActor(nullptr)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IndexedDatabaseManager::~IndexedDatabaseManager()
{
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
    sIsMainProcess = XRE_IsParentProcess();

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

    mDeleteTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_STATE(mDeleteTimer);
  }

  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kTestingPref,
                                       &gTestingMode);
  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kPrefExperimental,
                                       &gExperimentalFeaturesEnabled);
  Preferences::RegisterCallbackAndCall(AtomicBoolPrefChangedCallback,
                                       kPrefFileHandle,
                                       &gFileHandleEnabled);

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

#ifdef ENABLE_INTL_API
  const nsAdoptingCString& acceptLang =
    Preferences::GetLocalizedCString("intl.accept_languages");

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
    mLocale.AssignLiteral("en-US");
  }
#endif

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

  if (sIsMainProcess && mDeleteTimer) {
    if (NS_FAILED(mDeleteTimer->Cancel())) {
      NS_WARNING("Failed to cancel timer!");
    }

    mDeleteTimer = nullptr;
  }

  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kTestingPref,
                                  &gTestingMode);
  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kPrefExperimental,
                                  &gExperimentalFeaturesEnabled);
  Preferences::UnregisterCallback(AtomicBoolPrefChangedCallback,
                                  kPrefFileHandle,
                                  &gFileHandleEnabled);

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
  RefPtr<IDBRequest> request;
  if (NS_FAILED(eventTarget->QueryInterface(kIDBRequestIID,
                                            getter_AddRefs(request))) ||
      !request) {
    return NS_OK;
  }

  RefPtr<DOMError> error = request->GetErrorAfterResult();

  nsString errorName;
  if (error) {
    error->GetName(errorName);
  }

  ThreadsafeAutoJSContext cx;
  RootedDictionary<ErrorEventInit> init(cx);
  request->GetCallerLocation(init.mFilename, &init.mLineno, &init.mColno);

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

    RefPtr<WorkerGlobalScope> globalScope = workerPrivate->GlobalScope();
    MOZ_ASSERT(globalScope);

    RefPtr<ErrorEvent> errorEvent =
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
                                    init.mColno,
                                    nsIScriptError::errorFlag,
                                    category,
                                    innerWindowID)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      scriptError->Init(errorName,
                        init.mFilename,
                        /* aSourceLine */ EmptyString(),
                        init.mLineno,
                        init.mColno,
                        nsIScriptError::errorFlag,
                        category.get())));
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(consoleService->LogMessage(scriptError)));

  return NS_OK;
}

// static
bool
IndexedDatabaseManager::DefineIndexedDB(JSContext* aCx,
                                        JS::Handle<JSObject*> aGlobal)
{
  MOZ_ASSERT(NS_IsMainThread());
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
      !IDBLocaleAwareKeyRangeBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBMutableFileBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBObjectStoreBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBOpenDBRequestBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBRequestBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBTransactionBinding::GetConstructorObject(aCx, aGlobal) ||
      !IDBVersionChangeEventBinding::GetConstructorObject(aCx, aGlobal))
  {
    return false;
  }

  RefPtr<IDBFactory> factory;
  if (NS_FAILED(IDBFactory::CreateForMainThreadJS(aCx,
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
  NS_ASSERTION((XRE_IsParentProcess()) ==
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
mozilla::LogModule*
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

// static
bool
IndexedDatabaseManager::IsFileHandleEnabled()
{
  MOZ_ASSERT(gDBManager,
             "IsFileHandleEnabled() called before indexedDB has been "
             "initialized!");

  return gFileHandleEnabled;
}

void
IndexedDatabaseManager::ClearBackgroundActor()
{
  MOZ_ASSERT(NS_IsMainThread());

  mBackgroundActor = nullptr;
}

void
IndexedDatabaseManager::NoteBackgroundThread(nsIEventTarget* aBackgroundThread)
{
  MOZ_ASSERT(IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundThread);

  mBackgroundThread = aBackgroundThread;
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

  RefPtr<FileManager> fileManager =
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

  for (auto iter = mFileManagerInfos.ConstIter(); !iter.Done(); iter.Next()) {
    auto value = iter.Data();
    MOZ_ASSERT(value);

    value->InvalidateAllFileManagers();
  }

  mFileManagerInfos.Clear();
}

void
IndexedDatabaseManager::InvalidateFileManagers(PersistenceType aPersistenceType,
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
  MOZ_ASSERT(IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFileManager);
  MOZ_ASSERT(aFileId > 0);
  MOZ_ASSERT(mDeleteTimer);

  nsresult rv = mDeleteTimer->Cancel();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mDeleteTimer->InitWithCallback(this, kDeleteTimeoutMs,
                                      nsITimer::TYPE_ONE_SHOT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<int64_t>* array;
  if (!mPendingDeleteInfos.Get(aFileManager, &array)) {
    array = new nsTArray<int64_t>();
    mPendingDeleteInfos.Put(aFileManager, array);
  }

  array->AppendElement(aFileId);

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

    mBackgroundActor =
      static_cast<BackgroundUtilsChild*>(
        bgActor->SendPBackgroundIndexedDBUtilsConstructor(actor));
  }

  if (NS_WARN_IF(!mBackgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  if (!mBackgroundActor->SendGetFileReferences(aPersistenceType,
                                               nsCString(aOrigin),
                                               nsString(aDatabaseName),
                                               aFileId,
                                               aRefCnt,
                                               aDBRefCnt,
                                               aSliceRefCnt,
                                               aResult)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
IndexedDatabaseManager::FlushPendingFileDeletions()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!InTestingMode())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (IsMainProcess()) {
    nsresult rv = mDeleteTimer->Cancel();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = Notify(mDeleteTimer);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    PBackgroundChild* bgActor = BackgroundChild::GetForCurrentThread();
    if (NS_WARN_IF(!bgActor)) {
      return NS_ERROR_FAILURE;
    }

    if (!bgActor->SendFlushPendingFileDeletions()) {
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

#ifdef ENABLE_INTL_API
// static
const nsCString&
IndexedDatabaseManager::GetLocale()
{
  IndexedDatabaseManager* idbManager = Get();
  MOZ_ASSERT(idbManager, "IDBManager is not ready!");

  return idbManager->mLocale;
}
#endif

NS_IMPL_ADDREF(IndexedDatabaseManager)
NS_IMPL_RELEASE_WITH_DESTROY(IndexedDatabaseManager, Destroy())
NS_IMPL_QUERY_INTERFACE(IndexedDatabaseManager, nsIObserver, nsITimerCallback)

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

NS_IMETHODIMP
IndexedDatabaseManager::Notify(nsITimer* aTimer)
{
  MOZ_ASSERT(IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());

  for (auto iter = mPendingDeleteInfos.ConstIter(); !iter.Done(); iter.Next()) {
    auto key = iter.Key();
    auto value = iter.Data();
    MOZ_ASSERT(!value->IsEmpty());

    RefPtr<DeleteFilesRunnable> runnable =
      new DeleteFilesRunnable(mBackgroundThread, key, *value);

    MOZ_ASSERT(value->IsEmpty());

    runnable->Dispatch();
  }

  mPendingDeleteInfos.Clear();

  return NS_OK;
}

already_AddRefed<FileManager>
FileManagerInfo::GetFileManager(PersistenceType aPersistenceType,
                                const nsAString& aName) const
{
  AssertIsOnIOThread();

  const nsTArray<RefPtr<FileManager> >& managers =
    GetImmutableArray(aPersistenceType);

  for (uint32_t i = 0; i < managers.Length(); i++) {
    const RefPtr<FileManager>& fileManager = managers[i];

    if (fileManager->DatabaseName() == aName) {
      RefPtr<FileManager> result = fileManager;
      return result.forget();
    }
  }

  return nullptr;
}

void
FileManagerInfo::AddFileManager(FileManager* aFileManager)
{
  AssertIsOnIOThread();

  nsTArray<RefPtr<FileManager> >& managers = GetArray(aFileManager->Type());

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

  nsTArray<RefPtr<FileManager > >& managers = GetArray(aPersistenceType);

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

  nsTArray<RefPtr<FileManager > >& managers = GetArray(aPersistenceType);

  for (uint32_t i = 0; i < managers.Length(); i++) {
    RefPtr<FileManager>& fileManager = managers[i];
    if (fileManager->DatabaseName() == aName) {
      fileManager->Invalidate();
      managers.RemoveElementAt(i);
      return;
    }
  }
}

nsTArray<RefPtr<FileManager> >&
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

DeleteFilesRunnable::DeleteFilesRunnable(nsIEventTarget* aBackgroundThread,
                                         FileManager* aFileManager,
                                         nsTArray<int64_t>& aFileIds)
  : mBackgroundThread(aBackgroundThread)
  , mFileManager(aFileManager)
  , mState(State_Initial)
{
  mFileIds.SwapElements(aFileIds);
}

void
DeleteFilesRunnable::Dispatch()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_Initial);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    mBackgroundThread->Dispatch(this, NS_DISPATCH_NORMAL)));
}

NS_IMPL_ISUPPORTS(DeleteFilesRunnable, nsIRunnable)

NS_IMETHODIMP
DeleteFilesRunnable::Run()
{
  nsresult rv;

  switch (mState) {
    case State_Initial:
      rv = Open();
      break;

    case State_DatabaseWorkOpen:
      rv = DoDatabaseWork();
      break;

    case State_UnblockingOpen:
      UnblockOpen();
      return NS_OK;

    case State_DirectoryOpenPending:
    default:
      MOZ_CRASH("Should never get here!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State_UnblockingOpen) {
    Finish();
  }

  return NS_OK;
}

void
DeleteFilesRunnable::DirectoryLockAcquired(DirectoryLock* aLock)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread
  mState = State_DatabaseWorkOpen;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish();
    return;
  }
}

void
DeleteFilesRunnable::DirectoryLockFailed()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  Finish();
}

nsresult
DeleteFilesRunnable::Open()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_Initial);

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return NS_ERROR_FAILURE;
  }

  mState = State_DirectoryOpenPending;

  quotaManager->OpenDirectory(mFileManager->Type(),
                              mFileManager->Group(),
                              mFileManager->Origin(),
                              mFileManager->IsApp(),
                              Client::IDB,
                              /* aExclusive */ false,
                              this);

  return NS_OK;
}

nsresult
DeleteFilesRunnable::DeleteFile(int64_t aFileId)
{
  MOZ_ASSERT(mDirectory);
  MOZ_ASSERT(mJournalDirectory);

  nsCOMPtr<nsIFile> file = mFileManager->GetFileForId(mDirectory, aFileId);
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

  file = mFileManager->GetFileForId(mJournalDirectory, aFileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  rv = file->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
DeleteFilesRunnable::DoDatabaseWork()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DatabaseWorkOpen);

  if (!mFileManager->Invalidated()) {
    mDirectory = mFileManager->GetDirectory();
    if (NS_WARN_IF(!mDirectory)) {
      return NS_ERROR_FAILURE;
    }

    mJournalDirectory = mFileManager->GetJournalDirectory();
    if (NS_WARN_IF(!mJournalDirectory)) {
      return NS_ERROR_FAILURE;
    }

    for (int64_t fileId : mFileIds) {
      if (NS_FAILED(DeleteFile(fileId))) {
        NS_WARNING("Failed to delete file!");
      }
    }
  }

  Finish();

  return NS_OK;
}

void
DeleteFilesRunnable::Finish()
{
  // Must set mState before dispatching otherwise we will race with the main
  // thread.
  mState = State_UnblockingOpen;

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    mBackgroundThread->Dispatch(this, NS_DISPATCH_NORMAL)));
}

void
DeleteFilesRunnable::UnblockOpen()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_UnblockingOpen);

  mDirectoryLock = nullptr;

  mState = State_Completed;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
