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
class EncodedFrame
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EncodedFrame)
public:
  EncodedFrame() :
    mTimeStamp(0),
    mDuration(0),
    mFrameType(UNKNOW)
  {}
  enum FrameType {
    I_FRAME,      // intraframe
    P_FRAME,      // predicted frame
    B_FRAME,      // bidirectionally predicted frame
    AUDIO_FRAME,  // audio frame
    UNKNOW        // FrameType not set
  };
  const nsTArray<uint8_t>& GetFrameData() const
  {
    return mFrameData;
  }
  void SetFrameData(nsTArray<uint8_t> *aData)
  {
    mFrameData.SwapElements(*aData);
  }
  uint64_t GetTimeStamp() const { return mTimeStamp; }
  void SetTimeStamp(uint64_t aTimeStamp) { mTimeStamp = aTimeStamp; }

  uint64_t GetDuration() const { return mDuration; }
  void SetDuration(uint64_t aDuration) { mDuration = aDuration; }

  FrameType GetFrameType() const { return mFrameType; }
  void SetFrameType(FrameType aFrameType) { mFrameType = aFrameType; }
private:
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
