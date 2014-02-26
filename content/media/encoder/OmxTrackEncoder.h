/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OmxTrackEncoder_h_
#define OmxTrackEncoder_h_

#include "TrackEncoder.h"

namespace android {
class OMXVideoEncoder;
class OMXAudioEncoder;
}

/**
 * There are two major classes defined in file OmxTrackEncoder;
 * OmxVideoTrackEncoder and OmxAudioTrackEncoder, the video and audio track
 * encoder for media type AVC/H.264 and AAC. OMXCodecWrapper wraps and controls
 * an instance of MediaCodec, defined in libstagefright, runs on Android Jelly
 * Bean platform.
 */

namespace mozilla {

class OmxVideoTrackEncoder: public VideoTrackEncoder
{
public:
  OmxVideoTrackEncoder()
    : VideoTrackEncoder()
  {}

  already_AddRefed<TrackMetadataBase> GetMetadata() MOZ_OVERRIDE;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) MOZ_OVERRIDE;

protected:
  nsresult Init(int aWidth, int aHeight,
                int aDisplayWidth, int aDisplayHeight,
                TrackRate aTrackRate) MOZ_OVERRIDE;

private:
  nsAutoPtr<android::OMXVideoEncoder> mEncoder;
};

class OmxAudioTrackEncoder MOZ_FINAL : public AudioTrackEncoder
{
public:
  OmxAudioTrackEncoder()
    : AudioTrackEncoder()
  {}

  already_AddRefed<TrackMetadataBase> GetMetadata() MOZ_OVERRIDE;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) MOZ_OVERRIDE;

protected:
  nsresult Init(int aChannels, int aSamplingRate) MOZ_OVERRIDE;

private:
  // Append encoded frames to aContainer.
  nsresult AppendEncodedFrames(EncodedFrameContainer& aContainer);

  nsAutoPtr<android::OMXAudioEncoder> mEncoder;
};

}
#endif
