/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DataStore.h"
#include "mozilla/dom/DataStoreCursor.h"
#include "mozilla/dom/DataStoreBinding.h"
#include "mozilla/dom/DataStoreImplBinding.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "AccessCheck.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(DataStore, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DataStore, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DataStore)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(DataStore, DOMEventTargetHelper, mStore)

DataStore::DataStore(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

DataStore::~DataStore()
{
}

already_AddRefed<DataStore>
DataStore::Constructor(GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DataStore> store = new DataStore(window);
  return store.forget();
}

JSObject*
DataStore::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DataStoreBinding::Wrap(aCx, this, aGivenProto);
}

/*static*/ bool
DataStore::EnabledForScope(JSContext* aCx, JS::Handle<JSObject*> aObj)
{
  // Only expose the interface when it is:
  // 1. enabled by the preference and
  // 2. accessed by the chrome codes in Gecko.
  return (Navigator::HasDataStoreSupport(aCx, aObj) &&
          nsContentUtils::ThreadsafeIsCallerChrome());
}

void
DataStore::GetName(nsAString& aName, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  nsAutoString name;
  mStore->GetName(name, aRv);
  aName.Assign(name);
}

void
DataStore::GetOwner(nsAString& aOwner, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  nsAutoString owner;
  mStore->GetOwner(owner, aRv);
  aOwner.Assign(owner);
}

bool
DataStore::GetReadOnly(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->GetReadOnly(aRv);
}

already_AddRefed<Promise>
DataStore::Get(const Sequence<OwningStringOrUnsignedLong>& aId,
               ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->Get(aId, aRv);
}

already_AddRefed<Promise>
DataStore::Put(JSContext* aCx,
               JS::Handle<JS::Value> aObj,
               const StringOrUnsignedLong& aId,
               const nsAString& aRevisionId,
               ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->Put(aObj, aId, aRevisionId, aRv);
}

already_AddRefed<Promise>
DataStore::Add(JSContext* aCx,
               JS::Handle<JS::Value> aObj,
               const Optional<StringOrUnsignedLong>& aId,
               const nsAString& aRevisionId,
               ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->Add(aObj, aId, aRevisionId, aRv);
}

already_AddRefed<Promise>
DataStore::Remove(const StringOrUnsignedLong& aId,
                  const nsAString& aRevisionId,
                  ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->Remove(aId, aRevisionId, aRv);
}

already_AddRefed<Promise>
DataStore::Clear(const nsAString& aRevisionId, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->Clear(aRevisionId, aRv);
}

void
DataStore::GetRevisionId(nsAString& aRevisionId, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  nsAutoString revisionId;
  mStore->GetRevisionId(revisionId, aRv);
  aRevisionId.Assign(revisionId);
}

already_AddRefed<Promise>
DataStore::GetLength(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->GetLength(aRv);
}

already_AddRefed<DataStoreCursor>
DataStore::Sync(const nsAString& aRevisionId, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStore);

  return mStore->Sync(aRevisionId, aRv);
}

void
DataStore::SetDataStoreImpl(DataStoreImpl& aStore, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mStore);

  mStore = &aStore;
  mStore->SetEventTarget(*this, aRv);
}

} //namespace dom
} //namespace mozilla
