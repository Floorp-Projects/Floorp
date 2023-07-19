/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "libwebrtcglue/FrameTransformer.h"
#include "api/frame_transformer_interface.h"
#include "mozilla/Mutex.h"
#include <memory>
#include <utility>
#include "api/scoped_refptr.h"
#include <stdint.h>
#include "libwebrtcglue/FrameTransformerProxy.h"

namespace mozilla {

FrameTransformer::FrameTransformer(bool aVideo)
    : webrtc::FrameTransformerInterface(),
      mVideo(aVideo),
      mCallbacksMutex("FrameTransformer::mCallbacksMutex"),
      mProxyMutex("FrameTransformer::mProxyMutex") {}

FrameTransformer::~FrameTransformer() {
  if (mProxy) {
    mProxy->SetLibwebrtcTransformer(nullptr);
  }
}

void FrameTransformer::Transform(
    std::unique_ptr<webrtc::TransformableFrameInterface> aFrame) {
  MutexAutoLock lock(mProxyMutex);
  if (mProxy) {
    mProxy->Transform(std::move(aFrame));
    return;
  }

  // No transformer, just passthrough
  OnTransformedFrame(std::move(aFrame));
}

void FrameTransformer::RegisterTransformedFrameCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> aCallback) {
  MutexAutoLock lock(mCallbacksMutex);
  mCallback = aCallback;
}

void FrameTransformer::UnregisterTransformedFrameCallback() {
  MutexAutoLock lock(mCallbacksMutex);
  mCallback = nullptr;
}

void FrameTransformer::RegisterTransformedFrameSinkCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> aCallback,
    uint32_t aSsrc) {
  MutexAutoLock lock(mCallbacksMutex);
  mCallbacksBySsrc[aSsrc] = aCallback;
}

void FrameTransformer::UnregisterTransformedFrameSinkCallback(uint32_t aSsrc) {
  MutexAutoLock lock(mCallbacksMutex);
  mCallbacksBySsrc.erase(aSsrc);
}

void FrameTransformer::OnTransformedFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> aFrame) {
  MutexAutoLock lock(mCallbacksMutex);
  if (mCallback) {
    mCallback->OnTransformedFrame(std::move(aFrame));
  } else if (auto it = mCallbacksBySsrc.find(aFrame->GetSsrc());
             it != mCallbacksBySsrc.end()) {
    it->second->OnTransformedFrame(std::move(aFrame));
  }
}

void FrameTransformer::SetProxy(FrameTransformerProxy* aProxy) {
  MutexAutoLock lock(mProxyMutex);
  if (mProxy) {
    mProxy->SetLibwebrtcTransformer(nullptr);
  }
  mProxy = aProxy;
  if (mProxy) {
    mProxy->SetLibwebrtcTransformer(this);
  }
}

}  // namespace mozilla
