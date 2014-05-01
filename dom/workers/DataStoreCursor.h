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
class DataStoreCursorImpl;

namespace workers {

class WorkerDataStore;

class WorkerDataStoreCursor MOZ_FINAL
{
public:
  NS_INLINE_DECL_REFCOUNTING(WorkerDataStoreCursor)

  // WebIDL (internal functions)

  static already_AddRefed<WorkerDataStoreCursor> Constructor(GlobalObject& aGlobal,
                                                             ErrorResult& aRv);

  JSObject* WrapObject(JSContext *aCx);

  // WebIDL (public APIs)

  already_AddRefed<WorkerDataStore> GetStore(JSContext *aCx, ErrorResult& aRv);

  already_AddRefed<Promise> Next(JSContext *aCx, ErrorResult& aRv);

  void Close(JSContext *aCx, ErrorResult& aRv);

  // We don't use this for the WorkerDataStore.
  void SetDataStoreCursorImpl(DataStoreCursorImpl& aCursor);

  void SetBackingDataStoreCursor(
    const nsMainThreadPtrHandle<DataStoreCursor>& aBackingCursor);

protected:
  virtual ~WorkerDataStoreCursor() {}

private:
  nsMainThreadPtrHandle<DataStoreCursor> mBackingCursor;
};

} //namespace workers
} //namespace dom
} //namespace mozilla

#endif