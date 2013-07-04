/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_ipc_indexeddbparent_h__
#define mozilla_dom_indexeddb_ipc_indexeddbparent_h__

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/indexedDB/PIndexedDBParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBCursorParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBDatabaseParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBDeleteDatabaseRequestParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBIndexParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBObjectStoreParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBRequestParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBTransactionParent.h"

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

class IndexedDBCursorParent;
class IndexedDBDatabaseParent;
class IndexedDBDeleteDatabaseRequestParent;
class IndexedDBIndexParent;
class IndexedDBObjectStoreParent;
class IndexedDBTransactionParent;
class IndexedDBVersionChangeTransactionParent;
class IndexedDBVersionChangeObjectStoreParent;

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
  HandleEvent(nsIDOMEvent* aEvent) MOZ_OVERRIDE
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

class IndexedDBParent : private PIndexedDBParent
{
  friend class mozilla::dom::ContentParent;
  friend class mozilla::dom::TabParent;
  friend class IndexedDBDatabaseParent;
  friend class IndexedDBDeleteDatabaseRequestParent;

  nsRefPtr<IDBFactory> mFactory;
  nsCString mASCIIOrigin;

  ContentParent* mManagerContent;
  TabParent* mManagerTab;

  bool mDisconnected;

public:
  IndexedDBParent(ContentParent* aContentParent);
  IndexedDBParent(TabParent* aTabParent);

  virtual ~IndexedDBParent();

  const nsCString&
  GetASCIIOrigin() const
  {
    return mASCIIOrigin;
  }

  void
  Disconnect();

  bool
  IsDisconnected() const
  {
    return mDisconnected;
  }

  ContentParent*
  GetManagerContent() const
  {
    return mManagerContent;
  }

  TabParent*
  GetManagerTab() const
  {
    return mManagerTab;
  }

  bool
  CheckReadPermission(const nsAString& aDatabaseName);

  bool
  CheckWritePermission(const nsAString& aDatabaseName);

protected:
  bool
  CheckPermissionInternal(const nsAString& aDatabaseName,
                          const nsDependentCString& aPermission);

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

class IndexedDBDatabaseParent : private PIndexedDBDatabaseParent
{
  friend class IndexedDBParent;
  friend class IndexedDBTransactionParent;
  friend class IndexedDBVersionChangeTransactionParent;

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

  void
  Disconnect();

  bool
  IsDisconnected() const
  {
    return static_cast<IndexedDBParent*>(Manager())->IsDisconnected();
  }

  bool
  CheckWritePermission(const nsAString& aDatabaseName);

  void
  Invalidate();

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

class IndexedDBTransactionParent : protected PIndexedDBTransactionParent
{
  friend class IndexedDBCursorParent;
  friend class IndexedDBDatabaseParent;
  friend class IndexedDBObjectStoreParent;

protected:
  AutoWeakEventListener<IndexedDBTransactionParent> mEventListener;

  nsRefPtr<IDBTransaction> mTransaction;

  bool mArtificialRequestCount;

public:
  IndexedDBTransactionParent();
  virtual ~IndexedDBTransactionParent();

  bool
  IsDisconnected() const
  {
    return static_cast<IndexedDBDatabaseParent*>(Manager())->IsDisconnected();
  }

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
  friend class IndexedDBVersionChangeObjectStoreParent;

public:
  IndexedDBVersionChangeTransactionParent();
  virtual ~IndexedDBVersionChangeTransactionParent();

  bool
  IsDisconnected() const
  {
    return static_cast<IndexedDBDatabaseParent*>(Manager())->IsDisconnected();
  }

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
 * IndexedDBCursorParent
 ******************************************************************************/

class IndexedDBCursorParent : private PIndexedDBCursorParent
{
  friend class IndexedDBIndexParent;
  friend class IndexedDBObjectStoreParent;

  nsRefPtr<IDBCursor> mCursor;

public:
  IDBCursor*
  GetCursor() const
  {
    return mCursor;
  }

  bool
  IsDisconnected() const;

protected:
  IndexedDBCursorParent(IDBCursor* aCursor);
  virtual ~IndexedDBCursorParent();

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
 * IndexedDBObjectStoreParent
 ******************************************************************************/

class IndexedDBObjectStoreParent : protected PIndexedDBObjectStoreParent
{
  friend class IndexedDBIndexParent;
  friend class IndexedDBTransactionParent;
  friend class IndexedDBVersionChangeTransactionParent;

  typedef mozilla::dom::indexedDB::ipc::OpenCursorResponse OpenCursorResponse;

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

  bool
  IsDisconnected() const
  {
    IndexedDBTransactionParent* manager =
      static_cast<IndexedDBTransactionParent*>(Manager());
    return manager->IsDisconnected();
  }

  // Ordinarily callers could just do this manually using
  // PIndexedDBObjectStoreParent::SendPIndexedDBCursorConstructor but we're
  // inheriting the abstract protocol class privately to prevent outside code
  // from sending messages without checking the disconnected state. Therefore
  // we need a helper method.
  bool
  OpenCursor(IDBCursor* aCursor,
             const ObjectStoreCursorConstructorParams& aParams,
             OpenCursorResponse& aResponse) NS_WARN_UNUSED_RESULT
  {
    if (IsDisconnected()) {
      return true;
    }

    IndexedDBCursorParent* cursorActor = new IndexedDBCursorParent(aCursor);

    if (!SendPIndexedDBCursorConstructor(cursorActor, aParams)) {
      return false;
    }

    aResponse = cursorActor;
    return true;
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
  friend class IndexedDBVersionChangeTransactionParent;

public:
  IndexedDBVersionChangeObjectStoreParent();
  virtual ~IndexedDBVersionChangeObjectStoreParent();

protected:
  bool
  IsDisconnected() const
  {
    IndexedDBVersionChangeTransactionParent* manager =
      static_cast<IndexedDBVersionChangeTransactionParent*>(Manager());
    return manager->IsDisconnected();
  }

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

class IndexedDBIndexParent : private PIndexedDBIndexParent
{
  friend class IndexedDBObjectStoreParent;
  friend class IndexedDBVersionChangeObjectStoreParent;

  typedef mozilla::dom::indexedDB::ipc::OpenCursorResponse OpenCursorResponse;

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

  // Ordinarily callers could just do this manually using
  // PIndexedDBIndexParent::SendPIndexedDBCursorConstructor but we're
  // inheriting the abstract protocol class privately to prevent outside code
  // from sending messages without checking the disconnected state. Therefore
  // we need a helper method.
  bool
  OpenCursor(IDBCursor* aCursor, const IndexCursorConstructorParams& aParams,
             OpenCursorResponse& aResponse) NS_WARN_UNUSED_RESULT
  {
    if (IsDisconnected()) {
      return true;
    }

    IndexedDBCursorParent* cursorActor = new IndexedDBCursorParent(aCursor);

    if (!SendPIndexedDBCursorConstructor(cursorActor, aParams)) {
      return false;
    }

    aResponse = cursorActor;
    return true;
  }

  bool
  IsDisconnected() const
  {
    IndexedDBObjectStoreParent* manager =
      static_cast<IndexedDBObjectStoreParent*>(Manager());
    return manager->IsDisconnected();
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
 * IndexedDBRequestParentBase
 ******************************************************************************/

class IndexedDBRequestParentBase : public PIndexedDBRequestParent
{
public:
  bool
  SendResponse(const ResponseValue& aResponse) NS_WARN_UNUSED_RESULT
  {
    if (IsDisconnected()) {
      return true;
    }

    return Send__delete__(this, aResponse);
  }

protected:
  // Don't let anyone call this directly, instead go through SendResponse.
  using PIndexedDBRequestParent::Send__delete__;

  typedef ipc::ResponseValue ResponseValue;
  typedef PIndexedDBRequestParent::PBlobParent PBlobParent;

  nsRefPtr<IDBRequest> mRequest;

  IndexedDBRequestParentBase();
  virtual ~IndexedDBRequestParentBase();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  IsDisconnected() = 0;
};

/*******************************************************************************
 * IndexedDBObjectStoreRequestParent
 ******************************************************************************/

class IndexedDBObjectStoreRequestParent : public IndexedDBRequestParentBase
{
  friend class IndexedDBObjectStoreParent;

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

private:
  virtual bool
  IsDisconnected() MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBIndexRequestParent
 ******************************************************************************/

class IndexedDBIndexRequestParent : public IndexedDBRequestParentBase
{
  friend class IndexedDBIndexParent;

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

private:
  virtual bool
  IsDisconnected() MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBCursorRequestParent
 ******************************************************************************/

class IndexedDBCursorRequestParent : public IndexedDBRequestParentBase
{
  friend class IndexedDBCursorParent;

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

private:
  virtual bool
  IsDisconnected() MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestParent
 ******************************************************************************/

class IndexedDBDeleteDatabaseRequestParent :
  private PIndexedDBDeleteDatabaseRequestParent
{
  friend class IndexedDBParent;

  AutoWeakEventListener<IndexedDBDeleteDatabaseRequestParent> mEventListener;

  nsRefPtr<IDBFactory> mFactory;
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;

public:
  nsresult
  HandleEvent(nsIDOMEvent* aEvent);

protected:
  IndexedDBDeleteDatabaseRequestParent(IDBFactory* aFactory);
  virtual ~IndexedDBDeleteDatabaseRequestParent();

  nsresult
  SetOpenRequest(IDBOpenDBRequest* aOpenRequest);

  bool
  IsDisconnected() const
  {
    return static_cast<IndexedDBParent*>(Manager())->IsDisconnected();
  }
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_ipc_indexeddbparent_h__
