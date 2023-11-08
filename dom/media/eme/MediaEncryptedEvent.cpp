/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEncryptedEvent.h"
#include "mozilla/dom/MediaEncryptedEventBinding.h"
#include "nsContentUtils.h"
#include "js/ArrayBuffer.h"
#include "jsfriendapi.h"
#include "nsINode.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/HoldDropJSObjects.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaEncryptedEvent)

NS_IMPL_ADDREF_INHERITED(MediaEncryptedEvent, Event)
NS_IMPL_RELEASE_INHERITED(MediaEncryptedEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaEncryptedEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MediaEncryptedEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mInitData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaEncryptedEvent, Event)
  mozilla::DropJSObjects(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaEncryptedEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

MediaEncryptedEvent::MediaEncryptedEvent(EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr) {
  mozilla::HoldJSObjects(this);
}

MediaEncryptedEvent::~MediaEncryptedEvent() { mozilla::DropJSObjects(this); }

JSObject* MediaEncryptedEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaEncryptedEvent_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<MediaEncryptedEvent> MediaEncryptedEvent::Constructor(
    EventTarget* aOwner) {
  RefPtr<MediaEncryptedEvent> e = new MediaEncryptedEvent(aOwner);
  e->InitEvent(u"encrypted"_ns, CanBubble::eNo, Cancelable::eNo);
  e->SetTrusted(true);
  return e.forget();
}

already_AddRefed<MediaEncryptedEvent> MediaEncryptedEvent::Constructor(
    EventTarget* aOwner, const nsAString& aInitDataType,
    const nsTArray<uint8_t>& aInitData) {
  RefPtr<MediaEncryptedEvent> e = new MediaEncryptedEvent(aOwner);
  e->InitEvent(u"encrypted"_ns, CanBubble::eNo, Cancelable::eNo);
  e->mInitDataType = aInitDataType;
  e->mRawInitData = aInitData.Clone();
  e->SetTrusted(true);
  return e.forget();
}

already_AddRefed<MediaEncryptedEvent> MediaEncryptedEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const MediaKeyNeededEventInit& aEventInitDict, ErrorResult& aRv) {
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<MediaEncryptedEvent> e = new MediaEncryptedEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->mInitDataType = aEventInitDict.mInitDataType;
  if (!aEventInitDict.mInitData.IsNull()) {
    JS::Rooted<JSObject*> buffer(aGlobal.Context(),
                                 aEventInitDict.mInitData.Value().Obj());
    e->mInitData = JS::CopyArrayBuffer(aGlobal.Context(), buffer);
    if (!e->mInitData) {
      aRv.NoteJSContextException(aGlobal.Context());
      return nullptr;
    }
  }
  e->SetTrusted(trusted);
  return e.forget();
}

void MediaEncryptedEvent::GetInitDataType(nsString& aRetVal) const {
  aRetVal = mInitDataType;
}

void MediaEncryptedEvent::GetInitData(JSContext* cx,
                                      JS::MutableHandle<JSObject*> aData,
                                      ErrorResult& aRv) {
  if (mRawInitData.Length()) {
    mInitData = ArrayBuffer::Create(cx, this, mRawInitData, aRv);
    if (aRv.Failed()) {
      return;
    }
    mRawInitData.Clear();
  }
  aData.set(mInitData);
}

}  // namespace mozilla::dom
