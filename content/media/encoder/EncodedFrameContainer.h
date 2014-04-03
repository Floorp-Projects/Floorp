/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EncodedFrameContainer_H_
#define EncodedFrameContainer_H_

#include "nsAutoPtr.h"
#include "nsTArray.h"

namespace mozilla {

class EncodedFrame;

/*
 * This container is used to carry video or audio encoded data from encoder to muxer.
 * The media data object is created by encoder and recycle by the destructor.
 * Only allow to store audio or video encoded data in EncodedData.
 */
class EncodedFrameContainer
{
public:
  // Append encoded frame data
  void AppendEncodedFrame(EncodedFrame* aEncodedFrame)
  {
    mEncodedFrames.AppendElement(aEncodedFrame);
  }
  // Retrieve all of the encoded frames
  const nsTArray<nsRefPtr<EncodedFrame> >& GetEncodedFrames() const
  {
    return mEncodedFrames;
  }
private:
  // This container is used to store the video or audio encoded packets.
  // Muxer should check mFrameType and get the encoded data type from mEncodedFrames.
  nsTArray<nsRefPtr<EncodedFrame> > mEncodedFrames;
};

// Represent one encoded frame
class EncodedFrame MOZ_FINAL
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EncodedFrame)
public:
  EncodedFrame() :
    mTimeStamp(0),
    mDuration(0),
    mFrameType(UNKNOWN)
  {}
  enum FrameType {
    VP8_I_FRAME,      // VP8 intraframe
    VP8_P_FRAME,      // VP8 predicted frame
    OPUS_AUDIO_FRAME, // Opus audio frame
    VORBIS_AUDIO_FRAME,
    AVC_I_FRAME,
    AVC_P_FRAME,
    AVC_B_FRAME,
    AVC_CSD,          // AVC codec specific data
    AAC_AUDIO_FRAME,
    AAC_CSD,          // AAC codec specific data
    AMR_AUDIO_CSD,
    AMR_AUDIO_FRAME,
    UNKNOWN           // FrameType not set
  };
  nsresult SwapInFrameData(nsTArray<uint8_t>& aData)
  {
    mFrameData.SwapElements(aData);
    return NS_OK;
  }
  nsresult SwapOutFrameData(nsTArray<uint8_t>& aData)
  {
    if (mFrameType != UNKNOWN) {
      // Reset this frame type to UNKNOWN once the data is swapped out.
      mFrameData.SwapElements(aData);
      mFrameType = UNKNOWN;
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  const nsTArray<uint8_t>& GetFrameData() const
  {
    return mFrameData;
  }
  uint64_t GetTimeStamp() const { return mTimeStamp; }
  void SetTimeStamp(uint64_t aTimeStamp) { mTimeStamp = aTimeStamp; }

  uint64_t GetDuration() const { return mDuration; }
  void SetDuration(uint64_t aDuration) { mDuration = aDuration; }

  FrameType GetFrameType() const { return mFrameType; }
  void SetFrameType(FrameType aFrameType) { mFrameType = aFrameType; }
private:
  // Private destructor, to discourage deletion outside of Release():
  ~EncodedFrame()
  {
  }

  // Encoded data
  nsTArray<uint8_t> mFrameData;
  uint64_t mTimeStamp;
  // The playback duration of this packet in number of samples
  uint64_t mDuration;
  // Represent what is in the FrameData
  FrameType mFrameType;
};

}
#endif
