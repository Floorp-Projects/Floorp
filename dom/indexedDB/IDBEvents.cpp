/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBEvents.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/IDBVersionChangeEventBinding.h"
#include "nsString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::indexedDB;

namespace mozilla::dom {
namespace indexedDB {

const char16_t* kAbortEventType = u"abort";
const char16_t* kBlockedEventType = u"blocked";
const char16_t* kCompleteEventType = u"complete";
const char16_t* kErrorEventType = u"error";
const char16_t* kSuccessEventType = u"success";
const char16_t* kUpgradeNeededEventType = u"upgradeneeded";
const char16_t* kVersionChangeEventType = u"versionchange";
const char16_t* kCloseEventType = u"close";

RefPtr<Event> CreateGenericEvent(EventTarget* aOwner,
                                 const nsDependentString& aType,
                                 Bubbles aBubbles, Cancelable aCancelable) {
  RefPtr<Event> event = MakeAndAddRef<Event>(aOwner, nullptr, nullptr);

  event->InitEvent(aType, aBubbles == eDoesBubble, aCancelable == eCancelable);

  event->SetTrusted(true);

  return event;
}

}  // namespace indexedDB

// static
RefPtr<IDBVersionChangeEvent> IDBVersionChangeEvent::CreateInternal(
    EventTarget* aOwner, const nsAString& aType, uint64_t aOldVersion,
    const Nullable<uint64_t>& aNewVersion) {
  RefPtr<IDBVersionChangeEvent> event =
      new IDBVersionChangeEvent(aOwner, aOldVersion);
  if (!aNewVersion.IsNull()) {
    event->mNewVersion.SetValue(aNewVersion.Value());
  }

  event->InitEvent(aType, false, false);

  event->SetTrusted(true);

  return event;
}

RefPtr<IDBVersionChangeEvent> IDBVersionChangeEvent::Create(
    EventTarget* aOwner, const nsDependentString& aName, uint64_t aOldVersion,
    uint64_t aNewVersion) {
  Nullable<uint64_t> newVersion(aNewVersion);
  return CreateInternal(aOwner, aName, aOldVersion, newVersion);
}

RefPtr<IDBVersionChangeEvent> IDBVersionChangeEvent::Create(
    EventTarget* aOwner, const nsDependentString& aName, uint64_t aOldVersion) {
  Nullable<uint64_t> newVersion(0);
  newVersion.SetNull();
  return CreateInternal(aOwner, aName, aOldVersion, newVersion);
}

RefPtr<IDBVersionChangeEvent> IDBVersionChangeEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const IDBVersionChangeEventInit& aOptions) {
  nsCOMPtr<EventTarget> target = do_QueryInterface(aGlobal.GetAsSupports());

  return CreateInternal(target, aType, aOptions.mOldVersion,
                        aOptions.mNewVersion);
}

NS_IMPL_ADDREF_INHERITED(IDBVersionChangeEvent, Event)
NS_IMPL_RELEASE_INHERITED(IDBVersionChangeEvent, Event)

NS_INTERFACE_MAP_BEGIN(IDBVersionChangeEvent)
  NS_INTERFACE_MAP_ENTRY(IDBVersionChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

JSObject* IDBVersionChangeEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return IDBVersionChangeEvent_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
