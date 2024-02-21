/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RTCDtlsTransport_h_
#define _RTCDtlsTransport_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "js/RootingAPI.h"
#include "transport/transportlayer.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

enum class RTCDtlsTransportState : uint8_t;

class RTCDtlsTransport : public DOMEventTargetHelper {
 public:
  explicit RTCDtlsTransport(nsPIDOMWindowInner* aWindow);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RTCDtlsTransport,
                                           DOMEventTargetHelper)

  // webidl
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  IMPL_EVENT_HANDLER(statechange)
  RTCDtlsTransportState State() const { return mState; }

  void UpdateStateNoEvent(TransportLayer::State aState);
  void UpdateState(TransportLayer::State aState);

 private:
  virtual ~RTCDtlsTransport() = default;

  RTCDtlsTransportState mState;
};

}  // namespace mozilla::dom
#endif  // _RTCDtlsTransport_h_
