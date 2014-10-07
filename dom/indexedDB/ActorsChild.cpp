/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

#include "BackgroundChildImpl.h"
#include "FileManager.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBMutableFile.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseFileChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBPermissionRequestChild.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIBFCacheEntry.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIEventTarget.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "PermissionRequestBase.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

#ifdef DEBUG
#include "IndexedDatabaseManager.h"
#endif

#define GC_ON_IPC_MESSAGES 0

#if defined(DEBUG) || GC_ON_IPC_MESSAGES

#include "js/GCAPI.h"
#include "nsJSEnvironment.h"

#define BUILD_GC_ON_IPC_MESSAGES

#endif // DEBUG || GC_ON_IPC_MESSAGES

namespace mozilla {
namespace dom {
namespace indexedDB {

/*******************************************************************************
 * Helpers
 ******************************************************************************/

namespace {

void
MaybeCollectGarbageOnIPCMessage()
{
#ifdef BUILD_GC_ON_IPC_MESSAGES
  static const bool kCollectGarbageOnIPCMessages =
#if GC_ON_IPC_MESSAGES
    true;
#else
    false;
#endif // GC_ON_IPC_MESSAGES

  if (!kCollectGarbageOnIPCMessages) {
    return;
  }

  static bool haveWarnedAboutGC = false;
  static bool haveWarnedAboutNonMainThread = false;

  if (!haveWarnedAboutGC) {
    haveWarnedAboutGC = true;
    NS_WARNING("IndexedDB child actor GC debugging enabled!");
  }

  if (!NS_IsMainThread()) {
    if (!haveWarnedAboutNonMainThread)  {
      haveWarnedAboutNonMainThread = true;
      NS_WARNING("Don't know how to GC on a non-main thread yet.");
    }
    return;
  }

  nsJSContext::GarbageCollectNow(JS::gcreason::DOM_IPC);
  nsJSContext::CycleCollectNow();
#endif // BUILD_GC_ON_IPC_MESSAGES
}

class MOZ_STACK_CLASS AutoSetCurrentTransaction MOZ_FINAL
{
  typedef mozilla::ipc::BackgroundChildImpl BackgroundChildImpl;

  IDBTransaction* const mTransaction;
  IDBTransaction* mPreviousTransaction;
  IDBTransaction** mThreadLocalSlot;

public:
  explicit AutoSetCurrentTransaction(IDBTransaction* aTransaction)
    : mTransaction(aTransaction)
    , mPreviousTransaction(nullptr)
    , mThreadLocalSlot(nullptr)
  {
    if (aTransaction) {
      BackgroundChildImpl::ThreadLocal* threadLocal =
        BackgroundChildImpl::GetThreadLocalForCurrentThread();
      MOZ_ASSERT(threadLocal);

      // Hang onto this location for resetting later.
      mThreadLocalSlot = &threadLocal->mCurrentTransaction;

      // Save the current value.
      mPreviousTransaction = *mThreadLocalSlot;

      // Set the new value.
      *mThreadLocalSlot = aTransaction;
    }
  }

  ~AutoSetCurrentTransaction()
  {
    MOZ_ASSERT_IF(mThreadLocalSlot, mTransaction);

    if (mThreadLocalSlot) {
      MOZ_ASSERT(*mThreadLocalSlot == mTransaction);

      // Reset old value.
      *mThreadLocalSlot = mPreviousTransaction;
    }
  }

  IDBTransaction*
  Transaction() const
  {
    return mTransaction;
  }
};

class MOZ_STACK_CLASS ResultHelper MOZ_FINAL
  : public IDBRequest::ResultCallback
{
  IDBRequest* mRequest;
  AutoSetCurrentTransaction mAutoTransaction;

  union
  {
    nsISupports* mISupports;
    StructuredCloneReadInfo* mStructuredClone;
    const nsTArray<StructuredCloneReadInfo>* mStructuredCloneArray;
    const Key* mKey;
    const nsTArray<Key>* mKeyArray;
    const JS::Value* mJSVal;
    const JS::Handle<JS::Value>* mJSValHandle;
  } mResult;

  enum
  {
    ResultTypeISupports,
    ResultTypeStructuredClone,
    ResultTypeStructuredCloneArray,
    ResultTypeKey,
    ResultTypeKeyArray,
    ResultTypeJSVal,
    ResultTypeJSValHandle,
  } mResultType;

public:
  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               nsISupports* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeISupports)
  {
    MOZ_ASSERT(NS_IsMainThread(), "This won't work off the main thread!");
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mISupports = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               StructuredCloneReadInfo* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeStructuredClone)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mStructuredClone = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               const nsTArray<StructuredCloneReadInfo>* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeStructuredCloneArray)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mStructuredCloneArray = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               const Key* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeKey)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mKey = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               const nsTArray<Key>* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeKeyArray)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mKeyArray = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               const JS::Value* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeJSVal)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(!aResult->isGCThing());

    mResult.mJSVal = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               const JS::Handle<JS::Value>* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeJSValHandle)
  {
    MOZ_ASSERT(aRequest);

    mResult.mJSValHandle = aResult;
  }

  IDBRequest*
  Request() const
  {
    return mRequest;
  }

  IDBTransaction*
  Transaction() const
  {
    return mAutoTransaction.Transaction();
  }

  virtual nsresult
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(mRequest);

    switch (mResultType) {
      case ResultTypeISupports:
        return GetResult(aCx, mResult.mISupports, aResult);

      case ResultTypeStructuredClone:
        return GetResult(aCx, mResult.mStructuredClone, aResult);

      case ResultTypeStructuredCloneArray:
        return GetResult(aCx, mResult.mStructuredCloneArray, aResult);

      case ResultTypeKey:
        return GetResult(aCx, mResult.mKey, aResult);

      case ResultTypeKeyArray:
        return GetResult(aCx, mResult.mKeyArray, aResult);

      case ResultTypeJSVal:
        aResult.set(*mResult.mJSVal);
        return NS_OK;

      case ResultTypeJSValHandle:
        aResult.set(*mResult.mJSValHandle);
        return NS_OK;

      default:
        MOZ_CRASH("Unknown result type!");
    }

    MOZ_CRASH("Should never get here!");
  }

private:
  nsresult
  GetResult(JSContext* aCx,
            nsISupports* aSupports,
            JS::MutableHandle<JS::Value> aResult)
  {
    MOZ_ASSERT(NS_IsMainThread(), "This won't work off the main thread!");

    if (!aSupports) {
      aResult.setNull();
      return NS_OK;
    }

    nsresult rv = nsContentUtils::WrapNative(aCx, aSupports, aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    return NS_OK;
  }

  nsresult
  GetResult(JSContext* aCx,
            StructuredCloneReadInfo* aCloneInfo,
            JS::MutableHandle<JS::Value> aResult)
  {
    bool ok = IDBObjectStore::DeserializeValue(aCx, *aCloneInfo, aResult);

    aCloneInfo->mCloneBuffer.clear();

    if (NS_WARN_IF(!ok)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }

    return NS_OK;
  }

  nsresult
  GetResult(JSContext* aCx,
            const nsTArray<StructuredCloneReadInfo>* aCloneInfos,
            JS::MutableHandle<JS::Value> aResult)
  {
    JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));
    if (NS_WARN_IF(!array)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (!aCloneInfos->IsEmpty()) {
      const uint32_t count = aCloneInfos->Length();

      if (NS_WARN_IF(!JS_SetArrayLength(aCx, array, count))) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      for (uint32_t index = 0; index < count; index++) {
        auto& cloneInfo =
          const_cast<StructuredCloneReadInfo&>(aCloneInfos->ElementAt(index));

        JS::Rooted<JS::Value> value(aCx);

        nsresult rv = GetResult(aCx, &cloneInfo, &value);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (NS_WARN_IF(!JS_SetElement(aCx, array, index, value))) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }
      }
    }

    aResult.setObject(*array);
    return NS_OK;
  }

  nsresult
  GetResult(JSContext* aCx,
            const Key* aKey,
            JS::MutableHandle<JS::Value> aResult)
  {
    nsresult rv = aKey->ToJSVal(aCx, aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  nsresult
  GetResult(JSContext* aCx,
            const nsTArray<Key>* aKeys,
            JS::MutableHandle<JS::Value> aResult)
  {
    JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));
    if (NS_WARN_IF(!array)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (!aKeys->IsEmpty()) {
      const uint32_t count = aKeys->Length();

      if (NS_WARN_IF(!JS_SetArrayLength(aCx, array, count))) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      for (uint32_t index = 0; index < count; index++) {
        const Key& key = aKeys->ElementAt(index);
        MOZ_ASSERT(!key.IsUnset());

        JS::Rooted<JS::Value> value(aCx);

        nsresult rv = GetResult(aCx, &key, &value);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (NS_WARN_IF(!JS_SetElement(aCx, array, index, value))) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }
      }
    }

    aResult.setObject(*array);
    return NS_OK;
  }
};

class PermissionRequestMainProcessHelper MOZ_FINAL
  : public PermissionRequestBase
{
  BackgroundFactoryRequestChild* mActor;
  nsRefPtr<IDBFactory> mFactory;

public:
  PermissionRequestMainProcessHelper(BackgroundFactoryRequestChild* aActor,
                                     IDBFactory* aFactory,
                                     nsPIDOMWindow* aWindow,
                                     nsIPrincipal* aPrincipal)
    : PermissionRequestBase(aWindow, aPrincipal)
    , mActor(aActor)
    , mFactory(aFactory)
  {
    MOZ_ASSERT(aActor);
    MOZ_ASSERT(aFactory);
    aActor->AssertIsOnOwningThread();
  }

protected:
  ~PermissionRequestMainProcessHelper()
  { }

private:
  virtual void
  OnPromptComplete(PermissionValue aPermissionValue) MOZ_OVERRIDE;
};

class PermissionRequestChildProcessActor MOZ_FINAL
  : public PIndexedDBPermissionRequestChild
{
  BackgroundFactoryRequestChild* mActor;
  nsRefPtr<IDBFactory> mFactory;

public:
  PermissionRequestChildProcessActor(BackgroundFactoryRequestChild* aActor,
                                     IDBFactory* aFactory)
    : mActor(aActor)
    , mFactory(aFactory)
  {
    MOZ_ASSERT(aActor);
    MOZ_ASSERT(aFactory);
    aActor->AssertIsOnOwningThread();
  }

protected:
  ~PermissionRequestChildProcessActor()
  { }

  virtual bool
  Recv__delete__(const uint32_t& aPermission) MOZ_OVERRIDE;
};

void
ConvertActorsToBlobs(IDBDatabase* aDatabase,
                     const SerializedStructuredCloneReadInfo& aCloneReadInfo,
                     nsTArray<StructuredCloneFile>& aFiles)
{
  MOZ_ASSERT(aFiles.IsEmpty());

  const nsTArray<PBlobChild*>& blobs = aCloneReadInfo.blobsChild();
  const nsTArray<intptr_t>& fileInfos = aCloneReadInfo.fileInfos();

  MOZ_ASSERT_IF(IndexedDatabaseManager::IsMainProcess(),
                blobs.Length() == fileInfos.Length());
  MOZ_ASSERT_IF(!IndexedDatabaseManager::IsMainProcess(), fileInfos.IsEmpty());

  if (!blobs.IsEmpty()) {
    const uint32_t count = blobs.Length();
    aFiles.SetCapacity(count);

    for (uint32_t index = 0; index < count; index++) {
      BlobChild* actor = static_cast<BlobChild*>(blobs[index]);

      nsCOMPtr<nsIDOMBlob> blob = actor->GetBlob();
      MOZ_ASSERT(blob);

      nsRefPtr<FileInfo> fileInfo;
      if (!fileInfos.IsEmpty()) {
        fileInfo = dont_AddRef(reinterpret_cast<FileInfo*>(fileInfos[index]));

        MOZ_ASSERT(fileInfo);
        MOZ_ASSERT(fileInfo->Id() > 0);

        blob->AddFileInfo(fileInfo);
      }

      aDatabase->NoteReceivedBlob(blob);

      StructuredCloneFile* file = aFiles.AppendElement();
      MOZ_ASSERT(file);

      file->mFile.swap(blob);
      file->mFileInfo.swap(fileInfo);
    }
  }
}

void
DispatchSuccessEvent(ResultHelper* aResultHelper,
                     nsIDOMEvent* aEvent = nullptr)
{
  MOZ_ASSERT(aResultHelper);

  PROFILER_LABEL("IndexedDB",
                 "DispatchSuccessEvent",
                 js::ProfileEntry::Category::STORAGE);

  nsRefPtr<IDBRequest> request = aResultHelper->Request();
  MOZ_ASSERT(request);
  request->AssertIsOnOwningThread();

  nsRefPtr<IDBTransaction> transaction = aResultHelper->Transaction();

  nsCOMPtr<nsIDOMEvent> successEvent;
  if (!aEvent) {
    successEvent = CreateGenericEvent(request,
                                      nsDependentString(kSuccessEventType),
                                      eDoesNotBubble,
                                      eNotCancelable);
    if (NS_WARN_IF(!successEvent)) {
      return;
    }

    aEvent = successEvent;
  }

  request->SetResultCallback(aResultHelper);

  MOZ_ASSERT(aEvent);
  MOZ_ASSERT_IF(transaction, transaction->IsOpen());

  bool dummy;
  nsresult rv = request->DispatchEvent(aEvent, &dummy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MOZ_ASSERT_IF(transaction,
                transaction->IsOpen() || transaction->IsAborted());

  WidgetEvent* internalEvent = aEvent->GetInternalNSEvent();
  MOZ_ASSERT(internalEvent);

  if (transaction &&
      transaction->IsOpen() &&
      internalEvent->mFlags.mExceptionHasBeenRisen) {
    transaction->Abort(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  }
}

void
DispatchErrorEvent(IDBRequest* aRequest,
                   nsresult aErrorCode,
                   IDBTransaction* aTransaction = nullptr,
                   nsIDOMEvent* aEvent = nullptr)
{
  MOZ_ASSERT(aRequest);
  aRequest->AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aErrorCode));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aErrorCode) == NS_ERROR_MODULE_DOM_INDEXEDDB);

  PROFILER_LABEL("IndexedDB",
                 "DispatchErrorEvent",
                 js::ProfileEntry::Category::STORAGE);

  nsRefPtr<IDBRequest> request = aRequest;
  nsRefPtr<IDBTransaction> transaction = aTransaction;

  request->SetError(aErrorCode);

  nsCOMPtr<nsIDOMEvent> errorEvent;
  if (!aEvent) {
    // Make an error event and fire it at the target.
    errorEvent = CreateGenericEvent(request,
                                    nsDependentString(kErrorEventType),
                                    eDoesBubble,
                                    eCancelable);
    if (NS_WARN_IF(!errorEvent)) {
      return;
    }

    aEvent = errorEvent;
  }

  Maybe<AutoSetCurrentTransaction> asct;
  if (aTransaction) {
    asct.emplace(aTransaction);
  }

  bool doDefault;
  nsresult rv = request->DispatchEvent(aEvent, &doDefault);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MOZ_ASSERT(!transaction || transaction->IsOpen() || transaction->IsAborted());

  if (transaction && transaction->IsOpen()) {
    WidgetEvent* internalEvent = aEvent->GetInternalNSEvent();
    MOZ_ASSERT(internalEvent);

    if (internalEvent->mFlags.mExceptionHasBeenRisen) {
      transaction->Abort(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
    } else if (doDefault) {
      transaction->Abort(request);
    }
  }
}

} // anonymous namespace

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

void
PermissionRequestMainProcessHelper::OnPromptComplete(
                                               PermissionValue aPermissionValue)
{
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  mActor->SendPermissionRetry();

  mActor = nullptr;
  mFactory = nullptr;
}

bool
PermissionRequestChildProcessActor::Recv__delete__(
                                              const uint32_t& /* aPermission */)
{
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();
  MOZ_ASSERT(mFactory);

  MaybeCollectGarbageOnIPCMessage();

  nsRefPtr<IDBFactory> factory;
  mFactory.swap(factory);

  mActor->SendPermissionRetry();
  mActor = nullptr;

  return true;
}

/*******************************************************************************
 * BackgroundRequestChildBase
 ******************************************************************************/

BackgroundRequestChildBase::BackgroundRequestChildBase(IDBRequest* aRequest)
  : mRequest(aRequest)
  , mActorDestroyed(false)
{
  MOZ_ASSERT(aRequest);
  aRequest->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundRequestChildBase);
}

BackgroundRequestChildBase::~BackgroundRequestChildBase()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(indexedDB::BackgroundRequestChildBase);
}

#ifdef DEBUG

void
BackgroundRequestChildBase::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif // DEBUG

void
BackgroundRequestChildBase::NoteActorDestroyed()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;
}

/*******************************************************************************
 * BackgroundFactoryChild
 ******************************************************************************/

BackgroundFactoryChild::BackgroundFactoryChild(IDBFactory* aFactory)
  : mFactory(aFactory)
#ifdef DEBUG
  , mOwningThread(NS_GetCurrentThread())
#endif
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundFactoryChild);
}

BackgroundFactoryChild::~BackgroundFactoryChild()
{
  MOZ_COUNT_DTOR(indexedDB::BackgroundFactoryChild);
}

#ifdef DEBUG

void
BackgroundFactoryChild::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread);

  bool current;
  MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

#endif // DEBUG

void
BackgroundFactoryChild::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();

  if (mFactory) {
    mFactory->ClearBackgroundActor();
    mFactory = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundIDBFactoryChild::SendDeleteMe());
  }
}

void
BackgroundFactoryChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (mFactory) {
    mFactory->ClearBackgroundActor();
#ifdef DEBUG
    mFactory = nullptr;
#endif
  }
}

PBackgroundIDBFactoryRequestChild*
BackgroundFactoryChild::AllocPBackgroundIDBFactoryRequestChild(
                                            const FactoryRequestParams& aParams)
{
  MOZ_CRASH("PBackgroundIDBFactoryRequestChild actors should be manually "
            "constructed!");
}

bool
BackgroundFactoryChild::DeallocPBackgroundIDBFactoryRequestChild(
                                      PBackgroundIDBFactoryRequestChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundFactoryRequestChild*>(aActor);
  return true;
}

PBackgroundIDBDatabaseChild*
BackgroundFactoryChild::AllocPBackgroundIDBDatabaseChild(
                                    const DatabaseSpec& aSpec,
                                    PBackgroundIDBFactoryRequestChild* aRequest)
{
  AssertIsOnOwningThread();

  auto request = static_cast<BackgroundFactoryRequestChild*>(aRequest);
  MOZ_ASSERT(request);

  return new BackgroundDatabaseChild(aSpec, request);
}

bool
BackgroundFactoryChild::DeallocPBackgroundIDBDatabaseChild(
                                            PBackgroundIDBDatabaseChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundDatabaseChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BackgroundFactoryRequestChild
 ******************************************************************************/

BackgroundFactoryRequestChild::BackgroundFactoryRequestChild(
                                               IDBFactory* aFactory,
                                               IDBOpenDBRequest* aOpenRequest,
                                               bool aIsDeleteOp,
                                               uint64_t aRequestedVersion)
  : BackgroundRequestChildBase(aOpenRequest)
  , mFactory(aFactory)
  , mRequestedVersion(aRequestedVersion)
  , mIsDeleteOp(aIsDeleteOp)
{
  // Can't assert owning thread here because IPDL has not yet set our manager!
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aOpenRequest);

  MOZ_COUNT_CTOR(indexedDB::BackgroundFactoryRequestChild);
}

BackgroundFactoryRequestChild::~BackgroundFactoryRequestChild()
{
  MOZ_COUNT_DTOR(indexedDB::BackgroundFactoryRequestChild);
}

IDBOpenDBRequest*
BackgroundFactoryRequestChild::GetOpenDBRequest() const
{
  AssertIsOnOwningThread();

  IDBRequest* baseRequest = BackgroundRequestChildBase::GetDOMObject();
  return static_cast<IDBOpenDBRequest*>(baseRequest);
}

bool
BackgroundFactoryRequestChild::HandleResponse(nsresult aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);

  mRequest->Reset();

  DispatchErrorEvent(mRequest, aResponse);

  return true;
}

bool
BackgroundFactoryRequestChild::HandleResponse(
                                   const OpenDatabaseRequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  mRequest->Reset();

  auto databaseActor =
    static_cast<BackgroundDatabaseChild*>(aResponse.databaseChild());
  MOZ_ASSERT(databaseActor);

  databaseActor->EnsureDOMObject();

  IDBDatabase* database = databaseActor->GetDOMObject();
  MOZ_ASSERT(database);

  ResultHelper helper(mRequest, nullptr,
                      static_cast<IDBWrapperCache*>(database));

  DispatchSuccessEvent(&helper);

  databaseActor->ReleaseDOMObject();

  return true;
}

bool
BackgroundFactoryRequestChild::HandleResponse(
                                 const DeleteDatabaseRequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, nullptr, &JS::UndefinedHandleValue);

  nsCOMPtr<nsIDOMEvent> successEvent =
    IDBVersionChangeEvent::Create(mRequest,
                                  nsDependentString(kSuccessEventType),
                                  aResponse.previousVersion());
  if (NS_WARN_IF(!successEvent)) {
    return false;
  }

  DispatchSuccessEvent(&helper, successEvent);

  return true;
}

void
BackgroundFactoryRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  NoteActorDestroyed();
}

bool
BackgroundFactoryRequestChild::Recv__delete__(
                                        const FactoryRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  MaybeCollectGarbageOnIPCMessage();

  switch (aResponse.type()) {
    case FactoryRequestResponse::Tnsresult:
      return HandleResponse(aResponse.get_nsresult());

    case FactoryRequestResponse::TOpenDatabaseRequestResponse:
      return HandleResponse(aResponse.get_OpenDatabaseRequestResponse());

    case FactoryRequestResponse::TDeleteDatabaseRequestResponse:
      return HandleResponse(aResponse.get_DeleteDatabaseRequestResponse());

    default:
      MOZ_CRASH("Unknown response type!");
  }

  MOZ_CRASH("Should never get here!");
}

bool
BackgroundFactoryRequestChild::RecvPermissionChallenge(
                                            const PrincipalInfo& aPrincipalInfo)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (!NS_IsMainThread()) {
    MOZ_CRASH("Implement me for workers!");
  }

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    mozilla::ipc::PrincipalInfoToPrincipal(aPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    nsCOMPtr<nsPIDOMWindow> window = mFactory->GetParentObject();
    MOZ_ASSERT(window);

    nsRefPtr<PermissionRequestMainProcessHelper> helper =
      new PermissionRequestMainProcessHelper(this, mFactory, window, principal);

    PermissionRequestBase::PermissionValue permission;
    if (NS_WARN_IF(NS_FAILED(helper->PromptIfNeeded(&permission)))) {
      return false;
    }

    MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
               permission == PermissionRequestBase::kPermissionDenied ||
               permission == PermissionRequestBase::kPermissionPrompt);

    if (permission != PermissionRequestBase::kPermissionPrompt) {
      SendPermissionRetry();
    }
    return true;
  }

  nsRefPtr<TabChild> tabChild = mFactory->GetTabChild();
  MOZ_ASSERT(tabChild);

  IPC::Principal ipcPrincipal(principal);

  auto* actor = new PermissionRequestChildProcessActor(this, mFactory);

  tabChild->SendPIndexedDBPermissionRequestConstructor(actor, ipcPrincipal);

  return true;
}

bool
BackgroundFactoryRequestChild::RecvBlocked(const uint64_t& aCurrentVersion)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  MaybeCollectGarbageOnIPCMessage();

  const nsDependentString type(kBlockedEventType);

  nsCOMPtr<nsIDOMEvent> blockedEvent;
  if (mIsDeleteOp) {
    blockedEvent =
      IDBVersionChangeEvent::Create(mRequest, type, aCurrentVersion);
  } else {
    blockedEvent =
      IDBVersionChangeEvent::Create(mRequest,
                                    type,
                                    aCurrentVersion,
                                    mRequestedVersion);
  }

  if (NS_WARN_IF(!blockedEvent)) {
    return false;
  }

  nsRefPtr<IDBRequest> kungFuDeathGrip = mRequest;

  bool dummy;
  if (NS_FAILED(mRequest->DispatchEvent(blockedEvent, &dummy))) {
    NS_WARNING("Failed to dispatch event!");
  }

  return true;
}

/*******************************************************************************
 * BackgroundDatabaseChild
 ******************************************************************************/

BackgroundDatabaseChild::BackgroundDatabaseChild(
                               const DatabaseSpec& aSpec,
                               BackgroundFactoryRequestChild* aOpenRequestActor)
  : mSpec(new DatabaseSpec(aSpec))
  , mOpenRequestActor(aOpenRequestActor)
  , mDatabase(nullptr)
{
  // Can't assert owning thread here because IPDL has not yet set our manager!
  MOZ_ASSERT(aOpenRequestActor);

  MOZ_COUNT_CTOR(indexedDB::BackgroundDatabaseChild);
}

BackgroundDatabaseChild::~BackgroundDatabaseChild()
{
  MOZ_COUNT_DTOR(indexedDB::BackgroundDatabaseChild);
}

void
BackgroundDatabaseChild::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mTemporaryStrongDatabase);
  MOZ_ASSERT(!mOpenRequestActor);

  if (mDatabase) {
    mDatabase->ClearBackgroundActor();
    mDatabase = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundIDBDatabaseChild::SendDeleteMe());
  }
}

void
BackgroundDatabaseChild::EnsureDOMObject()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenRequestActor);

  if (mTemporaryStrongDatabase) {
    MOZ_ASSERT(!mSpec);
    return;
  }

  MOZ_ASSERT(mSpec);

  auto request = mOpenRequestActor->GetDOMObject();
  MOZ_ASSERT(request);

  auto factory =
    static_cast<BackgroundFactoryChild*>(Manager())->GetDOMObject();
  MOZ_ASSERT(factory);

  mTemporaryStrongDatabase =
    IDBDatabase::Create(request, factory, this, mSpec);

  MOZ_ASSERT(mTemporaryStrongDatabase);
  mTemporaryStrongDatabase->AssertIsOnOwningThread();

  mDatabase = mTemporaryStrongDatabase;
  mSpec.forget();
}

void
BackgroundDatabaseChild::ReleaseDOMObject()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTemporaryStrongDatabase);
  mTemporaryStrongDatabase->AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenRequestActor);
  MOZ_ASSERT(mDatabase == mTemporaryStrongDatabase);

  mOpenRequestActor = nullptr;

  // This may be the final reference to the IDBDatabase object so we may end up
  // calling SendDeleteMeInternal() here. Make sure everything is cleaned up
  // properly before proceeding.
  mTemporaryStrongDatabase = nullptr;
}

void
BackgroundDatabaseChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (mDatabase) {
    mDatabase->ClearBackgroundActor();
#ifdef DEBUG
    mDatabase = nullptr;
#endif
  }
}

PBackgroundIDBDatabaseFileChild*
BackgroundDatabaseChild::AllocPBackgroundIDBDatabaseFileChild(
                                                         PBlobChild* aBlobChild)
{
  MOZ_CRASH("PBackgroundIDBFileChild actors should be manually constructed!");
}

bool
BackgroundDatabaseChild::DeallocPBackgroundIDBDatabaseFileChild(
                                        PBackgroundIDBDatabaseFileChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

PBackgroundIDBTransactionChild*
BackgroundDatabaseChild::AllocPBackgroundIDBTransactionChild(
                                    const nsTArray<nsString>& aObjectStoreNames,
                                    const Mode& aMode)
{
  MOZ_CRASH("PBackgroundIDBTransactionChild actors should be manually "
            "constructed!");
}

bool
BackgroundDatabaseChild::DeallocPBackgroundIDBTransactionChild(
                                         PBackgroundIDBTransactionChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundTransactionChild*>(aActor);
  return true;
}

PBackgroundIDBVersionChangeTransactionChild*
BackgroundDatabaseChild::AllocPBackgroundIDBVersionChangeTransactionChild(
                                              const uint64_t& aCurrentVersion,
                                              const uint64_t& aRequestedVersion,
                                              const int64_t& aNextObjectStoreId,
                                              const int64_t& aNextIndexId)
{
  AssertIsOnOwningThread();

  IDBOpenDBRequest* request = mOpenRequestActor->GetOpenDBRequest();
  MOZ_ASSERT(request);

  return new BackgroundVersionChangeTransactionChild(request);
}

bool
BackgroundDatabaseChild::RecvPBackgroundIDBVersionChangeTransactionConstructor(
                            PBackgroundIDBVersionChangeTransactionChild* aActor,
                            const uint64_t& aCurrentVersion,
                            const uint64_t& aRequestedVersion,
                            const int64_t& aNextObjectStoreId,
                            const int64_t& aNextIndexId)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mOpenRequestActor);

  MaybeCollectGarbageOnIPCMessage();

  EnsureDOMObject();

  auto actor = static_cast<BackgroundVersionChangeTransactionChild*>(aActor);

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::CreateVersionChange(mDatabase, actor, aNextObjectStoreId,
                                        aNextIndexId);
  if (NS_WARN_IF(!transaction)) {
    return false;
  }

  transaction->AssertIsOnOwningThread();

  actor->SetDOMTransaction(transaction);

  mDatabase->EnterSetVersionTransaction(aRequestedVersion);

  nsRefPtr<IDBOpenDBRequest> request = mOpenRequestActor->GetOpenDBRequest();
  MOZ_ASSERT(request);

  request->SetTransaction(transaction);

  nsCOMPtr<nsIDOMEvent> upgradeNeededEvent =
    IDBVersionChangeEvent::Create(request,
                                  nsDependentString(kUpgradeNeededEventType),
                                  aCurrentVersion,
                                  aRequestedVersion);
  if (NS_WARN_IF(!upgradeNeededEvent)) {
    return false;
  }

  ResultHelper helper(request, transaction,
                      static_cast<IDBWrapperCache*>(mDatabase));

  DispatchSuccessEvent(&helper, upgradeNeededEvent);

  return true;
}

bool
BackgroundDatabaseChild::DeallocPBackgroundIDBVersionChangeTransactionChild(
                            PBackgroundIDBVersionChangeTransactionChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundVersionChangeTransactionChild*>(aActor);
  return true;
}

bool
BackgroundDatabaseChild::RecvVersionChange(const uint64_t& aOldVersion,
                                           const NullableVersion& aNewVersion)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (!mDatabase || mDatabase->IsClosed()) {
    return true;
  }

  nsRefPtr<IDBDatabase> kungFuDeathGrip = mDatabase;

  // Handle bfcache'd windows.
  if (nsPIDOMWindow* owner = mDatabase->GetOwner()) {
    // The database must be closed if the window is already frozen.
    bool shouldAbortAndClose = owner->IsFrozen();

    // Anything in the bfcache has to be evicted and then we have to close the
    // database also.
    if (nsCOMPtr<nsIDocument> doc = owner->GetExtantDoc()) {
      if (nsCOMPtr<nsIBFCacheEntry> bfCacheEntry = doc->GetBFCacheEntry()) {
        bfCacheEntry->RemoveFromBFCacheSync();
        shouldAbortAndClose = true;
      }
    }

    if (shouldAbortAndClose) {
      // Invalidate() doesn't close the database in the parent, so we have
      // to call Close() and AbortTransactions() manually.
      mDatabase->AbortTransactions();
      mDatabase->Close();
      return true;
    }
  }

  // Otherwise fire a versionchange event.
  const nsDependentString type(kVersionChangeEventType);

  nsCOMPtr<nsIDOMEvent> versionChangeEvent;

  switch (aNewVersion.type()) {
    case NullableVersion::Tnull_t:
      versionChangeEvent =
        IDBVersionChangeEvent::Create(mDatabase, type, aOldVersion);
      break;

    case NullableVersion::Tuint64_t:
      versionChangeEvent =
        IDBVersionChangeEvent::Create(mDatabase,
                                      type,
                                      aOldVersion,
                                      aNewVersion.get_uint64_t());
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  if (NS_WARN_IF(!versionChangeEvent)) {
    return false;
  }

  bool dummy;
  if (NS_FAILED(mDatabase->DispatchEvent(versionChangeEvent, &dummy))) {
    NS_WARNING("Failed to dispatch event!");
  }

  if (!mDatabase->IsClosed()) {
    SendBlocked();
  }

  return true;
}

bool
BackgroundDatabaseChild::RecvInvalidate()
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (mDatabase) {
    mDatabase->Invalidate();
  }

  return true;
}

/*******************************************************************************
 * BackgroundTransactionBase
 ******************************************************************************/

BackgroundTransactionBase::BackgroundTransactionBase()
: mTransaction(nullptr)
{
  MOZ_COUNT_CTOR(indexedDB::BackgroundTransactionBase);
}

BackgroundTransactionBase::BackgroundTransactionBase(
                                                   IDBTransaction* aTransaction)
  : mTemporaryStrongTransaction(aTransaction)
  , mTransaction(aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundTransactionBase);
}

BackgroundTransactionBase::~BackgroundTransactionBase()
{
  MOZ_COUNT_DTOR(indexedDB::BackgroundTransactionBase);
}

#ifdef DEBUG

void
BackgroundTransactionBase::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnOwningThread();
}

#endif // DEBUG

void
BackgroundTransactionBase::NoteActorDestroyed()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mTemporaryStrongTransaction, mTransaction);

  if (mTransaction) {
    mTransaction->ClearBackgroundActor();

    // Normally this would be DEBUG-only but NoteActorDestroyed is also called
    // from SendDeleteMeInternal. In that case we're going to receive an actual
    // ActorDestroy call later and we don't want to touch a dead object.
    mTemporaryStrongTransaction = nullptr;
    mTransaction = nullptr;
  }
}

void
BackgroundTransactionBase::SetDOMTransaction(IDBTransaction* aTransaction)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnOwningThread();
  MOZ_ASSERT(!mTemporaryStrongTransaction);
  MOZ_ASSERT(!mTransaction);

  mTemporaryStrongTransaction = aTransaction;
  mTransaction = aTransaction;
}

void
BackgroundTransactionBase::NoteComplete()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mTransaction, mTemporaryStrongTransaction);

  mTemporaryStrongTransaction = nullptr;
}

/*******************************************************************************
 * BackgroundTransactionChild
 ******************************************************************************/

BackgroundTransactionChild::BackgroundTransactionChild(
                                                   IDBTransaction* aTransaction)
  : BackgroundTransactionBase(aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundTransactionChild);
}

BackgroundTransactionChild::~BackgroundTransactionChild()
{
  MOZ_COUNT_DTOR(indexedDB::BackgroundTransactionChild);
}

#ifdef DEBUG

void
BackgroundTransactionChild::AssertIsOnOwningThread() const
{
  static_cast<BackgroundDatabaseChild*>(Manager())->AssertIsOnOwningThread();
}

#endif // DEBUG

void
BackgroundTransactionChild::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();

  if (mTransaction) {
    NoteActorDestroyed();

    MOZ_ALWAYS_TRUE(PBackgroundIDBTransactionChild::SendDeleteMe());
  }
}

void
BackgroundTransactionChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  NoteActorDestroyed();
}

bool
BackgroundTransactionChild::RecvComplete(const nsresult& aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  MaybeCollectGarbageOnIPCMessage();

  mTransaction->FireCompleteOrAbortEvents(aResult);

  NoteComplete();
  return true;
}

PBackgroundIDBRequestChild*
BackgroundTransactionChild::AllocPBackgroundIDBRequestChild(
                                                   const RequestParams& aParams)
{
  MOZ_CRASH("PBackgroundIDBRequestChild actors should be manually "
            "constructed!");
}

bool
BackgroundTransactionChild::DeallocPBackgroundIDBRequestChild(
                                             PBackgroundIDBRequestChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundRequestChild*>(aActor);
  return true;
}

PBackgroundIDBCursorChild*
BackgroundTransactionChild::AllocPBackgroundIDBCursorChild(
                                                const OpenCursorParams& aParams)
{
  AssertIsOnOwningThread();

  MOZ_CRASH("PBackgroundIDBCursorChild actors should be manually constructed!");
}

bool
BackgroundTransactionChild::DeallocPBackgroundIDBCursorChild(
                                              PBackgroundIDBCursorChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundCursorChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BackgroundVersionChangeTransactionChild
 ******************************************************************************/

BackgroundVersionChangeTransactionChild::
BackgroundVersionChangeTransactionChild(IDBOpenDBRequest* aOpenDBRequest)
  : mOpenDBRequest(aOpenDBRequest)
{
  MOZ_ASSERT(aOpenDBRequest);
  aOpenDBRequest->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundVersionChangeTransactionChild);
}

BackgroundVersionChangeTransactionChild::
~BackgroundVersionChangeTransactionChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(indexedDB::BackgroundVersionChangeTransactionChild);
}

#ifdef DEBUG

void
BackgroundVersionChangeTransactionChild::AssertIsOnOwningThread() const
{
  static_cast<BackgroundDatabaseChild*>(Manager())->AssertIsOnOwningThread();
}

#endif // DEBUG

void
BackgroundVersionChangeTransactionChild::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();

  if (mTransaction) {
    NoteActorDestroyed();

    MOZ_ALWAYS_TRUE(PBackgroundIDBVersionChangeTransactionChild::
                      SendDeleteMe());
  }
}

void
BackgroundVersionChangeTransactionChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  mOpenDBRequest = nullptr;

  NoteActorDestroyed();
}

bool
BackgroundVersionChangeTransactionChild::RecvComplete(const nsresult& aResult)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (!mTransaction) {
    return true;
  }

  MOZ_ASSERT(mOpenDBRequest);

  IDBDatabase* database = mTransaction->Database();
  MOZ_ASSERT(database);

  database->ExitSetVersionTransaction();

  if (NS_FAILED(aResult)) {
    database->Close();
  }

  mTransaction->FireCompleteOrAbortEvents(aResult);

  mOpenDBRequest->SetTransaction(nullptr);
  mOpenDBRequest = nullptr;

  NoteComplete();
  return true;
}

PBackgroundIDBRequestChild*
BackgroundVersionChangeTransactionChild::AllocPBackgroundIDBRequestChild(
                                                   const RequestParams& aParams)
{
  MOZ_CRASH("PBackgroundIDBRequestChild actors should be manually "
            "constructed!");
}

bool
BackgroundVersionChangeTransactionChild::DeallocPBackgroundIDBRequestChild(
                                             PBackgroundIDBRequestChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundRequestChild*>(aActor);
  return true;
}

PBackgroundIDBCursorChild*
BackgroundVersionChangeTransactionChild::AllocPBackgroundIDBCursorChild(
                                                const OpenCursorParams& aParams)
{
  AssertIsOnOwningThread();

  MOZ_CRASH("PBackgroundIDBCursorChild actors should be manually constructed!");
}

bool
BackgroundVersionChangeTransactionChild::DeallocPBackgroundIDBCursorChild(
                                              PBackgroundIDBCursorChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundCursorChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BackgroundRequestChild
 ******************************************************************************/

BackgroundRequestChild::BackgroundRequestChild(IDBRequest* aRequest)
  : BackgroundRequestChildBase(aRequest)
  , mTransaction(aRequest->GetTransaction())
{
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundRequestChild);

  mTransaction->OnNewRequest();
}

BackgroundRequestChild::~BackgroundRequestChild()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(!IsActorDestroyed(), mTransaction);

  MOZ_COUNT_DTOR(indexedDB::BackgroundRequestChild);

  MaybeFinishTransactionEarly();
}

void
BackgroundRequestChild::HoldFileInfosUntilComplete(
                                       nsTArray<nsRefPtr<FileInfo>>& aFileInfos)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileInfos.IsEmpty());

  mFileInfos.SwapElements(aFileInfos);
}

void
BackgroundRequestChild::MaybeFinishTransactionEarly()
{
  AssertIsOnOwningThread();

  if (mTransaction) {
    mTransaction->AssertIsOnOwningThread();

    mTransaction->OnRequestFinished();
    mTransaction = nullptr;
  }
}

bool
BackgroundRequestChild::HandleResponse(nsresult aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(mTransaction);

  DispatchErrorEvent(mRequest, aResponse, mTransaction);
  return true;
}

bool
BackgroundRequestChild::HandleResponse(const Key& aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, mTransaction, &aResponse);

  DispatchSuccessEvent(&helper);
  return true;
}

bool
BackgroundRequestChild::HandleResponse(const nsTArray<Key>& aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, mTransaction, &aResponse);

  DispatchSuccessEvent(&helper);
  return true;
}

bool
BackgroundRequestChild::HandleResponse(
                             const SerializedStructuredCloneReadInfo& aResponse)
{
  AssertIsOnOwningThread();

  // XXX Fix this somehow...
  auto& serializedCloneInfo =
    const_cast<SerializedStructuredCloneReadInfo&>(aResponse);

  StructuredCloneReadInfo cloneReadInfo(Move(serializedCloneInfo));
  cloneReadInfo.mDatabase = mTransaction->Database();

  ConvertActorsToBlobs(mTransaction->Database(),
                       aResponse,
                       cloneReadInfo.mFiles);

  ResultHelper helper(mRequest, mTransaction, &cloneReadInfo);

  DispatchSuccessEvent(&helper);
  return true;
}

bool
BackgroundRequestChild::HandleResponse(
                   const nsTArray<SerializedStructuredCloneReadInfo>& aResponse)
{
  AssertIsOnOwningThread();

  nsTArray<StructuredCloneReadInfo> cloneReadInfos;

  if (!aResponse.IsEmpty()) {
    const uint32_t count = aResponse.Length();

    cloneReadInfos.SetCapacity(count);

    IDBDatabase* database = mTransaction->Database();

    for (uint32_t index = 0; index < count; index++) {
      // XXX Fix this somehow...
      auto& serializedCloneInfo =
        const_cast<SerializedStructuredCloneReadInfo&>(aResponse[index]);

      StructuredCloneReadInfo* cloneReadInfo = cloneReadInfos.AppendElement();

      *cloneReadInfo = Move(serializedCloneInfo);

      cloneReadInfo->mDatabase = mTransaction->Database();

      ConvertActorsToBlobs(database,
                           serializedCloneInfo,
                           cloneReadInfo->mFiles);
    }
  }

  ResultHelper helper(mRequest, mTransaction, &cloneReadInfos);

  DispatchSuccessEvent(&helper);
  return true;
}

bool
BackgroundRequestChild::HandleResponse(JS::Handle<JS::Value> aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, mTransaction, &aResponse);

  DispatchSuccessEvent(&helper);
  return true;
}

bool
BackgroundRequestChild::HandleResponse(uint64_t aResponse)
{
  AssertIsOnOwningThread();

  JS::Value response(JS::NumberValue(aResponse));

  ResultHelper helper(mRequest, mTransaction, &response);

  DispatchSuccessEvent(&helper);
  return true;
}

void
BackgroundRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  MaybeFinishTransactionEarly();

  NoteActorDestroyed();
}

bool
BackgroundRequestChild::Recv__delete__(const RequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);

  MaybeCollectGarbageOnIPCMessage();

  // Always fire an "error" event with ABORT_ERR if the transaction was aborted,
  // even if the request succeeded or failed with another error.
  if (mTransaction->IsAborted()) {
    return HandleResponse(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  }

  switch (aResponse.type()) {
    case RequestResponse::Tnsresult:
      return HandleResponse(aResponse.get_nsresult());

    case RequestResponse::TObjectStoreAddResponse:
      return HandleResponse(aResponse.get_ObjectStoreAddResponse().key());

    case RequestResponse::TObjectStorePutResponse:
      return HandleResponse(aResponse.get_ObjectStorePutResponse().key());

    case RequestResponse::TObjectStoreGetResponse:
      return HandleResponse(aResponse.get_ObjectStoreGetResponse().cloneInfo());

    case RequestResponse::TObjectStoreGetAllResponse:
      return HandleResponse(aResponse.get_ObjectStoreGetAllResponse()
                                     .cloneInfos());

    case RequestResponse::TObjectStoreGetAllKeysResponse:
      return HandleResponse(aResponse.get_ObjectStoreGetAllKeysResponse()
                                     .keys());

    case RequestResponse::TObjectStoreDeleteResponse:
      return HandleResponse(JS::UndefinedHandleValue);

    case RequestResponse::TObjectStoreClearResponse:
      return HandleResponse(JS::UndefinedHandleValue);

    case RequestResponse::TObjectStoreCountResponse:
      return HandleResponse(aResponse.get_ObjectStoreCountResponse().count());

    case RequestResponse::TIndexGetResponse:
      return HandleResponse(aResponse.get_IndexGetResponse().cloneInfo());

    case RequestResponse::TIndexGetKeyResponse:
      return HandleResponse(aResponse.get_IndexGetKeyResponse().key());

    case RequestResponse::TIndexGetAllResponse:
      return HandleResponse(aResponse.get_IndexGetAllResponse().cloneInfos());

    case RequestResponse::TIndexGetAllKeysResponse:
      return HandleResponse(aResponse.get_IndexGetAllKeysResponse().keys());

    case RequestResponse::TIndexCountResponse:
      return HandleResponse(aResponse.get_IndexCountResponse().count());

    default:
      MOZ_CRASH("Unknown response type!");
  }

  MOZ_CRASH("Should never get here!");
}

/*******************************************************************************
 * BackgroundCursorChild
 ******************************************************************************/

class BackgroundCursorChild::DelayedDeleteRunnable MOZ_FINAL
  : public nsIRunnable
{
  BackgroundCursorChild* mActor;
  nsRefPtr<IDBRequest> mRequest;

public:
  explicit DelayedDeleteRunnable(BackgroundCursorChild* aActor)
    : mActor(aActor)
    , mRequest(aActor->mRequest)
  {
    MOZ_ASSERT(aActor);
    aActor->AssertIsOnOwningThread();
    MOZ_ASSERT(mRequest);
  }

  // Does not need to be threadsafe since this only runs on one thread.
  NS_DECL_ISUPPORTS

private:
  ~DelayedDeleteRunnable()
  { }

  NS_DECL_NSIRUNNABLE
};

BackgroundCursorChild::BackgroundCursorChild(IDBRequest* aRequest,
                                             IDBObjectStore* aObjectStore,
                                             Direction aDirection)
  : mRequest(aRequest)
  , mTransaction(aRequest->GetTransaction())
  , mObjectStore(aObjectStore)
  , mIndex(nullptr)
  , mCursor(nullptr)
  , mStrongRequest(aRequest)
  , mDirection(aDirection)
{
  MOZ_ASSERT(aObjectStore);
  aObjectStore->AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  MOZ_COUNT_CTOR(indexedDB::BackgroundCursorChild);

#ifdef DEBUG
  mOwningThread = PR_GetCurrentThread();
  MOZ_ASSERT(mOwningThread);
#endif
}

BackgroundCursorChild::BackgroundCursorChild(IDBRequest* aRequest,
                                             IDBIndex* aIndex,
                                             Direction aDirection)
  : mRequest(aRequest)
  , mTransaction(aRequest->GetTransaction())
  , mObjectStore(nullptr)
  , mIndex(aIndex)
  , mCursor(nullptr)
  , mStrongRequest(aRequest)
  , mDirection(aDirection)
{
  MOZ_ASSERT(aIndex);
  aIndex->AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  MOZ_COUNT_CTOR(indexedDB::BackgroundCursorChild);

#ifdef DEBUG
  mOwningThread = PR_GetCurrentThread();
  MOZ_ASSERT(mOwningThread);
#endif
}

BackgroundCursorChild::~BackgroundCursorChild()
{
  MOZ_COUNT_DTOR(indexedDB::BackgroundCursorChild);
}

#ifdef DEBUG

void
BackgroundCursorChild::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread == PR_GetCurrentThread());
}

#endif // DEBUG

void
BackgroundCursorChild::SendContinueInternal(const CursorRequestParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  // Make sure all our DOM objects stay alive.
  mStrongRequest = mRequest;
  mStrongCursor = mCursor;

  MOZ_ASSERT(mRequest->ReadyState() == IDBRequestReadyState::Done);
  mRequest->Reset();

  mTransaction->OnNewRequest();

  MOZ_ALWAYS_TRUE(PBackgroundIDBCursorChild::SendContinue(aParams));
}

void
BackgroundCursorChild::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  mRequest = nullptr;
  mTransaction = nullptr;
  mObjectStore = nullptr;
  mIndex = nullptr;

  if (mCursor) {
    mCursor->ClearBackgroundActor();
    mCursor = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundIDBCursorChild::SendDeleteMe());
  }
}

void
BackgroundCursorChild::HandleResponse(nsresult aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  DispatchErrorEvent(mRequest, aResponse, mTransaction);
}

void
BackgroundCursorChild::HandleResponse(const void_t& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  if (mCursor) {
    mCursor->Reset();
  }

  ResultHelper helper(mRequest, mTransaction, &JS::NullHandleValue);
  DispatchSuccessEvent(&helper);

  if (!mCursor) {
    nsCOMPtr<nsIRunnable> deleteRunnable = new DelayedDeleteRunnable(this);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(deleteRunnable)));
  }
}

void
BackgroundCursorChild::HandleResponse(
                                     const ObjectStoreCursorResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mObjectStore);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  // XXX Fix this somehow...
  auto& response = const_cast<ObjectStoreCursorResponse&>(aResponse);

  StructuredCloneReadInfo cloneReadInfo(Move(response.cloneInfo()));

  ConvertActorsToBlobs(mTransaction->Database(),
                       response.cloneInfo(),
                       cloneReadInfo.mFiles);

  nsRefPtr<IDBCursor> newCursor;

  if (mCursor) {
    mCursor->Reset(Move(response.key()), Move(cloneReadInfo));
  } else {
    newCursor = IDBCursor::Create(mObjectStore,
                                  this,
                                  mDirection,
                                  Move(response.key()),
                                  Move(cloneReadInfo));
    mCursor = newCursor;
  }

  ResultHelper helper(mRequest, mTransaction, mCursor);
  DispatchSuccessEvent(&helper);
}

void
BackgroundCursorChild::HandleResponse(
                                  const ObjectStoreKeyCursorResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mObjectStore);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  // XXX Fix this somehow...
  auto& response = const_cast<ObjectStoreKeyCursorResponse&>(aResponse);

  nsRefPtr<IDBCursor> newCursor;

  if (mCursor) {
    mCursor->Reset(Move(response.key()));
  } else {
    newCursor = IDBCursor::Create(mObjectStore,
                                  this,
                                  mDirection,
                                  Move(response.key()));
    mCursor = newCursor;
  }

  ResultHelper helper(mRequest, mTransaction, mCursor);
  DispatchSuccessEvent(&helper);
}

void
BackgroundCursorChild::HandleResponse(const IndexCursorResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mIndex);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  // XXX Fix this somehow...
  auto& response = const_cast<IndexCursorResponse&>(aResponse);

  StructuredCloneReadInfo cloneReadInfo(Move(response.cloneInfo()));

  ConvertActorsToBlobs(mTransaction->Database(),
                       aResponse.cloneInfo(),
                       cloneReadInfo.mFiles);

  nsRefPtr<IDBCursor> newCursor;

  if (mCursor) {
    mCursor->Reset(Move(response.key()),
                   Move(response.objectKey()),
                   Move(cloneReadInfo));
  } else {
    newCursor = IDBCursor::Create(mIndex,
                                  this,
                                  mDirection,
                                  Move(response.key()),
                                  Move(response.objectKey()),
                                  Move(cloneReadInfo));
    mCursor = newCursor;
  }

  ResultHelper helper(mRequest, mTransaction, mCursor);
  DispatchSuccessEvent(&helper);
}

void
BackgroundCursorChild::HandleResponse(const IndexKeyCursorResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mIndex);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  // XXX Fix this somehow...
  auto& response = const_cast<IndexKeyCursorResponse&>(aResponse);

  nsRefPtr<IDBCursor> newCursor;

  if (mCursor) {
    mCursor->Reset(Move(response.key()), Move(response.objectKey()));
  } else {
    newCursor = IDBCursor::Create(mIndex,
                                  this,
                                  mDirection,
                                  Move(response.key()),
                                  Move(response.objectKey()));
    mCursor = newCursor;
  }

  ResultHelper helper(mRequest, mTransaction, mCursor);
  DispatchSuccessEvent(&helper);
}

void
BackgroundCursorChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(aWhy == Deletion, !mStrongRequest);
  MOZ_ASSERT_IF(aWhy == Deletion, !mStrongCursor);

  MaybeCollectGarbageOnIPCMessage();

  if (mStrongRequest && !mStrongCursor && mTransaction) {
    mTransaction->OnRequestFinished();
  }

  if (mCursor) {
    mCursor->ClearBackgroundActor();
#ifdef DEBUG
    mCursor = nullptr;
#endif
  }

#ifdef DEBUG
  mRequest = nullptr;
  mTransaction = nullptr;
  mObjectStore = nullptr;
  mIndex = nullptr;
#endif
}

bool
BackgroundCursorChild::RecvResponse(const CursorResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mStrongRequest);
  MOZ_ASSERT_IF(mCursor, mStrongCursor);

  MaybeCollectGarbageOnIPCMessage();

  nsRefPtr<IDBRequest> request;
  mStrongRequest.swap(request);

  nsRefPtr<IDBCursor> cursor;
  mStrongCursor.swap(cursor);

  switch (aResponse.type()) {
    case CursorResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case CursorResponse::Tvoid_t:
      HandleResponse(aResponse.get_void_t());
      break;

    case CursorResponse::TObjectStoreCursorResponse:
      HandleResponse(aResponse.get_ObjectStoreCursorResponse());
      break;

    case CursorResponse::TObjectStoreKeyCursorResponse:
      HandleResponse(aResponse.get_ObjectStoreKeyCursorResponse());
      break;

    case CursorResponse::TIndexCursorResponse:
      HandleResponse(aResponse.get_IndexCursorResponse());
      break;

    case CursorResponse::TIndexKeyCursorResponse:
      HandleResponse(aResponse.get_IndexKeyCursorResponse());
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  mTransaction->OnRequestFinished();

  return true;
}

// XXX This doesn't belong here. However, we're not yet porting MutableFile
//     stuff to PBackground so this is necessary for the time being.
void
DispatchMutableFileResult(IDBRequest* aRequest,
                          nsresult aResultCode,
                          IDBMutableFile* aMutableFile)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT_IF(NS_SUCCEEDED(aResultCode), aMutableFile);

  if (NS_SUCCEEDED(aResultCode)) {
    ResultHelper helper(aRequest, nullptr, aMutableFile);
    DispatchSuccessEvent(&helper);
  } else {
    DispatchErrorEvent(aRequest, aResultCode);
  }
}

NS_IMPL_ISUPPORTS(BackgroundCursorChild::DelayedDeleteRunnable,
                  nsIRunnable)

NS_IMETHODIMP
BackgroundCursorChild::
DelayedDeleteRunnable::Run()
{
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  mActor->SendDeleteMeInternal();

  mActor = nullptr;
  mRequest = nullptr;

  return NS_OK;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
