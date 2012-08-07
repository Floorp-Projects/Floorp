/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_ipc_indexeddbparent_h__
#define mozilla_dom_indexeddb_ipc_indexeddbparent_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/indexedDB/PIndexedDBParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBCursorParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBDatabaseParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBDeleteDatabaseRequestParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBIndexParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBObjectStoreParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBRequestParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBTransactionParent.h"

#include "mozilla/Attributes.h"

#include "nsIDOMEventListener.h"

namespace mozilla {
namespace dom {
class ContentParent;
class PBlobParent;
class TabParent;
}
}

class nsIDOMBlob;
class nsIDOMEvent;

BEGIN_INDEXEDDB_NAMESPACE

class IDBCursor;
class IDBDatabase;
class IDBFactory;
class IDBIndex;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBTransaction;

/*******************************************************************************
 * AutoSetCurrentTransaction
 ******************************************************************************/

class AutoSetCurrentTransaction
{
public:
  AutoSetCurrentTransaction(IDBTransaction* aTransaction);
  ~AutoSetCurrentTransaction();
};

/*******************************************************************************
 * WeakEventListener
 ******************************************************************************/

class WeakEventListenerBase : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

protected:
  WeakEventListenerBase()
  { }

  virtual ~WeakEventListenerBase()
  { }
};

template <class T>
class WeakEventListener : public WeakEventListenerBase
{
  T* mActor;

public:
  WeakEventListener(T* aActor)
  : mActor(aActor)
  { }

  void
  NoteDyingActor()
  {
    mActor = NULL;
  }

  NS_IMETHOD
  HandleEvent(nsIDOMEvent* aEvent)
  {
    return mActor ? mActor->HandleEvent(aEvent) : NS_OK;
  }

protected:
  virtual ~WeakEventListener()
  { }
};

template <class T>
class AutoWeakEventListener
{
  nsRefPtr<WeakEventListener<T> > mEventListener;

public:
  AutoWeakEventListener(T* aActor)
  {
    mEventListener = new WeakEventListener<T>(aActor);
  }

  ~AutoWeakEventListener()
  {
    mEventListener->NoteDyingActor();
  }

  template <class U>
  operator U*()
  {
    return mEventListener;
  }

  T*
  operator ->()
  {
    return mEventListener;
  }
};

/*******************************************************************************
 * IndexedDBParent
 ******************************************************************************/

class IndexedDBParent : public PIndexedDBParent
{
  friend class mozilla::dom::ContentParent;
  friend class mozilla::dom::TabParent;

  nsRefPtr<IDBFactory> mFactory;
  nsCString mASCIIOrigin;

public:
  IndexedDBParent();
  virtual ~IndexedDBParent();

  const nsCString&
  GetASCIIOrigin() const
  {
    return mASCIIOrigin;
  }

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBDatabaseConstructor(PIndexedDBDatabaseParent* aActor,
                                    const nsString& aName,
                                    const uint64_t& aVersion) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBDeleteDatabaseRequestConstructor(
                                  PIndexedDBDeleteDatabaseRequestParent* aActor,
                                  const nsString& aName) MOZ_OVERRIDE;

  virtual PIndexedDBDatabaseParent*
  AllocPIndexedDBDatabase(const nsString& aName, const uint64_t& aVersion)
                          MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBDatabase(PIndexedDBDatabaseParent* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBDeleteDatabaseRequestParent*
  AllocPIndexedDBDeleteDatabaseRequest(const nsString& aName) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBDeleteDatabaseRequest(
                                  PIndexedDBDeleteDatabaseRequestParent* aActor)
                                  MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBDatabaseParent
 ******************************************************************************/

class IndexedDBDatabaseParent : public PIndexedDBDatabaseParent
{
  AutoWeakEventListener<IndexedDBDatabaseParent> mEventListener;

  nsRefPtr<IDBOpenDBRequest> mOpenRequest;
  nsRefPtr<IDBDatabase> mDatabase;

public:
  IndexedDBDatabaseParent();
  virtual ~IndexedDBDatabaseParent();

  nsresult
  SetOpenRequest(IDBOpenDBRequest* aRequest);

  nsresult
  HandleEvent(nsIDOMEvent* aEvent);

protected:
  nsresult
  HandleRequestEvent(nsIDOMEvent* aEvent, const nsAString& aType);

  nsresult
  HandleDatabaseEvent(nsIDOMEvent* aEvent, const nsAString& aType);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvClose(const bool& aUnlinked) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBTransactionConstructor(PIndexedDBTransactionParent* aActor,
                                       const TransactionParams& aParams)
                                       MOZ_OVERRIDE;

  virtual PIndexedDBTransactionParent*
  AllocPIndexedDBTransaction(const TransactionParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBTransaction(PIndexedDBTransactionParent* aActor)
                               MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBTransactionParent
 ******************************************************************************/

class IndexedDBTransactionParent : public PIndexedDBTransactionParent
{
protected:
  AutoWeakEventListener<IndexedDBTransactionParent> mEventListener;

  nsRefPtr<IDBTransaction> mTransaction;

  bool mArtificialRequestCount;

public:
  IndexedDBTransactionParent();
  virtual ~IndexedDBTransactionParent();

  nsresult
  SetTransaction(IDBTransaction* aTransaction);

  IDBTransaction*
  GetTransaction() const
  {
    return mTransaction;
  }

  nsresult
  HandleEvent(nsIDOMEvent* aEvent);

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvAbort(const nsresult& aAbortCode) MOZ_OVERRIDE;

  virtual bool
  RecvAllRequestsFinished() MOZ_OVERRIDE;

  virtual bool
  RecvDeleteObjectStore(const nsString& aName) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBObjectStoreConstructor(
                                    PIndexedDBObjectStoreParent* aActor,
                                    const ObjectStoreConstructorParams& aParams)
                                    MOZ_OVERRIDE;

  virtual PIndexedDBObjectStoreParent*
  AllocPIndexedDBObjectStore(const ObjectStoreConstructorParams& aParams)
                             MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBObjectStore(PIndexedDBObjectStoreParent* aActor)
                               MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBVersionChangeTransactionParent
 ******************************************************************************/

class IndexedDBVersionChangeTransactionParent :
  public IndexedDBTransactionParent
{
public:
  IndexedDBVersionChangeTransactionParent();
  virtual ~IndexedDBVersionChangeTransactionParent();

protected:
  virtual bool
  RecvDeleteObjectStore(const nsString& aName) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBObjectStoreConstructor(
                                    PIndexedDBObjectStoreParent* aActor,
                                    const ObjectStoreConstructorParams& aParams)
                                    MOZ_OVERRIDE;

  virtual PIndexedDBObjectStoreParent*
  AllocPIndexedDBObjectStore(const ObjectStoreConstructorParams& aParams)
                             MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBObjectStoreParent
 ******************************************************************************/

class IndexedDBObjectStoreParent : public PIndexedDBObjectStoreParent
{
protected:
  nsRefPtr<IDBObjectStore> mObjectStore;

public:
  IndexedDBObjectStoreParent();
  virtual ~IndexedDBObjectStoreParent();

  void
  SetObjectStore(IDBObjectStore* aObjectStore);

  IDBObjectStore*
  GetObjectStore() const
  {
    return mObjectStore;
  }

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvDeleteIndex(const nsString& aName) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBRequestConstructor(PIndexedDBRequestParent* aActor,
                                   const ObjectStoreRequestParams& aParams)
                                   MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBIndexConstructor(PIndexedDBIndexParent* aActor,
                                 const IndexConstructorParams& aParams)
                                 MOZ_OVERRIDE;

  virtual PIndexedDBRequestParent*
  AllocPIndexedDBRequest(const ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequest(PIndexedDBRequestParent* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBIndexParent*
  AllocPIndexedDBIndex(const IndexConstructorParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBIndex(PIndexedDBIndexParent* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBCursorParent*
  AllocPIndexedDBCursor(const ObjectStoreCursorConstructorParams& aParams)
                        MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBCursor(PIndexedDBCursorParent* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBVersionChangeObjectStoreParent
 ******************************************************************************/

class IndexedDBVersionChangeObjectStoreParent :
  public IndexedDBObjectStoreParent
{
public:
  IndexedDBVersionChangeObjectStoreParent();
  virtual ~IndexedDBVersionChangeObjectStoreParent();

protected:
  virtual bool
  RecvDeleteIndex(const nsString& aName) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBIndexConstructor(PIndexedDBIndexParent* aActor,
                                 const IndexConstructorParams& aParams)
                                 MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBIndexParent
 ******************************************************************************/

class IndexedDBIndexParent : public PIndexedDBIndexParent
{
  nsRefPtr<IDBIndex> mIndex;

public:
  IndexedDBIndexParent();
  virtual ~IndexedDBIndexParent();

  void
  SetIndex(IDBIndex* aObjectStore);

  IDBIndex*
  GetIndex() const
  {
    return mIndex;
  }

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBRequestConstructor(PIndexedDBRequestParent* aActor,
                                   const IndexRequestParams& aParams)
                                   MOZ_OVERRIDE;

  virtual PIndexedDBRequestParent*
  AllocPIndexedDBRequest(const IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequest(PIndexedDBRequestParent* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBCursorParent*
  AllocPIndexedDBCursor(const IndexCursorConstructorParams& aParams)
                        MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBCursor(PIndexedDBCursorParent* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBCursorParent
 ******************************************************************************/

class IndexedDBCursorParent : public PIndexedDBCursorParent
{
  nsRefPtr<IDBCursor> mCursor;

public:
  IndexedDBCursorParent(IDBCursor* aCursor);
  virtual ~IndexedDBCursorParent();

  IDBCursor*
  GetCursor() const
  {
    return mCursor;
  }

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBRequestConstructor(PIndexedDBRequestParent* aActor,
                                   const CursorRequestParams& aParams)
                                   MOZ_OVERRIDE;

  virtual PIndexedDBRequestParent*
  AllocPIndexedDBRequest(const CursorRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequest(PIndexedDBRequestParent* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBRequestParentBase
 ******************************************************************************/

class IndexedDBRequestParentBase : public PIndexedDBRequestParent
{
protected:
  typedef ipc::ResponseValue ResponseValue;

  nsRefPtr<IDBRequest> mRequest;

  IndexedDBRequestParentBase();
  virtual ~IndexedDBRequestParentBase();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBObjectStoreRequestParent
 ******************************************************************************/

class IndexedDBObjectStoreRequestParent : public IndexedDBRequestParentBase
{
  nsRefPtr<IDBObjectStore> mObjectStore;

  typedef ipc::ObjectStoreRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

  typedef ipc::AddParams AddParams;
  typedef ipc::PutParams PutParams;
  typedef ipc::ClearParams ClearParams;
  typedef ipc::DeleteParams DeleteParams;
  typedef ipc::FIXME_Bug_521898_objectstore::GetParams GetParams;
  typedef ipc::FIXME_Bug_521898_objectstore::GetAllParams GetAllParams;
  typedef ipc::FIXME_Bug_521898_objectstore::CountParams CountParams;
  typedef ipc::FIXME_Bug_521898_objectstore::OpenCursorParams OpenCursorParams;

public:
  IndexedDBObjectStoreRequestParent(IDBObjectStore* aObjectStore,
                                    RequestType aRequestType);
  virtual ~IndexedDBObjectStoreRequestParent();

  bool
  Get(const GetParams& aParams);

  bool
  GetAll(const GetAllParams& aParams);

  bool
  Add(const AddParams& aParams);

  bool
  Put(const PutParams& aParams);

  bool
  Delete(const DeleteParams& aParams);

  bool
  Clear(const ClearParams& aParams);

  bool
  Count(const CountParams& aParams);

  bool
  OpenCursor(const OpenCursorParams& aParams);

protected:
  void
  ConvertBlobActors(const InfallibleTArray<PBlobParent*>& aActors,
                    nsTArray<nsCOMPtr<nsIDOMBlob> >& aBlobs);
};

/*******************************************************************************
 * IndexedDBIndexRequestParent
 ******************************************************************************/

class IndexedDBIndexRequestParent : public IndexedDBRequestParentBase
{
  nsRefPtr<IDBIndex> mIndex;

  typedef ipc::IndexRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

  typedef ipc::GetKeyParams GetKeyParams;
  typedef ipc::GetAllKeysParams GetAllKeysParams;
  typedef ipc::OpenKeyCursorParams OpenKeyCursorParams;
  typedef ipc::FIXME_Bug_521898_index::GetParams GetParams;
  typedef ipc::FIXME_Bug_521898_index::GetAllParams GetAllParams;
  typedef ipc::FIXME_Bug_521898_index::CountParams CountParams;
  typedef ipc::FIXME_Bug_521898_index::OpenCursorParams OpenCursorParams;

public:
  IndexedDBIndexRequestParent(IDBIndex* aIndex, RequestType aRequestType);
  virtual ~IndexedDBIndexRequestParent();

  bool
  Get(const GetParams& aParams);

  bool
  GetKey(const GetKeyParams& aParams);

  bool
  GetAll(const GetAllParams& aParams);

  bool
  GetAllKeys(const GetAllKeysParams& aParams);

  bool
  Count(const CountParams& aParams);

  bool
  OpenCursor(const OpenCursorParams& aParams);

  bool
  OpenKeyCursor(const OpenKeyCursorParams& aParams);
};

/*******************************************************************************
 * IndexedDBCursorRequestParent
 ******************************************************************************/

class IndexedDBCursorRequestParent : public IndexedDBRequestParentBase
{
  nsRefPtr<IDBCursor> mCursor;

  typedef ipc::CursorRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

  typedef ipc::ContinueParams ContinueParams;

public:
  IndexedDBCursorRequestParent(IDBCursor* aCursor, RequestType aRequestType);
  virtual ~IndexedDBCursorRequestParent();

  bool
  Continue(const ContinueParams& aParams);
};

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestParent
 ******************************************************************************/

class IndexedDBDeleteDatabaseRequestParent :
  public PIndexedDBDeleteDatabaseRequestParent
{
  AutoWeakEventListener<IndexedDBDeleteDatabaseRequestParent> mEventListener;

  nsRefPtr<IDBFactory> mFactory;
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;

public:
  IndexedDBDeleteDatabaseRequestParent(IDBFactory* aFactory);
  virtual ~IndexedDBDeleteDatabaseRequestParent();

  nsresult
  SetOpenRequest(IDBOpenDBRequest* aOpenRequest);

  nsresult
  HandleEvent(nsIDOMEvent* aEvent);
};


END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_ipc_indexeddbparent_h__
