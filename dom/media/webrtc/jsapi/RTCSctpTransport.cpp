/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCSctpTransport.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/RTCSctpTransportBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RTCSctpTransport, DOMEventTargetHelper,
                                   mDtlsTransport)

NS_IMPL_ADDREF_INHERITED(RTCSctpTransport, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(RTCSctpTransport, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCSctpTransport)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

RTCSctpTransport::RTCSctpTransport(nsPIDOMWindowInner* aWindow,
                                   RTCDtlsTransport& aDtlsTransport,
                                   double aMaxMessageSize,
                                   const Nullable<uint16_t>& aMaxChannels)
    : DOMEventTargetHelper(aWindow),
      mState(RTCSctpTransportState::Connecting),
      mDtlsTransport(&aDtlsTransport),
      mMaxMessageSize(aMaxMessageSize),
      mMaxChannels(aMaxChannels) {}

JSObject* RTCSctpTransport::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return RTCSctpTransport_Binding::Wrap(aCx, this, aGivenProto);
}

void RTCSctpTransport::UpdateState(RTCSctpTransportState aState) {
  if (mState == RTCSctpTransportState::Closed || mState == aState) {
    return;
  }

  mState = aState;

  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, u"statechange"_ns, init);

  DispatchTrustedEvent(event);
}

}  // namespace mozilla::dom
