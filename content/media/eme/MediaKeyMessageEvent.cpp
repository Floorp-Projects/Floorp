/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "js/GCAPI.h"
#include "jsfriendapi.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/TypedArray.h"
#include "nsContentUtils.h"
#include "mozilla/dom/MediaKeys.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaKeyMessageEvent)

NS_IMPL_ADDREF_INHERITED(MediaKeyMessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(MediaKeyMessageEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaKeyMessageEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MediaKeyMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMessage)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaKeyMessageEvent, Event)
  tmp->mMessage = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaKeyMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

MediaKeyMessageEvent::MediaKeyMessageEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
  mozilla::HoldJSObjects(this);
}

MediaKeyMessageEvent::~MediaKeyMessageEvent()
{
  mMessage = nullptr;
  mozilla::DropJSObjects(this);
}

MediaKeyMessageEvent*
MediaKeyMessageEvent::AsMediaKeyMessageEvent()
{
  return this;
}

JSObject*
MediaKeyMessageEvent::WrapObjectInternal(JSContext* aCx)
{
  return MediaKeyMessageEventBinding::Wrap(aCx, this);
}

already_AddRefed<MediaKeyMessageEvent>
MediaKeyMessageEvent::Constructor(EventTarget* aOwner,
                                  const nsAString& aURL,
                                  const nsTArray<uint8_t>& aMessage)
{
  nsRefPtr<MediaKeyMessageEvent> e = new MediaKeyMessageEvent(aOwner);
  e->InitEvent(NS_LITERAL_STRING("message"), false, false);
  e->mRawMessage = aMessage;
  e->mDestinationURL = aURL;
  e->SetTrusted(true);
  return e.forget();
}

already_AddRefed<MediaKeyMessageEvent>
MediaKeyMessageEvent::Constructor(const GlobalObject& aGlobal,
                                  const nsAString& aType,
                                  const MediaKeyMessageEventInit& aEventInitDict,
                                  ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<MediaKeyMessageEvent> e = new MediaKeyMessageEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  const uint8_t* data = nullptr;
  size_t length = 0;
  if (aEventInitDict.mMessage.WasPassed()) {
    const auto& a = aEventInitDict.mMessage.Value();
    a.ComputeLengthAndData();
    data = a.Data();
    length = a.Length();
  }
  e->mMessage = ArrayBuffer::Create(aGlobal.Context(), length, data);
  if (!e->mMessage) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  e->mDestinationURL = aEventInitDict.mDestinationURL;
  e->SetTrusted(trusted);
  return e.forget();
}

void
MediaKeyMessageEvent::GetMessage(JSContext* cx,
                                 JS::MutableHandle<JSObject*> aMessage,
                                 ErrorResult& aRv)
{
  if (!mMessage) {
    mMessage = ArrayBuffer::Create(cx,
                                   mRawMessage.Length(),
                                   mRawMessage.Elements());
    if (!mMessage) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    mRawMessage.Clear();
  }
  JS::ExposeObjectToActiveJS(mMessage);
  aMessage.set(mMessage);
}

void
MediaKeyMessageEvent::GetDestinationURL(nsString& aRetVal) const
{
  aRetVal = mDestinationURL;
}

} // namespace dom
} // namespace mozilla
