/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DataStore.h"
#include "mozilla/dom/DataStoreCursor.h"
#include "mozilla/dom/DataStoreBinding.h"
#include "mozilla/dom/DataStoreImplBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(DataStoreCursor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DataStoreCursor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DataStoreCursor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(DataStoreCursor, mCursor)

already_AddRefed<DataStoreCursor>
DataStoreCursor::Constructor(GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsRefPtr<DataStoreCursor> cursor = new DataStoreCursor();
  return cursor.forget();
}

bool
DataStoreCursor::WrapObject(JSContext* aCx,
                            JS::Handle<JSObject*> aGivenProto,
                            JS::MutableHandle<JSObject*> aReflector)
{
  return DataStoreCursorBinding::Wrap(aCx, this, aGivenProto, aReflector);
}

already_AddRefed<DataStore>
DataStoreCursor::GetStore(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mCursor);

  return mCursor->GetStore(aRv);
}

already_AddRefed<Promise>
DataStoreCursor::Next(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mCursor);

  return mCursor->Next(aRv);
}

void
DataStoreCursor::Close(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mCursor);

  mCursor->Close(aRv);
}

void
DataStoreCursor::SetDataStoreCursorImpl(DataStoreCursorImpl& aCursor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mCursor);

  mCursor = &aCursor;
}

} //namespace dom
} //namespace mozilla
