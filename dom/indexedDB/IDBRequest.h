/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbrequest_h__
#define mozilla_dom_idbrequest_h__

#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/IDBRequestBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsCycleCollectionParticipant.h"
#include "ReportInternalError.h"
#include "SafeRefPtr.h"

#define PRIVATE_IDBREQUEST_IID                       \
  {                                                  \
    0xe68901e5, 0x1d50, 0x4ee9, {                    \
      0xaf, 0x49, 0x90, 0x99, 0x4a, 0xff, 0xc8, 0x39 \
    }                                                \
  }

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMException;
class IDBCursor;
class IDBDatabase;
class IDBFactory;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;
template <typename>
struct Nullable;
class OwningIDBObjectStoreOrIDBIndexOrIDBCursor;
class StrongWorkerRef;

namespace detail {
// This class holds the IID for use with NS_GET_IID.
class PrivateIDBRequest {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(PRIVATE_IDBREQUEST_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(PrivateIDBRequest, PRIVATE_IDBREQUEST_IID)

}  // namespace detail

class IDBRequest : public DOMEventTargetHelper {
 protected:
  // mSourceAsObjectStore and mSourceAsIndex are exclusive and one must always
  // be set. mSourceAsCursor is sometimes set also.
  RefPtr<IDBObjectStore> mSourceAsObjectStore;
  RefPtr<IDBIndex> mSourceAsIndex;
  RefPtr<IDBCursor> mSourceAsCursor;

  SafeRefPtr<IDBTransaction> mTransaction;

  JS::Heap<JS::Value> mResultVal;
  RefPtr<DOMException> mError;

  nsString mFilename;
  uint64_t mLoggingSerialNumber;
  nsresult mErrorCode;
  uint32_t mLineNo;
  uint32_t mColumn;
  bool mHaveResultOrErrorCode;

 public:
  [[nodiscard]] static MovingNotNull<RefPtr<IDBRequest>> Create(
      JSContext* aCx, IDBDatabase* aDatabase,
      SafeRefPtr<IDBTransaction> aTransaction);

  [[nodiscard]] static MovingNotNull<RefPtr<IDBRequest>> Create(
      JSContext* aCx, IDBObjectStore* aSource, IDBDatabase* aDatabase,
      SafeRefPtr<IDBTransaction> aTransaction);

  [[nodiscard]] static MovingNotNull<RefPtr<IDBRequest>> Create(
      JSContext* aCx, IDBIndex* aSource, IDBDatabase* aDatabase,
      SafeRefPtr<IDBTransaction> aTransaction);

  static void CaptureCaller(JSContext* aCx, nsAString& aFilename,
                            uint32_t* aLineNo, uint32_t* aColumn);

  static uint64_t NextSerialNumber();

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  void GetSource(
      Nullable<OwningIDBObjectStoreOrIDBIndexOrIDBCursor>& aSource) const;

  void Reset();

  template <typename ResultCallback>
  void SetResult(const ResultCallback& aCallback) {
    AssertIsOnOwningThread();
    MOZ_ASSERT(!mHaveResultOrErrorCode);
    MOZ_ASSERT(mResultVal.isUndefined());
    MOZ_ASSERT(!mError);

    // Already disconnected from the owner.
    if (!GetOwnerGlobal()) {
      SetError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return;
    }

    // See this global is still valid.
    if (NS_WARN_IF(NS_FAILED(CheckCurrentGlobalCorrectness()))) {
      SetError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return;
    }

    AutoJSAPI autoJS;
    if (!autoJS.Init(GetOwnerGlobal())) {
      IDB_WARNING("Failed to initialize AutoJSAPI!");
      SetError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return;
    }

    JSContext* cx = autoJS.cx();

    JS::Rooted<JS::Value> result(cx);
    nsresult rv = aCallback(cx, &result);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // This can only fail if the structured clone contains a mutable file
      // and the child is not in the main thread and main process.
      // In that case CreateAndWrapMutableFile() returns false which shows up
      // as NS_ERROR_DOM_DATA_CLONE_ERR here.
      MOZ_ASSERT(rv == NS_ERROR_DOM_DATA_CLONE_ERR);

      // We are not setting a result or an error object here since we want to
      // throw an exception when the 'result' property is being touched.
      return;
    }

    mError = nullptr;

    mResultVal = result;
    mozilla::HoldJSObjects(this);

    mHaveResultOrErrorCode = true;
  }

  void SetError(nsresult aRv);

  nsresult GetErrorCode() const
#ifdef DEBUG
      ;
#else
  {
    return mErrorCode;
  }
#endif

  DOMException* GetErrorAfterResult() const
#ifdef DEBUG
      ;
#else
  {
    return mError;
  }
#endif

  DOMException* GetError(ErrorResult& aRv);

  void GetCallerLocation(nsAString& aFilename, uint32_t* aLineNo,
                         uint32_t* aColumn) const;

  bool IsPending() const { return !mHaveResultOrErrorCode; }

  uint64_t LoggingSerialNumber() const {
    AssertIsOnOwningThread();

    return mLoggingSerialNumber;
  }

  void SetLoggingSerialNumber(uint64_t aLoggingSerialNumber);

  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  void GetResult(JS::MutableHandle<JS::Value> aResult, ErrorResult& aRv) const;

  void GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                 ErrorResult& aRv) const {
    GetResult(aResult, aRv);
  }

  Maybe<IDBTransaction&> MaybeTransactionRef() const {
    AssertIsOnOwningThread();

    return mTransaction.maybeDeref();
  }

  IDBTransaction& MutableTransactionRef() const {
    AssertIsOnOwningThread();

    return *mTransaction;
  }

  SafeRefPtr<IDBTransaction> AcquireTransaction() const {
    AssertIsOnOwningThread();

    return mTransaction.clonePtr();
  }

  // For WebIDL binding.
  RefPtr<IDBTransaction> GetTransaction() const {
    AssertIsOnOwningThread();

    return AsRefPtr(mTransaction.clonePtr());
  }

  IDBRequestReadyState ReadyState() const;

  void SetSource(IDBCursor* aSource);

  IMPL_EVENT_HANDLER(success);
  IMPL_EVENT_HANDLER(error);

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(IDBRequest); }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBRequest,
                                                         DOMEventTargetHelper)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 protected:
  explicit IDBRequest(IDBDatabase* aDatabase);
  explicit IDBRequest(nsIGlobalObject* aGlobal);
  ~IDBRequest();

  void InitMembers();

  void ConstructResult();
};

class IDBOpenDBRequest final : public IDBRequest {
  // Only touched on the owning thread.
  SafeRefPtr<IDBFactory> mFactory;

  RefPtr<StrongWorkerRef> mWorkerRef;

  const bool mFileHandleDisabled;
  bool mIncreasedActiveDatabaseCount;

 public:
  [[nodiscard]] static RefPtr<IDBOpenDBRequest> Create(
      JSContext* aCx, SafeRefPtr<IDBFactory> aFactory,
      nsIGlobalObject* aGlobal);

  bool IsFileHandleDisabled() const { return mFileHandleDisabled; }

  void SetTransaction(SafeRefPtr<IDBTransaction> aTransaction);

  void DispatchNonTransactionError(nsresult aErrorCode);

  void NoteComplete();

  // EventTarget
  IMPL_EVENT_HANDLER(blocked);
  IMPL_EVENT_HANDLER(upgradeneeded);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBOpenDBRequest, IDBRequest)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  IDBOpenDBRequest(SafeRefPtr<IDBFactory> aFactory, nsIGlobalObject* aGlobal,
                   bool aFileHandleDisabled);

  ~IDBOpenDBRequest();

  void IncreaseActiveDatabaseCount();

  void MaybeDecreaseActiveDatabaseCount();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbrequest_h__
