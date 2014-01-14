/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ISOTrackMetadata_h_
#define ISOTrackMetadata_h_

#include "TrackMetadataBase.h"

namespace mozilla {

class AACTrackMetadata : public TrackMetadataBase {
public:
  uint32_t SampleRate;     // From 14496-3 table 1.16, it could be 7350 ~ 96000.
  uint32_t FrameDuration;  // Audio frame duration based on SampleRate.
  uint32_t FrameSize;      // Audio frame size, 0 is variant size.
  uint32_t Channels;       // Channel number, it should be 1 or 2.

  AACTrackMetadata()
    : SampleRate(0)
    , FrameDuration(0)
    , FrameSize(0)
    , Channels(0) {
    MOZ_COUNT_CTOR(AACTrackMetadata);
  }
  ~AACTrackMetadata() { MOZ_COUNT_DTOR(AACTrackMetadata); }
  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_AAC; }
};

class AVCTrackMetadata : public TrackMetadataBase {
public:
  uint32_t Height;
  uint32_t Width;
  uint32_t VideoFrequency;  // for AVC, it should be 90k Hz.
  uint32_t FrameRate;       // frames per second

  AVCTrackMetadata()
    : Height(0)
    , Width(0)
    , VideoFrequency(0)
    , FrameRate(0) {
    MOZ_COUNT_CTOR(AVCTrackMetadata);
  }
  ~AVCTrackMetadata() { MOZ_COUNT_DTOR(AVCTrackMetadata); }
  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_AVC; }
};

}

#endif // ISOTrackMetadata_h_
