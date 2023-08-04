/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "jsapi/RTCEncodedVideoFrame.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "api/frame_transformer_interface.h"

#include "jsapi/RTCEncodedFrameBase.h"
#include "mozilla/dom/RTCEncodedVideoFrameBinding.h"
#include "mozilla/dom/RTCRtpScriptTransformer.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsContentUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/fallible.h"
#include "mozilla/Maybe.h"
#include "js/RootingAPI.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RTCEncodedVideoFrame, RTCEncodedFrameBase,
                                   mOwner)
NS_IMPL_ADDREF_INHERITED(RTCEncodedVideoFrame, RTCEncodedFrameBase)
NS_IMPL_RELEASE_INHERITED(RTCEncodedVideoFrame, RTCEncodedFrameBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCEncodedVideoFrame)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(RTCEncodedFrameBase)

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    nsIGlobalObject* aGlobal,
    std::unique_ptr<webrtc::TransformableFrameInterface> aFrame,
    uint64_t aCounter, RTCRtpScriptTransformer* aOwner)
    : RTCEncodedFrameBase(aGlobal, std::move(aFrame), aCounter),
      mOwner(aOwner) {
  const auto& videoFrame(
      static_cast<webrtc::TransformableVideoFrameInterface&>(*mFrame));
  mType = videoFrame.IsKeyFrame() ? RTCEncodedVideoFrameType::Key
                                  : RTCEncodedVideoFrameType::Delta;
  auto metadata = videoFrame.Metadata();

  if (metadata.GetFrameId().has_value()) {
    mMetadata.mFrameId.Construct(*metadata.GetFrameId());
  }
  mMetadata.mDependencies.Construct();
  for (const auto dep : metadata.GetFrameDependencies()) {
    Unused << mMetadata.mDependencies.Value().AppendElement(
        static_cast<unsigned long long>(dep), fallible);
  }
  mMetadata.mWidth.Construct(metadata.GetWidth());
  mMetadata.mHeight.Construct(metadata.GetHeight());
  if (metadata.GetSpatialIndex() >= 0) {
    mMetadata.mSpatialIndex.Construct(metadata.GetSpatialIndex());
  }
  if (metadata.GetTemporalIndex() >= 0) {
    mMetadata.mTemporalIndex.Construct(metadata.GetTemporalIndex());
  }
  mMetadata.mSynchronizationSource.Construct(videoFrame.GetSsrc());
  mMetadata.mPayloadType.Construct(videoFrame.GetPayloadType());
  mMetadata.mContributingSources.Construct();
  for (const auto csrc : metadata.GetCsrcs()) {
    Unused << mMetadata.mContributingSources.Value().AppendElement(csrc,
                                                                   fallible);
  }

  // The metadata timestamp is different, and not presently present in the
  // libwebrtc types
  if (!videoFrame.GetRid().empty()) {
    mRid = Some(videoFrame.GetRid());
  }

  // Base class needs this, but can't do it itself because of an assertion in
  // the cycle-collector.
  mozilla::HoldJSObjects(this);
}

RTCEncodedVideoFrame::~RTCEncodedVideoFrame() {
  // Base class needs this, but can't do it itself because of an assertion in
  // the cycle-collector.
  mozilla::DropJSObjects(this);
}

JSObject* RTCEncodedVideoFrame::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return RTCEncodedVideoFrame_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* RTCEncodedVideoFrame::GetParentObject() const {
  return mGlobal;
}

RTCEncodedVideoFrameType RTCEncodedVideoFrame::Type() const { return mType; }

void RTCEncodedVideoFrame::GetMetadata(
    RTCEncodedVideoFrameMetadata& aMetadata) {
  aMetadata = mMetadata;
}

bool RTCEncodedVideoFrame::CheckOwner(RTCRtpScriptTransformer* aOwner) const {
  return aOwner == mOwner;
}

Maybe<std::string> RTCEncodedVideoFrame::Rid() const { return mRid; }

}  // namespace mozilla::dom
