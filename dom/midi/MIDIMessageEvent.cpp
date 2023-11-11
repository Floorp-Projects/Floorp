/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Realm.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/MIDIMessageEvent.h"
#include "mozilla/dom/MIDIMessageEventBinding.h"
#include "js/GCAPI.h"
#include "jsfriendapi.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Performance.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_WITH_JS_MEMBERS(MIDIMessageEvent, Event, (),
                                                   (mData))

NS_IMPL_ADDREF_INHERITED(MIDIMessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(MIDIMessageEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MIDIMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

MIDIMessageEvent::MIDIMessageEvent(mozilla::dom::EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr) {
  mozilla::HoldJSObjects(this);
}

MIDIMessageEvent::~MIDIMessageEvent() { mozilla::DropJSObjects(this); }

JSObject* MIDIMessageEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MIDIMessageEvent_Binding::Wrap(aCx, this, aGivenProto);
}

MIDIMessageEvent* MIDIMessageEvent::AsMIDIMessageEvent() { return this; }

already_AddRefed<MIDIMessageEvent> MIDIMessageEvent::Constructor(
    EventTarget* aOwner, const class TimeStamp& aReceivedTime,
    const nsTArray<uint8_t>& aData) {
  MOZ_ASSERT(aOwner);
  RefPtr<MIDIMessageEvent> e = new MIDIMessageEvent(aOwner);
  e->InitEvent(u"midimessage"_ns, false, false);
  e->mEvent->mTimeStamp = aReceivedTime;
  e->mRawData = aData.Clone();
  e->SetTrusted(true);
  return e.forget();
}

already_AddRefed<MIDIMessageEvent> MIDIMessageEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const MIDIMessageEventInit& aEventInitDict, ErrorResult& aRv) {
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<MIDIMessageEvent> e = new MIDIMessageEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  // Set data for event. Timestamp will always be set to Now() (default for
  // event) using this constructor.
  if (aEventInitDict.mData.WasPassed()) {
    JSAutoRealm ar(aGlobal.Context(), aGlobal.Get());
    JS::Rooted<JSObject*> data(aGlobal.Context(),
                               aEventInitDict.mData.Value().Obj());
    e->mData = JS_NewUint8ArrayFromArray(aGlobal.Context(), data);
    if (NS_WARN_IF(!e->mData)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  e->SetTrusted(trusted);
  mozilla::HoldJSObjects(e.get());
  return e.forget();
}

void MIDIMessageEvent::GetData(JSContext* cx,
                               JS::MutableHandle<JSObject*> aData,
                               ErrorResult& aRv) {
  if (!mData) {
    mData = Uint8Array::Create(cx, this, mRawData, aRv);
    if (aRv.Failed()) {
      return;
    }
    mRawData.Clear();
  }
  aData.set(mData);
}

}  // namespace mozilla::dom
