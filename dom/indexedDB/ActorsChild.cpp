/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/TypeTraits.h"
#include "mozilla/dom/Element.h"
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
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

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

using ipc::PrincipalInfo;

namespace dom {

using namespace workers;

namespace indexedDB {

/*******************************************************************************
 * ThreadLocal
 ******************************************************************************/

ThreadLocal::ThreadLocal(const nsID& aBackgroundChildLoggingId)
  : mLoggingInfo(aBackgroundChildLoggingId, 1, -1, 1)
  , mCurrentTransaction(0)
#ifdef DEBUG
  , mOwningThread(PR_GetCurrentThread())
#endif
{
  MOZ_ASSERT(mOwningThread);

  MOZ_COUNT_CTOR(mozilla::dom::indexedDB::ThreadLocal);

  // NSID_LENGTH counts the null terminator, SetLength() does not.
  mLoggingIdString.SetLength(NSID_LENGTH - 1);

  aBackgroundChildLoggingId.ToProvidedString(
    *reinterpret_cast<char(*)[NSID_LENGTH]>(mLoggingIdString.BeginWriting()));
}

ThreadLocal::~ThreadLocal()
{
  MOZ_COUNT_DTOR(mozilla::dom::indexedDB::ThreadLocal);
}

#ifdef DEBUG

void
ThreadLocal::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
}

#endif // DEBUG

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

class MOZ_STACK_CLASS AutoSetCurrentTransaction final
{
  typedef mozilla::ipc::BackgroundChildImpl BackgroundChildImpl;

  IDBTransaction* const mTransaction;
  IDBTransaction* mPreviousTransaction;
  ThreadLocal* mThreadLocal;

public:
  explicit AutoSetCurrentTransaction(IDBTransaction* aTransaction)
    : mTransaction(aTransaction)
    , mPreviousTransaction(nullptr)
    , mThreadLocal(nullptr)
  {
    if (aTransaction) {
      BackgroundChildImpl::ThreadLocal* threadLocal =
        BackgroundChildImpl::GetThreadLocalForCurrentThread();
      MOZ_ASSERT(threadLocal);

      // Hang onto this for resetting later.
      mThreadLocal = threadLocal->mIndexedDBThreadLocal;
      MOZ_ASSERT(mThreadLocal);

      // Save the current value.
      mPreviousTransaction = mThreadLocal->GetCurrentTransaction();

      // Set the new value.
      mThreadLocal->SetCurrentTransaction(aTransaction);
    }
  }

  ~AutoSetCurrentTransaction()
  {
    MOZ_ASSERT_IF(mThreadLocal, mTransaction);
    MOZ_ASSERT_IF(mThreadLocal,
                  mThreadLocal->GetCurrentTransaction() == mTransaction);

    if (mThreadLocal) {
      // Reset old value.
      mThreadLocal->SetCurrentTransaction(mPreviousTransaction);
    }
  }

  IDBTransaction*
  Transaction() const
  {
    return mTransaction;
  }
};

class MOZ_STACK_CLASS ResultHelper final
  : public IDBRequest::ResultCallback
{
  IDBRequest* mRequest;
  AutoSetCurrentTransaction mAutoTransaction;

  union
  {
    IDBDatabase* mDatabase;
    IDBCursor* mCursor;
    IDBMutableFile* mMutableFile;
    StructuredCloneReadInfo* mStructuredClone;
    const nsTArray<StructuredCloneReadInfo>* mStructuredCloneArray;
    const Key* mKey;
    const nsTArray<Key>* mKeyArray;
    const JS::Value* mJSVal;
    const JS::Handle<JS::Value>* mJSValHandle;
  } mResult;

  enum
  {
    ResultTypeDatabase,
    ResultTypeCursor,
    ResultTypeMutableFile,
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
               IDBDatabase* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeDatabase)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mDatabase = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               IDBCursor* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeCursor)
  {
    MOZ_ASSERT(aRequest);

    mResult.mCursor = aResult;
  }

  ResultHelper(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               IDBMutableFile* aResult)
    : mRequest(aRequest)
    , mAutoTransaction(aTransaction)
    , mResultType(ResultTypeMutableFile)
  {
    MOZ_ASSERT(aRequest);

    mResult.mMutableFile = aResult;
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
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) override
  {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(mRequest);

    switch (mResultType) {
      case ResultTypeDatabase:
        return GetResult(aCx, mResult.mDatabase, aResult);

      case ResultTypeCursor:
        return GetResult(aCx, mResult.mCursor, aResult);

      case ResultTypeMutableFile:
        return GetResult(aCx, mResult.mMutableFile, aResult);

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
  template <class T>
  typename EnableIf<IsSame<T, IDBDatabase>::value ||
                    IsSame<T, IDBCursor>::value ||
                    IsSame<T, IDBMutableFile>::value,
                    nsresult>::Type
  GetResult(JSContext* aCx,
            T* aDOMObject,
            JS::MutableHandle<JS::Value> aResult)
  {
    if (!aDOMObject) {
      aResult.setNull();
      return NS_OK;
    }

    bool ok = GetOrCreateDOMReflector(aCx, aDOMObject, aResult);
    if (NS_WARN_IF(!ok)) {
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

        if (NS_WARN_IF(!JS_DefineElement(aCx, array, index, value,
                                         JSPROP_ENUMERATE))) {
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

        if (NS_WARN_IF(!JS_DefineElement(aCx, array, index, value,
                                         JSPROP_ENUMERATE))) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }
      }
    }

    aResult.setObject(*array);
    return NS_OK;
  }
};

class PermissionRequestMainProcessHelper final
  : public PermissionRequestBase
{
  BackgroundFactoryRequestChild* mActor;
  nsRefPtr<IDBFactory> mFactory;

public:
  PermissionRequestMainProcessHelper(BackgroundFactoryRequestChild* aActor,
                                     IDBFactory* aFactory,
                                     Element* aOwnerElement,
                                     nsIPrincipal* aPrincipal)
    : PermissionRequestBase(aOwnerElement, aPrincipal)
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
  OnPromptComplete(PermissionValue aPermissionValue) override;
};

class PermissionRequestChildProcessActor final
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
  Recv__delete__(const uint32_t& aPermission) override;
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

      nsRefPtr<BlobImpl> blobImpl = actor->GetBlobImpl();
      MOZ_ASSERT(blobImpl);

      nsRefPtr<Blob> blob = Blob::Create(aDatabase->GetOwner(), blobImpl);

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

      file->mBlob.swap(blob);
      file->mFileInfo.swap(fileInfo);
    }
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
    MOZ_ASSERT(errorEvent);

    aEvent = errorEvent;
  }

  Maybe<AutoSetCurrentTransaction> asct;
  if (aTransaction) {
    asct.emplace(aTransaction);
  }

  if (transaction) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "Firing %s event with error 0x%x",
                 "IndexedDB %s: C T[%lld] R[%llu]: %s (0x%x)",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(aEvent, kErrorEventType),
                 aErrorCode);
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Request[%llu]: "
                   "Firing %s event with error 0x%x",
                 "IndexedDB %s: C R[%llu]: %s (0x%x)",
                 IDB_LOG_ID_STRING(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(aEvent, kErrorEventType),
                 aErrorCode);
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

  if (transaction && transaction->IsAborted()) {
    DispatchErrorEvent(request, transaction->AbortCode(), transaction);
    return;
  }

  nsCOMPtr<nsIDOMEvent> successEvent;
  if (!aEvent) {
    successEvent = CreateGenericEvent(request,
                                      nsDependentString(kSuccessEventType),
                                      eDoesNotBubble,
                                      eNotCancelable);
    MOZ_ASSERT(successEvent);

    aEvent = successEvent;
  }

  request->SetResultCallback(aResultHelper);

  MOZ_ASSERT(aEvent);
  MOZ_ASSERT_IF(transaction, transaction->IsOpen());

  if (transaction) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "Firing %s event",
                 "IndexedDB %s: C T[%lld] R[%llu]: %s",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(aEvent, kSuccessEventType));
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Request[%llu]: Firing %s event",
                 "IndexedDB %s: C R[%llu]: %s",
                 IDB_LOG_ID_STRING(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(aEvent, kSuccessEventType));
  }

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

class WorkerPermissionChallenge;

// This class calles WorkerPermissionChallenge::OperationCompleted() in the
// worker thread.
class WorkerPermissionOperationCompleted final : public WorkerRunnable
{
  nsRefPtr<WorkerPermissionChallenge> mChallenge;

public:
  WorkerPermissionOperationCompleted(WorkerPrivate* aWorkerPrivate,
                                     WorkerPermissionChallenge* aChallenge)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mChallenge(aChallenge)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;
};

// This class used to do prompting in the main thread and main process.
class WorkerPermissionRequest final : public PermissionRequestBase
{
  nsRefPtr<WorkerPermissionChallenge> mChallenge;

public:
  WorkerPermissionRequest(Element* aElement,
                          nsIPrincipal* aPrincipal,
                          WorkerPermissionChallenge* aChallenge)
    : PermissionRequestBase(aElement, aPrincipal)
    , mChallenge(aChallenge)
  {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aChallenge);
  }

private:
  ~WorkerPermissionRequest()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual void
  OnPromptComplete(PermissionValue aPermissionValue) override;
};

// This class is used in the main thread of all child processes.
class WorkerPermissionRequestChildProcessActor final
  : public PIndexedDBPermissionRequestChild
{
  nsRefPtr<WorkerPermissionChallenge> mChallenge;

public:
  explicit WorkerPermissionRequestChildProcessActor(
                                          WorkerPermissionChallenge* aChallenge)
    : mChallenge(aChallenge)
  {
    MOZ_ASSERT(!XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aChallenge);
  }

protected:
  ~WorkerPermissionRequestChildProcessActor()
  {}

  virtual bool
  Recv__delete__(const uint32_t& aPermission) override;
};

class WorkerPermissionChallenge final : public nsRunnable
                                      , public WorkerFeature
{
public:
  WorkerPermissionChallenge(WorkerPrivate* aWorkerPrivate,
                            BackgroundFactoryRequestChild* aActor,
                            IDBFactory* aFactory,
                            const PrincipalInfo& aPrincipalInfo)
    : mWorkerPrivate(aWorkerPrivate)
    , mActor(aActor)
    , mFactory(aFactory)
    , mPrincipalInfo(aPrincipalInfo)
  {
    MOZ_ASSERT(mWorkerPrivate);
    MOZ_ASSERT(aActor);
    MOZ_ASSERT(aFactory);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_IMETHOD
  Run() override
  {
    bool completed = RunInternal();
    if (completed) {
      OperationCompleted();
    }

    return NS_OK;
  }

  virtual bool
  Notify(JSContext* aCx, workers::Status aStatus) override
  {
    // We don't care about the notification. We just want to keep the
    // mWorkerPrivate alive.
    return true;
  }

  void
  OperationCompleted()
  {
    if (NS_IsMainThread()) {
      nsRefPtr<WorkerPermissionOperationCompleted> runnable =
        new WorkerPermissionOperationCompleted(mWorkerPrivate, this);

      if (!runnable->Dispatch(nullptr)) {
        NS_WARNING("Failed to dispatch a runnable to the worker thread.");
        return;
      }

      return;
    }

    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    MaybeCollectGarbageOnIPCMessage();

    nsRefPtr<IDBFactory> factory;
    mFactory.swap(factory);

    mActor->SendPermissionRetry();
    mActor = nullptr;

    mWorkerPrivate->AssertIsOnWorkerThread();
    JSContext* cx = mWorkerPrivate->GetJSContext();
    mWorkerPrivate->RemoveFeature(cx, this);
  }

private:
  bool
  RunInternal()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    nsPIDOMWindow* window = wp->GetWindow();
    if (!window) {
      return true;
    }

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
      mozilla::ipc::PrincipalInfoToPrincipal(mPrincipalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return true;
    }

    if (XRE_IsParentProcess()) {
      nsCOMPtr<Element> ownerElement =
        do_QueryInterface(window->GetChromeEventHandler());
      if (NS_WARN_IF(!ownerElement)) {
        return true;
      }

      nsRefPtr<WorkerPermissionRequest> helper =
        new WorkerPermissionRequest(ownerElement, principal, this);

      PermissionRequestBase::PermissionValue permission;
      if (NS_WARN_IF(NS_FAILED(helper->PromptIfNeeded(&permission)))) {
        return true;
      }

      MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
                 permission == PermissionRequestBase::kPermissionDenied ||
                 permission == PermissionRequestBase::kPermissionPrompt);

      return permission != PermissionRequestBase::kPermissionPrompt;
    }

    TabChild* tabChild = TabChild::GetFrom(window);
    MOZ_ASSERT(tabChild);

    IPC::Principal ipcPrincipal(principal);

    auto* actor = new WorkerPermissionRequestChildProcessActor(this);
    tabChild->SendPIndexedDBPermissionRequestConstructor(actor, ipcPrincipal);
    return false;
  }

private:
  WorkerPrivate* mWorkerPrivate;
  BackgroundFactoryRequestChild* mActor;
  nsRefPtr<IDBFactory> mFactory;
  PrincipalInfo mPrincipalInfo;
};

void
WorkerPermissionRequest::OnPromptComplete(PermissionValue aPermissionValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  mChallenge->OperationCompleted();
}

bool
WorkerPermissionOperationCompleted::WorkerRun(JSContext* aCx,
                                              WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  mChallenge->OperationCompleted();
  return true;
}

bool
WorkerPermissionRequestChildProcessActor::Recv__delete__(
                                              const uint32_t& /* aPermission */)
{
  MOZ_ASSERT(NS_IsMainThread());
  mChallenge->OperationCompleted();
  return true;
}

} // namespace

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

  IDBDatabase* database = databaseActor->GetDOMObject();
  if (!database) {
    databaseActor->EnsureDOMObject();

    database = databaseActor->GetDOMObject();
    MOZ_ASSERT(database);

    MOZ_ASSERT(!database->IsClosed());
  }

  if (database->IsClosed()) {
    // If the database was closed already, which is only possible if we fired an
    // "upgradeneeded" event, then we shouldn't fire a "success" event here.
    // Instead we fire an error event with AbortErr.
    DispatchErrorEvent(mRequest, NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  } else {
    ResultHelper helper(mRequest, nullptr, database);

    DispatchSuccessEvent(&helper);
  }

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
  MOZ_ASSERT(successEvent);

  DispatchSuccessEvent(&helper, successEvent);

  return true;
}

void
BackgroundFactoryRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  NoteActorDestroyed();

  if (aWhy != Deletion) {
    IDBOpenDBRequest* openRequest = GetOpenDBRequest();
    if (openRequest) {
      openRequest->NoteComplete();
    }
  }
}

bool
BackgroundFactoryRequestChild::Recv__delete__(
                                        const FactoryRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  MaybeCollectGarbageOnIPCMessage();

  bool result;

  switch (aResponse.type()) {
    case FactoryRequestResponse::Tnsresult:
      result = HandleResponse(aResponse.get_nsresult());
      break;

    case FactoryRequestResponse::TOpenDatabaseRequestResponse:
      result = HandleResponse(aResponse.get_OpenDatabaseRequestResponse());
      break;

    case FactoryRequestResponse::TDeleteDatabaseRequestResponse:
      result = HandleResponse(aResponse.get_DeleteDatabaseRequestResponse());
      break;

    default:
      MOZ_CRASH("Unknown response type!");
  }

  IDBOpenDBRequest* request = GetOpenDBRequest();
  MOZ_ASSERT(request);

  request->NoteComplete();

  if (NS_WARN_IF(!result)) {
    return false;
  }

  return true;
}

bool
BackgroundFactoryRequestChild::RecvPermissionChallenge(
                                            const PrincipalInfo& aPrincipalInfo)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<WorkerPermissionChallenge> challenge =
      new WorkerPermissionChallenge(workerPrivate, this, mFactory,
                                    aPrincipalInfo);

    JSContext* cx = workerPrivate->GetJSContext();
    MOZ_ASSERT(cx);

    if (!workerPrivate->AddFeature(cx, challenge)) {
      return false;
    }

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(challenge)));
    return true;
  }

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    mozilla::ipc::PrincipalInfoToPrincipal(aPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsPIDOMWindow> window = mFactory->GetParentObject();
    MOZ_ASSERT(window);

    nsCOMPtr<Element> ownerElement =
      do_QueryInterface(window->GetChromeEventHandler());
    if (NS_WARN_IF(!ownerElement)) {
      return false;
    }

    nsRefPtr<PermissionRequestMainProcessHelper> helper =
      new PermissionRequestMainProcessHelper(this, mFactory, ownerElement, principal);

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
    MOZ_ASSERT(blockedEvent);
  } else {
    blockedEvent =
      IDBVersionChangeEvent::Create(mRequest,
                                    type,
                                    aCurrentVersion,
                                    mRequestedVersion);
    MOZ_ASSERT(blockedEvent);
  }

  nsRefPtr<IDBRequest> kungFuDeathGrip = mRequest;

  IDB_LOG_MARK("IndexedDB %s: Child  Request[%llu]: Firing \"blocked\" event",
               "IndexedDB %s: C R[%llu]: \"blocked\"",
               IDB_LOG_ID_STRING(),
               mRequest->LoggingSerialNumber());

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

  auto* actor = static_cast<BackgroundVersionChangeTransactionChild*>(aActor);

  nsRefPtr<IDBOpenDBRequest> request = mOpenRequestActor->GetOpenDBRequest();
  MOZ_ASSERT(request);

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::CreateVersionChange(mDatabase,
                                        actor,
                                        request,
                                        aNextObjectStoreId,
                                        aNextIndexId);
  if (NS_WARN_IF(!transaction)) {
    // This can happen if we receive events after a worker has begun its
    // shutdown process.
    MOZ_ASSERT(!NS_IsMainThread());

    // Report this to the console.
    IDB_REPORT_INTERNAL_ERR();

    MOZ_ALWAYS_TRUE(aActor->SendDeleteMe());
    return true;
  }

  transaction->AssertIsOnOwningThread();

  actor->SetDOMTransaction(transaction);

  mDatabase->EnterSetVersionTransaction(aRequestedVersion);

  request->SetTransaction(transaction);

  nsCOMPtr<nsIDOMEvent> upgradeNeededEvent =
    IDBVersionChangeEvent::Create(request,
                                  nsDependentString(kUpgradeNeededEventType),
                                  aCurrentVersion,
                                  aRequestedVersion);
  MOZ_ASSERT(upgradeNeededEvent);

  ResultHelper helper(request, transaction, mDatabase);

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
      mDatabase->AbortTransactions(/* aShouldWarn */ false);
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
      MOZ_ASSERT(versionChangeEvent);
      break;

    case NullableVersion::Tuint64_t:
      versionChangeEvent =
        IDBVersionChangeEvent::Create(mDatabase,
                                      type,
                                      aOldVersion,
                                      aNewVersion.get_uint64_t());
      MOZ_ASSERT(versionChangeEvent);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  IDB_LOG_MARK("IndexedDB %s: Child : Firing \"versionchange\" event",
               "IndexedDB %s: C: IDBDatabase \"versionchange\" event",
               IDB_LOG_ID_STRING());

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
BackgroundVersionChangeTransactionChild::SendDeleteMeInternal(
                                                        bool aFailedConstructor)
{
  AssertIsOnOwningThread();

  if (mTransaction || aFailedConstructor) {
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
}

BackgroundRequestChild::~BackgroundRequestChild()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mTransaction);

  MOZ_COUNT_DTOR(indexedDB::BackgroundRequestChild);
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
BackgroundRequestChild::HandleResponse(nsresult aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(mTransaction);

  DispatchErrorEvent(mRequest, aResponse, mTransaction);
}

void
BackgroundRequestChild::HandleResponse(const Key& aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, mTransaction, &aResponse);

  DispatchSuccessEvent(&helper);
}

void
BackgroundRequestChild::HandleResponse(const nsTArray<Key>& aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, mTransaction, &aResponse);

  DispatchSuccessEvent(&helper);
}

void
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
}

void
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
}

void
BackgroundRequestChild::HandleResponse(JS::Handle<JS::Value> aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, mTransaction, &aResponse);

  DispatchSuccessEvent(&helper);
}

void
BackgroundRequestChild::HandleResponse(uint64_t aResponse)
{
  AssertIsOnOwningThread();

  JS::Value response(JS::NumberValue(aResponse));

  ResultHelper helper(mRequest, mTransaction, &response);

  DispatchSuccessEvent(&helper);
}

void
BackgroundRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  NoteActorDestroyed();

  if (mTransaction) {
    mTransaction->AssertIsOnOwningThread();

    mTransaction->OnRequestFinished(/* aActorDestroyedNormally */
                                    aWhy == Deletion);
#ifdef DEBUG
    mTransaction = nullptr;
#endif
  }
}

bool
BackgroundRequestChild::Recv__delete__(const RequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);

  MaybeCollectGarbageOnIPCMessage();

  if (mTransaction->IsAborted()) {
    // Always fire an "error" event with ABORT_ERR if the transaction was
    // aborted, even if the request succeeded or failed with another error.
    HandleResponse(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  } else {
    switch (aResponse.type()) {
      case RequestResponse::Tnsresult:
        HandleResponse(aResponse.get_nsresult());
        break;

      case RequestResponse::TObjectStoreAddResponse:
        HandleResponse(aResponse.get_ObjectStoreAddResponse().key());
        break;

      case RequestResponse::TObjectStorePutResponse:
        HandleResponse(aResponse.get_ObjectStorePutResponse().key());
        break;

      case RequestResponse::TObjectStoreGetResponse:
        HandleResponse(aResponse.get_ObjectStoreGetResponse().cloneInfo());
        break;

      case RequestResponse::TObjectStoreGetAllResponse:
        HandleResponse(aResponse.get_ObjectStoreGetAllResponse().cloneInfos());
        break;

      case RequestResponse::TObjectStoreGetAllKeysResponse:
        HandleResponse(aResponse.get_ObjectStoreGetAllKeysResponse().keys());
        break;

      case RequestResponse::TObjectStoreDeleteResponse:
        HandleResponse(JS::UndefinedHandleValue);
        break;

      case RequestResponse::TObjectStoreClearResponse:
        HandleResponse(JS::UndefinedHandleValue);
        break;

      case RequestResponse::TObjectStoreCountResponse:
        HandleResponse(aResponse.get_ObjectStoreCountResponse().count());
        break;

      case RequestResponse::TIndexGetResponse:
        HandleResponse(aResponse.get_IndexGetResponse().cloneInfo());
        break;

      case RequestResponse::TIndexGetKeyResponse:
        HandleResponse(aResponse.get_IndexGetKeyResponse().key());
        break;

      case RequestResponse::TIndexGetAllResponse:
        HandleResponse(aResponse.get_IndexGetAllResponse().cloneInfos());
        break;

      case RequestResponse::TIndexGetAllKeysResponse:
        HandleResponse(aResponse.get_IndexGetAllKeysResponse().keys());
        break;

      case RequestResponse::TIndexCountResponse:
        HandleResponse(aResponse.get_IndexCountResponse().count());
        break;

      default:
        MOZ_CRASH("Unknown response type!");
    }
  }

  mTransaction->OnRequestFinished(/* aActorDestroyedNormally */ true);

  // Null this out so that we don't try to call OnRequestFinished() again in
  // ActorDestroy.
  mTransaction = nullptr;

  return true;
}

/*******************************************************************************
 * BackgroundCursorChild
 ******************************************************************************/

class BackgroundCursorChild::DelayedActionRunnable final
  : public nsICancelableRunnable
{
  using ActionFunc = void (BackgroundCursorChild::*)();

  BackgroundCursorChild* mActor;
  nsRefPtr<IDBRequest> mRequest;
  ActionFunc mActionFunc;

public:
  explicit
  DelayedActionRunnable(BackgroundCursorChild* aActor, ActionFunc aActionFunc)
    : mActor(aActor)
    , mRequest(aActor->mRequest)
    , mActionFunc(aActionFunc)
  {
    MOZ_ASSERT(aActor);
    aActor->AssertIsOnOwningThread();
    MOZ_ASSERT(mRequest);
    MOZ_ASSERT(mActionFunc);
  }

  // Does not need to be threadsafe since this only runs on one thread.
  NS_DECL_ISUPPORTS

private:
  ~DelayedActionRunnable()
  { }

  NS_DECL_NSIRUNNABLE
  NS_DECL_NSICANCELABLERUNNABLE
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
BackgroundCursorChild::SendContinueInternal(const CursorRequestParams& aParams,
                                            const Key& aKey)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  // Make sure all our DOM objects stay alive.
  mStrongCursor = mCursor;

  MOZ_ASSERT(mRequest->ReadyState() == IDBRequestReadyState::Done);
  mRequest->Reset();

  mTransaction->OnNewRequest();

  CursorRequestParams params = aParams;
  Key key = aKey;

  switch (params.type()) {
    case CursorRequestParams::TContinueParams: {
      if (key.IsUnset()) {
        break;
      }
      while (!mCachedResponses.IsEmpty()) {
        if (mCachedResponses[0].mKey == key) {
          break;
        }
        mCachedResponses.RemoveElementAt(0);
      }
      break;
    }

    case CursorRequestParams::TAdvanceParams: {
      uint32_t& advanceCount = params.get_AdvanceParams().count();
      while (advanceCount > 1 && !mCachedResponses.IsEmpty()) {
        key = mCachedResponses[0].mKey;
        mCachedResponses.RemoveElementAt(0);
        --advanceCount;
      }
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  if (!mCachedResponses.IsEmpty()) {
    nsCOMPtr<nsIRunnable> continueRunnable = new DelayedActionRunnable(
      this, &BackgroundCursorChild::SendDelayedContinueInternal);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(continueRunnable)));
  } else {
    MOZ_ALWAYS_TRUE(PBackgroundIDBCursorChild::SendContinue(params, key));
  }
}

void
BackgroundCursorChild::SendDelayedContinueInternal()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mStrongCursor);
  MOZ_ASSERT(!mCachedResponses.IsEmpty());

  nsRefPtr<IDBCursor> cursor;
  mStrongCursor.swap(cursor);

  auto& item = mCachedResponses[0];
  mCursor->Reset(Move(item.mKey), Move(item.mCloneInfo));
  mCachedResponses.RemoveElementAt(0);

  ResultHelper helper(mRequest, mTransaction, mCursor);
  DispatchSuccessEvent(&helper);

  mTransaction->OnRequestFinished(/* aActorDestroyedNormally */ true);
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
BackgroundCursorChild::InvalidateCachedResponses()
{
  AssertIsOnOwningThread();

  mCachedResponses.Clear();
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
    nsCOMPtr<nsIRunnable> deleteRunnable = new DelayedActionRunnable(
      this, &BackgroundCursorChild::SendDeleteMeInternal);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(deleteRunnable)));
  }
}

void
BackgroundCursorChild::HandleResponse(
    const nsTArray<ObjectStoreCursorResponse>& aResponses)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mObjectStore);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  MOZ_ASSERT(aResponses.Length() == 1);

  // XXX Fix this somehow...
  auto& responses =
    const_cast<nsTArray<ObjectStoreCursorResponse>&>(aResponses);

  for (ObjectStoreCursorResponse& response : responses) {
    StructuredCloneReadInfo cloneReadInfo(Move(response.cloneInfo()));
    cloneReadInfo.mDatabase = mTransaction->Database();

    ConvertActorsToBlobs(mTransaction->Database(),
                         response.cloneInfo(),
                         cloneReadInfo.mFiles);

    nsRefPtr<IDBCursor> newCursor;

    if (mCursor) {
      if (mCursor->IsContinueCalled()) {
        mCursor->Reset(Move(response.key()), Move(cloneReadInfo));
      } else {
        CachedResponse cachedResponse;
        cachedResponse.mKey = Move(response.key());
        cachedResponse.mCloneInfo = Move(cloneReadInfo);
        mCachedResponses.AppendElement(Move(cachedResponse));
      }
    } else {
      newCursor = IDBCursor::Create(this,
                                    Move(response.key()),
                                    Move(cloneReadInfo));
      mCursor = newCursor;
    }
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
    newCursor = IDBCursor::Create(this, Move(response.key()));
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
  cloneReadInfo.mDatabase = mTransaction->Database();

  ConvertActorsToBlobs(mTransaction->Database(),
                       aResponse.cloneInfo(),
                       cloneReadInfo.mFiles);

  nsRefPtr<IDBCursor> newCursor;

  if (mCursor) {
    mCursor->Reset(Move(response.key()),
                   Move(response.sortKey()),
                   Move(response.objectKey()),
                   Move(cloneReadInfo));
  } else {
    newCursor = IDBCursor::Create(this,
                                  Move(response.key()),
                                  Move(response.sortKey()),
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
    mCursor->Reset(Move(response.key()),
                   Move(response.sortKey()),
                   Move(response.objectKey()));
  } else {
    newCursor = IDBCursor::Create(this,
                                  Move(response.key()),
                                  Move(response.sortKey()),
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
    mTransaction->OnRequestFinished(/* aActorDestroyedNormally */
                                    aWhy == Deletion);
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
  MOZ_ASSERT_IF(mCursor, mStrongCursor);
  MOZ_ASSERT_IF(!mCursor, mStrongRequest);

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

    case CursorResponse::TArrayOfObjectStoreCursorResponse:
      HandleResponse(aResponse.get_ArrayOfObjectStoreCursorResponse());
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

  mTransaction->OnRequestFinished(/* aActorDestroyedNormally */ true);

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

NS_IMPL_ISUPPORTS(BackgroundCursorChild::DelayedActionRunnable,
                  nsIRunnable,
                  nsICancelableRunnable)

NS_IMETHODIMP
BackgroundCursorChild::
DelayedActionRunnable::Run()
{
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mActionFunc);

  (mActor->*mActionFunc)();

  mActor = nullptr;
  mRequest = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
BackgroundCursorChild::
DelayedActionRunnable::Cancel()
{
  if (NS_WARN_IF(!mActor)) {
    return NS_ERROR_UNEXPECTED;
  }

  // This must always run to clean up our state.
  Run();

  return NS_OK;
}

BackgroundCursorChild::CachedResponse::CachedResponse()
{
}

BackgroundCursorChild::CachedResponse::CachedResponse(CachedResponse&& aOther)
  : mKey(Move(aOther.mKey))
{
  mCloneInfo = Move(aOther.mCloneInfo);
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
