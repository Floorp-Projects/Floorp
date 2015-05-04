/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_DataStoreCursor_h
#define mozilla_dom_workers_DataStoreCursor_h

#include "nsProxyRelease.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
class GlobalObject;
class DataStoreCursor;

namespace workers {

class WorkerDataStore;

class WorkerDataStoreCursor final
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WorkerDataStoreCursor)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(WorkerDataStoreCursor)

  explicit WorkerDataStoreCursor(WorkerDataStore* aWorkerStore);

  // WebIDL (internal functions)

  static already_AddRefed<WorkerDataStoreCursor> Constructor(GlobalObject& aGlobal,
                                                             ErrorResult& aRv);

  bool WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

  // WebIDL (public APIs)

  already_AddRefed<WorkerDataStore> GetStore(JSContext *aCx, ErrorResult& aRv);

  already_AddRefed<Promise> Next(JSContext *aCx, ErrorResult& aRv);

  void Close(JSContext *aCx, ErrorResult& aRv);

  void SetBackingDataStoreCursor(
    const nsMainThreadPtrHandle<DataStoreCursor>& aBackingCursor);

protected:
  virtual ~WorkerDataStoreCursor() {}

private:
  nsMainThreadPtrHandle<DataStoreCursor> mBackingCursor;

  // Keep track of the WorkerDataStore which owns this WorkerDataStoreCursor.
  nsRefPtr<WorkerDataStore> mWorkerStore;
};

} //namespace workers
} //namespace dom
} //namespace mozilla

#endif
