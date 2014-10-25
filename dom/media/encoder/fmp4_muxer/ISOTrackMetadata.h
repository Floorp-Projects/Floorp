/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ISOTrackMetadata_h_
#define ISOTrackMetadata_h_

#include "TrackMetadataBase.h"

namespace mozilla {

class AACTrackMetadata : public AudioTrackMetadata {
public:
  // AudioTrackMetadata members
  uint32_t GetAudioFrameDuration() MOZ_OVERRIDE { return mFrameDuration; }
  uint32_t GetAudioFrameSize() MOZ_OVERRIDE { return mFrameSize; }
  uint32_t GetAudioSampleRate() MOZ_OVERRIDE { return mSampleRate; }
  uint32_t GetAudioChannels() MOZ_OVERRIDE { return mChannels; }

  // TrackMetadataBase member
  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_AAC; }

  // AACTrackMetadata members
  AACTrackMetadata()
    : mSampleRate(0)
    , mFrameDuration(0)
    , mFrameSize(0)
    , mChannels(0) {
    MOZ_COUNT_CTOR(AACTrackMetadata);
  }
  ~AACTrackMetadata() { MOZ_COUNT_DTOR(AACTrackMetadata); }

  uint32_t mSampleRate;     // From 14496-3 table 1.16, it could be 7350 ~ 96000.
  uint32_t mFrameDuration;  // Audio frame duration based on SampleRate.
  uint32_t mFrameSize;      // Audio frame size, 0 is variant size.
  uint32_t mChannels;       // Channel number, it should be 1 or 2.
};

// AVC clock rate is 90k Hz.
#define AVC_CLOCK_RATE 90000

class AVCTrackMetadata : public VideoTrackMetadata {
public:
  // VideoTrackMetadata members
  uint32_t GetVideoHeight() MOZ_OVERRIDE { return mHeight; }
  uint32_t GetVideoWidth() MOZ_OVERRIDE {return mWidth; }
  uint32_t GetVideoDisplayHeight() MOZ_OVERRIDE { return mDisplayHeight; }
  uint32_t GetVideoDisplayWidth() MOZ_OVERRIDE { return mDisplayWidth;  }
  uint32_t GetVideoClockRate() MOZ_OVERRIDE { return AVC_CLOCK_RATE; }
  uint32_t GetVideoFrameRate() MOZ_OVERRIDE { return mFrameRate; }

  // TrackMetadataBase member
  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_AVC; }

  // AVCTrackMetadata
  AVCTrackMetadata()
    : mHeight(0)
    , mWidth(0)
    , mDisplayHeight(0)
    , mDisplayWidth(0)
    , mFrameRate(0) {
    MOZ_COUNT_CTOR(AVCTrackMetadata);
  }
  ~AVCTrackMetadata() { MOZ_COUNT_DTOR(AVCTrackMetadata); }

  uint32_t mHeight;
  uint32_t mWidth;
  uint32_t mDisplayHeight;
  uint32_t mDisplayWidth;
  uint32_t mFrameRate;       // frames per second
};


// AMR sample rate is 8000 samples/s.
#define AMR_SAMPLE_RATE 8000

// Channel number is always 1.
#define AMR_CHANNELS    1

// AMR speech codec, 3GPP TS 26.071. Encoder and continer support AMR-NB only
// currently.
class AMRTrackMetadata : public AudioTrackMetadata {
public:
  // AudioTrackMetadata members
  //
  // The number of sample sets generates by encoder is variant. So the
  // frame duration and frame size are both 0.
  uint32_t GetAudioFrameDuration() MOZ_OVERRIDE { return 0; }
  uint32_t GetAudioFrameSize() MOZ_OVERRIDE { return 0; }
  uint32_t GetAudioSampleRate() MOZ_OVERRIDE { return AMR_SAMPLE_RATE; }
  uint32_t GetAudioChannels() MOZ_OVERRIDE { return AMR_CHANNELS; }

  // TrackMetadataBase member
  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_AMR; }

  // AMRTrackMetadata members
  AMRTrackMetadata() { MOZ_COUNT_CTOR(AMRTrackMetadata); }
  ~AMRTrackMetadata() { MOZ_COUNT_DTOR(AMRTrackMetadata); }
};

}

#endif // ISOTrackMetadata_h_
