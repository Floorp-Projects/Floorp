/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIMessageEvent_h
#define mozilla_dom_MIDIMessageEvent_h

#include <cstdint>
#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/Event.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

struct JSContext;
namespace mozilla::dom {
struct MIDIMessageEventInit;

/**
 * Event that fires whenever a MIDI message is received by the MIDIInput object.
 *
 */
class MIDIMessageEvent final : public Event {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MIDIMessageEvent,
                                                         Event)
 protected:
  explicit MIDIMessageEvent(mozilla::dom::EventTarget* aOwner);

  JS::Heap<JSObject*> mData;

 public:
  virtual MIDIMessageEvent* AsMIDIMessageEvent();
  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<MIDIMessageEvent> Constructor(
      EventTarget* aOwner, const class TimeStamp& aReceivedTime,
      const nsTArray<uint8_t>& aData);

  static already_AddRefed<MIDIMessageEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const MIDIMessageEventInit& aEventInitDict, ErrorResult& aRv);

  // Getter for message data
  void GetData(JSContext* cx, JS::MutableHandle<JSObject*> aData,
               ErrorResult& aRv);

 private:
  ~MIDIMessageEvent();
  nsTArray<uint8_t> mRawData;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MIDIMessageEvent_h
