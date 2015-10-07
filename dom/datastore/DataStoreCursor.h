/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataStoreCursor_h
#define mozilla_dom_DataStoreCursor_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
class DataStore;
class GlobalObject;
class DataStoreCursorImpl;

class DataStoreCursor final : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DataStoreCursor)

  // WebIDL (internal functions)

  static already_AddRefed<DataStoreCursor> Constructor(GlobalObject& aGlobal,
                                                       ErrorResult& aRv);

  bool WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

  // WebIDL (public APIs)

  already_AddRefed<DataStore> GetStore(ErrorResult& aRv);

  already_AddRefed<Promise> Next(ErrorResult& aRv);

  void Close(ErrorResult& aRv);

  // This internal function (ChromeOnly) is aimed to make the DataStoreCursor
  // keep a reference to the DataStoreCursorImpl which really implements the
  // API's logic in JS.
  void SetDataStoreCursorImpl(DataStoreCursorImpl& aCursor);

private:
  ~DataStoreCursor() {}
  nsRefPtr<DataStoreCursorImpl> mCursor;
};

} //namespace dom
} //namespace mozilla

#endif
