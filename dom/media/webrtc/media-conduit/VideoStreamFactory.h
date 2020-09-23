/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef VideoStreamFactory_h
#define VideoStreamFactory_h

#include "CodecConfig.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtr.h"
#include "webrtc/media/base/videoadapter.h"
#include "call/video_config.h"

namespace mozilla {

// Factory class for VideoStreams... vie_encoder.cc will call this to
// reconfigure.
class VideoStreamFactory
    : public webrtc::VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  struct ResolutionAndBitrateLimits {
    int resolution_in_mb;
    int min_bitrate_bps;
    int start_bitrate_bps;
    int max_bitrate_bps;
  };

  VideoStreamFactory(VideoCodecConfig aConfig,
                     webrtc::VideoCodecMode aCodecMode, int aMinBitrate,
                     int aStartBitrate, int aPrefMaxBitrate,
                     int aNegotiatedMaxBitrate, unsigned int aSendingFramerate)
      : mCodecMode(aCodecMode),
        mSendingFramerate(aSendingFramerate),
        mCodecConfig(std::forward<VideoCodecConfig>(aConfig)),
        mMinBitrate(aMinBitrate),
        mStartBitrate(aStartBitrate),
        mPrefMaxBitrate(aPrefMaxBitrate),
        mNegotiatedMaxBitrate(aNegotiatedMaxBitrate),
        mSimulcastAdapter(MakeUnique<cricket::VideoAdapter>()) {}

  void SetCodecMode(webrtc::VideoCodecMode aCodecMode);
  void SetSendingFramerate(unsigned int aSendingFramerate);

  // This gets called off-main thread and may hold internal webrtc.org
  // locks. May *NOT* lock the conduit's mutex, to avoid deadlocks.
  std::vector<webrtc::VideoStream> CreateEncoderStreams(
      int width, int height, const webrtc::VideoEncoderConfig& config) override;

 private:
  // Used to limit number of streams for screensharing.
  Atomic<webrtc::VideoCodecMode> mCodecMode;

  // The framerate we're currently sending at.
  Atomic<unsigned int> mSendingFramerate;

  // The current send codec config, containing simulcast layer configs.
  const VideoCodecConfig mCodecConfig;

  // Bitrate limits in bps.
  const int mMinBitrate = 0;
  const int mStartBitrate = 0;
  const int mPrefMaxBitrate = 0;
  const int mNegotiatedMaxBitrate = 0;

  // Adapter for simulcast layers. We use this to handle scaleResolutionDownBy
  // for layers. It's separate from the conduit's mVideoAdapter to not affect
  // scaling settings for incoming frames.
  UniquePtr<cricket::VideoAdapter> mSimulcastAdapter;
};

}  // namespace mozilla

#endif
