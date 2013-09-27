/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDatabaseManager.h"

#include "nsIConsoleService.h"
#include "nsIDiskSpaceWatcher.h"
#include "nsIFile.h"
#include "nsIFileStorage.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"

#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CondVar.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/Utilities.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/Services.h"
#include "mozilla/storage.h"
#include "mozilla/Util.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsThreadUtils.h"

#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"

// Bindings for ResolveConstructors
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBDatabaseBinding.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/IDBIndexBinding.h"
#include "mozilla/dom/IDBObjectStoreBinding.h"
#include "mozilla/dom/IDBOpenDBRequestBinding.h"
#include "mozilla/dom/IDBRequestBinding.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "mozilla/dom/IDBVersionChangeEventBinding.h"

// The two possible values for the data argument when receiving the disk space
// observer notification.
#define LOW_DISK_SPACE_DATA_FULL "full"
#define LOW_DISK_SPACE_DATA_FREE "free"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::dom;
USING_QUOTA_NAMESPACE

BEGIN_INDEXEDDB_NAMESPACE

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
           !mTemporaryStorageFileManagers.IsEmpty();
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
};

END_INDEXEDDB_NAMESPACE

namespace {

mozilla::StaticRefPtr<IndexedDatabaseManager> gInstance;

mozilla::Atomic<int32_t> gInitialized(0);
mozilla::Atomic<int32_t> gClosed(0);

// See ResolveConstructors below.
struct ConstructorInfo {
  const char* const name;
  JS::Handle<JSObject*> (* const resolve)(JSContext*, JS::Handle<JSObject*>,
                                          bool);
  jsid id;
};

ConstructorInfo gConstructorInfo[] = {

#define BINDING_ENTRY(_name) \
  { #_name, _name##Binding::GetConstructorObject, JSID_VOID },

  BINDING_ENTRY(IDBFactory)
  BINDING_ENTRY(IDBDatabase)
  BINDING_ENTRY(IDBTransaction)
  BINDING_ENTRY(IDBObjectStore)
  BINDING_ENTRY(IDBIndex)
  BINDING_ENTRY(IDBCursor)
  BINDING_ENTRY(IDBRequest)
  BINDING_ENTRY(IDBOpenDBRequest)
  BINDING_ENTRY(IDBVersionChangeEvent)

#undef BINDING_ENTRY
};

class AsyncDeleteFileRunnable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteFileRunnable(FileManager* aFileManager, int64_t aFileId);

private:
  nsRefPtr<FileManager> mFileManager;
  int64_t mFileId;
};

class GetFileReferencesHelper MOZ_FINAL : public nsIRunnable
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

struct MOZ_STACK_CLASS InvalidateInfo
{
  InvalidateInfo(PersistenceType aPersistenceType, const nsACString& aPattern)
  : persistenceType(aPersistenceType), pattern(aPattern)
  { }

  PersistenceType persistenceType;
  const nsACString& pattern;
};

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
mozilla::Atomic<int32_t> IndexedDatabaseManager::sLowDiskSpaceMode(0);

// static
IndexedDatabaseManager*
IndexedDatabaseManager::GetOrCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsClosed()) {
    NS_ERROR("Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gInstance) {
    sIsMainProcess = XRE_GetProcessType() == GeckoProcessType_Default;

    if (sIsMainProcess) {
      // See if we're starting up in low disk space conditions.
      nsCOMPtr<nsIDiskSpaceWatcher> watcher =
        do_GetService(DISKSPACEWATCHER_CONTRACTID);
      if (watcher) {
        bool isDiskFull;
        if (NS_SUCCEEDED(watcher->GetIsDiskFull(&isDiskFull))) {
          sLowDiskSpaceMode = isDiskFull ? 1 : 0;
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

    if (gInitialized.exchange(1)) {
      NS_ERROR("Initialized more than once?!");
    }

    gInstance = instance;

    ClearOnShutdown(&gInstance);
  }

  return gInstance;
}

// static
IndexedDatabaseManager*
IndexedDatabaseManager::Get()
{
  // Does not return an owning reference.
  return gInstance;
}

// static
IndexedDatabaseManager*
IndexedDatabaseManager::FactoryCreate()
{
  // Returns a raw pointer that carries an owning reference! Lame, but the
  // singleton factory macros force this.
  IndexedDatabaseManager* mgr = GetOrCreate();
  NS_IF_ADDREF(mgr);
  return mgr;
}

nsresult
IndexedDatabaseManager::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Make sure that the quota manager is up.
  QuotaManager* qm = QuotaManager::GetOrCreate();
  NS_ENSURE_STATE(qm);

  // During Init() we can't yet call IsMainProcess(), just check sIsMainProcess
  // directly.
  if (sIsMainProcess) {
    // Must initialize the storage service on the main thread.
    nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
    NS_ENSURE_STATE(ss);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    NS_ENSURE_STATE(obs);

    nsresult rv =
      obs->AddObserver(this, DISKSPACEWATCHER_OBSERVER_TOPIC, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
IndexedDatabaseManager::Destroy()
{
  // Setting the closed flag prevents the service from being recreated.
  // Don't set it though if there's no real instance created.
  if (!!gInitialized && gClosed.exchange(1)) {
    NS_ERROR("Shutdown more than once?!");
  }

  delete this;
}

// static
nsresult
IndexedDatabaseManager::FireWindowOnError(nsPIDOMWindow* aOwner,
                                          nsEventChainPostVisitor& aVisitor)
{
  NS_ENSURE_TRUE(aVisitor.mDOMEvent, NS_ERROR_UNEXPECTED);
  if (!aOwner) {
    return NS_OK;
  }

  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  nsString type;
  nsresult rv = aVisitor.mDOMEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type.EqualsLiteral(ERROR_EVT_STR)) {
    return NS_OK;
  }

  nsCOMPtr<EventTarget> eventTarget =
    aVisitor.mDOMEvent->InternalDOMEvent()->GetTarget();

  IDBRequest* request = static_cast<IDBRequest*>(eventTarget.get());
  NS_ENSURE_TRUE(request, NS_ERROR_UNEXPECTED);

  ErrorResult ret;
  nsRefPtr<DOMError> error = request->GetError(ret);
  if (ret.Failed()) {
    return ret.ErrorCode();
  }

  nsString errorName;
  if (error) {
    error->GetName(errorName);
  }

  nsScriptErrorEvent event(true, NS_LOAD_ERROR);
  request->FillScriptErrorEvent(&event);
  NS_ABORT_IF_FALSE(event.fileName,
                    "FillScriptErrorEvent should give us a non-null string "
                    "for our error's fileName");

  event.errorMsg = errorName.get();

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aOwner));
  NS_ASSERTION(sgo, "How can this happen?!");

  nsEventStatus status = nsEventStatus_eIgnore;
  if (NS_FAILED(sgo->HandleScriptError(&event, &status))) {
    NS_WARNING("Failed to dispatch script error event");
    status = nsEventStatus_eIgnore;
  }

  bool preventDefaultCalled = status == nsEventStatus_eConsumeNoDefault;
  if (preventDefaultCalled) {
    return NS_OK;
  }

  // Log an error to the error console.
  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_FAILED(scriptError->InitWithWindowID(errorName,
                                              nsDependentString(event.fileName),
                                              EmptyString(), event.lineNr,
                                              0, 0,
                                              "IndexedDB",
                                              aOwner->WindowID()))) {
    NS_WARNING("Failed to init script error!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return consoleService->LogMessage(scriptError);
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
IndexedDatabaseManager::IsClosed()
{
  return !!gClosed;
}

#ifdef DEBUG
// static
bool
IndexedDatabaseManager::IsMainProcess()
{
  NS_ASSERTION(gInstance,
               "IsMainProcess() called before indexedDB has been initialized!");
  NS_ASSERTION((XRE_GetProcessType() == GeckoProcessType_Default) ==
               sIsMainProcess, "XRE_GetProcessType changed its tune!");
  return sIsMainProcess;
}

//static
bool
IndexedDatabaseManager::InLowDiskSpaceMode()
{
  NS_ASSERTION(gInstance,
               "InLowDiskSpaceMode() called before indexedDB has been "
               "initialized!");
  return !!sLowDiskSpaceMode;
}
#endif

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

// static
PLDHashOperator
IndexedDatabaseManager::InvalidateAndRemoveFileManagers(
                                             const nsACString& aKey,
                                             nsAutoPtr<FileManagerInfo>& aValue,
                                             void* aUserArg)
{
  AssertIsOnIOThread();
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  if (!aUserArg) {
    aValue->InvalidateAllFileManagers();
    return PL_DHASH_REMOVE;
  }

  InvalidateInfo* info = static_cast<InvalidateInfo*>(aUserArg);

  if (PatternMatchesOrigin(info->pattern, aKey)) {
    aValue->InvalidateAndRemoveFileManagers(info->persistenceType);

    if (!aValue->HasFileManagers()) {
      return PL_DHASH_REMOVE;
    }
  }

  return PL_DHASH_NEXT;
}

void
IndexedDatabaseManager::InvalidateAllFileManagers()
{
  AssertIsOnIOThread();

  mFileManagerInfos.Enumerate(InvalidateAndRemoveFileManagers, nullptr);
}

void
IndexedDatabaseManager::InvalidateFileManagers(
                                  PersistenceType aPersistenceType,
                                  const OriginOrPatternString& aOriginOrPattern)
{
  AssertIsOnIOThread();
  NS_ASSERTION(!aOriginOrPattern.IsEmpty(), "Empty pattern!");

  if (aOriginOrPattern.IsOrigin()) {
    FileManagerInfo* info;
    if (!mFileManagerInfos.Get(aOriginOrPattern, &info)) {
      return;
    }

    info->InvalidateAndRemoveFileManagers(aPersistenceType);

    if (!info->HasFileManagers()) {
      mFileManagerInfos.Remove(aOriginOrPattern);
    }
  }
  else {
    InvalidateInfo info(aPersistenceType, aOriginOrPattern);
    mFileManagerInfos.Enumerate(InvalidateAndRemoveFileManagers, &info);
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
  if (quotaManager->IsClearOriginPending(aFileManager->Origin())) {
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
  nsRefPtr<GetFileReferencesHelper> helper =
    new GetFileReferencesHelper(aPersistenceType, aOrigin, aDatabaseName,
                                aFileId);

  nsresult rv = helper->DispatchAndReturnFileReferences(aRefCnt, aDBRefCnt,
                                                        aSliceRefCnt, aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_ADDREF(IndexedDatabaseManager)
NS_IMPL_RELEASE_WITH_DESTROY(IndexedDatabaseManager, Destroy())
NS_IMPL_QUERY_INTERFACE2(IndexedDatabaseManager, nsIIndexedDatabaseManager,
                                                 nsIObserver)

NS_IMETHODIMP
IndexedDatabaseManager::InitWindowless(const jsval& aObj, JSContext* aCx)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG(!JSVAL_IS_PRIMITIVE(aObj));

  JS::Rooted<JSObject*> obj(aCx, JSVAL_TO_OBJECT(aObj));

  bool hasIndexedDB;
  if (!JS_HasProperty(aCx, obj, "indexedDB", &hasIndexedDB)) {
    return NS_ERROR_FAILURE;
  }

  if (hasIndexedDB) {
    NS_WARNING("Passed object already has an 'indexedDB' property!");
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForObject(aCx, obj));
  NS_ASSERTION(global, "What?! No global!");

  nsRefPtr<IDBFactory> factory;
  nsresult rv =
    IDBFactory::Create(aCx, global, nullptr, getter_AddRefs(factory));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(factory, "This should never fail for chrome!");

  JS::Rooted<JS::Value> indexedDBVal(aCx);
  rv = nsContentUtils::WrapNative(aCx, obj, factory, indexedDBVal.address());
  NS_ENSURE_SUCCESS(rv, rv);

  if (!JS_DefineProperty(aCx, obj, "indexedDB", indexedDBVal, nullptr,
                         nullptr, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> keyrangeObj(aCx,
    JS_NewObject(aCx, nullptr, nullptr, nullptr));
  NS_ENSURE_TRUE(keyrangeObj, NS_ERROR_OUT_OF_MEMORY);

  if (!IDBKeyRange::DefineConstructors(aCx, keyrangeObj)) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(aCx, obj, "IDBKeyRange", OBJECT_TO_JSVAL(keyrangeObj),
                         nullptr, nullptr, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::Observe(nsISupports* aSubject, const char* aTopic,
                                const PRUnichar* aData)
{
  NS_ASSERTION(IsMainProcess(), "Wrong process!");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, DISKSPACEWATCHER_OBSERVER_TOPIC)) {
    NS_ASSERTION(aData, "No data?!");

    const nsDependentString data(aData);

    if (data.EqualsLiteral(LOW_DISK_SPACE_DATA_FULL)) {
      sLowDiskSpaceMode = 1;
    }
    else if (data.EqualsLiteral(LOW_DISK_SPACE_DATA_FREE)) {
      sLowDiskSpaceMode = 0;
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

    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad storage type value!");
      return mPersistentStorageFileManagers;
  }
}

AsyncDeleteFileRunnable::AsyncDeleteFileRunnable(FileManager* aFileManager,
                                                 int64_t aFileId)
: mFileManager(aFileManager), mFileId(aFileId)
{
}

NS_IMPL_ISUPPORTS1(AsyncDeleteFileRunnable,
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

  if (mFileManager->Privilege() != Chrome) {
    rv = file->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  }

  rv = file->Remove(false);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  if (mFileManager->Privilege() != Chrome) {
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

NS_IMPL_ISUPPORTS1(GetFileReferencesHelper,
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

BEGIN_INDEXEDDB_NAMESPACE

bool
ResolveConstructors(JSContext* aCx, JS::HandleObject aObj, JS::HandleId aId,
                    JS::MutableHandleObject aObjp)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The first time this function is called we need to intern all the strings we
  // care about.
  if (JSID_IS_VOID(gConstructorInfo[0].id)) {
    for (uint32_t i = 0; i < mozilla::ArrayLength(gConstructorInfo); i++) {
      JS::RootedString str(aCx, JS_InternString(aCx, gConstructorInfo[i].name));
      if (!str) {
        NS_WARNING("Failed to intern string!");
        while (i) {
          gConstructorInfo[--i].id = JSID_VOID;
        }
        return false;
      }
      gConstructorInfo[i].id = INTERNED_STRING_TO_JSID(aCx, str);
    }
  }

  // Now resolve.
  for (uint32_t i = 0; i < mozilla::ArrayLength(gConstructorInfo); i++) {
    if (gConstructorInfo[i].id == aId) {
      JS::RootedObject constructor(aCx,
        gConstructorInfo[i].resolve(aCx, aObj, true));
      if (!constructor) {
        return false;
      }

      aObjp.set(aObj);
      return true;
    }
  }

  // Not resolved.
  aObjp.set(nullptr);
  return true;
}

END_INDEXEDDB_NAMESPACE
