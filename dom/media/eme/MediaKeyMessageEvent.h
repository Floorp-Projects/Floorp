/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaKeyMessageEvent_h__
#define mozilla_dom_MediaKeyMessageEvent_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TypedArray.h"
#include "js/TypeDecls.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"

namespace mozilla {
namespace dom {

struct MediaKeyMessageEventInit;

class MediaKeyMessageEvent final : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MediaKeyMessageEvent, Event)
protected:
  virtual ~MediaKeyMessageEvent();
  explicit MediaKeyMessageEvent(EventTarget* aOwner);

  MediaKeyMessageType mMessageType;
  JS::Heap<JSObject*> mMessage;

public:
  virtual MediaKeyMessageEvent* AsMediaKeyMessageEvent();

  JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<MediaKeyMessageEvent>
  Constructor(EventTarget* aOwner,
              MediaKeyMessageType aMessageType,
              const nsTArray<uint8_t>& aMessage);

  static already_AddRefed<MediaKeyMessageEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const MediaKeyMessageEventInit& aEventInitDict,
              ErrorResult& aRv);

  MediaKeyMessageType MessageType() const { return mMessageType; }

  void GetMessage(JSContext* cx,
                  JS::MutableHandle<JSObject*> aMessage,
                  ErrorResult& aRv);

private:
  nsTArray<uint8_t> mRawMessage;
};


} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaKeyMessageEvent_h__
