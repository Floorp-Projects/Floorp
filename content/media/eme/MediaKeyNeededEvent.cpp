/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeyNeededEvent.h"
#include "mozilla/dom/MediaKeyNeededEventBinding.h"
#include "nsContentUtils.h"
#include "jsfriendapi.h"
#include "nsINode.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaKeyNeededEvent)

NS_IMPL_ADDREF_INHERITED(MediaKeyNeededEvent, Event)
NS_IMPL_RELEASE_INHERITED(MediaKeyNeededEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaKeyNeededEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MediaKeyNeededEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mInitData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaKeyNeededEvent, Event)
  tmp->mInitData = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaKeyNeededEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

MediaKeyNeededEvent::MediaKeyNeededEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
  mozilla::HoldJSObjects(this);
}

MediaKeyNeededEvent::~MediaKeyNeededEvent()
{
  mInitData = nullptr;
  mozilla::DropJSObjects(this);
}

JSObject*
MediaKeyNeededEvent::WrapObject(JSContext* aCx)
{
  return MediaKeyNeededEventBinding::Wrap(aCx, this);
}

already_AddRefed<MediaKeyNeededEvent>
MediaKeyNeededEvent::Constructor(EventTarget* aOwner,
                                 const nsAString& aInitDataType,
                                 const nsTArray<uint8_t>& aInitData)
{
  nsRefPtr<MediaKeyNeededEvent> e = new MediaKeyNeededEvent(aOwner);
  e->InitEvent(NS_LITERAL_STRING("needkey"), false, false);
  e->mInitDataType = aInitDataType;
  e->mRawInitData = aInitData;
  e->SetTrusted(true);
  return e.forget();
}

already_AddRefed<MediaKeyNeededEvent>
MediaKeyNeededEvent::Constructor(const GlobalObject& aGlobal,
                                 const nsAString& aType,
                                 const MediaKeyNeededEventInit& aEventInitDict,
                                 ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<MediaKeyNeededEvent> e = new MediaKeyNeededEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->mInitDataType = aEventInitDict.mInitDataType;
  if (aEventInitDict.mInitData.WasPassed() &&
      !aEventInitDict.mInitData.Value().IsNull()) {
    const auto& a = aEventInitDict.mInitData.Value().Value();
    a.ComputeLengthAndData();
    e->mInitData = Uint8Array::Create(aGlobal.Context(), owner, a.Length(), a.Data());
    if (!e->mInitData) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }
  e->SetTrusted(trusted);
  return e.forget();
}

void
MediaKeyNeededEvent::GetInitDataType(nsString& aRetVal) const
{
  aRetVal = mInitDataType;
}

void
MediaKeyNeededEvent::GetInitData(JSContext* cx,
                                 JS::MutableHandle<JSObject*> aData,
                                 ErrorResult& aRv)
{
  if (mRawInitData.Length()) {
    mInitData = Uint8Array::Create(cx,
                                   this,
                                   mRawInitData.Length(),
                                   mRawInitData.Elements());
    if (!mInitData) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    mRawInitData.Clear();
  }
  if (mInitData) {
    JS::ExposeObjectToActiveJS(mInitData);
  }
  aData.set(mInitData);
}

} // namespace dom
} // namespace mozilla
