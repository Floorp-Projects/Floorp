/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RtspExtractor_h_)
#define RtspExtractor_h_

#include "RtspMediaResource.h"

#include <stagefright/MediaBufferGroup.h>
#include <stagefright/MediaExtractor.h>
#include <stagefright/MediaSource.h>
#include <stagefright/MetaData.h>

namespace mozilla {

// RtspExtractor is a custom extractor for Rtsp stream, whereas the other
// XXXExtractors are made for container media content.
// The extractor is used for |OmxDecoder::Init|, it provides the essential
// information for creating OMXCodec instance.
// For example, the |getTrackMetaData| returns metadata that includes the
// codec type.
class RtspExtractor: public android::MediaExtractor
{
public:
  size_t countTracks() final override;
  android::sp<android::MediaSource> getTrack(size_t index)
    final override;
  android::sp<android::MetaData> getTrackMetaData(
    size_t index, uint32_t flag = 0) final override;
  uint32_t flags() const final override;

  RtspExtractor(RtspMediaResource* aResource)
    : mRtspResource(aResource) {
    MOZ_ASSERT(aResource);
    mController = mRtspResource->GetMediaStreamController();
    MOZ_ASSERT(mController);
  }
  virtual ~RtspExtractor() override {}
private:
  // mRtspResource is a pointer to RtspMediaResource. When |getTrack| is called
  // we use mRtspResource to construct a RtspMediaSource.
  RtspMediaResource* mRtspResource;
  // Through the mController in mRtspResource, we can get the essential
  // information for the extractor.
  RefPtr<nsIStreamingProtocolController> mController;
};

} // namespace mozilla

#endif
