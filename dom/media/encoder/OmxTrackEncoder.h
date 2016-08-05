/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OmxTrackEncoder_h_
#define OmxTrackEncoder_h_

#include "nsAutoPtr.h"
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
  explicit OmxVideoTrackEncoder(TrackRate aTrackRate);
  ~OmxVideoTrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() override;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) override;

protected:
  nsresult Init(int aWidth, int aHeight,
                int aDisplayWidth, int aDisplayHeight) override;

private:
  nsAutoPtr<android::OMXVideoEncoder> mEncoder;
};

class OmxAudioTrackEncoder : public AudioTrackEncoder
{
public:
  OmxAudioTrackEncoder();
  ~OmxAudioTrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() = 0;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) override;

protected:
  nsresult Init(int aChannels, int aSamplingRate) = 0;

  // Append encoded frames to aContainer.
  nsresult AppendEncodedFrames(EncodedFrameContainer& aContainer);

  nsAutoPtr<android::OMXAudioEncoder> mEncoder;
};

class OmxAACAudioTrackEncoder final : public OmxAudioTrackEncoder
{
public:
  OmxAACAudioTrackEncoder()
    : OmxAudioTrackEncoder()
  {}

  already_AddRefed<TrackMetadataBase> GetMetadata() override;

protected:
  nsresult Init(int aChannels, int aSamplingRate) override;
};

class OmxAMRAudioTrackEncoder final : public OmxAudioTrackEncoder
{
public:
  OmxAMRAudioTrackEncoder()
    : OmxAudioTrackEncoder()
  {}

  enum {
    AMR_NB_SAMPLERATE = 8000,
  };
  already_AddRefed<TrackMetadataBase> GetMetadata() override;

protected:
  nsresult Init(int aChannels, int aSamplingRate) override;
};

class OmxEVRCAudioTrackEncoder final : public OmxAudioTrackEncoder
{
public:
  OmxEVRCAudioTrackEncoder()
    : OmxAudioTrackEncoder()
  {}

  enum {
    EVRC_SAMPLERATE = 8000,
  };
  already_AddRefed<TrackMetadataBase> GetMetadata() override;

protected:
  nsresult Init(int aChannels, int aSamplingRate) override;
};

}
#endif
