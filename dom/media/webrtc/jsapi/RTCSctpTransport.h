/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RTCSctpTransport_h_
#define _RTCSctpTransport_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "js/RootingAPI.h"
#include "RTCDtlsTransport.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

enum class RTCSctpTransportState : uint8_t;

class RTCSctpTransport : public DOMEventTargetHelper {
 public:
  explicit RTCSctpTransport(nsPIDOMWindowInner* aWindow,
                            RTCDtlsTransport& aDtlsTransport,
                            double aMaxMessageSize,
                            const Nullable<uint16_t>& aMaxChannels);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RTCSctpTransport,
                                           DOMEventTargetHelper)

  // webidl
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  IMPL_EVENT_HANDLER(statechange)

  RTCDtlsTransport* Transport() const { return mDtlsTransport; }
  RTCSctpTransportState State() const { return mState; }
  double MaxMessageSize() const { return mMaxMessageSize; }
  Nullable<uint16_t> GetMaxChannels() const { return mMaxChannels; }

  void SetTransport(RTCDtlsTransport& aTransport) {
    mDtlsTransport = &aTransport;
  }

  void SetMaxMessageSize(double aMaxMessageSize) {
    mMaxMessageSize = aMaxMessageSize;
  }

  void SetMaxChannels(const Nullable<uint16_t>& aMaxChannels) {
    mMaxChannels = aMaxChannels;
  }

  void UpdateState(RTCSctpTransportState aState);

 private:
  virtual ~RTCSctpTransport() = default;

  RTCSctpTransportState mState;
  RefPtr<RTCDtlsTransport> mDtlsTransport;
  double mMaxMessageSize;
  Nullable<uint16_t> mMaxChannels;
};

}  // namespace mozilla::dom
#endif  // _RTCSctpTransport_h_
