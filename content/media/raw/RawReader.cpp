/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "RawReader.h"
#include "RawDecoder.h"
#include "VideoUtils.h"
#include "nsISeekableStream.h"

using namespace mozilla;

RawReader::RawReader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder),
    mCurrentFrame(0), mFrameSize(0)
{
  MOZ_COUNT_CTOR(RawReader);
}

RawReader::~RawReader()
{
  MOZ_COUNT_DTOR(RawReader);
}

nsresult RawReader::Init(MediaDecoderReader* aCloneDonor)
{
  return NS_OK;
}

nsresult RawReader::ResetDecode()
{
  mCurrentFrame = 0;
  return MediaDecoderReader::ResetDecode();
}

nsresult RawReader::ReadMetadata(MediaInfo* aInfo,
                                 MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(),
               "Should be on decode thread.");

  MediaResource* resource = mDecoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");

  if (!ReadFromResource(resource, reinterpret_cast<uint8_t*>(&mMetadata),
                        sizeof(mMetadata)))
    return NS_ERROR_FAILURE;

  // Validate the header
  if (!(mMetadata.headerPacketID == 0 /* Packet ID of 0 for the header*/ &&
        mMetadata.codecID == RAW_ID /* "YUV" */ &&
        mMetadata.majorVersion == 0 &&
        mMetadata.minorVersion == 1))
    return NS_ERROR_FAILURE;

  CheckedUint32 dummy = CheckedUint32(static_cast<uint32_t>(mMetadata.frameWidth)) *
                          static_cast<uint32_t>(mMetadata.frameHeight);
  NS_ENSURE_TRUE(dummy.isValid(), NS_ERROR_FAILURE);

  if (mMetadata.aspectDenominator == 0 ||
      mMetadata.framerateDenominator == 0)
    return NS_ERROR_FAILURE; // Invalid data

  // Determine and verify frame display size.
  float pixelAspectRatio = static_cast<float>(mMetadata.aspectNumerator) / 
                            mMetadata.aspectDenominator;
  nsIntSize display(mMetadata.frameWidth, mMetadata.frameHeight);
  ScaleDisplayByAspectRatio(display, pixelAspectRatio);
  mPicture = nsIntRect(0, 0, mMetadata.frameWidth, mMetadata.frameHeight);
  nsIntSize frameSize(mMetadata.frameWidth, mMetadata.frameHeight);
  if (!VideoInfo::ValidateVideoRegion(frameSize, mPicture, display)) {
    // Video track's frame sizes will overflow. Fail.
    return NS_ERROR_FAILURE;
  }

  mInfo.mVideo.mHasVideo = true;
  mInfo.mVideo.mDisplay = display;

  mFrameRate = static_cast<float>(mMetadata.framerateNumerator) /
               mMetadata.framerateDenominator;

  // Make some sanity checks
  if (mFrameRate > 45 ||
      mFrameRate == 0 ||
      pixelAspectRatio == 0 ||
      mMetadata.frameWidth > 2000 ||
      mMetadata.frameHeight > 2000 ||
      mMetadata.chromaChannelBpp != 4 ||
      mMetadata.lumaChannelBpp != 8 ||
      mMetadata.colorspace != 1 /* 4:2:0 */)
    return NS_ERROR_FAILURE;

  mFrameSize = mMetadata.frameWidth * mMetadata.frameHeight *
    (mMetadata.lumaChannelBpp + mMetadata.chromaChannelBpp) / 8.0 +
    sizeof(RawPacketHeader);

  int64_t length = resource->GetLength();
  if (length != -1) {
    ReentrantMonitorAutoEnter autoMonitor(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(USECS_PER_S *
                                      (length - sizeof(RawVideoHeader)) /
                                      (mFrameSize * mFrameRate));
  }

  *aInfo = mInfo;

  *aTags = nullptr;

  return NS_OK;
}

 bool RawReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on state machine thread or decode thread.");
  return false;
}

// Helper method that either reads until it gets aLength bytes 
// or returns false
bool RawReader::ReadFromResource(MediaResource *aResource, uint8_t* aBuf,
                                   uint32_t aLength)
{
  while (aLength > 0) {
    uint32_t bytesRead = 0;
    nsresult rv;

    rv = aResource->Read(reinterpret_cast<char*>(aBuf), aLength, &bytesRead);
    NS_ENSURE_SUCCESS(rv, false);

    if (bytesRead == 0) {
      return false;
    }

    aLength -= bytesRead;
    aBuf += bytesRead;
  }

  return true;
}

bool RawReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                     int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(),
               "Should be on decode thread.");

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  if (!mFrameSize)
    return false; // Metadata read failed.  We should refuse to play.

  int64_t currentFrameTime = USECS_PER_S * mCurrentFrame / mFrameRate;
  uint32_t length = mFrameSize - sizeof(RawPacketHeader);

  nsAutoArrayPtr<uint8_t> buffer(new uint8_t[length]);
  MediaResource* resource = mDecoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");

  // We're always decoding one frame when called
  while(true) {
    RawPacketHeader header;

    // Read in a packet header and validate
    if (!(ReadFromResource(resource, reinterpret_cast<uint8_t*>(&header),
                           sizeof(header))) ||
        !(header.packetID == 0xFF && header.codecID == RAW_ID /* "YUV" */)) {
      return false;
    }

    if (!ReadFromResource(resource, buffer, length)) {
      return false;
    }

    parsed++;

    if (currentFrameTime >= aTimeThreshold)
      break;

    mCurrentFrame++;
    currentFrameTime += static_cast<double>(USECS_PER_S) / mFrameRate;
  }

  VideoData::YCbCrBuffer b;
  b.mPlanes[0].mData = buffer;
  b.mPlanes[0].mStride = mMetadata.frameWidth * mMetadata.lumaChannelBpp / 8.0;
  b.mPlanes[0].mHeight = mMetadata.frameHeight;
  b.mPlanes[0].mWidth = mMetadata.frameWidth;
  b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

  uint32_t cbcrStride = mMetadata.frameWidth * mMetadata.chromaChannelBpp / 8.0;

  b.mPlanes[1].mData = buffer + mMetadata.frameHeight * b.mPlanes[0].mStride;
  b.mPlanes[1].mStride = cbcrStride;
  b.mPlanes[1].mHeight = mMetadata.frameHeight / 2;
  b.mPlanes[1].mWidth = mMetadata.frameWidth / 2;
  b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = b.mPlanes[1].mData + mMetadata.frameHeight * cbcrStride / 2;
  b.mPlanes[2].mStride = cbcrStride;
  b.mPlanes[2].mHeight = mMetadata.frameHeight / 2;
  b.mPlanes[2].mWidth = mMetadata.frameWidth / 2;
  b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

  VideoData *v = VideoData::Create(mInfo.mVideo,
                                   mDecoder->GetImageContainer(),
                                   -1,
                                   currentFrameTime,
                                   (USECS_PER_S / mFrameRate),
                                   b,
                                   1, // In raw video every frame is a keyframe
                                   -1,
                                   mPicture);
  if (!v)
    return false;

  mVideoQueue.Push(v);
  mCurrentFrame++;
  decoded++;
  currentFrameTime += USECS_PER_S / mFrameRate;

  return true;
}

nsresult RawReader::Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(),
               "Should be on decode thread.");

  MediaResource *resource = mDecoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");

  uint32_t frame = mCurrentFrame;
  if (aTime >= UINT_MAX)
    return NS_ERROR_FAILURE;
  mCurrentFrame = aTime * mFrameRate / USECS_PER_S;

  CheckedUint32 offset = CheckedUint32(mCurrentFrame) * mFrameSize;
  offset += sizeof(RawVideoHeader);
  NS_ENSURE_TRUE(offset.isValid(), NS_ERROR_FAILURE);

  nsresult rv = resource->Seek(nsISeekableStream::NS_SEEK_SET, offset.value());
  NS_ENSURE_SUCCESS(rv, rv);

  mVideoQueue.Erase();

  while(mVideoQueue.GetSize() == 0) {
    bool keyframeSkip = false;
    if (!DecodeVideoFrame(keyframeSkip, 0)) {
      mCurrentFrame = frame;
      return NS_ERROR_FAILURE;
    }

    {
      ReentrantMonitorAutoEnter autoMonitor(mDecoder->GetReentrantMonitor());
      if (mDecoder->IsShutdown()) {
        mCurrentFrame = frame;
        return NS_ERROR_FAILURE;
      }
    }

    nsAutoPtr<VideoData> video(mVideoQueue.PeekFront());
    if (video && video->GetEndTime() < aTime) {
      mVideoQueue.PopFront();
      video = nullptr;
    } else {
      video.forget();
    }
  }

  return NS_OK;
}

nsresult RawReader::GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime)
{
  return NS_OK;
}
