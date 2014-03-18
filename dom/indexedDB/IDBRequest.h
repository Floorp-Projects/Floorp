/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbrequest_h__
#define mozilla_dom_indexeddb_idbrequest_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/IDBRequestBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/indexedDB/IDBWrapperCache.h"

class nsIScriptContext;
class nsPIDOMWindow;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {
class OwningIDBObjectStoreOrIDBIndexOrIDBCursor;
class ErrorEventInit;
}
}

BEGIN_INDEXEDDB_NAMESPACE

class HelperBase;
class IDBCursor;
class IDBFactory;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;
class IndexedDBRequestParentBase;

class IDBRequest : public IDBWrapperCache
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBRequest,
                                                         IDBWrapperCache)

  static
  already_AddRefed<IDBRequest> Create(IDBDatabase* aDatabase,
                                      IDBTransaction* aTransaction);

  static
  already_AddRefed<IDBRequest> Create(IDBObjectStore* aSource,
                                      IDBDatabase* aDatabase,
                                      IDBTransaction* aTransaction);

  static
  already_AddRefed<IDBRequest> Create(IDBIndex* aSource,
                                      IDBDatabase* aDatabase,
                                      IDBTransaction* aTransaction);

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

  void GetSource(Nullable<OwningIDBObjectStoreOrIDBIndexOrIDBCursor>& aSource) const;

  void Reset();

  nsresult NotifyHelperCompleted(HelperBase* aHelper);
  void NotifyHelperSentResultsToChildProcess(nsresult aRv);

  void SetError(nsresult aRv);

  nsresult
  GetErrorCode() const
#ifdef DEBUG
  ;
#else
  {
    return mErrorCode;
  }
#endif

  DOMError* GetError(ErrorResult& aRv);

  JSContext* GetJSContext();

  void
  SetActor(IndexedDBRequestParentBase* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent,
                 "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  IndexedDBRequestParentBase*
  GetActorParent() const
  {
    return mActorParent;
  }

  void CaptureCaller();

  void FillScriptErrorEvent(ErrorEventInit& aEventInit) const;

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

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  JS::Value
  GetResult(JSContext* aCx, ErrorResult& aRv) const;

  IDBTransaction*
  GetTransaction() const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return mTransaction;
  }

  IDBRequestReadyState
  ReadyState() const;

  IMPL_EVENT_HANDLER(success);
  IMPL_EVENT_HANDLER(error);

protected:
  IDBRequest(IDBDatabase* aDatabase);
  IDBRequest(nsPIDOMWindow* aOwner);
  ~IDBRequest();

  // At most one of these three fields can be non-null.
  nsRefPtr<IDBObjectStore> mSourceAsObjectStore;
  nsRefPtr<IDBIndex> mSourceAsIndex;
  nsRefPtr<IDBCursor> mSourceAsCursor;

  // Check that the above condition holds.
#ifdef DEBUG
  void AssertSourceIsCorrect() const;
#else
  void AssertSourceIsCorrect() const {}
#endif

  nsRefPtr<IDBTransaction> mTransaction;

  JS::Heap<JS::Value> mResultVal;
  nsRefPtr<mozilla::dom::DOMError> mError;
  IndexedDBRequestParentBase* mActorParent;
  nsString mFilename;
#ifdef MOZ_ENABLE_PROFILER_SPS
  uint64_t mSerialNumber;
#endif
  nsresult mErrorCode;
  uint32_t mLineNo;
  bool mHaveResultOrErrorCode;
};

class IDBOpenDBRequest : public IDBRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBOpenDBRequest, IDBRequest)

  static
  already_AddRefed<IDBOpenDBRequest>
  Create(IDBFactory* aFactory,
         nsPIDOMWindow* aOwner,
         JS::Handle<JSObject*> aScriptOwner);

  void SetTransaction(IDBTransaction* aTransaction);

  // nsIDOMEventTarget
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) MOZ_OVERRIDE;

  DOMError* GetError(ErrorResult& aRv)
  {
    return IDBRequest::GetError(aRv);
  }

  IDBFactory*
  Factory() const
  {
    return mFactory;
  }

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  IMPL_EVENT_HANDLER(blocked);
  IMPL_EVENT_HANDLER(upgradeneeded);

protected:
  IDBOpenDBRequest(nsPIDOMWindow* aOwner);
  ~IDBOpenDBRequest();

  // Only touched on the main thread.
  nsRefPtr<IDBFactory> mFactory;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbrequest_h__
