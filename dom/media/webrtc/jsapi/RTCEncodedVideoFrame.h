/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCENCODEDVIDEOFRAME_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCENCODEDVIDEOFRAME_H_

#include "mozilla/RefPtr.h"
#include "nsIGlobalObject.h"
#include "jsapi/RTCEncodedFrameBase.h"
#include "mozilla/dom/RTCEncodedVideoFrameBinding.h"

namespace mozilla::dom {
class RTCRtpScriptTransformer;

// Wraps a libwebrtc frame, allowing the frame buffer to be modified, and
// providing read-only access to various metadata. After the libwebrtc frame is
// extracted (with RTCEncodedFrameBase::TakeFrame), the frame buffer is
// detached, but the metadata remains accessible.
class RTCEncodedVideoFrame final : public RTCEncodedFrameBase {
 public:
  explicit RTCEncodedVideoFrame(
      nsIGlobalObject* aGlobal,
      std::unique_ptr<webrtc::TransformableFrameInterface> aFrame,
      uint64_t aCounter, RTCRtpScriptTransformer* aOwner);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RTCEncodedVideoFrame,
                                           RTCEncodedFrameBase)

  // webidl (timestamp and data accessors live in base class)
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject() const;

  RTCEncodedVideoFrameType Type() const;

  void GetMetadata(RTCEncodedVideoFrameMetadata& aMetadata);

  bool CheckOwner(RTCRtpScriptTransformer* aOwner) const override;

  bool IsVideo() const override { return true; }

  // Not in webidl right now. Might change.
  // https://github.com/w3c/webrtc-encoded-transform/issues/147
  Maybe<std::string> Rid() const;

 private:
  virtual ~RTCEncodedVideoFrame();
  RefPtr<RTCRtpScriptTransformer> mOwner;
  RTCEncodedVideoFrameType mType;
  RTCEncodedVideoFrameMetadata mMetadata;
  Maybe<std::string> mRid;
};

}  // namespace mozilla::dom
#endif  // MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCENCODEDVIDEOFRAME_H_
