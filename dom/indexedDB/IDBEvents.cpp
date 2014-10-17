/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

namespace mozilla {
namespace dom {
namespace indexedDB {

const char16_t* kAbortEventType = MOZ_UTF16("abort");
const char16_t* kBlockedEventType = MOZ_UTF16("blocked");
const char16_t* kCompleteEventType = MOZ_UTF16("complete");
const char16_t* kErrorEventType = MOZ_UTF16("error");
const char16_t* kSuccessEventType = MOZ_UTF16("success");
const char16_t* kUpgradeNeededEventType = MOZ_UTF16("upgradeneeded");
const char16_t* kVersionChangeEventType = MOZ_UTF16("versionchange");

already_AddRefed<nsIDOMEvent>
CreateGenericEvent(EventTarget* aOwner,
                   const nsDependentString& aType,
                   Bubbles aBubbles,
                   Cancelable aCancelable)
{
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = NS_NewDOMEvent(getter_AddRefs(event), aOwner, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  rv = event->InitEvent(aType,
                        aBubbles == eDoesBubble ? true : false,
                        aCancelable == eCancelable ? true : false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  event->SetTrusted(true);

  return event.forget();
}

// static
already_AddRefed<IDBVersionChangeEvent>
IDBVersionChangeEvent::CreateInternal(EventTarget* aOwner,
                                      const nsAString& aType,
                                      uint64_t aOldVersion,
                                      Nullable<uint64_t> aNewVersion)
{
  nsRefPtr<IDBVersionChangeEvent> event =
    new IDBVersionChangeEvent(aOwner, aOldVersion);
  if (!aNewVersion.IsNull()) {
    event->mNewVersion.SetValue(aNewVersion.Value());
  }

  nsresult rv = event->InitEvent(aType, false, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  event->SetTrusted(true);

  return event.forget();
}

already_AddRefed<IDBVersionChangeEvent>
IDBVersionChangeEvent::Constructor(const GlobalObject& aGlobal,
                                   const nsAString& aType,
                                   const IDBVersionChangeEventInit& aOptions,
                                   ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> target = do_QueryInterface(aGlobal.GetAsSupports());

  return CreateInternal(target,
                        aType,
                        aOptions.mOldVersion,
                        aOptions.mNewVersion);
}

NS_IMPL_ADDREF_INHERITED(IDBVersionChangeEvent, Event)
NS_IMPL_RELEASE_INHERITED(IDBVersionChangeEvent, Event)

NS_INTERFACE_MAP_BEGIN(IDBVersionChangeEvent)
  NS_INTERFACE_MAP_ENTRY(IDBVersionChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

JSObject*
IDBVersionChangeEvent::WrapObjectInternal(JSContext* aCx)
{
  return IDBVersionChangeEventBinding::Wrap(aCx, this);
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
