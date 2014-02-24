/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_DataStore_h
#define mozilla_dom_workers_DataStore_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsProxyRelease.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
class DataStore;
class DataStoreImpl;
class StringOrUnsignedLong;
class OwningStringOrUnsignedLong;

namespace workers {

class WorkerDataStoreCursor;
class WorkerGlobalScope;

class WorkerDataStore MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  WorkerDataStore(WorkerGlobalScope* aScope);

  // WebIDL (internal functions)

  static already_AddRefed<WorkerDataStore> Constructor(GlobalObject& aGlobal,
                                                       ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext *aCx) MOZ_OVERRIDE;

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

  // We don't use this for the WorkerDataStore.
  void SetDataStoreImpl(DataStoreImpl& aStore, ErrorResult& aRv);

  void SetBackingDataStore(
    const nsMainThreadPtrHandle<DataStore>& aBackingStore);

protected:
  virtual ~WorkerDataStore() {}

private:
  nsMainThreadPtrHandle<DataStore> mBackingStore;
};

} //namespace workers
} //namespace dom
} //namespace mozilla

#endif