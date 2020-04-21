/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

#include <type_traits>

#include "BackgroundChildImpl.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBFileHandle.h"
#include "IDBIndex.h"
#include "IDBMutableFile.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"
#include "js/Array.h"  // JS::NewArrayObject, JS::SetArrayLength
#include "js/Date.h"   // JS::NewDateObject, JS::TimeClip
#include <mozIIPCBlobInputStream.h>
#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Maybe.h"
#include "mozilla/SnappyUncompressInputStream.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseFileChild.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/Encoding.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/TaskQueue.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIAsyncInputStream.h"
#include "nsIBFCacheEntry.h"
#include "mozilla/dom/Document.h"
#include "nsIEventTarget.h"
#include "nsIFileStreams.h"
#include "nsNetCID.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "PermissionRequestBase.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

#ifdef DEBUG
#  include "IndexedDatabaseManager.h"
#endif

#define GC_ON_IPC_MESSAGES 0

#if defined(DEBUG) || GC_ON_IPC_MESSAGES

#  include "js/GCAPI.h"
#  include "nsJSEnvironment.h"

#  define BUILD_GC_ON_IPC_MESSAGES

#endif  // DEBUG || GC_ON_IPC_MESSAGES

namespace mozilla {

using ipc::PrincipalInfo;

namespace dom {

namespace indexedDB {

namespace {

/*******************************************************************************
 * Constants
 ******************************************************************************/

const uint32_t kFileCopyBufferSize = 32768;

}  // namespace

/*******************************************************************************
 * ThreadLocal
 ******************************************************************************/

ThreadLocal::ThreadLocal(const nsID& aBackgroundChildLoggingId)
    : mLoggingInfo(aBackgroundChildLoggingId, 1, -1, 1) {
  MOZ_COUNT_CTOR(mozilla::dom::indexedDB::ThreadLocal);

  // NSID_LENGTH counts the null terminator, SetLength() does not.
  mLoggingIdString.SetLength(NSID_LENGTH - 1);

  aBackgroundChildLoggingId.ToProvidedString(
      *reinterpret_cast<char(*)[NSID_LENGTH]>(mLoggingIdString.BeginWriting()));
}

ThreadLocal::~ThreadLocal() {
  MOZ_COUNT_DTOR(mozilla::dom::indexedDB::ThreadLocal);
}

/*******************************************************************************
 * Helpers
 ******************************************************************************/

namespace {

void MaybeCollectGarbageOnIPCMessage() {
#ifdef BUILD_GC_ON_IPC_MESSAGES
  static const bool kCollectGarbageOnIPCMessages =
#  if GC_ON_IPC_MESSAGES
      true;
#  else
      false;
#  endif  // GC_ON_IPC_MESSAGES

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
    if (!haveWarnedAboutNonMainThread) {
      haveWarnedAboutNonMainThread = true;
      NS_WARNING("Don't know how to GC on a non-main thread yet.");
    }
    return;
  }

  nsJSContext::GarbageCollectNow(JS::GCReason::DOM_IPC);
  nsJSContext::CycleCollectNow();
#endif  // BUILD_GC_ON_IPC_MESSAGES
}

class MOZ_STACK_CLASS AutoSetCurrentTransaction final {
  typedef mozilla::ipc::BackgroundChildImpl BackgroundChildImpl;

  Maybe<IDBTransaction&> const mTransaction;
  Maybe<IDBTransaction&> mPreviousTransaction;
  ThreadLocal* mThreadLocal;

 public:
  AutoSetCurrentTransaction(const AutoSetCurrentTransaction&) = delete;
  AutoSetCurrentTransaction(AutoSetCurrentTransaction&&) = delete;
  AutoSetCurrentTransaction& operator=(const AutoSetCurrentTransaction&) =
      delete;
  AutoSetCurrentTransaction& operator=(AutoSetCurrentTransaction&&) = delete;

  explicit AutoSetCurrentTransaction(Maybe<IDBTransaction&> aTransaction)
      : mTransaction(aTransaction),
        mPreviousTransaction(),
        mThreadLocal(nullptr) {
    if (aTransaction) {
      BackgroundChildImpl::ThreadLocal* threadLocal =
          BackgroundChildImpl::GetThreadLocalForCurrentThread();
      MOZ_ASSERT(threadLocal);

      // Hang onto this for resetting later.
      mThreadLocal = threadLocal->mIndexedDBThreadLocal.get();
      MOZ_ASSERT(mThreadLocal);

      // Save the current value.
      mPreviousTransaction = mThreadLocal->MaybeCurrentTransactionRef();

      // Set the new value.
      mThreadLocal->SetCurrentTransaction(aTransaction);
    }
  }

  ~AutoSetCurrentTransaction() {
    MOZ_ASSERT_IF(mThreadLocal, mTransaction);
    MOZ_ASSERT_IF(mThreadLocal,
                  ReferenceEquals(mThreadLocal->MaybeCurrentTransactionRef(),
                                  mTransaction));

    if (mThreadLocal) {
      // Reset old value.
      mThreadLocal->SetCurrentTransaction(mPreviousTransaction);
    }
  }
};

class MOZ_STACK_CLASS ResultHelper final : public IDBRequest::ResultCallback {
  const RefPtr<IDBRequest> mRequest;
  const AutoSetCurrentTransaction mAutoTransaction;
  const SafeRefPtr<IDBTransaction> mTransaction;

  union {
    IDBDatabase* mDatabase;
    IDBCursor* mCursor;
    IDBMutableFile* mMutableFile;
    StructuredCloneReadInfoChild* mStructuredClone;
    nsTArray<StructuredCloneReadInfoChild>* mStructuredCloneArray;
    const Key* mKey;
    const nsTArray<Key>* mKeyArray;
    const JS::Value* mJSVal;
    const JS::Handle<JS::Value>* mJSValHandle;
  } mResult;

  enum {
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
  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               IDBDatabase* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeDatabase) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mDatabase = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               IDBCursor* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeCursor) {
    MOZ_ASSERT(aRequest);

    mResult.mCursor = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               IDBMutableFile* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeMutableFile) {
    MOZ_ASSERT(aRequest);

    mResult.mMutableFile = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               StructuredCloneReadInfoChild* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeStructuredClone) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mStructuredClone = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               nsTArray<StructuredCloneReadInfoChild>* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeStructuredCloneArray) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mStructuredCloneArray = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               const Key* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeKey) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mKey = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               const nsTArray<Key>* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeKeyArray) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aResult);

    mResult.mKeyArray = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               const JS::Value* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeJSVal) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(!aResult->isGCThing());

    mResult.mJSVal = aResult;
  }

  ResultHelper(IDBRequest* aRequest, SafeRefPtr<IDBTransaction> aTransaction,
               const JS::Handle<JS::Value>* aResult)
      : mRequest(aRequest),
        mAutoTransaction(aTransaction ? SomeRef(*aTransaction) : Nothing()),
        mTransaction(std::move(aTransaction)),
        mResultType(ResultTypeJSValHandle) {
    MOZ_ASSERT(aRequest);

    mResult.mJSValHandle = aResult;
  }

  virtual nsresult GetResult(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aResult) override {
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
        return GetResult(aCx, std::move(*mResult.mStructuredClone), aResult);

      case ResultTypeStructuredCloneArray:
        return GetResult(aCx, std::move(*mResult.mStructuredCloneArray),
                         aResult);

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

  void DispatchSuccessEvent(RefPtr<Event> aEvent = nullptr);

 private:
  template <class T>
  std::enable_if_t<std::is_same_v<T, IDBDatabase> ||
                       std::is_same_v<T, IDBCursor> ||
                       std::is_same_v<T, IDBMutableFile>,
                   nsresult>
  GetResult(JSContext* aCx, T* aDOMObject,
            JS::MutableHandle<JS::Value> aResult) {
    if (!aDOMObject) {
      aResult.setNull();
      return NS_OK;
    }

    const bool ok = GetOrCreateDOMReflector(aCx, aDOMObject, aResult);
    if (NS_WARN_IF(!ok)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    return NS_OK;
  }

  nsresult GetResult(JSContext* aCx, StructuredCloneReadInfoChild&& aCloneInfo,
                     JS::MutableHandle<JS::Value> aResult) {
    const bool ok =
        IDBObjectStore::DeserializeValue(aCx, std::move(aCloneInfo), aResult);

    if (NS_WARN_IF(!ok)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }

    return NS_OK;
  }

  nsresult GetResult(JSContext* aCx,
                     nsTArray<StructuredCloneReadInfoChild>&& aCloneInfos,
                     JS::MutableHandle<JS::Value> aResult) {
    JS::Rooted<JSObject*> array(aCx, JS::NewArrayObject(aCx, 0));
    if (NS_WARN_IF(!array)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (!aCloneInfos.IsEmpty()) {
      const uint32_t count = aCloneInfos.Length();

      if (NS_WARN_IF(!JS::SetArrayLength(aCx, array, count))) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      for (uint32_t index = 0; index < count; index++) {
        auto& cloneInfo = aCloneInfos.ElementAt(index);

        JS::Rooted<JS::Value> value(aCx);

        const nsresult rv = GetResult(aCx, std::move(cloneInfo), &value);
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

  nsresult GetResult(JSContext* aCx, const Key* aKey,
                     JS::MutableHandle<JS::Value> aResult) {
    const nsresult rv = aKey->ToJSVal(aCx, aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  nsresult GetResult(JSContext* aCx, const nsTArray<Key>* aKeys,
                     JS::MutableHandle<JS::Value> aResult) {
    JS::Rooted<JSObject*> array(aCx, JS::NewArrayObject(aCx, 0));
    if (NS_WARN_IF(!array)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (!aKeys->IsEmpty()) {
      const uint32_t count = aKeys->Length();

      if (NS_WARN_IF(!JS::SetArrayLength(aCx, array, count))) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      for (uint32_t index = 0; index < count; index++) {
        const Key& key = aKeys->ElementAt(index);
        MOZ_ASSERT(!key.IsUnset());

        JS::Rooted<JS::Value> value(aCx);

        const nsresult rv = GetResult(aCx, &key, &value);
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

class PermissionRequestMainProcessHelper final : public PermissionRequestBase {
  BackgroundFactoryRequestChild* mActor;
  SafeRefPtr<IDBFactory> mFactory;

 public:
  PermissionRequestMainProcessHelper(BackgroundFactoryRequestChild* aActor,
                                     SafeRefPtr<IDBFactory> aFactory,
                                     Element* aOwnerElement,
                                     nsIPrincipal* aPrincipal)
      : PermissionRequestBase(aOwnerElement, aPrincipal),
        mActor(aActor),
        mFactory(std::move(aFactory)) {
    MOZ_ASSERT(aActor);
    MOZ_ASSERT(mFactory);
    aActor->AssertIsOnOwningThread();
  }

 protected:
  ~PermissionRequestMainProcessHelper() = default;

 private:
  virtual void OnPromptComplete(PermissionValue aPermissionValue) override;
};

auto DeserializeStructuredCloneFiles(
    IDBDatabase* aDatabase,
    const nsTArray<SerializedStructuredCloneFile>& aSerializedFiles,
    bool aForPreprocess) {
  MOZ_ASSERT_IF(aForPreprocess, aSerializedFiles.Length() == 1);

  return TransformIntoNewArray(
      aSerializedFiles,
      [aForPreprocess, &database = *aDatabase](
          const auto& serializedFile) -> StructuredCloneFileChild {
        MOZ_ASSERT_IF(
            aForPreprocess,
            serializedFile.type() == StructuredCloneFileBase::eStructuredClone);

        const BlobOrMutableFile& blobOrMutableFile = serializedFile.file();

        switch (serializedFile.type()) {
          case StructuredCloneFileBase::eBlob: {
            MOZ_ASSERT(blobOrMutableFile.type() == BlobOrMutableFile::TIPCBlob);

            const IPCBlob& ipcBlob = blobOrMutableFile.get_IPCBlob();

            const RefPtr<BlobImpl> blobImpl =
                IPCBlobUtils::Deserialize(ipcBlob);
            MOZ_ASSERT(blobImpl);

            RefPtr<Blob> blob =
                Blob::Create(database.GetOwnerGlobal(), blobImpl);
            MOZ_ASSERT(blob);

            return {StructuredCloneFileBase::eBlob, std::move(blob)};
          }

          case StructuredCloneFileBase::eMutableFile: {
            MOZ_ASSERT(blobOrMutableFile.type() == BlobOrMutableFile::Tnull_t ||
                       blobOrMutableFile.type() ==
                           BlobOrMutableFile::TPBackgroundMutableFileChild);

            switch (blobOrMutableFile.type()) {
              case BlobOrMutableFile::Tnull_t:
                return StructuredCloneFileChild{
                    StructuredCloneFileBase::eMutableFile};

              case BlobOrMutableFile::TPBackgroundMutableFileChild: {
                auto* const actor = static_cast<BackgroundMutableFileChild*>(
                    blobOrMutableFile.get_PBackgroundMutableFileChild());
                MOZ_ASSERT(actor);

                actor->EnsureDOMObject();

                auto* const mutableFile =
                    static_cast<IDBMutableFile*>(actor->GetDOMObject());
                MOZ_ASSERT(mutableFile);

                auto file = StructuredCloneFileChild{mutableFile};

                actor->ReleaseDOMObject();

                return file;
              }

              default:
                MOZ_CRASH("Should never get here!");
            }
          }

          case StructuredCloneFileBase::eStructuredClone: {
            if (aForPreprocess) {
              MOZ_ASSERT(blobOrMutableFile.type() ==
                         BlobOrMutableFile::TIPCBlob);

              const IPCBlob& ipcBlob = blobOrMutableFile.get_IPCBlob();

              const RefPtr<BlobImpl> blobImpl =
                  IPCBlobUtils::Deserialize(ipcBlob);
              MOZ_ASSERT(blobImpl);

              RefPtr<Blob> blob =
                  Blob::Create(database.GetOwnerGlobal(), blobImpl);
              MOZ_ASSERT(blob);

              return {StructuredCloneFileBase::eStructuredClone,
                      std::move(blob)};
            }
            MOZ_ASSERT(blobOrMutableFile.type() == BlobOrMutableFile::Tnull_t);

            return StructuredCloneFileChild{
                StructuredCloneFileBase::eStructuredClone};
          }

          case StructuredCloneFileBase::eWasmBytecode:
          case StructuredCloneFileBase::eWasmCompiled: {
            MOZ_ASSERT(blobOrMutableFile.type() == BlobOrMutableFile::Tnull_t);

            return StructuredCloneFileChild{serializedFile.type()};

            // Don't set mBlob, support for storing WebAssembly.Modules has been
            // removed in bug 1469395. Support for de-serialization of
            // WebAssembly.Modules has been removed in bug 1561876. Full removal
            // is tracked in bug 1487479.
          }

          default:
            MOZ_CRASH("Should never get here!");
        }
      });
}

JSStructuredCloneData PreprocessingNotSupported() {
  MOZ_CRASH("Preprocessing not (yet) supported!");
}

template <typename PreprocessInfoAccessor>
StructuredCloneReadInfoChild DeserializeStructuredCloneReadInfo(
    SerializedStructuredCloneReadInfo&& aSerialized,
    IDBDatabase* const aDatabase,
    PreprocessInfoAccessor preprocessInfoAccessor) {
  // XXX Make this a class invariant of SerializedStructuredCloneReadInfo.
  MOZ_ASSERT_IF(aSerialized.hasPreprocessInfo(),
                0 == aSerialized.data().data.Size());
  return {aSerialized.hasPreprocessInfo() ? preprocessInfoAccessor()
                                          : std::move(aSerialized.data().data),
          DeserializeStructuredCloneFiles(aDatabase, aSerialized.files(),
                                          /* aForPreprocess */ false),
          aDatabase};
}

// TODO: Remove duplication between DispatchErrorEvent and DispatchSucessEvent.

void DispatchErrorEvent(
    const RefPtr<IDBRequest>& aRequest, nsresult aErrorCode,
    const SafeRefPtr<IDBTransaction>& aTransaction = nullptr,
    RefPtr<Event> aEvent = nullptr) {
  MOZ_ASSERT(aRequest);
  aRequest->AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aErrorCode));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aErrorCode) == NS_ERROR_MODULE_DOM_INDEXEDDB);

  AUTO_PROFILER_LABEL("IndexedDB:DispatchErrorEvent", DOM);

  const RefPtr<IDBRequest> request = aRequest;

  request->SetError(aErrorCode);

  if (!aEvent) {
    // Make an error event and fire it at the target.
    aEvent = CreateGenericEvent(request, nsDependentString(kErrorEventType),
                                eDoesBubble, eCancelable);
  }
  MOZ_ASSERT(aEvent);

  // XXX This is redundant if we are called from
  // ResultHelper::DispatchSuccessEvent.
  Maybe<AutoSetCurrentTransaction> asct;
  if (aTransaction) {
    asct.emplace(SomeRef(*aTransaction));
  }

  if (aTransaction && aTransaction->IsInactive()) {
    aTransaction->TransitionToActive();
  }

  if (aTransaction) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "Firing %s event with error 0x%x", "%s (0x%x)",
        aTransaction->LoggingSerialNumber(), request->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(aEvent, kErrorEventType), aErrorCode);
  } else {
    IDB_LOG_MARK_CHILD_REQUEST("Firing %s event with error 0x%x", "%s (0x%x)",
                               request->LoggingSerialNumber(),
                               IDB_LOG_STRINGIFY(aEvent, kErrorEventType),
                               aErrorCode);
  }

  IgnoredErrorResult rv;
  const bool doDefault =
      request->DispatchEvent(*aEvent, CallerType::System, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return;
  }

  MOZ_ASSERT(!aTransaction || aTransaction->IsActive() ||
             aTransaction->IsAborted() ||
             aTransaction->WasExplicitlyCommitted());

  if (aTransaction && aTransaction->IsActive()) {
    aTransaction->TransitionToInactive();

    // Do not abort the transaction here if this request is failed due to the
    // abortion of its transaction to ensure that the correct error cause of
    // the abort event be set in IDBTransaction::FireCompleteOrAbortEvents()
    // later.
    if (aErrorCode != NS_ERROR_DOM_INDEXEDDB_ABORT_ERR) {
      WidgetEvent* const internalEvent = aEvent->WidgetEventPtr();
      MOZ_ASSERT(internalEvent);

      if (internalEvent->mFlags.mExceptionWasRaised) {
        aTransaction->Abort(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
      } else if (doDefault) {
        aTransaction->Abort(request);
      }
    }
  }
}

void ResultHelper::DispatchSuccessEvent(RefPtr<Event> aEvent) {
  AUTO_PROFILER_LABEL("IndexedDB:DispatchSuccessEvent", DOM);

  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();

  if (mTransaction && mTransaction->IsAborted()) {
    DispatchErrorEvent(mRequest, mTransaction->AbortCode(), mTransaction);
    return;
  }

  if (!aEvent) {
    aEvent = CreateGenericEvent(mRequest, nsDependentString(kSuccessEventType),
                                eDoesNotBubble, eNotCancelable);
  }
  MOZ_ASSERT(aEvent);

  mRequest->SetResultCallback(this);

  if (mTransaction && mTransaction->IsInactive()) {
    mTransaction->TransitionToActive();
  }

  if (mTransaction) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "Firing %s event", "%s", mTransaction->LoggingSerialNumber(),
        mRequest->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(aEvent, kSuccessEventType));
  } else {
    IDB_LOG_MARK_CHILD_REQUEST("Firing %s event", "%s",
                               mRequest->LoggingSerialNumber(),
                               IDB_LOG_STRINGIFY(aEvent, kSuccessEventType));
  }

  MOZ_ASSERT_IF(mTransaction && !mTransaction->WasExplicitlyCommitted(),
                mTransaction->IsActive() && !mTransaction->IsAborted());

  IgnoredErrorResult rv;
  mRequest->DispatchEvent(*aEvent, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return;
  }

  WidgetEvent* const internalEvent = aEvent->WidgetEventPtr();
  MOZ_ASSERT(internalEvent);

  if (mTransaction && mTransaction->IsActive()) {
    mTransaction->TransitionToInactive();

    if (internalEvent->mFlags.mExceptionWasRaised) {
      mTransaction->Abort(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
    } else {
      // To handle upgrade transaction.
      mTransaction->CommitIfNotStarted();
    }
  }
}

PRFileDesc* GetFileDescriptorFromStream(nsIInputStream* aStream) {
  MOZ_ASSERT(aStream);

  const nsCOMPtr<nsIFileMetadata> fileMetadata = do_QueryInterface(aStream);
  if (NS_WARN_IF(!fileMetadata)) {
    return nullptr;
  }

  PRFileDesc* fileDesc;
  const nsresult rv = fileMetadata->GetFileDescriptor(&fileDesc);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  MOZ_ASSERT(fileDesc);

  return fileDesc;
}

class WorkerPermissionChallenge;

// This class calles WorkerPermissionChallenge::OperationCompleted() in the
// worker thread.
class WorkerPermissionOperationCompleted final : public WorkerControlRunnable {
  RefPtr<WorkerPermissionChallenge> mChallenge;

 public:
  WorkerPermissionOperationCompleted(WorkerPrivate* aWorkerPrivate,
                                     WorkerPermissionChallenge* aChallenge)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mChallenge(aChallenge) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override;
};

// This class used to do prompting in the main thread and main process.
class WorkerPermissionRequest final : public PermissionRequestBase {
  RefPtr<WorkerPermissionChallenge> mChallenge;

 public:
  WorkerPermissionRequest(Element* aElement, nsIPrincipal* aPrincipal,
                          WorkerPermissionChallenge* aChallenge)
      : PermissionRequestBase(aElement, aPrincipal), mChallenge(aChallenge) {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aChallenge);
  }

 private:
  ~WorkerPermissionRequest() { MOZ_ASSERT(NS_IsMainThread()); }

  virtual void OnPromptComplete(PermissionValue aPermissionValue) override;
};

class WorkerPermissionChallenge final : public Runnable {
 public:
  WorkerPermissionChallenge(WorkerPrivate* aWorkerPrivate,
                            BackgroundFactoryRequestChild* aActor,
                            SafeRefPtr<IDBFactory> aFactory,
                            PrincipalInfo&& aPrincipalInfo)
      : Runnable("indexedDB::WorkerPermissionChallenge"),
        mWorkerPrivate(aWorkerPrivate),
        mActor(aActor),
        mFactory(std::move(aFactory)),
        mPrincipalInfo(std::move(aPrincipalInfo)) {
    MOZ_ASSERT(mWorkerPrivate);
    MOZ_ASSERT(aActor);
    MOZ_ASSERT(mFactory);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool Dispatch() {
    mWorkerPrivate->AssertIsOnWorkerThread();
    if (NS_WARN_IF(!mWorkerPrivate->ModifyBusyCountFromWorker(true))) {
      return false;
    }

    if (NS_WARN_IF(NS_FAILED(mWorkerPrivate->DispatchToMainThread(this)))) {
      mWorkerPrivate->ModifyBusyCountFromWorker(false);
      return false;
    }

    return true;
  }

  NS_IMETHOD
  Run() override {
    const bool completed = RunInternal();
    if (completed) {
      OperationCompleted();
    }

    return NS_OK;
  }

  void OperationCompleted() {
    if (NS_IsMainThread()) {
      const RefPtr<WorkerPermissionOperationCompleted> runnable =
          new WorkerPermissionOperationCompleted(mWorkerPrivate, this);

      MOZ_ALWAYS_TRUE(runnable->Dispatch());
      return;
    }

    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    MaybeCollectGarbageOnIPCMessage();

    const SafeRefPtr<IDBFactory> factory = std::move(mFactory);
    Unused << factory;  // XXX see Bug 1605075

    mActor->SendPermissionRetry();
    mActor = nullptr;

    mWorkerPrivate->AssertIsOnWorkerThread();
    mWorkerPrivate->ModifyBusyCountFromWorker(false);
  }

 private:
  bool RunInternal() {
    MOZ_ASSERT(NS_IsMainThread());

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    nsPIDOMWindowInner* const window = wp->GetWindow();
    if (!window) {
      return true;
    }

    nsresult rv;
    const nsCOMPtr<nsIPrincipal> principal =
        mozilla::ipc::PrincipalInfoToPrincipal(mPrincipalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return true;
    }

    if (XRE_IsParentProcess()) {
      const nsCOMPtr<Element> ownerElement =
          do_QueryInterface(window->GetChromeEventHandler());
      if (NS_WARN_IF(!ownerElement)) {
        return true;
      }

      RefPtr<WorkerPermissionRequest> helper =
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

    BrowserChild* browserChild = BrowserChild::GetFrom(window);
    MOZ_ASSERT(browserChild);

    RefPtr<WorkerPermissionChallenge> self(this);
    browserChild->SendIndexedDBPermissionRequest(principal)->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self](const uint32_t& aPermission) { self->OperationCompleted(); },
        [](const mozilla::ipc::ResponseRejectReason) {});
    return false;
  }

 private:
  WorkerPrivate* const mWorkerPrivate;
  BackgroundFactoryRequestChild* mActor;
  SafeRefPtr<IDBFactory> mFactory;
  const PrincipalInfo mPrincipalInfo;
};

void WorkerPermissionRequest::OnPromptComplete(
    PermissionValue aPermissionValue) {
  MOZ_ASSERT(NS_IsMainThread());
  mChallenge->OperationCompleted();
}

bool WorkerPermissionOperationCompleted::WorkerRun(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  mChallenge->OperationCompleted();
  return true;
}

class MOZ_STACK_CLASS AutoSetCurrentFileHandle final {
  typedef mozilla::ipc::BackgroundChildImpl BackgroundChildImpl;

  IDBFileHandle* const mFileHandle;
  IDBFileHandle* mPreviousFileHandle;
  IDBFileHandle** mThreadLocalSlot;

 public:
  explicit AutoSetCurrentFileHandle(IDBFileHandle* aFileHandle)
      : mFileHandle(aFileHandle),
        mPreviousFileHandle(nullptr),
        mThreadLocalSlot(nullptr) {
    if (aFileHandle) {
      BackgroundChildImpl::ThreadLocal* threadLocal =
          BackgroundChildImpl::GetThreadLocalForCurrentThread();
      MOZ_ASSERT(threadLocal);

      // Hang onto this location for resetting later.
      mThreadLocalSlot = &threadLocal->mCurrentFileHandle;

      // Save the current value.
      mPreviousFileHandle = *mThreadLocalSlot;

      // Set the new value.
      *mThreadLocalSlot = aFileHandle;
    }
  }

  ~AutoSetCurrentFileHandle() {
    MOZ_ASSERT_IF(mThreadLocalSlot, mFileHandle);
    MOZ_ASSERT_IF(mThreadLocalSlot, *mThreadLocalSlot == mFileHandle);

    if (mThreadLocalSlot) {
      // Reset old value.
      *mThreadLocalSlot = mPreviousFileHandle;
    }
  }

  IDBFileHandle* FileHandle() const { return mFileHandle; }
};

class MOZ_STACK_CLASS FileHandleResultHelper final
    : public IDBFileRequest::ResultCallback {
  IDBFileRequest* const mFileRequest;
  AutoSetCurrentFileHandle mAutoFileHandle;

  union {
    File* mFile;
    const nsCString* mString;
    const FileRequestMetadata* mMetadata;
    const JS::Handle<JS::Value>* mJSValHandle;
  } mResult;

  enum {
    ResultTypeFile,
    ResultTypeString,
    ResultTypeMetadata,
    ResultTypeJSValHandle,
  } mResultType;

 public:
  FileHandleResultHelper(IDBFileRequest* aFileRequest,
                         IDBFileHandle* aFileHandle, File* aResult)
      : mFileRequest(aFileRequest),
        mAutoFileHandle(aFileHandle),
        mResultType(ResultTypeFile) {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mFile = aResult;
  }

  FileHandleResultHelper(IDBFileRequest* aFileRequest,
                         IDBFileHandle* aFileHandle, const nsCString* aResult)
      : mFileRequest(aFileRequest),
        mAutoFileHandle(aFileHandle),
        mResultType(ResultTypeString) {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mString = aResult;
  }

  FileHandleResultHelper(IDBFileRequest* aFileRequest,
                         IDBFileHandle* aFileHandle,
                         const FileRequestMetadata* aResult)
      : mFileRequest(aFileRequest),
        mAutoFileHandle(aFileHandle),
        mResultType(ResultTypeMetadata) {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mMetadata = aResult;
  }

  FileHandleResultHelper(IDBFileRequest* aFileRequest,
                         IDBFileHandle* aFileHandle,
                         const JS::Handle<JS::Value>* aResult)
      : mFileRequest(aFileRequest),
        mAutoFileHandle(aFileHandle),
        mResultType(ResultTypeJSValHandle) {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mJSValHandle = aResult;
  }

  IDBFileRequest* FileRequest() const { return mFileRequest; }

  IDBFileHandle* FileHandle() const { return mAutoFileHandle.FileHandle(); }

  virtual nsresult GetResult(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aResult) override {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(mFileRequest);

    switch (mResultType) {
      case ResultTypeFile:
        return GetResult(aCx, mResult.mFile, aResult);

      case ResultTypeString:
        return GetResult(aCx, mResult.mString, aResult);

      case ResultTypeMetadata:
        return GetResult(aCx, mResult.mMetadata, aResult);

      case ResultTypeJSValHandle:
        aResult.set(*mResult.mJSValHandle);
        return NS_OK;

      default:
        MOZ_CRASH("Unknown result type!");
    }

    MOZ_CRASH("Should never get here!");
  }

 private:
  nsresult GetResult(JSContext* aCx, File* aFile,
                     JS::MutableHandle<JS::Value> aResult) {
    const bool ok = GetOrCreateDOMReflector(aCx, aFile, aResult);
    if (NS_WARN_IF(!ok)) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    return NS_OK;
  }

  nsresult GetResult(JSContext* aCx, const nsCString* aString,
                     JS::MutableHandle<JS::Value> aResult) {
    const nsCString& data = *aString;

    nsresult rv;

    if (!mFileRequest->HasEncoding()) {
      JS::Rooted<JSObject*> arrayBuffer(aCx);
      rv = nsContentUtils::CreateArrayBuffer(aCx, data, arrayBuffer.address());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }

      aResult.setObject(*arrayBuffer);
      return NS_OK;
    }

    // Try the API argument.
    const Encoding* encoding = Encoding::ForLabel(mFileRequest->GetEncoding());
    if (!encoding) {
      // API argument failed. Since we are dealing with a file system file,
      // we don't have a meaningful type attribute for the blob available,
      // so proceeding to the next step, which is defaulting to UTF-8.
      encoding = UTF_8_ENCODING;
    }

    nsString tmpString;
    Tie(rv, encoding) = encoding->Decode(data, tmpString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    if (NS_WARN_IF(!xpc::StringToJsval(aCx, tmpString, aResult))) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    return NS_OK;
  }

  nsresult GetResult(JSContext* aCx, const FileRequestMetadata* aMetadata,
                     JS::MutableHandle<JS::Value> aResult) {
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!obj)) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    const Maybe<uint64_t>& size = aMetadata->size();
    if (size.isSome()) {
      JS::Rooted<JS::Value> number(aCx, JS_NumberValue(size.value()));

      if (NS_WARN_IF(!JS_DefineProperty(aCx, obj, "size", number, 0))) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }
    }

    const Maybe<int64_t>& lastModified = aMetadata->lastModified();
    if (lastModified.isSome()) {
      JS::Rooted<JSObject*> date(
          aCx, JS::NewDateObject(aCx, JS::TimeClip(lastModified.value())));
      if (NS_WARN_IF(!date)) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }

      if (NS_WARN_IF(!JS_DefineProperty(aCx, obj, "lastModified", date, 0))) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }
    }

    aResult.setObject(*obj);
    return NS_OK;
  }
};

void DispatchFileHandleErrorEvent(IDBFileRequest* aFileRequest,
                                  nsresult aErrorCode,
                                  IDBFileHandle* aFileHandle) {
  MOZ_ASSERT(aFileRequest);
  aFileRequest->AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aErrorCode));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aErrorCode) == NS_ERROR_MODULE_DOM_FILEHANDLE);
  MOZ_ASSERT(aFileHandle);

  const RefPtr<IDBFileRequest> fileRequest = aFileRequest;
  const RefPtr<IDBFileHandle> fileHandle = aFileHandle;

  AutoSetCurrentFileHandle ascfh(aFileHandle);

  fileRequest->FireError(aErrorCode);

  MOZ_ASSERT(fileHandle->IsOpen() || fileHandle->IsAborted());
}

void DispatchFileHandleSuccessEvent(FileHandleResultHelper* aResultHelper) {
  MOZ_ASSERT(aResultHelper);

  const RefPtr<IDBFileRequest> fileRequest = aResultHelper->FileRequest();
  MOZ_ASSERT(fileRequest);
  fileRequest->AssertIsOnOwningThread();

  const RefPtr<IDBFileHandle> fileHandle = aResultHelper->FileHandle();
  MOZ_ASSERT(fileHandle);

  if (fileHandle->IsAborted()) {
    fileRequest->FireError(NS_ERROR_DOM_FILEHANDLE_ABORT_ERR);
    return;
  }

  MOZ_ASSERT(fileHandle->IsOpen());

  fileRequest->SetResultCallback(aResultHelper);

  MOZ_ASSERT(fileHandle->IsOpen() || fileHandle->IsAborted());
}

auto GetKeyOperator(const IDBCursorDirection aDirection) {
  switch (aDirection) {
    case IDBCursorDirection::Next:
    case IDBCursorDirection::Nextunique:
      return &Key::operator>=;
    case IDBCursorDirection::Prev:
    case IDBCursorDirection::Prevunique:
      return &Key::operator<=;
    default:
      MOZ_CRASH("Should never get here.");
  }
}

// Does not need to be threadsafe since this only runs on one thread, but
// inheriting from CancelableRunnable is easy.
template <typename T>
class DelayedActionRunnable final : public CancelableRunnable {
  using ActionFunc = void (T::*)();

  T* mActor;
  RefPtr<IDBRequest> mRequest;
  ActionFunc mActionFunc;

 public:
  explicit DelayedActionRunnable(T* aActor, ActionFunc aActionFunc)
      : CancelableRunnable("indexedDB::DelayedActionRunnable"),
        mActor(aActor),
        mRequest(aActor->GetRequest()),
        mActionFunc(aActionFunc) {
    MOZ_ASSERT(aActor);
    aActor->AssertIsOnOwningThread();
    MOZ_ASSERT(mRequest);
    MOZ_ASSERT(mActionFunc);
  }

 private:
  ~DelayedActionRunnable() = default;

  NS_DECL_NSIRUNNABLE
  nsresult Cancel() override;
};

}  // namespace

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

// CancelableRunnable is used to make workers happy.
class BackgroundRequestChild::PreprocessHelper final
    : public CancelableRunnable,
      public nsIInputStreamCallback,
      public nsIFileMetadataCallback {
  enum class State {
    // Just created on the owning thread, dispatched to the thread pool. Next
    // step is either Finishing if stream was ready to be read or
    // WaitingForStreamReady if the stream is not ready.
    Initial,

    // Waiting for stream to be ready on a thread pool thread. Next state is
    // Finishing.
    WaitingForStreamReady,

    // Waiting to finish/finishing on the owning thread. Next step is Completed.
    Finishing,

    // All done.
    Completed
  };

  const nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  RefPtr<TaskQueue> mTaskQueue;
  nsCOMPtr<nsIEventTarget> mTaskQueueEventTarget;
  nsCOMPtr<nsIInputStream> mStream;
  UniquePtr<JSStructuredCloneData> mCloneData;
  BackgroundRequestChild* mActor;
  const uint32_t mCloneDataIndex;
  nsresult mResultCode;
  State mState;

 public:
  PreprocessHelper(uint32_t aCloneDataIndex, BackgroundRequestChild* aActor)
      : CancelableRunnable(
            "indexedDB::BackgroundRequestChild::PreprocessHelper"),
        mOwningEventTarget(aActor->GetActorEventTarget()),
        mActor(aActor),
        mCloneDataIndex(aCloneDataIndex),
        mResultCode(NS_OK),
        mState(State::Initial) {
    AssertIsOnOwningThread();
    MOZ_ASSERT(aActor);
    aActor->AssertIsOnOwningThread();
  }

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const { MOZ_ASSERT(IsOnOwningThread()); }

  void ClearActor() {
    AssertIsOnOwningThread();

    mActor = nullptr;
  }

  nsresult Init(const StructuredCloneFileChild& aFile);

  nsresult Dispatch();

 private:
  ~PreprocessHelper() {
    MOZ_ASSERT(mState == State::Initial || mState == State::Completed);

    if (mTaskQueue) {
      mTaskQueue->BeginShutdown();
    }
  }

  nsresult Start();

  nsresult ProcessStream();

  void Finish();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIFILEMETADATACALLBACK
};

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

void PermissionRequestMainProcessHelper::OnPromptComplete(
    PermissionValue aPermissionValue) {
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  mActor->SendPermissionRetry();

  mActor = nullptr;
  mFactory = nullptr;
}

/*******************************************************************************
 * BackgroundRequestChildBase
 ******************************************************************************/

BackgroundRequestChildBase::BackgroundRequestChildBase(IDBRequest* aRequest)
    : mRequest(aRequest) {
  MOZ_ASSERT(aRequest);
  aRequest->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundRequestChildBase);
}

BackgroundRequestChildBase::~BackgroundRequestChildBase() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(indexedDB::BackgroundRequestChildBase);
}

#ifdef DEBUG

void BackgroundRequestChildBase::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif  // DEBUG

/*******************************************************************************
 * BackgroundFactoryChild
 ******************************************************************************/

BackgroundFactoryChild::BackgroundFactoryChild(IDBFactory& aFactory)
    : mFactory(&aFactory) {
  AssertIsOnOwningThread();
  mFactory->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundFactoryChild);
}

BackgroundFactoryChild::~BackgroundFactoryChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundFactoryChild);
}

void BackgroundFactoryChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mFactory) {
    mFactory->ClearBackgroundActor();
    mFactory = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundIDBFactoryChild::SendDeleteMe());
  }
}

void BackgroundFactoryChild::ActorDestroy(ActorDestroyReason aWhy) {
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
    const FactoryRequestParams& aParams) {
  MOZ_CRASH(
      "PBackgroundIDBFactoryRequestChild actors should be manually "
      "constructed!");
}

bool BackgroundFactoryChild::DeallocPBackgroundIDBFactoryRequestChild(
    PBackgroundIDBFactoryRequestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundFactoryRequestChild*>(aActor);
  return true;
}

PBackgroundIDBDatabaseChild*
BackgroundFactoryChild::AllocPBackgroundIDBDatabaseChild(
    const DatabaseSpec& aSpec, PBackgroundIDBFactoryRequestChild* aRequest) {
  AssertIsOnOwningThread();

  auto* const request = static_cast<BackgroundFactoryRequestChild*>(aRequest);
  MOZ_ASSERT(request);

  return new BackgroundDatabaseChild(aSpec, request);
}

bool BackgroundFactoryChild::DeallocPBackgroundIDBDatabaseChild(
    PBackgroundIDBDatabaseChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundDatabaseChild*>(aActor);
  return true;
}

mozilla::ipc::IPCResult
BackgroundFactoryChild::RecvPBackgroundIDBDatabaseConstructor(
    PBackgroundIDBDatabaseChild* aActor, const DatabaseSpec& aSpec,
    PBackgroundIDBFactoryRequestChild* aRequest) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aActor->GetActorEventTarget(),
             "The event target shall be inherited from its manager actor.");

  return IPC_OK();
}

/*******************************************************************************
 * BackgroundFactoryRequestChild
 ******************************************************************************/

BackgroundFactoryRequestChild::BackgroundFactoryRequestChild(
    SafeRefPtr<IDBFactory> aFactory, IDBOpenDBRequest* aOpenRequest,
    bool aIsDeleteOp, uint64_t aRequestedVersion)
    : BackgroundRequestChildBase(aOpenRequest),
      mFactory(std::move(aFactory)),
      mDatabaseActor(nullptr),
      mRequestedVersion(aRequestedVersion),
      mIsDeleteOp(aIsDeleteOp) {
  // Can't assert owning thread here because IPDL has not yet set our manager!
  MOZ_ASSERT(mFactory);
  mFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aOpenRequest);

  MOZ_COUNT_CTOR(indexedDB::BackgroundFactoryRequestChild);
}

BackgroundFactoryRequestChild::~BackgroundFactoryRequestChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundFactoryRequestChild);
}

IDBOpenDBRequest* BackgroundFactoryRequestChild::GetOpenDBRequest() const {
  AssertIsOnOwningThread();

  return static_cast<IDBOpenDBRequest*>(mRequest.get());
}

void BackgroundFactoryRequestChild::SetDatabaseActor(
    BackgroundDatabaseChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aActor || !mDatabaseActor);

  mDatabaseActor = aActor;
}

bool BackgroundFactoryRequestChild::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);

  mRequest->Reset();

  DispatchErrorEvent(mRequest, aResponse);

  if (mDatabaseActor) {
    mDatabaseActor->ReleaseDOMObject();
    MOZ_ASSERT(!mDatabaseActor);
  }

  return true;
}

bool BackgroundFactoryRequestChild::HandleResponse(
    const OpenDatabaseRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  mRequest->Reset();

  auto databaseActor =
      static_cast<BackgroundDatabaseChild*>(aResponse.databaseChild());
  MOZ_ASSERT(databaseActor);

  IDBDatabase* database = databaseActor->GetDOMObject();
  if (!database) {
    databaseActor->EnsureDOMObject();
    MOZ_ASSERT(mDatabaseActor);

    database = databaseActor->GetDOMObject();
    MOZ_ASSERT(database);

    MOZ_ASSERT(!database->IsClosed());
  }

  MOZ_ASSERT(mDatabaseActor == databaseActor);

  if (database->IsClosed()) {
    // If the database was closed already, which is only possible if we fired an
    // "upgradeneeded" event, then we shouldn't fire a "success" event here.
    // Instead we fire an error event with AbortErr.
    DispatchErrorEvent(mRequest, NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  } else {
    ResultHelper helper(mRequest, nullptr, database);

    helper.DispatchSuccessEvent();
  }

  databaseActor->ReleaseDOMObject();
  MOZ_ASSERT(!mDatabaseActor);

  return true;
}

bool BackgroundFactoryRequestChild::HandleResponse(
    const DeleteDatabaseRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, nullptr, &JS::UndefinedHandleValue);

  RefPtr<Event> successEvent = IDBVersionChangeEvent::Create(
      mRequest, nsDependentString(kSuccessEventType),
      aResponse.previousVersion());
  MOZ_ASSERT(successEvent);

  helper.DispatchSuccessEvent(std::move(successEvent));

  MOZ_ASSERT(!mDatabaseActor);

  return true;
}

void BackgroundFactoryRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (aWhy != Deletion) {
    IDBOpenDBRequest* openRequest = GetOpenDBRequest();
    if (openRequest) {
      openRequest->NoteComplete();
    }
  }
}

mozilla::ipc::IPCResult BackgroundFactoryRequestChild::Recv__delete__(
    const FactoryRequestResponse& aResponse) {
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
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundFactoryRequestChild::RecvPermissionChallenge(
    PrincipalInfo&& aPrincipalInfo) {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    RefPtr<WorkerPermissionChallenge> challenge = new WorkerPermissionChallenge(
        workerPrivate, this, mFactory.clonePtr(), std::move(aPrincipalInfo));
    if (!challenge->Dispatch()) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
      mozilla::ipc::PrincipalInfoToPrincipal(aPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIGlobalObject> global = mFactory->GetParentObject();
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
    MOZ_ASSERT(window);

    nsCOMPtr<Element> ownerElement =
        do_QueryInterface(window->GetChromeEventHandler());
    if (NS_WARN_IF(!ownerElement)) {
      // If this fails, the page was navigated. Fail the permission check by
      // forcing an immediate retry.
      if (!SendPermissionRetry()) {
        return IPC_FAIL_NO_REASON(this);
      }
      return IPC_OK();
    }

    RefPtr<PermissionRequestMainProcessHelper> helper =
        new PermissionRequestMainProcessHelper(this, mFactory.clonePtr(),
                                               ownerElement, principal);

    PermissionRequestBase::PermissionValue permission;
    if (NS_WARN_IF(NS_FAILED(helper->PromptIfNeeded(&permission)))) {
      return IPC_FAIL_NO_REASON(this);
    }

    MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
               permission == PermissionRequestBase::kPermissionDenied ||
               permission == PermissionRequestBase::kPermissionPrompt);

    if (permission != PermissionRequestBase::kPermissionPrompt) {
      SendPermissionRetry();
    }
    return IPC_OK();
  }

  RefPtr<BrowserChild> browserChild = mFactory->GetBrowserChild();
  MOZ_ASSERT(browserChild);

  browserChild->SendIndexedDBPermissionRequest(principal)->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [this](const uint32_t& aPermission) {
        this->AssertIsOnOwningThread();
        MaybeCollectGarbageOnIPCMessage();
        this->SendPermissionRetry();
      },
      [](const mozilla::ipc::ResponseRejectReason) {});

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundFactoryRequestChild::RecvBlocked(
    const uint64_t aCurrentVersion) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  MaybeCollectGarbageOnIPCMessage();

  const nsDependentString type(kBlockedEventType);

  RefPtr<Event> blockedEvent;
  if (mIsDeleteOp) {
    blockedEvent =
        IDBVersionChangeEvent::Create(mRequest, type, aCurrentVersion);
    MOZ_ASSERT(blockedEvent);
  } else {
    blockedEvent = IDBVersionChangeEvent::Create(
        mRequest, type, aCurrentVersion, mRequestedVersion);
    MOZ_ASSERT(blockedEvent);
  }

  RefPtr<IDBRequest> kungFuDeathGrip = mRequest;

  IDB_LOG_MARK_CHILD_REQUEST("Firing \"blocked\" event", "\"blocked\"",
                             kungFuDeathGrip->LoggingSerialNumber());

  IgnoredErrorResult rv;
  kungFuDeathGrip->DispatchEvent(*blockedEvent, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to dispatch event!");
  }

  return IPC_OK();
}

/*******************************************************************************
 * BackgroundDatabaseChild
 ******************************************************************************/

BackgroundDatabaseChild::BackgroundDatabaseChild(
    const DatabaseSpec& aSpec, BackgroundFactoryRequestChild* aOpenRequestActor)
    : mSpec(MakeUnique<DatabaseSpec>(aSpec)),
      mOpenRequestActor(aOpenRequestActor),
      mDatabase(nullptr) {
  // Can't assert owning thread here because IPDL has not yet set our manager!
  MOZ_ASSERT(aOpenRequestActor);

  MOZ_COUNT_CTOR(indexedDB::BackgroundDatabaseChild);
}

BackgroundDatabaseChild::~BackgroundDatabaseChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundDatabaseChild);
}

#ifdef DEBUG

void BackgroundDatabaseChild::AssertIsOnOwningThread() const {
  static_cast<BackgroundFactoryChild*>(Manager())->AssertIsOnOwningThread();
}

#endif  // DEBUG

void BackgroundDatabaseChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mTemporaryStrongDatabase);
  MOZ_ASSERT(!mOpenRequestActor);

  if (mDatabase) {
    mDatabase->ClearBackgroundActor();
    mDatabase = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundIDBDatabaseChild::SendDeleteMe());
  }
}

void BackgroundDatabaseChild::EnsureDOMObject() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenRequestActor);

  if (mTemporaryStrongDatabase) {
    MOZ_ASSERT(!mSpec);
    return;
  }

  MOZ_ASSERT(mSpec);

  auto request = mOpenRequestActor->GetOpenDBRequest();
  MOZ_ASSERT(request);

  auto& factory =
      static_cast<BackgroundFactoryChild*>(Manager())->GetDOMObject();

  // TODO: This AcquireStrongRefFromRawPtr looks suspicious. This should be
  // changed or at least well explained, see also comment on
  // BackgroundFactoryChild.
  mTemporaryStrongDatabase = IDBDatabase::Create(
      request, SafeRefPtr{&factory, AcquireStrongRefFromRawPtr{}}, this,
      std::move(mSpec));

  MOZ_ASSERT(mTemporaryStrongDatabase);
  mTemporaryStrongDatabase->AssertIsOnOwningThread();

  mDatabase = mTemporaryStrongDatabase;

  mOpenRequestActor->SetDatabaseActor(this);
}

void BackgroundDatabaseChild::ReleaseDOMObject() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTemporaryStrongDatabase);
  mTemporaryStrongDatabase->AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenRequestActor);
  MOZ_ASSERT(mDatabase == mTemporaryStrongDatabase);

  mOpenRequestActor->SetDatabaseActor(nullptr);

  mOpenRequestActor = nullptr;

  // This may be the final reference to the IDBDatabase object so we may end up
  // calling SendDeleteMeInternal() here. Make sure everything is cleaned up
  // properly before proceeding.
  mTemporaryStrongDatabase = nullptr;
}

void BackgroundDatabaseChild::ActorDestroy(ActorDestroyReason aWhy) {
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
    const IPCBlob& aIPCBlob) {
  MOZ_CRASH("PBackgroundIDBFileChild actors should be manually constructed!");
}

bool BackgroundDatabaseChild::DeallocPBackgroundIDBDatabaseFileChild(
    PBackgroundIDBDatabaseFileChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

PBackgroundIDBDatabaseRequestChild*
BackgroundDatabaseChild::AllocPBackgroundIDBDatabaseRequestChild(
    const DatabaseRequestParams& aParams) {
  MOZ_CRASH(
      "PBackgroundIDBDatabaseRequestChild actors should be manually "
      "constructed!");
}

bool BackgroundDatabaseChild::DeallocPBackgroundIDBDatabaseRequestChild(
    PBackgroundIDBDatabaseRequestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundDatabaseRequestChild*>(aActor);
  return true;
}

PBackgroundIDBTransactionChild*
BackgroundDatabaseChild::AllocPBackgroundIDBTransactionChild(
    const nsTArray<nsString>& aObjectStoreNames, const Mode& aMode) {
  MOZ_CRASH(
      "PBackgroundIDBTransactionChild actors should be manually "
      "constructed!");
}

bool BackgroundDatabaseChild::DeallocPBackgroundIDBTransactionChild(
    PBackgroundIDBTransactionChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundTransactionChild*>(aActor);
  return true;
}

PBackgroundIDBVersionChangeTransactionChild*
BackgroundDatabaseChild::AllocPBackgroundIDBVersionChangeTransactionChild(
    const uint64_t aCurrentVersion, const uint64_t aRequestedVersion,
    const int64_t aNextObjectStoreId, const int64_t aNextIndexId) {
  AssertIsOnOwningThread();

  IDBOpenDBRequest* request = mOpenRequestActor->GetOpenDBRequest();
  MOZ_ASSERT(request);

  return new BackgroundVersionChangeTransactionChild(request);
}

mozilla::ipc::IPCResult
BackgroundDatabaseChild::RecvPBackgroundIDBVersionChangeTransactionConstructor(
    PBackgroundIDBVersionChangeTransactionChild* aActor,
    const uint64_t& aCurrentVersion, const uint64_t& aRequestedVersion,
    const int64_t& aNextObjectStoreId, const int64_t& aNextIndexId) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aActor->GetActorEventTarget(),
             "The event target shall be inherited from its manager actor.");
  MOZ_ASSERT(mOpenRequestActor);

  MaybeCollectGarbageOnIPCMessage();

  EnsureDOMObject();

  auto* actor = static_cast<BackgroundVersionChangeTransactionChild*>(aActor);

  RefPtr<IDBOpenDBRequest> request = mOpenRequestActor->GetOpenDBRequest();
  MOZ_ASSERT(request);

  SafeRefPtr<IDBTransaction> transaction = IDBTransaction::CreateVersionChange(
      mDatabase, actor, request, aNextObjectStoreId, aNextIndexId);
  MOZ_ASSERT(transaction);

  transaction->AssertIsOnOwningThread();

  actor->SetDOMTransaction(transaction.clonePtr());

  mDatabase->EnterSetVersionTransaction(aRequestedVersion);

  request->SetTransaction(transaction.clonePtr());

  RefPtr<Event> upgradeNeededEvent = IDBVersionChangeEvent::Create(
      request, nsDependentString(kUpgradeNeededEventType), aCurrentVersion,
      aRequestedVersion);
  MOZ_ASSERT(upgradeNeededEvent);

  ResultHelper helper(request, std::move(transaction), mDatabase);

  helper.DispatchSuccessEvent(std::move(upgradeNeededEvent));

  return IPC_OK();
}

bool BackgroundDatabaseChild::
    DeallocPBackgroundIDBVersionChangeTransactionChild(
        PBackgroundIDBVersionChangeTransactionChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundVersionChangeTransactionChild*>(aActor);
  return true;
}

PBackgroundMutableFileChild*
BackgroundDatabaseChild::AllocPBackgroundMutableFileChild(
    const nsString& aName, const nsString& aType) {
  AssertIsOnOwningThread();

  return new BackgroundMutableFileChild(aName, aType);
}

bool BackgroundDatabaseChild::DeallocPBackgroundMutableFileChild(
    PBackgroundMutableFileChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundMutableFileChild*>(aActor);
  return true;
}

mozilla::ipc::IPCResult BackgroundDatabaseChild::RecvVersionChange(
    const uint64_t aOldVersion, const Maybe<uint64_t> aNewVersion) {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (!mDatabase || mDatabase->IsClosed()) {
    return IPC_OK();
  }

  RefPtr<IDBDatabase> kungFuDeathGrip = mDatabase;

  // Handle bfcache'd windows.
  if (nsPIDOMWindowInner* owner = kungFuDeathGrip->GetOwner()) {
    // The database must be closed if the window is already frozen.
    bool shouldAbortAndClose = owner->IsFrozen();

    // Anything in the bfcache has to be evicted and then we have to close the
    // database also.
    if (nsCOMPtr<Document> doc = owner->GetExtantDoc()) {
      if (nsCOMPtr<nsIBFCacheEntry> bfCacheEntry = doc->GetBFCacheEntry()) {
        bfCacheEntry->RemoveFromBFCacheSync();
        shouldAbortAndClose = true;
      }
    }

    if (shouldAbortAndClose) {
      // Invalidate() doesn't close the database in the parent, so we have
      // to call Close() and AbortTransactions() manually.
      kungFuDeathGrip->AbortTransactions(/* aShouldWarn */ false);
      kungFuDeathGrip->Close();
      return IPC_OK();
    }
  }

  // Otherwise fire a versionchange event.
  const nsDependentString type(kVersionChangeEventType);

  RefPtr<Event> versionChangeEvent;

  if (aNewVersion.isNothing()) {
    versionChangeEvent =
        IDBVersionChangeEvent::Create(kungFuDeathGrip, type, aOldVersion);
    MOZ_ASSERT(versionChangeEvent);
  } else {
    versionChangeEvent = IDBVersionChangeEvent::Create(
        kungFuDeathGrip, type, aOldVersion, aNewVersion.value());
    MOZ_ASSERT(versionChangeEvent);
  }

  IDB_LOG_MARK("Child : Firing \"versionchange\" event",
               "C: IDBDatabase \"versionchange\" event", IDB_LOG_ID_STRING());

  IgnoredErrorResult rv;
  kungFuDeathGrip->DispatchEvent(*versionChangeEvent, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to dispatch event!");
  }

  if (!kungFuDeathGrip->IsClosed()) {
    SendBlocked();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundDatabaseChild::RecvInvalidate() {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (mDatabase) {
    mDatabase->Invalidate();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundDatabaseChild::RecvCloseAfterInvalidationComplete() {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (mDatabase) {
    mDatabase->DispatchTrustedEvent(nsDependentString(kCloseEventType));
  }

  return IPC_OK();
}

/*******************************************************************************
 * BackgroundDatabaseRequestChild
 ******************************************************************************/

BackgroundDatabaseRequestChild::BackgroundDatabaseRequestChild(
    IDBDatabase* aDatabase, IDBRequest* aRequest)
    : BackgroundRequestChildBase(aRequest), mDatabase(aDatabase) {
  // Can't assert owning thread here because IPDL has not yet set our manager!
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();
  MOZ_ASSERT(aRequest);

  MOZ_COUNT_CTOR(indexedDB::BackgroundDatabaseRequestChild);
}

BackgroundDatabaseRequestChild::~BackgroundDatabaseRequestChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundDatabaseRequestChild);
}

bool BackgroundDatabaseRequestChild::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);

  mRequest->Reset();

  DispatchErrorEvent(mRequest, aResponse);

  return true;
}

bool BackgroundDatabaseRequestChild::HandleResponse(
    const CreateFileRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  mRequest->Reset();

  auto mutableFileActor =
      static_cast<BackgroundMutableFileChild*>(aResponse.mutableFileChild());
  MOZ_ASSERT(mutableFileActor);

  mutableFileActor->EnsureDOMObject();

  auto mutableFile =
      static_cast<IDBMutableFile*>(mutableFileActor->GetDOMObject());
  MOZ_ASSERT(mutableFile);

  ResultHelper helper(mRequest, nullptr, mutableFile);

  helper.DispatchSuccessEvent();

  mutableFileActor->ReleaseDOMObject();

  return true;
}

mozilla::ipc::IPCResult BackgroundDatabaseRequestChild::Recv__delete__(
    const DatabaseRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  switch (aResponse.type()) {
    case DatabaseRequestResponse::Tnsresult:
      if (!HandleResponse(aResponse.get_nsresult())) {
        return IPC_FAIL_NO_REASON(this);
      }
      return IPC_OK();

    case DatabaseRequestResponse::TCreateFileRequestResponse:
      if (!HandleResponse(aResponse.get_CreateFileRequestResponse())) {
        return IPC_FAIL_NO_REASON(this);
      }
      return IPC_OK();

    default:
      MOZ_CRASH("Unknown response type!");
  }

  MOZ_CRASH("Should never get here!");
}

/*******************************************************************************
 * BackgroundTransactionBase
 ******************************************************************************/

BackgroundTransactionBase::BackgroundTransactionBase(
    SafeRefPtr<IDBTransaction> aTransaction)
    : mTemporaryStrongTransaction(std::move(aTransaction)),
      mTransaction(mTemporaryStrongTransaction.unsafeGetRawPtr()) {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(BackgroundTransactionBase);
}

#ifdef DEBUG

void BackgroundTransactionBase::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnOwningThread();
}

#endif  // DEBUG

void BackgroundTransactionBase::NoteActorDestroyed() {
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

void BackgroundTransactionBase::SetDOMTransaction(
    SafeRefPtr<IDBTransaction> aTransaction) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnOwningThread();
  MOZ_ASSERT(!mTemporaryStrongTransaction);
  MOZ_ASSERT(!mTransaction);

  mTemporaryStrongTransaction = std::move(aTransaction);
  mTransaction = mTemporaryStrongTransaction.unsafeGetRawPtr();
}

void BackgroundTransactionBase::NoteComplete() {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mTransaction, mTemporaryStrongTransaction);

  mTemporaryStrongTransaction = nullptr;
}

/*******************************************************************************
 * BackgroundTransactionChild
 ******************************************************************************/

BackgroundTransactionChild::BackgroundTransactionChild(
    SafeRefPtr<IDBTransaction> aTransaction)
    : BackgroundTransactionBase(std::move(aTransaction)) {
  MOZ_COUNT_CTOR(indexedDB::BackgroundTransactionChild);
}

BackgroundTransactionChild::~BackgroundTransactionChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundTransactionChild);
}

#ifdef DEBUG

void BackgroundTransactionChild::AssertIsOnOwningThread() const {
  static_cast<BackgroundDatabaseChild*>(Manager())->AssertIsOnOwningThread();
}

#endif  // DEBUG

void BackgroundTransactionChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mTransaction) {
    NoteActorDestroyed();

    MOZ_ALWAYS_TRUE(PBackgroundIDBTransactionChild::SendDeleteMe());
  }
}

void BackgroundTransactionChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult BackgroundTransactionChild::RecvComplete(
    const nsresult aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  MaybeCollectGarbageOnIPCMessage();

  mTransaction->FireCompleteOrAbortEvents(aResult);

  NoteComplete();
  return IPC_OK();
}

PBackgroundIDBRequestChild*
BackgroundTransactionChild::AllocPBackgroundIDBRequestChild(
    const RequestParams& aParams) {
  MOZ_CRASH(
      "PBackgroundIDBRequestChild actors should be manually "
      "constructed!");
}

bool BackgroundTransactionChild::DeallocPBackgroundIDBRequestChild(
    PBackgroundIDBRequestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundRequestChild*>(aActor);
  return true;
}

PBackgroundIDBCursorChild*
BackgroundTransactionChild::AllocPBackgroundIDBCursorChild(
    const OpenCursorParams& aParams) {
  AssertIsOnOwningThread();

  MOZ_CRASH("PBackgroundIDBCursorChild actors should be manually constructed!");
}

bool BackgroundTransactionChild::DeallocPBackgroundIDBCursorChild(
    PBackgroundIDBCursorChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

/*******************************************************************************
 * BackgroundVersionChangeTransactionChild
 ******************************************************************************/

BackgroundVersionChangeTransactionChild::
    BackgroundVersionChangeTransactionChild(IDBOpenDBRequest* aOpenDBRequest)
    : mOpenDBRequest(aOpenDBRequest) {
  MOZ_ASSERT(aOpenDBRequest);
  aOpenDBRequest->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundVersionChangeTransactionChild);
}

BackgroundVersionChangeTransactionChild::
    ~BackgroundVersionChangeTransactionChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(indexedDB::BackgroundVersionChangeTransactionChild);
}

#ifdef DEBUG

void BackgroundVersionChangeTransactionChild::AssertIsOnOwningThread() const {
  static_cast<BackgroundDatabaseChild*>(Manager())->AssertIsOnOwningThread();
}

#endif  // DEBUG

void BackgroundVersionChangeTransactionChild::SendDeleteMeInternal(
    bool aFailedConstructor) {
  AssertIsOnOwningThread();

  if (mTransaction || aFailedConstructor) {
    NoteActorDestroyed();

    MOZ_ALWAYS_TRUE(
        PBackgroundIDBVersionChangeTransactionChild::SendDeleteMe());
  }
}

void BackgroundVersionChangeTransactionChild::ActorDestroy(
    ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  mOpenDBRequest = nullptr;

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult BackgroundVersionChangeTransactionChild::RecvComplete(
    const nsresult aResult) {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  if (!mTransaction) {
    return IPC_OK();
  }

  MOZ_ASSERT(mOpenDBRequest);

  IDBDatabase* database = mTransaction->Database();
  MOZ_ASSERT(database);

  database->ExitSetVersionTransaction();

  if (NS_FAILED(aResult)) {
    database->Close();
  }

  RefPtr<IDBOpenDBRequest> request = mOpenDBRequest;
  MOZ_ASSERT(request);

  mTransaction->FireCompleteOrAbortEvents(aResult);

  request->SetTransaction(nullptr);
  request = nullptr;

  mOpenDBRequest = nullptr;

  NoteComplete();
  return IPC_OK();
}

PBackgroundIDBRequestChild*
BackgroundVersionChangeTransactionChild::AllocPBackgroundIDBRequestChild(
    const RequestParams& aParams) {
  MOZ_CRASH(
      "PBackgroundIDBRequestChild actors should be manually "
      "constructed!");
}

bool BackgroundVersionChangeTransactionChild::DeallocPBackgroundIDBRequestChild(
    PBackgroundIDBRequestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundRequestChild*>(aActor);
  return true;
}

PBackgroundIDBCursorChild*
BackgroundVersionChangeTransactionChild::AllocPBackgroundIDBCursorChild(
    const OpenCursorParams& aParams) {
  AssertIsOnOwningThread();

  MOZ_CRASH("PBackgroundIDBCursorChild actors should be manually constructed!");
}

bool BackgroundVersionChangeTransactionChild::DeallocPBackgroundIDBCursorChild(
    PBackgroundIDBCursorChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

/*******************************************************************************
 * BackgroundMutableFileChild
 ******************************************************************************/

BackgroundMutableFileChild::BackgroundMutableFileChild(const nsAString& aName,
                                                       const nsAString& aType)
    : mMutableFile(nullptr), mName(aName), mType(aType) {
  // Can't assert owning thread here because IPDL has not yet set our manager!
  MOZ_COUNT_CTOR(indexedDB::BackgroundMutableFileChild);
}

BackgroundMutableFileChild::~BackgroundMutableFileChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundMutableFileChild);
}

#ifdef DEBUG

void BackgroundMutableFileChild::AssertIsOnOwningThread() const {
  static_cast<BackgroundDatabaseChild*>(Manager())->AssertIsOnOwningThread();
}

#endif  // DEBUG

void BackgroundMutableFileChild::EnsureDOMObject() {
  AssertIsOnOwningThread();

  if (mTemporaryStrongMutableFile) {
    return;
  }

  auto database =
      static_cast<BackgroundDatabaseChild*>(Manager())->GetDOMObject();
  MOZ_ASSERT(database);

  mTemporaryStrongMutableFile =
      new IDBMutableFile(database, this, mName, mType);

  MOZ_ASSERT(mTemporaryStrongMutableFile);
  mTemporaryStrongMutableFile->AssertIsOnOwningThread();

  mMutableFile = mTemporaryStrongMutableFile;
}

void BackgroundMutableFileChild::ReleaseDOMObject() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTemporaryStrongMutableFile);
  mTemporaryStrongMutableFile->AssertIsOnOwningThread();
  MOZ_ASSERT(mMutableFile == mTemporaryStrongMutableFile);

  mTemporaryStrongMutableFile = nullptr;
}

void BackgroundMutableFileChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mTemporaryStrongMutableFile);

  if (mMutableFile) {
    mMutableFile->ClearBackgroundActor();
    mMutableFile = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundMutableFileChild::SendDeleteMe());
  }
}

void BackgroundMutableFileChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mMutableFile) {
    mMutableFile->ClearBackgroundActor();
#ifdef DEBUG
    mMutableFile = nullptr;
#endif
  }
}

PBackgroundFileHandleChild*
BackgroundMutableFileChild::AllocPBackgroundFileHandleChild(
    const FileMode& aMode) {
  MOZ_CRASH(
      "PBackgroundFileHandleChild actors should be manually "
      "constructed!");
}

bool BackgroundMutableFileChild::DeallocPBackgroundFileHandleChild(
    PBackgroundFileHandleChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundFileHandleChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BackgroundRequestChild
 ******************************************************************************/

BackgroundRequestChild::BackgroundRequestChild(IDBRequest* aRequest)
    : BackgroundRequestChildBase(aRequest),
      mTransaction(aRequest->AcquireTransaction()),
      mRunningPreprocessHelpers(0),
      mCurrentCloneDataIndex(0),
      mPreprocessResultCode(NS_OK),
      mGetAll(false) {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundRequestChild);
}

BackgroundRequestChild::~BackgroundRequestChild() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mTransaction);

  MOZ_COUNT_DTOR(indexedDB::BackgroundRequestChild);
}

void BackgroundRequestChild::MaybeSendContinue() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRunningPreprocessHelpers > 0);

  if (--mRunningPreprocessHelpers == 0) {
    PreprocessResponse response;

    if (NS_SUCCEEDED(mPreprocessResultCode)) {
      if (mGetAll) {
        response = ObjectStoreGetAllPreprocessResponse();
      } else {
        response = ObjectStoreGetPreprocessResponse();
      }
    } else {
      response = mPreprocessResultCode;
    }

    MOZ_ALWAYS_TRUE(SendContinue(response));
  }
}

void BackgroundRequestChild::OnPreprocessFinished(
    uint32_t aCloneDataIndex, UniquePtr<JSStructuredCloneData> aCloneData) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCloneDataIndex < mCloneInfos.Length());
  MOZ_ASSERT(aCloneData);

  auto& cloneInfo = mCloneInfos[aCloneDataIndex];
  MOZ_ASSERT(cloneInfo.mPreprocessHelper);
  MOZ_ASSERT(!cloneInfo.mCloneData);

  cloneInfo.mCloneData = std::move(aCloneData);

  MaybeSendContinue();

  cloneInfo.mPreprocessHelper = nullptr;
}

void BackgroundRequestChild::OnPreprocessFailed(uint32_t aCloneDataIndex,
                                                nsresult aErrorCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCloneDataIndex < mCloneInfos.Length());
  MOZ_ASSERT(NS_FAILED(aErrorCode));

  auto& cloneInfo = mCloneInfos[aCloneDataIndex];
  MOZ_ASSERT(cloneInfo.mPreprocessHelper);
  MOZ_ASSERT(!cloneInfo.mCloneData);

  if (NS_SUCCEEDED(mPreprocessResultCode)) {
    mPreprocessResultCode = aErrorCode;
  }

  MaybeSendContinue();

  cloneInfo.mPreprocessHelper = nullptr;
}

UniquePtr<JSStructuredCloneData> BackgroundRequestChild::GetNextCloneData() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCurrentCloneDataIndex < mCloneInfos.Length());
  MOZ_ASSERT(mCloneInfos[mCurrentCloneDataIndex].mCloneData);

  return std::move(mCloneInfos[mCurrentCloneDataIndex++].mCloneData);
}

void BackgroundRequestChild::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(mTransaction);

  DispatchErrorEvent(mRequest, aResponse, mTransaction.clonePtr());
}

void BackgroundRequestChild::HandleResponse(const Key& aResponse) {
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, AcquireTransaction(), &aResponse);

  helper.DispatchSuccessEvent();
}

void BackgroundRequestChild::HandleResponse(const nsTArray<Key>& aResponse) {
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, AcquireTransaction(), &aResponse);

  helper.DispatchSuccessEvent();
}

void BackgroundRequestChild::HandleResponse(
    SerializedStructuredCloneReadInfo&& aResponse) {
  AssertIsOnOwningThread();

  auto cloneReadInfo = DeserializeStructuredCloneReadInfo(
      std::move(aResponse), mTransaction->Database(),
      [this] { return std::move(*GetNextCloneData()); });

  ResultHelper helper(mRequest, AcquireTransaction(), &cloneReadInfo);

  helper.DispatchSuccessEvent();
}

void BackgroundRequestChild::HandleResponse(
    nsTArray<SerializedStructuredCloneReadInfo>&& aResponse) {
  AssertIsOnOwningThread();

  nsTArray<StructuredCloneReadInfoChild> cloneReadInfos;

  if (!aResponse.IsEmpty()) {
    const uint32_t count = aResponse.Length();

    cloneReadInfos.SetCapacity(count);

    std::transform(
        std::make_move_iterator(aResponse.begin()),
        std::make_move_iterator(aResponse.end()),
        MakeBackInserter(cloneReadInfos),
        [database = mTransaction->Database(),
         this](SerializedStructuredCloneReadInfo&& serializedCloneInfo) {
          return DeserializeStructuredCloneReadInfo(
              std::move(serializedCloneInfo), database,
              [this] { return std::move(*GetNextCloneData()); });
        });
  }

  ResultHelper helper(mRequest, AcquireTransaction(), &cloneReadInfos);

  helper.DispatchSuccessEvent();
}

void BackgroundRequestChild::HandleResponse(JS::Handle<JS::Value> aResponse) {
  AssertIsOnOwningThread();

  ResultHelper helper(mRequest, AcquireTransaction(), &aResponse);

  helper.DispatchSuccessEvent();
}

void BackgroundRequestChild::HandleResponse(uint64_t aResponse) {
  AssertIsOnOwningThread();

  JS::Value response(JS::NumberValue(aResponse));

  ResultHelper helper(mRequest, AcquireTransaction(), &response);

  helper.DispatchSuccessEvent();
}

nsresult BackgroundRequestChild::HandlePreprocess(
    const PreprocessInfo& aPreprocessInfo) {
  return HandlePreprocessInternal(
      AutoTArray<PreprocessInfo, 1>{aPreprocessInfo});
}

nsresult BackgroundRequestChild::HandlePreprocess(
    const nsTArray<PreprocessInfo>& aPreprocessInfos) {
  AssertIsOnOwningThread();
  mGetAll = true;

  return HandlePreprocessInternal(aPreprocessInfos);
}

nsresult BackgroundRequestChild::HandlePreprocessInternal(
    const nsTArray<PreprocessInfo>& aPreprocessInfos) {
  AssertIsOnOwningThread();

  IDBDatabase* database = mTransaction->Database();

  const uint32_t count = aPreprocessInfos.Length();

  mCloneInfos.SetLength(count);

  // TODO: Since we use the stream transport service, this can spawn 25 threads
  //       and has the potential to cause some annoying browser hiccups.
  //       Consider using a single thread or a very small threadpool.
  for (uint32_t index = 0; index < count; index++) {
    const PreprocessInfo& preprocessInfo = aPreprocessInfos[index];

    const auto files =
        DeserializeStructuredCloneFiles(database, preprocessInfo.files(),
                                        /* aForPreprocess */ true);

    MOZ_ASSERT(files.Length() == 1);

    auto& preprocessHelper = mCloneInfos[index].mPreprocessHelper;
    preprocessHelper = MakeRefPtr<PreprocessHelper>(index, this);

    nsresult rv = preprocessHelper->Init(files[0]);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = preprocessHelper->Dispatch();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mRunningPreprocessHelpers++;
  }

  return NS_OK;
}

void BackgroundRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  MaybeCollectGarbageOnIPCMessage();

  for (auto& cloneInfo : mCloneInfos) {
    const auto& preprocessHelper = cloneInfo.mPreprocessHelper;

    if (preprocessHelper) {
      preprocessHelper->ClearActor();
    }
  }
  mCloneInfos.Clear();

  if (mTransaction) {
    mTransaction->AssertIsOnOwningThread();

    mTransaction->OnRequestFinished(/* aRequestCompletedSuccessfully */
                                    aWhy == Deletion);
#ifdef DEBUG
    mTransaction = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult BackgroundRequestChild::Recv__delete__(
    RequestResponse&& aResponse) {
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
        HandleResponse(
            std::move(aResponse.get_ObjectStoreGetResponse().cloneInfo()));
        break;

      case RequestResponse::TObjectStoreGetKeyResponse:
        HandleResponse(aResponse.get_ObjectStoreGetKeyResponse().key());
        break;

      case RequestResponse::TObjectStoreGetAllResponse:
        HandleResponse(
            std::move(aResponse.get_ObjectStoreGetAllResponse().cloneInfos()));
        break;

      case RequestResponse::TObjectStoreGetAllKeysResponse:
        HandleResponse(aResponse.get_ObjectStoreGetAllKeysResponse().keys());
        break;

      case RequestResponse::TObjectStoreDeleteResponse:
      case RequestResponse::TObjectStoreClearResponse:
        HandleResponse(JS::UndefinedHandleValue);
        break;

      case RequestResponse::TObjectStoreCountResponse:
        HandleResponse(aResponse.get_ObjectStoreCountResponse().count());
        break;

      case RequestResponse::TIndexGetResponse:
        HandleResponse(std::move(aResponse.get_IndexGetResponse().cloneInfo()));
        break;

      case RequestResponse::TIndexGetKeyResponse:
        HandleResponse(aResponse.get_IndexGetKeyResponse().key());
        break;

      case RequestResponse::TIndexGetAllResponse:
        HandleResponse(
            std::move(aResponse.get_IndexGetAllResponse().cloneInfos()));
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

  mTransaction->OnRequestFinished(/* aRequestCompletedSuccessfully */ true);

  // Null this out so that we don't try to call OnRequestFinished() again in
  // ActorDestroy.
  mTransaction = nullptr;

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundRequestChild::RecvPreprocess(
    const PreprocessParams& aParams) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  MaybeCollectGarbageOnIPCMessage();

  nsresult rv;

  switch (aParams.type()) {
    case PreprocessParams::TObjectStoreGetPreprocessParams: {
      const auto& params = aParams.get_ObjectStoreGetPreprocessParams();

      rv = HandlePreprocess(params.preprocessInfo());

      break;
    }

    case PreprocessParams::TObjectStoreGetAllPreprocessParams: {
      const auto& params = aParams.get_ObjectStoreGetAllPreprocessParams();

      rv = HandlePreprocess(params.preprocessInfos());

      break;
    }

    default:
      MOZ_CRASH("Unknown params type!");
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (!SendContinue(rv)) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  return IPC_OK();
}

nsresult BackgroundRequestChild::PreprocessHelper::Init(
    const StructuredCloneFileChild& aFile) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFile.HasBlob());
  MOZ_ASSERT(aFile.Type() == StructuredCloneFileBase::eStructuredClone);
  MOZ_ASSERT(mState == State::Initial);

  // The stream transport service is used for asynchronous processing. It has a
  // threadpool with a high cap of 25 threads. Fortunately, the service can be
  // used on workers too.
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  // We use a TaskQueue here in order to be sure that the events are dispatched
  // in the correct order. This is not guaranteed in case we use the I/O thread
  // directly.
  mTaskQueue = MakeRefPtr<TaskQueue>(target.forget());
  mTaskQueueEventTarget = mTaskQueue->WrapAsEventTarget();

  ErrorResult errorResult;

  nsCOMPtr<nsIInputStream> stream;
  // XXX After Bug 1620560, MutableBlob is not needed here anymore.
  aFile.MutableBlob().CreateInputStream(getter_AddRefs(stream), errorResult);
  if (NS_WARN_IF(errorResult.Failed())) {
    return errorResult.StealNSResult();
  }

  mStream = std::move(stream);

  mCloneData = MakeUnique<JSStructuredCloneData>(
      JS::StructuredCloneScope::DifferentProcessForIndexedDB);

  return NS_OK;
}

nsresult BackgroundRequestChild::PreprocessHelper::Dispatch() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Initial);

  nsresult rv = mTaskQueueEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult BackgroundRequestChild::PreprocessHelper::Start() {
  MOZ_ASSERT(!IsOnOwningThread());
  MOZ_ASSERT(mStream);
  MOZ_ASSERT(mState == State::Initial);

  nsresult rv;

  PRFileDesc* fileDesc = GetFileDescriptorFromStream(mStream);
  if (fileDesc) {
    rv = ProcessStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  mState = State::WaitingForStreamReady;

  nsCOMPtr<nsIAsyncFileMetadata> asyncFileMetadata = do_QueryInterface(mStream);
  if (asyncFileMetadata) {
    rv = asyncFileMetadata->AsyncFileMetadataWait(this, mTaskQueueEventTarget);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(mStream);
  if (!asyncStream) {
    return NS_ERROR_NO_INTERFACE;
  }

  rv = asyncStream->AsyncWait(this, 0, 0, mTaskQueueEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult BackgroundRequestChild::PreprocessHelper::ProcessStream() {
  MOZ_ASSERT(!IsOnOwningThread());
  MOZ_ASSERT(mStream);
  MOZ_ASSERT(mState == State::Initial ||
             mState == State::WaitingForStreamReady);

  // We need to get the internal stream (which is an nsFileInputStream) because
  // SnappyUncompressInputStream doesn't support reading from async input
  // streams.

  nsCOMPtr<mozIIPCBlobInputStream> blobInputStream = do_QueryInterface(mStream);
  MOZ_ASSERT(blobInputStream);

  nsCOMPtr<nsIInputStream> internalInputStream =
      blobInputStream->GetInternalStream();
  MOZ_ASSERT(internalInputStream);

  RefPtr<SnappyUncompressInputStream> snappyInputStream =
      new SnappyUncompressInputStream(internalInputStream);

  nsresult rv;

  do {
    char buffer[kFileCopyBufferSize];

    uint32_t numRead;
    rv = snappyInputStream->Read(buffer, sizeof(buffer), &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (!numRead) {
      break;
    }

    if (NS_WARN_IF(!mCloneData->AppendBytes(buffer, numRead))) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
  } while (true);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mState = State::Finishing;

  rv = mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void BackgroundRequestChild::PreprocessHelper::Finish() {
  AssertIsOnOwningThread();

  if (mActor) {
    if (NS_SUCCEEDED(mResultCode)) {
      mActor->OnPreprocessFinished(mCloneDataIndex, std::move(mCloneData));

      MOZ_ASSERT(!mCloneData);
    } else {
      mActor->OnPreprocessFailed(mCloneDataIndex, mResultCode);
    }
  }

  mState = State::Completed;
}

NS_IMPL_ISUPPORTS_INHERITED(BackgroundRequestChild::PreprocessHelper,
                            CancelableRunnable, nsIInputStreamCallback,
                            nsIFileMetadataCallback)

NS_IMETHODIMP
BackgroundRequestChild::PreprocessHelper::Run() {
  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = Start();
      break;

    case State::WaitingForStreamReady:
      rv = ProcessStream();
      break;

    case State::Finishing:
      Finish();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::Finishing) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::Finishing;

    if (IsOnOwningThread()) {
      Finish();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
BackgroundRequestChild::PreprocessHelper::OnInputStreamReady(
    nsIAsyncInputStream* aStream) {
  MOZ_ASSERT(!IsOnOwningThread());
  MOZ_ASSERT(mState == State::WaitingForStreamReady);

  MOZ_ALWAYS_SUCCEEDS(this->Run());

  return NS_OK;
}

NS_IMETHODIMP
BackgroundRequestChild::PreprocessHelper::OnFileMetadataReady(
    nsIAsyncFileMetadata* aObject) {
  MOZ_ASSERT(!IsOnOwningThread());
  MOZ_ASSERT(mState == State::WaitingForStreamReady);

  MOZ_ALWAYS_SUCCEEDS(this->Run());

  return NS_OK;
}

/*******************************************************************************
 * BackgroundCursorChild
 ******************************************************************************/

BackgroundCursorChildBase::BackgroundCursorChildBase(IDBRequest* const aRequest,
                                                     const Direction aDirection)
    : mRequest(aRequest),
      mTransaction(aRequest->MaybeTransactionRef()),
      mStrongRequest(aRequest),
      mDirection(aDirection) {
  MOZ_ASSERT(mTransaction);
}

template <IDBCursorType CursorType>
BackgroundCursorChild<CursorType>::BackgroundCursorChild(IDBRequest* aRequest,
                                                         SourceType* aSource,
                                                         Direction aDirection)
    : BackgroundCursorChildBase(aRequest, aDirection),
      mSource(aSource),
      mCursor(nullptr),
      mInFlightResponseInvalidationNeeded(false) {
  aSource->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(indexedDB::BackgroundCursorChild<CursorType>);
}

template <IDBCursorType CursorType>
BackgroundCursorChild<CursorType>::~BackgroundCursorChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundCursorChild<CursorType>);
}

template <IDBCursorType CursorType>
void BackgroundCursorChild<CursorType>::SendContinueInternal(
    const CursorRequestParams& aParams,
    const CursorData<CursorType>& aCurrentData) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  // Make sure all our DOM objects stay alive.
  mStrongCursor = mCursor;

  MOZ_ASSERT(GetRequest()->ReadyState() == IDBRequestReadyState::Done);
  GetRequest()->Reset();

  mTransaction->OnNewRequest();

  CursorRequestParams params = aParams;
  Key currentKey = aCurrentData.mKey;
  Key currentObjectStoreKey;
  // TODO: This is still not nice.
  if constexpr (!CursorTypeTraits<CursorType>::IsObjectStoreCursor) {
    currentObjectStoreKey = aCurrentData.mObjectStoreKey;
  }

  switch (params.type()) {
    case CursorRequestParams::TContinueParams: {
      const auto& key = params.get_ContinueParams().key();
      if (key.IsUnset()) {
        break;
      }

      // Discard cache entries before the target key.
      DiscardCachedResponses(
          [&key, isLocaleAware = mCursor->IsLocaleAware(),
           keyOperator = GetKeyOperator(mDirection),
           transactionSerialNumber = mTransaction->LoggingSerialNumber(),
           requestSerialNumber = GetRequest()->LoggingSerialNumber()](
              const auto& currentCachedResponse) {
            // This duplicates the logic from the parent. We could avoid this
            // duplication if we invalidated the cached records always for any
            // continue-with-key operation, but would lose the benefits of
            // preloading then.
            const auto& cachedSortKey =
                currentCachedResponse.GetSortKey(isLocaleAware);
            const bool discard = !(cachedSortKey.*keyOperator)(key);
            if (discard) {
              IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
                  "PRELOAD: Continue to key %s, discarding cached key %s/%s",
                  "Continue, discarding", transactionSerialNumber,
                  requestSerialNumber, key.GetBuffer().get(),
                  cachedSortKey.GetBuffer().get(),
                  currentCachedResponse.GetObjectStoreKeyForLogging());
            } else {
              IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
                  "PRELOAD: Continue to key %s, keeping cached key %s/%s and "
                  "further",
                  "Continue, keeping", transactionSerialNumber,
                  requestSerialNumber, key.GetBuffer().get(),
                  cachedSortKey.GetBuffer().get(),
                  currentCachedResponse.GetObjectStoreKeyForLogging());
            }

            return discard;
          });

      break;
    }

    case CursorRequestParams::TContinuePrimaryKeyParams: {
      if constexpr (!CursorTypeTraits<CursorType>::IsObjectStoreCursor) {
        const auto& key = params.get_ContinuePrimaryKeyParams().key();
        const auto& primaryKey =
            params.get_ContinuePrimaryKeyParams().primaryKey();
        if (key.IsUnset() || primaryKey.IsUnset()) {
          break;
        }

        // Discard cache entries before the target key.
        DiscardCachedResponses([&key, &primaryKey,
                                isLocaleAware = mCursor->IsLocaleAware(),
                                keyCompareOperator = GetKeyOperator(mDirection),
                                transactionSerialNumber =
                                    mTransaction->LoggingSerialNumber(),
                                requestSerialNumber =
                                    GetRequest()->LoggingSerialNumber()](
                                   const auto& currentCachedResponse) {
          // This duplicates the logic from the parent. We could avoid this
          // duplication if we invalidated the cached records always for any
          // continue-with-key operation, but would lose the benefits of
          // preloading then.
          const auto& cachedSortKey =
              currentCachedResponse.GetSortKey(isLocaleAware);
          const auto& cachedSortPrimaryKey =
              currentCachedResponse.mObjectStoreKey;

          const bool discard =
              (cachedSortKey == key &&
               !(cachedSortPrimaryKey.*keyCompareOperator)(primaryKey)) ||
              !(cachedSortKey.*keyCompareOperator)(key);

          if (discard) {
            IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
                "PRELOAD: Continue to key %s with primary key %s, discarding "
                "cached key %s with cached primary key %s",
                "Continue, discarding", transactionSerialNumber,
                requestSerialNumber, key.GetBuffer().get(),
                primaryKey.GetBuffer().get(), cachedSortKey.GetBuffer().get(),
                cachedSortPrimaryKey.GetBuffer().get());
          } else {
            IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
                "PRELOAD: Continue to key %s with primary key %s, keeping "
                "cached key %s with cached primary key %s and further",
                "Continue, keeping", transactionSerialNumber,
                requestSerialNumber, key.GetBuffer().get(),
                primaryKey.GetBuffer().get(), cachedSortKey.GetBuffer().get(),
                cachedSortPrimaryKey.GetBuffer().get());
          }

          return discard;
        });
      } else {
        MOZ_CRASH("Shouldn't get here");
      }

      break;
    }

    case CursorRequestParams::TAdvanceParams: {
      uint32_t& advanceCount = params.get_AdvanceParams().count();
      IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
          "PRELOAD: Advancing %" PRIu32 " records", "Advancing",
          mTransaction->LoggingSerialNumber(),
          GetRequest()->LoggingSerialNumber(), advanceCount);

      // Discard cache entries.
      DiscardCachedResponses([&advanceCount, &currentKey,
                              &currentObjectStoreKey](
                                 const auto& currentCachedResponse) {
        const bool res = advanceCount > 1;
        if (res) {
          --advanceCount;

          // TODO: We only need to update currentKey on the last entry, the
          // others are overwritten in the next iteration anyway.
          currentKey = currentCachedResponse.mKey;
          if constexpr (!CursorTypeTraits<CursorType>::IsObjectStoreCursor) {
            currentObjectStoreKey = currentCachedResponse.mObjectStoreKey;
          } else {
            Unused << currentObjectStoreKey;
          }
        }
        return res;
      });
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  if (!mCachedResponses.empty()) {
    // We need to remove the response here from mCachedResponses, since when
    // requests are interleaved, other events may be processed before
    // CompleteContinueRequestFromCache, which may modify mCachedResponses.
    mDelayedResponses.emplace_back(std::move(mCachedResponses.front()));
    mCachedResponses.pop_front();

    // We cannot send the response right away, as we must preserve the request
    // order. Dispatching a DelayedActionRunnable only partially addresses this.
    // This is accompanied by invalidating cached entries at proper locations to
    // make it correct. To avoid this, further changes are necessary, see Bug
    // 1580499.
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(
        MakeAndAddRef<DelayedActionRunnable<BackgroundCursorChild<CursorType>>>(
            this, &BackgroundCursorChild::CompleteContinueRequestFromCache)));

    // TODO: Could we preload further entries in the background when the size of
    // mCachedResponses falls under some threshold? Or does the response
    // handling model disallow this?
  } else {
    MOZ_ALWAYS_TRUE(PBackgroundIDBCursorChild::SendContinue(
        params, currentKey, currentObjectStoreKey));
  }
}

template <IDBCursorType CursorType>
void BackgroundCursorChild<CursorType>::CompleteContinueRequestFromCache() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mStrongCursor);
  MOZ_ASSERT(!mDelayedResponses.empty());
  MOZ_ASSERT(mCursor->GetType() == CursorType);

  const RefPtr<IDBCursor> cursor = std::move(mStrongCursor);

  mCursor->Reset(std::move(mDelayedResponses.front()));
  mDelayedResponses.pop_front();

  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "PRELOAD: Consumed 1 cached response, %zu cached responses remaining",
      "Consumed cached response", mTransaction->LoggingSerialNumber(),
      GetRequest()->LoggingSerialNumber(),
      mDelayedResponses.size() + mCachedResponses.size());

  ResultHelper helper(GetRequest(),
                      mTransaction ? SafeRefPtr{&mTransaction.ref(),
                                                AcquireStrongRefFromRawPtr{}}
                                   : nullptr,
                      cursor);
  helper.DispatchSuccessEvent();

  mTransaction->OnRequestFinished(/* aRequestCompletedSuccessfully */ true);
}

template <IDBCursorType CursorType>
void BackgroundCursorChild<CursorType>::SendDeleteMeInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  mRequest.destroy();
  mTransaction = Nothing();
  // TODO: The things until here could be pulled up to
  // BackgroundCursorChildBase.

  mSource.destroy();

  if (mCursor) {
    mCursor->ClearBackgroundActor();
    mCursor = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundIDBCursorChild::SendDeleteMe());
  }
}

template <IDBCursorType CursorType>
void BackgroundCursorChild<CursorType>::InvalidateCachedResponses() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mRequest);

  // TODO: With more information on the reason for the invalidation, we might
  // only selectively invalidate cached responses. If the reason is an updated
  // value, we do not need to care for key-only cursors. If the key of the
  // changed entry is not in the remaining range of the cursor, we also do not
  // need to care, etc.

  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "PRELOAD: Invalidating all %zu cached responses", "Invalidating",
      mTransaction->LoggingSerialNumber(), GetRequest()->LoggingSerialNumber(),
      mCachedResponses.size());

  mCachedResponses.clear();

  // We only hold a strong cursor reference in mStrongCursor when
  // continue()/similar has been called. In those cases we expect a response
  // that will be received in the future, and it may include prefetched data
  // that needs to be discarded.
  if (mStrongCursor) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "PRELOAD: Setting flag to invalidate in-flight responses",
        "Set flag to invalidate in-flight responses",
        mTransaction->LoggingSerialNumber(),
        GetRequest()->LoggingSerialNumber());

    mInFlightResponseInvalidationNeeded = true;
  }
}

template <IDBCursorType CursorType>
template <typename Condition>
void BackgroundCursorChild<CursorType>::DiscardCachedResponses(
    const Condition& aConditionFunc) {
  size_t discardedCount = 0;
  while (!mCachedResponses.empty() &&
         aConditionFunc(mCachedResponses.front())) {
    mCachedResponses.pop_front();
    ++discardedCount;
  }
  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "PRELOAD: Discarded %zu cached responses, %zu remaining", "Discarded",
      mTransaction->LoggingSerialNumber(), GetRequest()->LoggingSerialNumber(),
      discardedCount, mCachedResponses.size());
}

void BackgroundCursorChildBase::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  DispatchErrorEvent(
      GetRequest(), aResponse,
      SafeRefPtr{&mTransaction.ref(), AcquireStrongRefFromRawPtr{}});
}

template <IDBCursorType CursorType>
void BackgroundCursorChild<CursorType>::HandleResponse(
    const void_t& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);

  if (mCursor) {
    mCursor->Reset();
  }

  ResultHelper helper(GetRequest(),
                      mTransaction ? SafeRefPtr{&mTransaction.ref(),
                                                AcquireStrongRefFromRawPtr{}}
                                   : nullptr,
                      &JS::NullHandleValue);
  helper.DispatchSuccessEvent();

  if (!mCursor) {
    MOZ_ALWAYS_SUCCEEDS(this->GetActorEventTarget()->Dispatch(
        MakeAndAddRef<DelayedActionRunnable<BackgroundCursorChild<CursorType>>>(
            this, &BackgroundCursorChild::SendDeleteMeInternal),
        NS_DISPATCH_NORMAL));
  }
}

template <IDBCursorType CursorType>
template <typename... Args>
RefPtr<IDBCursor>
BackgroundCursorChild<CursorType>::HandleIndividualCursorResponse(
    const bool aUseAsCurrentResult, Args&&... aArgs) {
  if (mCursor) {
    if (aUseAsCurrentResult) {
      mCursor->Reset(CursorData<CursorType>{std::forward<Args>(aArgs)...});
    } else {
      mCachedResponses.emplace_back(std::forward<Args>(aArgs)...);
    }
    return nullptr;
  }

  MOZ_ASSERT(aUseAsCurrentResult);

  // TODO: This still looks quite dangerous to me. Why is mCursor not a
  // RefPtr?
  auto newCursor = IDBCursor::Create(this, std::forward<Args>(aArgs)...);
  mCursor = newCursor;
  return newCursor;
}

template <IDBCursorType CursorType>
template <typename Func>
void BackgroundCursorChild<CursorType>::HandleMultipleCursorResponses(
    nsTArray<ResponseType>&& aResponses, const Func& aHandleRecord) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(!mStrongRequest);
  MOZ_ASSERT(!mStrongCursor);
  MOZ_ASSERT(aResponses.Length() > 0);

  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "PRELOAD: Received %zu cursor responses", "Received",
      mTransaction->LoggingSerialNumber(), GetRequest()->LoggingSerialNumber(),
      aResponses.Length());
  MOZ_ASSERT_IF(aResponses.Length() > 1, mCachedResponses.empty());

  // If a new cursor is created, we need to keep a reference to it until the
  // ResultHelper creates a DOM Binding.
  RefPtr<IDBCursor> strongNewCursor;

  bool isFirst = true;
  for (auto& response : aResponses) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "PRELOAD: Processing response for key %s", "Processing",
        mTransaction->LoggingSerialNumber(),
        GetRequest()->LoggingSerialNumber(), response.key().GetBuffer().get());

    // TODO: At the moment, we only send a cursor request to the parent if
    // requested by the user code. Therefore, the first result is always used
    // as the current result, and the potential extra results are cached. If
    // we extended this towards preloading in the background, all results
    // might need to be cached.
    auto maybeNewCursor =
        aHandleRecord(/* aUseAsCurrentResult */ isFirst, std::move(response));
    if (maybeNewCursor) {
      MOZ_ASSERT(!strongNewCursor);
      strongNewCursor = std::move(maybeNewCursor);
    }
    isFirst = false;

    if (mInFlightResponseInvalidationNeeded) {
      IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
          "PRELOAD: Discarding remaining responses since "
          "mInFlightResponseInvalidationNeeded is set",
          "Discarding responses", mTransaction->LoggingSerialNumber(),
          GetRequest()->LoggingSerialNumber());

      mInFlightResponseInvalidationNeeded = false;
      break;
    }
  }

  ResultHelper helper(GetRequest(),
                      mTransaction ? SafeRefPtr{&mTransaction.ref(),
                                                AcquireStrongRefFromRawPtr{}}
                                   : nullptr,
                      mCursor);
  helper.DispatchSuccessEvent();
}

template <IDBCursorType CursorType>
void BackgroundCursorChild<CursorType>::HandleResponse(
    nsTArray<ResponseType>&& aResponses) {
  AssertIsOnOwningThread();

  if constexpr (CursorType == IDBCursorType::ObjectStore) {
    MOZ_ASSERT(mTransaction);

    HandleMultipleCursorResponses(
        std::move(aResponses), [this](const bool useAsCurrentResult,
                                      ObjectStoreCursorResponse&& response) {
          // TODO: Maybe move the deserialization of the clone-read-info into
          // the cursor, so that it is only done for records actually accessed,
          // which might not be the case for all cached records.
          return HandleIndividualCursorResponse(
              useAsCurrentResult, std::move(response.key()),
              DeserializeStructuredCloneReadInfo(
                  std::move(response.cloneInfo()), mTransaction->Database(),
                  PreprocessingNotSupported));
        });
  }
  if constexpr (CursorType == IDBCursorType::ObjectStoreKey) {
    HandleMultipleCursorResponses(
        std::move(aResponses), [this](const bool useAsCurrentResult,
                                      ObjectStoreKeyCursorResponse&& response) {
          return HandleIndividualCursorResponse(useAsCurrentResult,
                                                std::move(response.key()));
        });
  }
  if constexpr (CursorType == IDBCursorType::Index) {
    MOZ_ASSERT(mTransaction);

    HandleMultipleCursorResponses(
        std::move(aResponses),
        [this](const bool useAsCurrentResult, IndexCursorResponse&& response) {
          return HandleIndividualCursorResponse(
              useAsCurrentResult, std::move(response.key()),
              std::move(response.sortKey()), std::move(response.objectKey()),
              DeserializeStructuredCloneReadInfo(
                  std::move(response.cloneInfo()), mTransaction->Database(),
                  PreprocessingNotSupported));
        });
  }
  if constexpr (CursorType == IDBCursorType::IndexKey) {
    HandleMultipleCursorResponses(
        std::move(aResponses), [this](const bool useAsCurrentResult,
                                      IndexKeyCursorResponse&& response) {
          return HandleIndividualCursorResponse(
              useAsCurrentResult, std::move(response.key()),
              std::move(response.sortKey()), std::move(response.objectKey()));
        });
  }
}

template <IDBCursorType CursorType>
void BackgroundCursorChild<CursorType>::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(aWhy == Deletion, !mStrongRequest);
  MOZ_ASSERT_IF(aWhy == Deletion, !mStrongCursor);

  MaybeCollectGarbageOnIPCMessage();

  if (mStrongRequest && !mStrongCursor && mTransaction) {
    mTransaction->OnRequestFinished(/* aRequestCompletedSuccessfully */
                                    aWhy == Deletion);
  }

  if (mCursor) {
    mCursor->ClearBackgroundActor();
#ifdef DEBUG
    mCursor = nullptr;
#endif
  }

#ifdef DEBUG
  mRequest.maybeDestroy();
  mTransaction = Nothing();
  mSource.maybeDestroy();
#endif
}

template <IDBCursorType CursorType>
mozilla::ipc::IPCResult BackgroundCursorChild<CursorType>::RecvResponse(
    CursorResponse&& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT_IF(mCursor, mStrongCursor);
  MOZ_ASSERT_IF(!mCursor, mStrongRequest);

  MaybeCollectGarbageOnIPCMessage();

  const RefPtr<IDBRequest> request = std::move(mStrongRequest);
  Unused << request;  // XXX see Bug 1605075
  const RefPtr<IDBCursor> cursor = std::move(mStrongCursor);
  Unused << cursor;  // XXX see Bug 1605075

  const auto transaction =
      SafeRefPtr{&mTransaction.ref(), AcquireStrongRefFromRawPtr{}};

  switch (aResponse.type()) {
    case CursorResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case CursorResponse::Tvoid_t:
      HandleResponse(aResponse.get_void_t());
      break;

    case CursorResponse::TArrayOfObjectStoreCursorResponse:
      if constexpr (CursorType == IDBCursorType::ObjectStore) {
        HandleResponse(
            std::move(aResponse.get_ArrayOfObjectStoreCursorResponse()));
      } else {
        MOZ_CRASH("Response type mismatch");
      }
      break;

    case CursorResponse::TArrayOfObjectStoreKeyCursorResponse:
      if constexpr (CursorType == IDBCursorType::ObjectStoreKey) {
        HandleResponse(
            std::move(aResponse.get_ArrayOfObjectStoreKeyCursorResponse()));
      } else {
        MOZ_CRASH("Response type mismatch");
      }
      break;

    case CursorResponse::TArrayOfIndexCursorResponse:
      if constexpr (CursorType == IDBCursorType::Index) {
        HandleResponse(std::move(aResponse.get_ArrayOfIndexCursorResponse()));
      } else {
        MOZ_CRASH("Response type mismatch");
      }
      break;

    case CursorResponse::TArrayOfIndexKeyCursorResponse:
      if constexpr (CursorType == IDBCursorType::IndexKey) {
        HandleResponse(
            std::move(aResponse.get_ArrayOfIndexKeyCursorResponse()));
      } else {
        MOZ_CRASH("Response type mismatch");
      }
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  transaction->OnRequestFinished(/* aRequestCompletedSuccessfully */ true);

  return IPC_OK();
}

template class BackgroundCursorChild<IDBCursorType::ObjectStore>;
template class BackgroundCursorChild<IDBCursorType::ObjectStoreKey>;
template class BackgroundCursorChild<IDBCursorType::Index>;
template class BackgroundCursorChild<IDBCursorType::IndexKey>;

template <typename T>
NS_IMETHODIMP DelayedActionRunnable<T>::Run() {
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mActionFunc);

  (mActor->*mActionFunc)();

  mActor = nullptr;
  mRequest = nullptr;

  return NS_OK;
}

template <typename T>
nsresult DelayedActionRunnable<T>::Cancel() {
  if (NS_WARN_IF(!mActor)) {
    return NS_ERROR_UNEXPECTED;
  }

  // This must always run to clean up our state.
  Run();

  return NS_OK;
}

/*******************************************************************************
 * BackgroundFileHandleChild
 ******************************************************************************/

BackgroundFileHandleChild::BackgroundFileHandleChild(IDBFileHandle* aFileHandle)
    : mTemporaryStrongFileHandle(aFileHandle), mFileHandle(aFileHandle) {
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(BackgroundFileHandleChild);
}

BackgroundFileHandleChild::~BackgroundFileHandleChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(BackgroundFileHandleChild);
}

#ifdef DEBUG

void BackgroundFileHandleChild::AssertIsOnOwningThread() const {
  static_cast<BackgroundMutableFileChild*>(Manager())->AssertIsOnOwningThread();
}

#endif  // DEBUG

void BackgroundFileHandleChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mFileHandle) {
    NoteActorDestroyed();

    MOZ_ALWAYS_TRUE(PBackgroundFileHandleChild::SendDeleteMe());
  }
}

void BackgroundFileHandleChild::NoteActorDestroyed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mTemporaryStrongFileHandle, mFileHandle);

  if (mFileHandle) {
    mFileHandle->ClearBackgroundActor();

    // Normally this would be DEBUG-only but NoteActorDestroyed is also called
    // from SendDeleteMeInternal. In that case we're going to receive an
    // actual ActorDestroy call later and we don't want to touch a dead
    // object.
    mTemporaryStrongFileHandle = nullptr;
    mFileHandle = nullptr;
  }
}

void BackgroundFileHandleChild::NoteComplete() {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mFileHandle, mTemporaryStrongFileHandle);

  mTemporaryStrongFileHandle = nullptr;
}

void BackgroundFileHandleChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult BackgroundFileHandleChild::RecvComplete(
    const bool aAborted) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandle);

  mFileHandle->FireCompleteOrAbortEvents(aAborted);

  NoteComplete();
  return IPC_OK();
}

PBackgroundFileRequestChild*
BackgroundFileHandleChild::AllocPBackgroundFileRequestChild(
    const FileRequestParams& aParams) {
  MOZ_CRASH(
      "PBackgroundFileRequestChild actors should be manually "
      "constructed!");
}

bool BackgroundFileHandleChild::DeallocPBackgroundFileRequestChild(
    PBackgroundFileRequestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundFileRequestChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BackgroundFileRequestChild
 ******************************************************************************/

BackgroundFileRequestChild::BackgroundFileRequestChild(
    IDBFileRequest* aFileRequest)
    : mFileRequest(aFileRequest),
      mFileHandle(aFileRequest->GetFileHandle()),
      mActorDestroyed(false) {
  MOZ_ASSERT(aFileRequest);
  aFileRequest->AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandle);
  mFileHandle->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(BackgroundFileRequestChild);
}

BackgroundFileRequestChild::~BackgroundFileRequestChild() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mFileHandle);

  MOZ_COUNT_DTOR(BackgroundFileRequestChild);
}

#ifdef DEBUG

void BackgroundFileRequestChild::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mFileRequest);
  mFileRequest->AssertIsOnOwningThread();
}

#endif  // DEBUG

void BackgroundFileRequestChild::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_FILEHANDLE);
  MOZ_ASSERT(mFileHandle);

  DispatchFileHandleErrorEvent(mFileRequest, aResponse, mFileHandle);
}

void BackgroundFileRequestChild::HandleResponse(const nsCString& aResponse) {
  AssertIsOnOwningThread();

  FileHandleResultHelper helper(mFileRequest, mFileHandle, &aResponse);

  DispatchFileHandleSuccessEvent(&helper);
}

void BackgroundFileRequestChild::HandleResponse(
    const FileRequestMetadata& aResponse) {
  AssertIsOnOwningThread();

  FileHandleResultHelper helper(mFileRequest, mFileHandle, &aResponse);

  DispatchFileHandleSuccessEvent(&helper);
}

void BackgroundFileRequestChild::HandleResponse(
    JS::Handle<JS::Value> aResponse) {
  AssertIsOnOwningThread();

  FileHandleResultHelper helper(mFileRequest, mFileHandle, &aResponse);

  DispatchFileHandleSuccessEvent(&helper);
}

void BackgroundFileRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (mFileHandle) {
    mFileHandle->AssertIsOnOwningThread();

    mFileHandle->OnRequestFinished(/* aActorDestroyedNormally */
                                   aWhy == Deletion);

#ifdef DEBUG
    mFileHandle = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult BackgroundFileRequestChild::Recv__delete__(
    const FileRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileRequest);
  MOZ_ASSERT(mFileHandle);

  if (mFileHandle->IsAborted()) {
    // Always handle an "error" with ABORT_ERR if the file handle was aborted,
    // even if the request succeeded or failed with another error.
    HandleResponse(NS_ERROR_DOM_FILEHANDLE_ABORT_ERR);
  } else {
    switch (aResponse.type()) {
      case FileRequestResponse::Tnsresult:
        HandleResponse(aResponse.get_nsresult());
        break;

      case FileRequestResponse::TFileRequestReadResponse:
        HandleResponse(aResponse.get_FileRequestReadResponse().data());
        break;

      case FileRequestResponse::TFileRequestWriteResponse:
      case FileRequestResponse::TFileRequestTruncateResponse:
      case FileRequestResponse::TFileRequestFlushResponse:
        HandleResponse(JS::UndefinedHandleValue);
        break;

      case FileRequestResponse::TFileRequestGetMetadataResponse:
        HandleResponse(
            aResponse.get_FileRequestGetMetadataResponse().metadata());
        break;

      default:
        MOZ_CRASH("Unknown response type!");
    }
  }

  mFileHandle->OnRequestFinished(/* aActorDestroyedNormally */ true);

  // Null this out so that we don't try to call OnRequestFinished() again in
  // ActorDestroy.
  mFileHandle = nullptr;

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundFileRequestChild::RecvProgress(
    const uint64_t aProgress, const uint64_t aProgressMax) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileRequest);

  mFileRequest->FireProgressEvent(aProgress, aProgressMax);

  return IPC_OK();
}

/*******************************************************************************
 * BackgroundUtilsChild
 ******************************************************************************/

BackgroundUtilsChild::BackgroundUtilsChild(IndexedDatabaseManager* aManager)
    : mManager(aManager) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aManager);

  MOZ_COUNT_CTOR(indexedDB::BackgroundUtilsChild);
}

BackgroundUtilsChild::~BackgroundUtilsChild() {
  MOZ_COUNT_DTOR(indexedDB::BackgroundUtilsChild);
}

void BackgroundUtilsChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mManager) {
    mManager->ClearBackgroundActor();
    mManager = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundIndexedDBUtilsChild::SendDeleteMe());
  }
}

void BackgroundUtilsChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mManager) {
    mManager->ClearBackgroundActor();
#ifdef DEBUG
    mManager = nullptr;
#endif
  }
}

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla
