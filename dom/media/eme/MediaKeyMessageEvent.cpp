/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "js/ArrayBuffer.h"
#include "js/RootingAPI.h"
#include "jsfriendapi.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/TypedArray.h"
#include "nsContentUtils.h"
#include "mozilla/dom/MediaKeys.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaKeyMessageEvent)

NS_IMPL_ADDREF_INHERITED(MediaKeyMessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(MediaKeyMessageEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaKeyMessageEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MediaKeyMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMessage)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaKeyMessageEvent, Event)
  mozilla::DropJSObjects(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeyMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

MediaKeyMessageEvent::MediaKeyMessageEvent(EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr),
      mMessageType(static_cast<MediaKeyMessageType>(0)) {
  mozilla::HoldJSObjects(this);
}

MediaKeyMessageEvent::~MediaKeyMessageEvent() { mozilla::DropJSObjects(this); }

MediaKeyMessageEvent* MediaKeyMessageEvent::AsMediaKeyMessageEvent() {
  return this;
}

JSObject* MediaKeyMessageEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaKeyMessageEvent_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<MediaKeyMessageEvent> MediaKeyMessageEvent::Constructor(
    EventTarget* aOwner, MediaKeyMessageType aMessageType,
    const nsTArray<uint8_t>& aMessage) {
  RefPtr<MediaKeyMessageEvent> e = new MediaKeyMessageEvent(aOwner);
  e->InitEvent(u"message"_ns, false, false);
  e->mMessageType = aMessageType;
  e->mRawMessage = aMessage.Clone();
  e->SetTrusted(true);
  return e.forget();
}

already_AddRefed<MediaKeyMessageEvent> MediaKeyMessageEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const MediaKeyMessageEventInit& aEventInitDict, ErrorResult& aRv) {
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<MediaKeyMessageEvent> e = new MediaKeyMessageEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  JS::Rooted<JSObject*> buffer(aGlobal.Context(),
                               aEventInitDict.mMessage.Obj());
  e->mMessage = JS::CopyArrayBuffer(aGlobal.Context(), buffer);
  if (!e->mMessage) {
    aRv.NoteJSContextException(aGlobal.Context());
    return nullptr;
  }
  e->mMessageType = aEventInitDict.mMessageType;
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

void MediaKeyMessageEvent::GetMessage(JSContext* cx,
                                      JS::MutableHandle<JSObject*> aMessage,
                                      ErrorResult& aRv) {
  if (!mMessage) {
    mMessage = ArrayBuffer::Create(cx, this, mRawMessage.Length(),
                                   mRawMessage.Elements());
    if (!mMessage) {
      aRv.NoteJSContextException(cx);
      return;
    }
    mRawMessage.Clear();
  }
  aMessage.set(mMessage);
}

}  // namespace mozilla::dom
