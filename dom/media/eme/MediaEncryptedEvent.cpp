/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEncryptedEvent.h"
#include "mozilla/dom/MediaEncryptedEventBinding.h"
#include "nsContentUtils.h"
#include "jsfriendapi.h"
#include "nsINode.h"
#include "mozilla/dom/MediaKeys.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaEncryptedEvent)

NS_IMPL_ADDREF_INHERITED(MediaEncryptedEvent, Event)
NS_IMPL_RELEASE_INHERITED(MediaEncryptedEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaEncryptedEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MediaEncryptedEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mInitData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaEncryptedEvent, Event)
  tmp->mInitData = nullptr;
  mozilla::DropJSObjects(this);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaEncryptedEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

MediaEncryptedEvent::MediaEncryptedEvent(EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr) {
  mozilla::HoldJSObjects(this);
}

MediaEncryptedEvent::~MediaEncryptedEvent() {
  mInitData = nullptr;
  mozilla::DropJSObjects(this);
}

JSObject* MediaEncryptedEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaEncryptedEvent_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<MediaEncryptedEvent> MediaEncryptedEvent::Constructor(
    EventTarget* aOwner) {
  RefPtr<MediaEncryptedEvent> e = new MediaEncryptedEvent(aOwner);
  e->InitEvent(NS_LITERAL_STRING("encrypted"), CanBubble::eNo, Cancelable::eNo);
  e->SetTrusted(true);
  return e.forget();
}

already_AddRefed<MediaEncryptedEvent> MediaEncryptedEvent::Constructor(
    EventTarget* aOwner, const nsAString& aInitDataType,
    const nsTArray<uint8_t>& aInitData) {
  RefPtr<MediaEncryptedEvent> e = new MediaEncryptedEvent(aOwner);
  e->InitEvent(NS_LITERAL_STRING("encrypted"), CanBubble::eNo, Cancelable::eNo);
  e->mInitDataType = aInitDataType;
  e->mRawInitData = aInitData;
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
    const auto& a = aEventInitDict.mInitData.Value();
    nsTArray<uint8_t> initData;
    CopyArrayBufferViewOrArrayBufferData(a, initData);
    e->mInitData = ArrayBuffer::Create(aGlobal.Context(), initData.Length(),
                                       initData.Elements());
    if (!e->mInitData) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
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
    mInitData = ArrayBuffer::Create(cx, this, mRawInitData.Length(),
                                    mRawInitData.Elements());
    if (!mInitData) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    mRawInitData.Clear();
  }
  aData.set(mInitData);
}

}  // namespace dom
}  // namespace mozilla
