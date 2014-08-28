/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaKeyNeededEvent_h__
#define mozilla_dom_MediaKeyNeededEvent_h__

#include "mozilla/dom/MediaEncryptedEventBinding.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class MediaEncryptedEvent MOZ_FINAL : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MediaEncryptedEvent, Event)
protected:
  virtual ~MediaEncryptedEvent();
  MediaEncryptedEvent(EventTarget* aOwner);

  nsString mInitDataType;
  JS::Heap<JSObject*> mInitData;

public:

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<MediaEncryptedEvent>
  Constructor(EventTarget* aOwner,
              const nsAString& aInitDataType,
              const nsTArray<uint8_t>& aInitData);

  static already_AddRefed<MediaEncryptedEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const MediaKeyNeededEventInit& aEventInitDict,
              ErrorResult& aRv);

  void GetInitDataType(nsString& aRetVal) const;

  void GetInitData(JSContext* cx,
                   JS::MutableHandle<JSObject*> aData,
                   ErrorResult& aRv);
private:
  nsTArray<uint8_t> mRawInitData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaKeyNeededEvent_h__
