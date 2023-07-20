/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_FRAMETRANSFORMERPROXY_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_FRAMETRANSFORMERPROXY_H_

#include "nsISupportsImpl.h"
#include "mozilla/Mutex.h"
#include "mozilla/Maybe.h"
#include <list>
#include <memory>

class nsIEventTarget;

namespace webrtc {
class TransformableFrameInterface;
class VideoReceiveStreamInterface;
}  // namespace webrtc

namespace mozilla {

class FrameTransformer;
class WebrtcVideoConduit;
class CopyableErrorResult;

namespace dom {
class RTCRtpScriptTransformer;
class RTCRtpSender;
class RTCRtpReceiver;
}  // namespace dom

// This corresponds to a single RTCRtpScriptTransform (and its
// RTCRtpScriptTransformer, once that is created on the worker thread). This
// is intended to decouple threading/lifecycle/include-dependencies between
// FrameTransformer (on the libwebrtc side of things), RTCRtpScriptTransformer
// (on the worker side of things), RTCRtpScriptTransform and
// RTCRtpSender/Receiver (on the main thread), and prevents frames from being
// lost while we're setting things up on the worker. In other words, this
// handles the inconvenient stuff.
class FrameTransformerProxy {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FrameTransformerProxy);

  FrameTransformerProxy();
  FrameTransformerProxy(const FrameTransformerProxy& aRhs) = delete;
  FrameTransformerProxy(FrameTransformerProxy&& aRhs) = delete;
  FrameTransformerProxy& operator=(const FrameTransformerProxy& aRhs) = delete;
  FrameTransformerProxy& operator=(FrameTransformerProxy&& aRhs) = delete;

  // Called at most once (might not be called if the worker is shutting down),
  // on the worker thread.
  void SetScriptTransformer(dom::RTCRtpScriptTransformer& aTransformer);

  // Can be called from the worker thread (if the worker is shutting down), or
  // main (if RTCRtpSender/RTCRtpReceiver is done with us).
  void ReleaseScriptTransformer();

  // RTCRtpScriptTransformer calls this when it is done transforming a frame.
  void OnTransformedFrame(
      std::unique_ptr<webrtc::TransformableFrameInterface> aFrame);

  Maybe<bool> IsVideo() const;

  // Called by FrameTransformer, on main. Only one FrameTransformer will ever
  // be registered over the lifetime of this object. This is where we route
  // transformed frames. If this is set, we can also expect to receive calls to
  // Transform.
  void SetLibwebrtcTransformer(FrameTransformer* aLibwebrtcTransformer);

  // FrameTransformer calls this while we're registered with it (by
  // SetLibwebrtcTransformer)
  void Transform(std::unique_ptr<webrtc::TransformableFrameInterface> aFrame);

  void SetSender(dom::RTCRtpSender* aSender);
  void SetReceiver(dom::RTCRtpReceiver* aReceiver);

  // Called on worker thread
  bool RequestKeyFrame();
  // Called on call thread
  void KeyFrameRequestDone(bool aSuccess);

  bool GenerateKeyFrame(const Maybe<std::string>& aRid);
  void GenerateKeyFrameError(const Maybe<std::string>& aRid,
                             const CopyableErrorResult& aResult);

 private:
  virtual ~FrameTransformerProxy();

  // Worker thread only. Set at most once.
  // Does not need any mutex protection.
  RefPtr<dom::RTCRtpScriptTransformer> mScriptTransformer;

  mutable Mutex mMutex;
  // Written on the worker thread. Read on libwebrtc threads, mainthread, and
  // the worker thread.
  RefPtr<nsIEventTarget> mWorkerThread MOZ_GUARDED_BY(mMutex);
  // We need a flag for this in case the ReleaseScriptTransformer call comes
  // _before_ the script transformer is set, to disable SetScriptTransformer.
  // Could be written on main or the worker thread. Read on main, worker, and
  // libwebrtc threads.
  bool mReleaseScriptTransformerCalled MOZ_GUARDED_BY(mMutex) = false;
  // Used when frames arrive before the script transformer is created, which
  // should be pretty rare. Accessed on worker and libwebrtc threads.
  std::list<std::unique_ptr<webrtc::TransformableFrameInterface>> mQueue
      MOZ_GUARDED_BY(mMutex);
  // Written on main, read on the worker thread.
  FrameTransformer* mLibwebrtcTransformer MOZ_GUARDED_BY(mMutex) = nullptr;

  // TODO: Will be used to route GenerateKeyFrame. Details TBD.
  RefPtr<dom::RTCRtpSender> mSender MOZ_GUARDED_BY(mMutex);
  // Set on mainthread. This is where we route RequestKeyFrame calls from the
  // worker thread. Mutex protected because spec wants sync errors if the
  // receiver is not set (or the right type). If spec drops this requirement,
  // this could be mainthread only and non-mutex-protected.
  RefPtr<dom::RTCRtpReceiver> mReceiver MOZ_GUARDED_BY(mMutex);
  Maybe<bool> mVideo MOZ_GUARDED_BY(mMutex);
};

}  // namespace mozilla

#endif  // MOZILLA_DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_FRAMETRANSFORMERPROXY_H_
