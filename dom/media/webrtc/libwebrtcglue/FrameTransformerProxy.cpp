/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "libwebrtcglue/FrameTransformerProxy.h"
#include "libwebrtcglue/FrameTransformer.h"
#include "mozilla/dom/RTCRtpSender.h"
#include "mozilla/dom/RTCRtpReceiver.h"
#include "mozilla/Logging.h"
#include "mozilla/Mutex.h"
#include "jsapi/RTCRtpScriptTransformer.h"
#include "nsThreadUtils.h"
#include "mozilla/Assertions.h"
#include <utility>
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nscore.h"
#include "ErrorList.h"
#include "nsIRunnable.h"
#include "nsIEventTarget.h"
#include "api/frame_transformer_interface.h"
#include <memory>
#include "nsDebug.h"
#include "nsISupports.h"
#include <string>

namespace mozilla {

LazyLogModule gFrameTransformerProxyLog("FrameTransformerProxy");

FrameTransformerProxy::FrameTransformerProxy()
    : mMutex("FrameTransformerProxy::mMutex") {}

FrameTransformerProxy::~FrameTransformerProxy() = default;

void FrameTransformerProxy::SetScriptTransformer(
    dom::RTCRtpScriptTransformer& aTransformer) {
  MutexAutoLock lock(mMutex);
  if (mReleaseScriptTransformerCalled) {
    MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Warning,
            ("RTCRtpScriptTransformer is ready, but ReleaseScriptTransformer "
             "has already been called."));
    // The mainthread side has torn down while the worker init was pending.
    // Don't grab a reference to the worker thread, or the script transformer.
    // Also, let the script transformer know that we do not need it after all.
    aTransformer.NotifyReleased();
    return;
  }

  MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Info,
          ("RTCRtpScriptTransformer is ready!"));
  mWorkerThread = GetCurrentSerialEventTarget();
  MOZ_ASSERT(mWorkerThread);

  MOZ_ASSERT(!mScriptTransformer);
  mScriptTransformer = &aTransformer;
  while (!mQueue.empty()) {
    mScriptTransformer->TransformFrame(std::move(mQueue.front()));
    mQueue.pop_front();
  }
}

Maybe<bool> FrameTransformerProxy::IsVideo() const {
  MutexAutoLock lock(mMutex);
  return mVideo;
}

void FrameTransformerProxy::ReleaseScriptTransformer() {
  MutexAutoLock lock(mMutex);
  MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Debug, ("In %s", __FUNCTION__));
  if (mReleaseScriptTransformerCalled) {
    return;
  }
  mReleaseScriptTransformerCalled = true;

  if (mWorkerThread) {
    mWorkerThread->Dispatch(NS_NewRunnableFunction(
        __func__, [this, self = RefPtr<FrameTransformerProxy>(this)] {
          if (mScriptTransformer) {
            mScriptTransformer->NotifyReleased();
            mScriptTransformer = nullptr;
          }

          // Make sure cycles are broken; this unset might have been caused by
          // something other than the sender/receiver being unset.
          GetMainThreadSerialEventTarget()->Dispatch(
              NS_NewRunnableFunction(__func__, [this, self] {
                MutexAutoLock lock(mMutex);
                mSender = nullptr;
                mReceiver = nullptr;
              }));
        }));
    mWorkerThread = nullptr;
  }
}

void FrameTransformerProxy::SetLibwebrtcTransformer(
    FrameTransformer* aLibwebrtcTransformer) {
  MutexAutoLock lock(mMutex);
  mLibwebrtcTransformer = aLibwebrtcTransformer;
  if (mLibwebrtcTransformer) {
    MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Info,
            ("mLibwebrtcTransformer is now set!"));
    mVideo = Some(mLibwebrtcTransformer->IsVideo());
  }
}

void FrameTransformerProxy::Transform(
    std::unique_ptr<webrtc::TransformableFrameInterface> aFrame) {
  MutexAutoLock lock(mMutex);
  MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Debug, ("In %s", __FUNCTION__));
  if (!mWorkerThread && !mReleaseScriptTransformerCalled) {
    MOZ_LOG(
        gFrameTransformerProxyLog, LogLevel::Info,
        ("In %s, queueing frame because RTCRtpScriptTransformer is not ready",
         __FUNCTION__));
    // We are still waiting for the script transformer to be created on the
    // worker thread.
    mQueue.push_back(std::move(aFrame));
    return;
  }

  if (mWorkerThread) {
    MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Debug,
            ("Queueing call to RTCRtpScriptTransformer::TransformFrame"));
    mWorkerThread->Dispatch(NS_NewRunnableFunction(
        __func__, [this, self = RefPtr<FrameTransformerProxy>(this),
                   frame = std::move(aFrame)]() mutable {
          if (NS_WARN_IF(!mScriptTransformer)) {
            // Could happen due to errors. Is there some
            // other processing we ought to do?
            return;
          }
          mScriptTransformer->TransformFrame(std::move(frame));
        }));
  }
}

void FrameTransformerProxy::OnTransformedFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> aFrame) {
  MutexAutoLock lock(mMutex);
  // If the worker thread has changed, we drop the frame, to avoid frames
  // arriving out of order.
  if (mLibwebrtcTransformer) {
    // This will lock, lock order is mMutex, FrameTransformer::mLibwebrtcMutex
    mLibwebrtcTransformer->OnTransformedFrame(std::move(aFrame));
  }
}

void FrameTransformerProxy::SetSender(dom::RTCRtpSender* aSender) {
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mReceiver);
    mSender = aSender;
  }
  if (!aSender) {
    MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Info, ("Sender set to null"));
    ReleaseScriptTransformer();
  }
}

void FrameTransformerProxy::SetReceiver(dom::RTCRtpReceiver* aReceiver) {
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mSender);
    mReceiver = aReceiver;
  }
  if (!aReceiver) {
    MOZ_LOG(gFrameTransformerProxyLog, LogLevel::Info,
            ("Receiver set to null"));
    ReleaseScriptTransformer();
  }
}

bool FrameTransformerProxy::RequestKeyFrame() {
  {
    // Spec wants this to reject synchronously if the RTCRtpScriptTransformer
    // is not associated with a video receiver. This may change to an async
    // check?
    MutexAutoLock lock(mMutex);
    if (!mReceiver || !mVideo.isSome() || !*mVideo) {
      return false;
    }
  }

  // Thread hop to main, and then the conduit thread-hops to the call thread.
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<FrameTransformerProxy>(this)] {
        MutexAutoLock lock(mMutex);
        if (mReceiver && mVideo.isSome() && *mVideo) {
          mReceiver->RequestKeyFrame();
        }
      }));
  return true;
}

void FrameTransformerProxy::KeyFrameRequestDone(bool aSuccess) {
  MutexAutoLock lock(mMutex);
  if (mWorkerThread) {
    mWorkerThread->Dispatch(NS_NewRunnableFunction(
        __func__, [this, self = RefPtr<FrameTransformerProxy>(this), aSuccess] {
          if (mScriptTransformer) {
            mScriptTransformer->KeyFrameRequestDone(aSuccess);
          }
        }));
  }
}

bool FrameTransformerProxy::GenerateKeyFrame(const Maybe<std::string>& aRid) {
  {
    // Spec wants this to reject synchronously if the RTCRtpScriptTransformer
    // is not associated with a video sender. This may change to an async
    // check?
    MutexAutoLock lock(mMutex);
    if (!mSender || !mVideo.isSome() || !*mVideo) {
      return false;
    }
  }

  // Thread hop to main, and then the conduit thread-hops to the call thread.
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<FrameTransformerProxy>(this), aRid] {
        MutexAutoLock lock(mMutex);
        if (!mSender || !mVideo.isSome() || !*mVideo ||
            !mSender->GenerateKeyFrame(aRid)) {
          CopyableErrorResult rv;
          rv.ThrowInvalidStateError("Not sending video");
          if (mWorkerThread) {
            mWorkerThread->Dispatch(NS_NewRunnableFunction(
                __func__,
                [this, self = RefPtr<FrameTransformerProxy>(this), aRid, rv] {
                  if (mScriptTransformer) {
                    mScriptTransformer->GenerateKeyFrameError(aRid, rv);
                  }
                }));
          }
        }
      }));
  return true;
}

void FrameTransformerProxy::GenerateKeyFrameError(
    const Maybe<std::string>& aRid, const CopyableErrorResult& aResult) {
  MutexAutoLock lock(mMutex);
  if (mWorkerThread) {
    mWorkerThread->Dispatch(NS_NewRunnableFunction(
        __func__,
        [this, self = RefPtr<FrameTransformerProxy>(this), aRid, aResult] {
          if (mScriptTransformer) {
            mScriptTransformer->GenerateKeyFrameError(aRid, aResult);
          }
        }));
  }
}

}  // namespace mozilla
