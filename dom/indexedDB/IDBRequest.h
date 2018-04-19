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
#include "mozilla/dom/IDBWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

#define PRIVATE_IDBREQUEST_IID \
  {0xe68901e5, 0x1d50, 0x4ee9, {0xaf, 0x49, 0x90, 0x99, 0x4a, 0xff, 0xc8, 0x39}}

class nsPIDOMWindowInner;

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
template <typename> struct Nullable;
class OwningIDBObjectStoreOrIDBIndexOrIDBCursor;
class StrongWorkerRef;

class IDBRequest
  : public IDBWrapperCache
{
protected:
  // mSourceAsObjectStore and mSourceAsIndex are exclusive and one must always
  // be set. mSourceAsCursor is sometimes set also.
  RefPtr<IDBObjectStore> mSourceAsObjectStore;
  RefPtr<IDBIndex> mSourceAsIndex;
  RefPtr<IDBCursor> mSourceAsCursor;

  RefPtr<IDBTransaction> mTransaction;

  JS::Heap<JS::Value> mResultVal;
  RefPtr<DOMException> mError;

  nsString mFilename;
  uint64_t mLoggingSerialNumber;
  nsresult mErrorCode;
  uint32_t mLineNo;
  uint32_t mColumn;
  bool mHaveResultOrErrorCode;

public:
  class ResultCallback;

  static already_AddRefed<IDBRequest>
  Create(JSContext* aCx, IDBDatabase* aDatabase, IDBTransaction* aTransaction);

  static already_AddRefed<IDBRequest>
  Create(JSContext* aCx,
         IDBObjectStore* aSource,
         IDBDatabase* aDatabase,
         IDBTransaction* aTransaction);

  static already_AddRefed<IDBRequest>
  Create(JSContext* aCx,
         IDBIndex* aSource,
         IDBDatabase* aDatabase,
         IDBTransaction* aTransaction);

  static void
  CaptureCaller(JSContext* aCx, nsAString& aFilename, uint32_t* aLineNo,
                uint32_t* aColumn);

  static uint64_t
  NextSerialNumber();

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  void
  GetSource(Nullable<OwningIDBObjectStoreOrIDBIndexOrIDBCursor>& aSource) const;

  void
  Reset();

  void
  SetResultCallback(ResultCallback* aCallback);

  void
  SetError(nsresult aRv);

  nsresult
  GetErrorCode() const
#ifdef DEBUG
  ;
#else
  {
    return mErrorCode;
  }
#endif

  DOMException*
  GetErrorAfterResult() const
#ifdef DEBUG
  ;
#else
  {
    return mError;
  }
#endif

  DOMException*
  GetError(ErrorResult& aRv);

  void
  GetCallerLocation(nsAString& aFilename, uint32_t* aLineNo,
                    uint32_t* aColumn) const;

  bool
  IsPending() const
  {
    return !mHaveResultOrErrorCode;
  }

  uint64_t
  LoggingSerialNumber() const
  {
    AssertIsOnOwningThread();

    return mLoggingSerialNumber;
  }

  void
  SetLoggingSerialNumber(uint64_t aLoggingSerialNumber);

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return GetOwner();
  }

  void
  GetResult(JS::MutableHandle<JS::Value> aResult, ErrorResult& aRv) const;

  void
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
            ErrorResult& aRv) const
  {
    GetResult(aResult, aRv);
  }

  IDBTransaction*
  GetTransaction() const
  {
    AssertIsOnOwningThread();

    return mTransaction;
  }

  IDBRequestReadyState
  ReadyState() const;

  void
  SetSource(IDBCursor* aSource);

  IMPL_EVENT_HANDLER(success);
  IMPL_EVENT_HANDLER(error);

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(IDBRequest);
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBRequest,
                                                         IDBWrapperCache)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  explicit IDBRequest(IDBDatabase* aDatabase);
  explicit IDBRequest(nsPIDOMWindowInner* aOwner);
  ~IDBRequest();

  void
  InitMembers();

  void
  ConstructResult();
};

class NS_NO_VTABLE IDBRequest::ResultCallback
{
public:
  virtual nsresult
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) = 0;

protected:
  ResultCallback()
  { }
};

class IDBOpenDBRequest final
  : public IDBRequest
{
  // Only touched on the owning thread.
  RefPtr<IDBFactory> mFactory;

  RefPtr<StrongWorkerRef> mWorkerRef;

  const bool mFileHandleDisabled;
  bool mIncreasedActiveDatabaseCount;

public:
  static already_AddRefed<IDBOpenDBRequest>
  CreateForWindow(JSContext* aCx,
                  IDBFactory* aFactory,
                  nsPIDOMWindowInner* aOwner,
                  JS::Handle<JSObject*> aScriptOwner);

  static already_AddRefed<IDBOpenDBRequest>
  CreateForJS(JSContext* aCx,
              IDBFactory* aFactory,
              JS::Handle<JSObject*> aScriptOwner);

  bool
  IsFileHandleDisabled() const
  {
    return mFileHandleDisabled;
  }

  void
  SetTransaction(IDBTransaction* aTransaction);

  void
  DispatchNonTransactionError(nsresult aErrorCode);

  void
  NoteComplete();

  // nsIDOMEventTarget
  virtual nsresult
  PostHandleEvent(EventChainPostVisitor& aVisitor) override;

  IDBFactory*
  Factory() const
  {
    return mFactory;
  }

  IMPL_EVENT_HANDLER(blocked);
  IMPL_EVENT_HANDLER(upgradeneeded);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBOpenDBRequest, IDBRequest)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBOpenDBRequest(IDBFactory* aFactory,
                   nsPIDOMWindowInner* aOwner,
                   bool aFileHandleDisabled);

  ~IDBOpenDBRequest();

  void
  IncreaseActiveDatabaseCount();

  void
  MaybeDecreaseActiveDatabaseCount();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idbrequest_h__
