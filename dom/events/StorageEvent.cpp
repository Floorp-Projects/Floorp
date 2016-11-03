/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/DOMStorage.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(StorageEvent)

NS_IMPL_ADDREF_INHERITED(StorageEvent, Event)
NS_IMPL_RELEASE_INHERITED(StorageEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(StorageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStorageArea)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(StorageEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(StorageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStorageArea)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(StorageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

StorageEvent::StorageEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
}

StorageEvent::~StorageEvent()
{
}

StorageEvent*
StorageEvent::AsStorageEvent()
{
  return this;
}

JSObject*
StorageEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return StorageEventBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<StorageEvent>
StorageEvent::Constructor(EventTarget* aOwner,
                          const nsAString& aType,
                          const StorageEventInit& aEventInitDict)
{
  RefPtr<StorageEvent> e = new StorageEvent(aOwner);

  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->mKey = aEventInitDict.mKey;
  e->mOldValue = aEventInitDict.mOldValue;
  e->mNewValue = aEventInitDict.mNewValue;
  e->mUrl = aEventInitDict.mUrl;
  e->mStorageArea = aEventInitDict.mStorageArea;
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

already_AddRefed<StorageEvent>
StorageEvent::Constructor(const GlobalObject& aGlobal,
                          const nsAString& aType,
                          const StorageEventInit& aEventInitDict,
                          ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict);
}

void
StorageEvent::InitStorageEvent(const nsAString& aType, bool aCanBubble,
                               bool aCancelable, const nsAString& aKey,
                               const nsAString& aOldValue,
                               const nsAString& aNewValue,
                               const nsAString& aURL,
                               DOMStorage* aStorageArea)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  InitEvent(aType, aCanBubble, aCancelable);
  mKey = aKey;
  mOldValue = aOldValue;
  mNewValue = aNewValue;
  mUrl = aURL;
  mStorageArea = aStorageArea;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<StorageEvent>
NS_NewDOMStorageEvent(EventTarget* aOwner)
{
  RefPtr<StorageEvent> e = new StorageEvent(aOwner);

  e->SetTrusted(e->Init(aOwner));
  return e.forget();
}

