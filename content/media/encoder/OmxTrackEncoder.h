/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OmxTrackEncoder_h_
#define OmxTrackEncoder_h_

#include "TrackEncoder.h"

namespace android {
class OMXVideoEncoder;
}

/**
 * There are two major classes defined in file OmxTrackEncoder;
 * OmxVideoTrackEncoder and OmxAudioTrackEncoder, the video and audio track
 * encoder for media type AVC/h.264 and AAC. OMXCodecWrapper wraps and controls
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
  nsresult Init(int aWidth, int aHeight, TrackRate aTrackRate) MOZ_OVERRIDE;

private:
  nsAutoPtr<android::OMXVideoEncoder> mEncoder;
};

}
#endif
