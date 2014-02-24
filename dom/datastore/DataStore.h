/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataStore_h
#define mozilla_dom_DataStore_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
class DataStoreCursor;
class DataStoreImpl;
class StringOrUnsignedLong;
class OwningStringOrUnsignedLong;

class DataStore MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DataStore,
                                           DOMEventTargetHelper)

  explicit DataStore(nsPIDOMWindow* aWindow);

  // WebIDL (internal functions)

  static already_AddRefed<DataStore> Constructor(GlobalObject& aGlobal,
                                                 ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext *aCx) MOZ_OVERRIDE;

  static bool EnabledForScope(JSContext* aCx, JS::Handle<JSObject*> aObj);

  // WebIDL (public APIs)

  void GetName(nsAString& aName, ErrorResult& aRv);

  void GetOwner(nsAString& aOwner, ErrorResult& aRv);

  bool GetReadOnly(ErrorResult& aRv);

  already_AddRefed<Promise> Get(const Sequence<OwningStringOrUnsignedLong>& aId,
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

  already_AddRefed<Promise> Remove(const StringOrUnsignedLong& aId,
                                   const nsAString& aRevisionId,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> Clear(const nsAString& aRevisionId,
                                  ErrorResult& aRv);

  void GetRevisionId(nsAString& aRevisionId, ErrorResult& aRv);

  already_AddRefed<Promise> GetLength(ErrorResult& aRv);

  already_AddRefed<DataStoreCursor> Sync(const nsAString& aRevisionId,
                                         ErrorResult& aRv);

  IMPL_EVENT_HANDLER(change)

  // This internal function (ChromeOnly) is aimed to make the DataStore keep a
  // reference to the DataStoreImpl which really implements the API's logic in
  // JS. We also need to let the DataStoreImpl implementation keep the event
  // target of DataStore, so that it can know where to fire the events.
  void SetDataStoreImpl(DataStoreImpl& aStore, ErrorResult& aRv);

private:
  nsRefPtr<DataStoreImpl> mStore;
};

} //namespace dom
} //namespace mozilla

#endif