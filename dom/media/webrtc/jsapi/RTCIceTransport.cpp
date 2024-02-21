/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCIceTransport.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/RTCIceTransportBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RTCIceTransport, DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(RTCIceTransport, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(RTCIceTransport, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCIceTransport)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

RTCIceTransport::RTCIceTransport(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mState(RTCIceTransportState::New),
      mGatheringState(RTCIceGathererState::New) {}

JSObject* RTCIceTransport::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return RTCIceTransport_Binding::Wrap(aCx, this, aGivenProto);
}

void RTCIceTransport::SetState(RTCIceTransportState aState) { mState = aState; }

void RTCIceTransport::SetGatheringState(RTCIceGathererState aState) {
  mGatheringState = aState;
}

void RTCIceTransport::FireStateChangeEvent() {
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, u"statechange"_ns, init);

  DispatchTrustedEvent(event);
}

void RTCIceTransport::FireGatheringStateChangeEvent() {
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event =
      Event::Constructor(this, u"gatheringstatechange"_ns, init);

  DispatchTrustedEvent(event);
}

}  // namespace mozilla::dom
