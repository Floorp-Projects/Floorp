/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCDtlsTransport.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/RTCDtlsTransportBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RTCDtlsTransport, DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(RTCDtlsTransport, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(RTCDtlsTransport, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCDtlsTransport)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

RTCDtlsTransport::RTCDtlsTransport(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow), mState(RTCDtlsTransportState::New) {}

JSObject* RTCDtlsTransport::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return RTCDtlsTransport_Binding::Wrap(aCx, this, aGivenProto);
}

void RTCDtlsTransport::UpdateStateNoEvent(TransportLayer::State aState) {
  if (mState == RTCDtlsTransportState::Closed) {
    return;
  }

  switch (aState) {
    case TransportLayer::TS_NONE:
      break;
    case TransportLayer::TS_INIT:
      break;
    case TransportLayer::TS_CONNECTING:
      mState = RTCDtlsTransportState::Connecting;
      break;
    case TransportLayer::TS_OPEN:
      mState = RTCDtlsTransportState::Connected;
      break;
    case TransportLayer::TS_CLOSED:
      mState = RTCDtlsTransportState::Closed;
      break;
    case TransportLayer::TS_ERROR:
      mState = RTCDtlsTransportState::Failed;
      break;
  }
}

void RTCDtlsTransport::UpdateState(TransportLayer::State aState) {
  RTCDtlsTransportState oldState = mState;
  UpdateStateNoEvent(aState);
  if (oldState == mState) {
    return;
  }

  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, u"statechange"_ns, init);

  DispatchTrustedEvent(event);
}

}  // namespace mozilla::dom
