/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCENCODEDFRAMEBASE_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCENCODEDFRAMEBASE_H_

#include "js/TypeDecls.h"
#include "mozilla/dom/TypedArray.h"  // ArrayBuffer
#include "mozilla/Assertions.h"
#include "api/frame_transformer_interface.h"
#include <memory>

class nsIGlobalObject;

namespace mozilla::dom {

class RTCRtpScriptTransformer;

class RTCEncodedFrameBase : public nsISupports, public nsWrapperCache {
 public:
  explicit RTCEncodedFrameBase(
      nsIGlobalObject* aGlobal,
      std::unique_ptr<webrtc::TransformableFrameInterface> aFrame,
      uint64_t aCounter);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(RTCEncodedFrameBase)

  // Common webidl for RTCEncodedVideoFrame/RTCEncodedAudioFrame
  unsigned long Timestamp() const;

  void SetData(const ArrayBuffer& aData);

  void GetData(JSContext* aCx, JS::Rooted<JSObject*>* aObj);

  uint64_t GetCounter() const;

  virtual bool CheckOwner(RTCRtpScriptTransformer* aOwner) const = 0;

  std::unique_ptr<webrtc::TransformableFrameInterface> TakeFrame();

  virtual bool IsVideo() const = 0;

 protected:
  virtual ~RTCEncodedFrameBase();
  RefPtr<nsIGlobalObject> mGlobal;
  std::unique_ptr<webrtc::TransformableFrameInterface> mFrame;
  const uint64_t mCounter = 0;
  const unsigned long mTimestamp = 0;
  JS::Heap<JSObject*> mData;
};

}  // namespace mozilla::dom
#endif  // MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCENCODEDFRAMEBASE_H_
