/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_ipc_indexeddbchild_h__
#define mozilla_dom_indexeddb_ipc_indexeddbchild_h__

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/indexedDB/PIndexedDBChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBCursorChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBDatabaseChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBDeleteDatabaseRequestChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBIndexChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBObjectStoreChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBRequestChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBTransactionChild.h"

namespace mozilla {
namespace dom {
class ContentChild;
class TabChild;
} // dom
} // mozilla

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class IDBCursor;
class IDBFactory;
class IDBIndex;
class IDBOpenDBRequest;
class IDBRequest;
class IDBTransactionListener;

/*******************************************************************************
 * IndexedDBChild
 ******************************************************************************/

class IndexedDBChild : public PIndexedDBChild
{
  IDBFactory* mFactory;
  ContentChild* mManagerContent;
  TabChild* mManagerTab;

  nsCString mASCIIOrigin;

#ifdef DEBUG
  bool mDisconnected;
#endif

public:
  IndexedDBChild(ContentChild* aContentChild, const nsCString& aASCIIOrigin);
  IndexedDBChild(TabChild* aTabChild, const nsCString& aASCIIOrigin);
  virtual ~IndexedDBChild();

  const nsCString&
  ASCIIOrigin() const
  {
    return mASCIIOrigin;
  }

  ContentChild*
  GetManagerContent() const
  {
    return mManagerContent;
  }

  TabChild*
  GetManagerTab() const
  {
    return mManagerTab;
  }

  void
  SetFactory(IDBFactory* aFactory);

  void
  Disconnect();

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PIndexedDBDatabaseChild*
  AllocPIndexedDBDatabaseChild(const nsString& aName, const uint64_t& aVersion,
                               const PersistenceType& aPersistenceType)
                               MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBDatabaseChild(PIndexedDBDatabaseChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBDeleteDatabaseRequestChild*
  AllocPIndexedDBDeleteDatabaseRequestChild(
                                        const nsString& aName,
                                        const PersistenceType& aPersistenceType)
                                        MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBDeleteDatabaseRequestChild(
                                   PIndexedDBDeleteDatabaseRequestChild* aActor)
                                   MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBDatabaseChild
 ******************************************************************************/

class IndexedDBDatabaseChild : public PIndexedDBDatabaseChild
{
  IDBDatabase* mDatabase;
  nsString mName;
  uint64_t mVersion;

  nsRefPtr<IDBOpenDBRequest> mRequest;
  nsRefPtr<AsyncConnectionHelper> mOpenHelper;

  // Only used during version change transactions and blocked events.
  nsRefPtr<IDBDatabase> mStrongDatabase;

public:
  IndexedDBDatabaseChild(const nsString& aName, uint64_t aVersion);
  virtual ~IndexedDBDatabaseChild();

  void
  SetRequest(IDBOpenDBRequest* aRequest);

  void
  Disconnect();

protected:
  bool
  EnsureDatabase(IDBOpenDBRequest* aRequest,
                 const DatabaseInfoGuts& aDBInfo,
                 const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvSuccess(const DatabaseInfoGuts& aDBInfo,
              const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
              MOZ_OVERRIDE;

  virtual bool
  RecvError(const nsresult& aRv) MOZ_OVERRIDE;

  virtual bool
  RecvBlocked(const uint64_t& aOldVersion) MOZ_OVERRIDE;

  virtual bool
  RecvVersionChange(const uint64_t& aOldVersion, const uint64_t& aNewVersion)
                    MOZ_OVERRIDE;

  virtual bool
  RecvInvalidate() MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBTransactionConstructor(PIndexedDBTransactionChild* aActor,
                                       const TransactionParams& aParams)
                                       MOZ_OVERRIDE;

  virtual PIndexedDBTransactionChild*
  AllocPIndexedDBTransactionChild(const TransactionParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBTransactionChild(PIndexedDBTransactionChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBTransactionChild
 ******************************************************************************/

class IndexedDBTransactionChild : public PIndexedDBTransactionChild
{
  IDBTransaction* mTransaction;

  nsRefPtr<IDBTransaction> mStrongTransaction;
  nsRefPtr<IDBTransactionListener> mTransactionListener;

public:
  IndexedDBTransactionChild();
  virtual ~IndexedDBTransactionChild();

  void
  SetTransaction(IDBTransaction* aTransaction);

  IDBTransaction*
  GetTransaction() const
  {
    return mTransaction;
  }

  void
  Disconnect();

protected:
  void
  FireCompleteEvent(nsresult aRv);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvComplete(const CompleteParams& aParams) MOZ_OVERRIDE;

  virtual PIndexedDBObjectStoreChild*
  AllocPIndexedDBObjectStoreChild(const ObjectStoreConstructorParams& aParams)
                                  MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBObjectStoreChild(PIndexedDBObjectStoreChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBObjectStoreChild
 ******************************************************************************/

class IndexedDBObjectStoreChild : public PIndexedDBObjectStoreChild
{
  IDBObjectStore* mObjectStore;

public:
  explicit IndexedDBObjectStoreChild(IDBObjectStore* aObjectStore);
  virtual ~IndexedDBObjectStoreChild();

  void
  Disconnect();

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBCursorConstructor(
                              PIndexedDBCursorChild* aActor,
                              const ObjectStoreCursorConstructorParams& aParams)
                              MOZ_OVERRIDE;

  virtual PIndexedDBRequestChild*
  AllocPIndexedDBRequestChild(const ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBIndexChild*
  AllocPIndexedDBIndexChild(const IndexConstructorParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBIndexChild(PIndexedDBIndexChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBCursorChild*
  AllocPIndexedDBCursorChild(const ObjectStoreCursorConstructorParams& aParams)
                             MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBCursorChild(PIndexedDBCursorChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBIndexChild
 ******************************************************************************/

class IndexedDBIndexChild : public PIndexedDBIndexChild
{
  IDBIndex* mIndex;

public:
  explicit IndexedDBIndexChild(IDBIndex* aIndex);
  virtual ~IndexedDBIndexChild();

  void
  Disconnect();

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBCursorConstructor(PIndexedDBCursorChild* aActor,
                                  const IndexCursorConstructorParams& aParams)
                                  MOZ_OVERRIDE;

  virtual PIndexedDBRequestChild*
  AllocPIndexedDBRequestChild(const IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBCursorChild*
  AllocPIndexedDBCursorChild(const IndexCursorConstructorParams& aParams)
                             MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBCursorChild(PIndexedDBCursorChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBCursorChild
 ******************************************************************************/

class IndexedDBCursorChild : public PIndexedDBCursorChild
{
  IDBCursor* mCursor;

  nsRefPtr<IDBCursor> mStrongCursor;

public:
  IndexedDBCursorChild();
  virtual ~IndexedDBCursorChild();

  void
  SetCursor(IDBCursor* aCursor);

  already_AddRefed<IDBCursor>
  ForgetStrongCursor()
  {
    return mStrongCursor.forget();
  }

  void
  Disconnect();

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PIndexedDBRequestChild*
  AllocPIndexedDBRequestChild(const CursorRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBRequestChildBase
 ******************************************************************************/

class IndexedDBRequestChildBase : public PIndexedDBRequestChild
{
protected:
  nsRefPtr<AsyncConnectionHelper> mHelper;

public:
  IDBRequest*
  GetRequest() const;

  void
  Disconnect();

protected:
  explicit IndexedDBRequestChildBase(AsyncConnectionHelper* aHelper);
  virtual ~IndexedDBRequestChildBase();

  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBObjectStoreRequestChild
 ******************************************************************************/

class IndexedDBObjectStoreRequestChild : public IndexedDBRequestChildBase
{
  nsRefPtr<IDBObjectStore> mObjectStore;

  typedef ipc::ObjectStoreRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

public:
  IndexedDBObjectStoreRequestChild(AsyncConnectionHelper* aHelper,
                                   IDBObjectStore* aObjectStore,
                                   RequestType aRequestType);
  virtual ~IndexedDBObjectStoreRequestChild();

protected:
  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBIndexRequestChild
 ******************************************************************************/

class IndexedDBIndexRequestChild : public IndexedDBRequestChildBase
{
  nsRefPtr<IDBIndex> mIndex;

  typedef ipc::IndexRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

public:
  IndexedDBIndexRequestChild(AsyncConnectionHelper* aHelper, IDBIndex* aIndex,
                             RequestType aRequestType);
  virtual ~IndexedDBIndexRequestChild();

protected:
  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBCursorRequestChild
 ******************************************************************************/

class IndexedDBCursorRequestChild : public IndexedDBRequestChildBase
{
  nsRefPtr<IDBCursor> mCursor;

  typedef ipc::CursorRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

public:
  IndexedDBCursorRequestChild(AsyncConnectionHelper* aHelper,
                              IDBCursor* aCursor,
                              RequestType aRequestType);
  virtual ~IndexedDBCursorRequestChild();

protected:
  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestChild
 ******************************************************************************/

class IndexedDBDeleteDatabaseRequestChild :
  public PIndexedDBDeleteDatabaseRequestChild
{
  nsRefPtr<IDBFactory> mFactory;
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;
  nsCString mDatabaseId;

public:
  IndexedDBDeleteDatabaseRequestChild(IDBFactory* aFactory,
                                      IDBOpenDBRequest* aOpenRequest,
                                      const nsACString& aDatabaseId);
  virtual ~IndexedDBDeleteDatabaseRequestChild();

protected:
  virtual bool
  Recv__delete__(const nsresult& aRv) MOZ_OVERRIDE;

  virtual bool
  RecvBlocked(const uint64_t& aCurrentVersion) MOZ_OVERRIDE;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_ipc_indexeddbchild_h__
