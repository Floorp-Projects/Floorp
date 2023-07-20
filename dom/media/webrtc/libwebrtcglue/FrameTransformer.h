/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_FRAMETRANSFORMER_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_FRAMETRANSFORMER_H_

#include "api/frame_transformer_interface.h"
#include "libwebrtcglue/FrameTransformerProxy.h"
#include "nsISupportsImpl.h"
#include "mozilla/Mutex.h"
#include "jsapi/RTCRtpScriptTransformer.h"

namespace mozilla {

// There is one of these per RTCRtpSender and RTCRtpReceiver, for its entire
// lifetime. SetProxy is used to activate/deactivate it. In the inactive state
// (the default), this is just a synchronous passthrough.
class FrameTransformer : public webrtc::FrameTransformerInterface {
 public:
  explicit FrameTransformer(bool aVideo);
  virtual ~FrameTransformer();

  // This is set when RTCRtpSender/Receiver.transform is set, and unset when
  // RTCRtpSender/Receiver.transform is unset.
  void SetProxy(FrameTransformerProxy* aProxy);

  // If no proxy is set (ie; RTCRtpSender/Receiver.transform is not set), this
  // synchronously calls OnTransformedFrame with no modifcation. If a proxy is
  // set, we send the frame to it, and eventually that frame should come back
  // to OnTransformedFrame.
  void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> aFrame) override;
  void OnTransformedFrame(
      std::unique_ptr<webrtc::TransformableFrameInterface> aFrame);

  // When libwebrtc uses the same callback for all ssrcs
  // (right now, this is used for audio, but we do not care in this class)
  void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback> aCallback) override;
  void UnregisterTransformedFrameCallback() override;

  // When libwebrtc uses a different callback for each ssrc
  // (right now, this is used for video, but we do not care in this class)
  void RegisterTransformedFrameSinkCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback> aCallback,
      uint32_t aSsrc) override;
  void UnregisterTransformedFrameSinkCallback(uint32_t aSsrc) override;

  bool IsVideo() const { return mVideo; }

 private:
  const bool mVideo;
  Mutex mCallbacksMutex;
  // Written on a libwebrtc thread, read on the worker thread.
  rtc::scoped_refptr<webrtc::TransformedFrameCallback> mCallback
      MOZ_GUARDED_BY(mCallbacksMutex);
  std::map<uint32_t, rtc::scoped_refptr<webrtc::TransformedFrameCallback>>
      mCallbacksBySsrc MOZ_GUARDED_BY(mCallbacksMutex);

  Mutex mProxyMutex;
  // Written on the call thread, read on a libwebrtc/gmp/mediadataencoder/call
  // thread (which one depends on the media type and direction). Right now,
  // these are:
  // Send video: VideoStreamEncoder::encoder_queue_,
  //    WebrtcMediaDataEncoder::mTaskQueue, or GMP encoder thread.
  // Recv video: Call::worker_thread_
  // Send audio: ChannelSend::encoder_queue_
  // Recv audio: ChannelReceive::worker_thread_
  // This should have little to no lock contention
  // This corresponds to the RTCRtpScriptTransform/RTCRtpScriptTransformer.
  RefPtr<FrameTransformerProxy> mProxy MOZ_GUARDED_BY(mProxyMutex);
};  // FrameTransformer

}  // namespace mozilla

#endif  // MOZILLA_DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_FRAMETRANSFORMER_H_
