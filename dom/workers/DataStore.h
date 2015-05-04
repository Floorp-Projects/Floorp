/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_DataStore_h
#define mozilla_dom_workers_DataStore_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsProxyRelease.h"

#include "WorkerFeature.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
class DataStore;
class StringOrUnsignedLong;
class OwningStringOrUnsignedLong;

namespace workers {

class DataStoreChangeEventProxy;
class WorkerDataStoreCursor;
class WorkerGlobalScope;

class WorkerDataStore final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit WorkerDataStore(WorkerGlobalScope* aScope);

  // WebIDL (internal functions)

  static already_AddRefed<WorkerDataStore> Constructor(GlobalObject& aGlobal,
                                                       ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)

  void GetName(JSContext* aCx, nsAString& aName, ErrorResult& aRv);

  void GetOwner(JSContext* aCx, nsAString& aOwner, ErrorResult& aRv);

  bool GetReadOnly(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<Promise> Get(JSContext* aCx,
                                const Sequence<OwningStringOrUnsignedLong>& aId,
                                ErrorResult& aRv);

  already_AddRefed<Promise> Put(JSContext* aCx,
                                JS::Handle<JS::Value> aObj,
                                const StringOrUnsignedLong& aId,
                                const nsAString& aRevisionId,
                                ErrorResult& aRv);

  already_AddRefed<Promise> Add(JSContext* aCx,
                                JS::Handle<JS::Value> aObj,
                                const Optional<StringOrUnsignedLong>& aId,
                                const nsAString& aRevisionId,
                                ErrorResult& aRv);

  already_AddRefed<Promise> Remove(JSContext* aCx,
                                   const StringOrUnsignedLong& aId,
                                   const nsAString& aRevisionId,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> Clear(JSContext* aCx,
                                  const nsAString& aRevisionId,
                                  ErrorResult& aRv);

  void GetRevisionId(JSContext* aCx, nsAString& aRevisionId, ErrorResult& aRv);

  already_AddRefed<Promise> GetLength(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<WorkerDataStoreCursor> Sync(JSContext* aCx,
                                               const nsAString& aRevisionId,
                                               ErrorResult& aRv);

  IMPL_EVENT_HANDLER(change)

  void SetBackingDataStore(
    const nsMainThreadPtrHandle<DataStore>& aBackingStore);

  void SetDataStoreChangeEventProxy(DataStoreChangeEventProxy* aEventProxy);

protected:
  virtual ~WorkerDataStore() {}

private:
  nsMainThreadPtrHandle<DataStore> mBackingStore;
  nsRefPtr<DataStoreChangeEventProxy> mEventProxy;
};

class DataStoreChangeEventProxy final : public nsIDOMEventListener
                                      , public WorkerFeature
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  DataStoreChangeEventProxy(WorkerPrivate* aWorkerPrivate,
                            WorkerDataStore* aWorkerStore);

  WorkerPrivate* GetWorkerPrivate() const;

  WorkerDataStore* GetWorkerStore() const;

protected:
  // WorkerFeature implementation.

  bool Notify(JSContext* aCx, Status aStatus) override;

private:
  ~DataStoreChangeEventProxy() {};

  WorkerPrivate* mWorkerPrivate;

  nsRefPtr<WorkerDataStore> mWorkerStore;

  bool mCleanedUp; // To specify if the worker has been cancelled.

  // Ensure the worker and the main thread won't race to access |mCleanedUp|.
  Mutex mCleanUpLock;
};

} //namespace workers
} //namespace dom
} //namespace mozilla

#endif
