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

class MediaKeyMessageEvent MOZ_FINAL : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MediaKeyMessageEvent, Event)
  virtual ~MediaKeyMessageEvent();
protected:
  MediaKeyMessageEvent(EventTarget* aOwner);

  JS::Heap<JSObject*> mMessage;
  nsString mDestinationURL;

public:
  virtual MediaKeyMessageEvent* AsMediaKeyMessageEvent();

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<MediaKeyMessageEvent>
    Constructor(EventTarget* aOwner,
                const nsAString& aURL,
                const nsTArray<uint8_t>& aMessage);

  static already_AddRefed<MediaKeyMessageEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const MediaKeyMessageEventInit& aEventInitDict,
              ErrorResult& aRv);

  void GetMessage(JSContext* cx,
                  JS::MutableHandle<JSObject*> aMessage,
                  ErrorResult& aRv);

  void GetDestinationURL(nsString& aRetVal) const;

private:
  nsTArray<uint8_t> mRawMessage;
};


} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaKeyMessageEvent_h__
