/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbrequest_h__
#define mozilla_dom_indexeddb_idbrequest_h__

#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/IDBRequestBinding.h"
#include "mozilla/dom/indexedDB/IDBWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsPIDOMWindow;
struct PRThread;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMError;
struct ErrorEventInit;
template <typename> struct Nullable;
class OwningIDBObjectStoreOrIDBIndexOrIDBCursor;

namespace indexedDB {

class IDBCursor;
class IDBDatabase;
class IDBFactory;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;

class IDBRequest
  : public IDBWrapperCache
{
protected:
  // mSourceAsObjectStore and mSourceAsIndex are exclusive and one must always
  // be set. mSourceAsCursor is sometimes set also.
  nsRefPtr<IDBObjectStore> mSourceAsObjectStore;
  nsRefPtr<IDBIndex> mSourceAsIndex;
  nsRefPtr<IDBCursor> mSourceAsCursor;

  nsRefPtr<IDBTransaction> mTransaction;

#ifdef DEBUG
  PRThread* mOwningThread;
#endif

  JS::Heap<JS::Value> mResultVal;
  nsRefPtr<DOMError> mError;

  nsString mFilename;
#ifdef MOZ_ENABLE_PROFILER_SPS
  uint64_t mSerialNumber;
#endif
  nsresult mErrorCode;
  uint32_t mLineNo;
  bool mHaveResultOrErrorCode;

public:
  class ResultCallback;

  static already_AddRefed<IDBRequest>
  Create(IDBDatabase* aDatabase, IDBTransaction* aTransaction);

  static already_AddRefed<IDBRequest>
  Create(IDBObjectStore* aSource,
         IDBDatabase* aDatabase,
         IDBTransaction* aTransaction);

  static already_AddRefed<IDBRequest>
  Create(IDBIndex* aSource,
         IDBDatabase* aDatabase,
         IDBTransaction* aTransaction);

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

  void
  GetSource(Nullable<OwningIDBObjectStoreOrIDBIndexOrIDBCursor>& aSource) const;

  void
  Reset();

  void
  DispatchNonTransactionError(nsresult aErrorCode);

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

  DOMError*
  GetError(ErrorResult& aRv);

  void
  FillScriptErrorEvent(ErrorEventInit& aEventInit) const;

  bool
  IsPending() const
  {
    return !mHaveResultOrErrorCode;
  }

#ifdef MOZ_ENABLE_PROFILER_SPS
  uint64_t
  GetSerialNumber() const
  {
    return mSerialNumber;
  }
#endif

  nsPIDOMWindow*
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
#ifdef DEBUG
  ;
#else
  { }
#endif

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBRequest,
                                                         IDBWrapperCache)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

protected:
  explicit IDBRequest(IDBDatabase* aDatabase);
  explicit IDBRequest(nsPIDOMWindow* aOwner);
  ~IDBRequest();

  void
  InitMembers();

  void
  ConstructResult();

  void
  CaptureCaller();
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

class IDBOpenDBRequest MOZ_FINAL
  : public IDBRequest
{
  // Only touched on the owning thread.
  nsRefPtr<IDBFactory> mFactory;

public:
  static already_AddRefed<IDBOpenDBRequest>
  CreateForWindow(IDBFactory* aFactory,
                  nsPIDOMWindow* aOwner,
                  JS::Handle<JSObject*> aScriptOwner);

  static already_AddRefed<IDBOpenDBRequest>
  CreateForJS(IDBFactory* aFactory,
              JS::Handle<JSObject*> aScriptOwner);

  void
  SetTransaction(IDBTransaction* aTransaction);

  // nsIDOMEventTarget
  virtual nsresult
  PostHandleEvent(EventChainPostVisitor& aVisitor) MOZ_OVERRIDE;

  DOMError*
  GetError(ErrorResult& aRv)
  {
    return IDBRequest::GetError(aRv);
  }

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
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

private:
  IDBOpenDBRequest(IDBFactory* aFactory, nsPIDOMWindow* aOwner);

  ~IDBOpenDBRequest();
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbrequest_h__
