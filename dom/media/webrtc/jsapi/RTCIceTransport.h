/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCICETRANSPORT_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCICETRANSPORT_H_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "js/RootingAPI.h"
#include "transport/transportlayer.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

enum class RTCIceTransportState : uint8_t;
enum class RTCIceGathererState : uint8_t;

class RTCIceTransport : public DOMEventTargetHelper {
 public:
  explicit RTCIceTransport(nsPIDOMWindowInner* aWindow);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RTCIceTransport,
                                           DOMEventTargetHelper)

  // webidl
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(gatheringstatechange)
  RTCIceTransportState State() const { return mState; }
  RTCIceGathererState GatheringState() const { return mGatheringState; }

  void SetState(RTCIceTransportState aState);
  void SetGatheringState(RTCIceGathererState aState);

  void FireStateChangeEvent();
  void FireGatheringStateChangeEvent();

 private:
  virtual ~RTCIceTransport() = default;

  RTCIceTransportState mState;
  RTCIceGathererState mGatheringState;
};

}  // namespace mozilla::dom
#endif  // MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCICETRANSPORT_H_
