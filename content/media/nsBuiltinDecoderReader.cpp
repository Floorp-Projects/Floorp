/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *  Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsISeekableStream.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "nsBuiltinDecoder.h"
#include "nsBuiltinDecoderReader.h"
#include "nsBuiltinDecoderStateMachine.h"
#include "mozilla/mozalloc.h"
#include "VideoUtils.h"

using namespace mozilla;
using mozilla::layers::ImageContainer;
using mozilla::layers::PlanarYCbCrImage;

// The maximum height and width of the video. Used for
// sanitizing the memory allocation of the RGB buffer.
// The maximum resolution we anticipate encountering in the
// wild is 2160p - 3840x2160 pixels.
#define MAX_VIDEO_WIDTH  4000
#define MAX_VIDEO_HEIGHT 3000

using mozilla::layers::PlanarYCbCrImage;

// Verify these values are sane. Once we've checked the frame sizes, we then
// can do less integer overflow checking.
PR_STATIC_ASSERT(MAX_VIDEO_WIDTH < PlanarYCbCrImage::MAX_DIMENSION);
PR_STATIC_ASSERT(MAX_VIDEO_HEIGHT < PlanarYCbCrImage::MAX_DIMENSION);
PR_STATIC_ASSERT(PlanarYCbCrImage::MAX_DIMENSION < PR_UINT32_MAX / PlanarYCbCrImage::MAX_DIMENSION);

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif
#else
#define LOG(type, msg)
#define SEEK_LOG(type, msg)
#endif

static PRBool
ValidatePlane(const VideoData::YCbCrBuffer::Plane& aPlane)
{
  return aPlane.mWidth <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPlane.mHeight <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPlane.mWidth * aPlane.mHeight < MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
         aPlane.mStride > 0;
}

PRBool
nsVideoInfo::ValidateVideoRegion(const nsIntSize& aFrame,
                                 const nsIntRect& aPicture,
                                 const nsIntSize& aDisplay)
{
  return
    aFrame.width <= PlanarYCbCrImage::MAX_DIMENSION &&
    aFrame.height <= PlanarYCbCrImage::MAX_DIMENSION &&
    aFrame.width * aFrame.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
    aFrame.width * aFrame.height != 0 &&
    aPicture.width <= PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.x < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.x + aPicture.width < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.height <= PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.y < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.y + aPicture.height < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.width * aPicture.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
    aPicture.width * aPicture.height != 0 &&
    aDisplay.width <= PlanarYCbCrImage::MAX_DIMENSION &&
    aDisplay.height <= PlanarYCbCrImage::MAX_DIMENSION &&
    aDisplay.width * aDisplay.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
    aDisplay.width * aDisplay.height != 0;
}

VideoData* VideoData::Create(nsVideoInfo& aInfo,
                             ImageContainer* aContainer,
                             PRInt64 aOffset,
                             PRInt64 aTime,
                             PRInt64 aEndTime,
                             const YCbCrBuffer& aBuffer,
                             PRBool aKeyframe,
                             PRInt64 aTimecode)
{
  if (!aContainer) {
    return nsnull;
  }

  // The following situation should never happen unless there is a bug
  // in the decoder
  if (aBuffer.mPlanes[1].mWidth != aBuffer.mPlanes[2].mWidth ||
      aBuffer.mPlanes[1].mHeight != aBuffer.mPlanes[2].mHeight) {
    NS_ERROR("C planes with different sizes");
    return nsnull;
  }

  // The following situations could be triggered by invalid input
  if (aInfo.mPicture.width <= 0 || aInfo.mPicture.height <= 0) {
    NS_WARNING("Empty picture rect");
    return nsnull;
  }
  if (!ValidatePlane(aBuffer.mPlanes[0]) || !ValidatePlane(aBuffer.mPlanes[1]) ||
      !ValidatePlane(aBuffer.mPlanes[2])) {
    NS_WARNING("Invalid plane size");
    return nsnull;
  }

  PRUint32 picX = aInfo.mPicture.x;
  PRUint32 picY = aInfo.mPicture.y;
  gfxIntSize picSize = gfxIntSize(aInfo.mPicture.width, aInfo.mPicture.height);

  if (aInfo.mFrame.width != aBuffer.mPlanes[0].mWidth ||
      aInfo.mFrame.height != aBuffer.mPlanes[0].mHeight)
  {
    // Frame size is different from what the container reports. This is legal
    // in WebM, and we will preserve the ratio of the crop rectangle as it
    // was reported relative to the picture size reported by the container.
    picX = (aInfo.mPicture.x * aBuffer.mPlanes[0].mWidth) / aInfo.mFrame.width;
    picY = (aInfo.mPicture.y * aBuffer.mPlanes[0].mHeight) / aInfo.mFrame.height;
    picSize = gfxIntSize((aBuffer.mPlanes[0].mWidth * aInfo.mPicture.width) / aInfo.mFrame.width,
                         (aBuffer.mPlanes[0].mHeight * aInfo.mPicture.height) / aInfo.mFrame.height);
  }

  // Ensure the picture size specified in the headers can be extracted out of
  // the frame we've been supplied without indexing out of bounds.
  PRUint32 picXLimit;
  PRUint32 picYLimit;
  if (!AddOverflow32(picX, picSize.width, picXLimit) ||
      picXLimit > aBuffer.mPlanes[0].mStride ||
      !AddOverflow32(picY, picSize.height, picYLimit) ||
      picYLimit > aBuffer.mPlanes[0].mHeight)
  {
    // The specified picture dimensions can't be contained inside the video
    // frame, we'll stomp memory if we try to copy it. Fail.
    NS_WARNING("Overflowing picture rect");
    return nsnull;
  }

  nsAutoPtr<VideoData> v(new VideoData(aOffset, aTime, aEndTime, aKeyframe, aTimecode));
  // Currently our decoder only knows how to output to PLANAR_YCBCR
  // format.
  Image::Format format = Image::PLANAR_YCBCR;
  v->mImage = aContainer->CreateImage(&format, 1);
  if (!v->mImage) {
    return nsnull;
  }
  NS_ASSERTION(v->mImage->GetFormat() == Image::PLANAR_YCBCR,
               "Wrong format?");
  PlanarYCbCrImage* videoImage = static_cast<PlanarYCbCrImage*>(v->mImage.get());

  PlanarYCbCrImage::Data data;
  data.mYChannel = aBuffer.mPlanes[0].mData;
  data.mYSize = gfxIntSize(aBuffer.mPlanes[0].mWidth, aBuffer.mPlanes[0].mHeight);
  data.mYStride = aBuffer.mPlanes[0].mStride;
  data.mCbChannel = aBuffer.mPlanes[1].mData;
  data.mCrChannel = aBuffer.mPlanes[2].mData;
  data.mCbCrSize = gfxIntSize(aBuffer.mPlanes[1].mWidth, aBuffer.mPlanes[1].mHeight);
  data.mCbCrStride = aBuffer.mPlanes[1].mStride;
  data.mPicX = picX;
  data.mPicY = picY;
  data.mPicSize = picSize;
  data.mStereoMode = aInfo.mStereoMode;

  videoImage->SetData(data); // Copies buffer
  return v.forget();
}

nsBuiltinDecoderReader::nsBuiltinDecoderReader(nsBuiltinDecoder* aDecoder)
  : mMonitor("media.decoderreader"),
    mDecoder(aDecoder),
    mDataOffset(0)
{
  MOZ_COUNT_CTOR(nsBuiltinDecoderReader);
}

nsBuiltinDecoderReader::~nsBuiltinDecoderReader()
{
  ResetDecode();
  MOZ_COUNT_DTOR(nsBuiltinDecoderReader);
}

nsresult nsBuiltinDecoderReader::ResetDecode()
{
  nsresult res = NS_OK;

  mVideoQueue.Reset();
  mAudioQueue.Reset();

  return res;
}

nsresult nsBuiltinDecoderReader::GetBufferedBytes(nsTArray<ByteRange>& aRanges)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");
  mMonitor.AssertCurrentThreadIn();
  PRInt64 startOffset = mDataOffset;
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  while (PR_TRUE) {
    PRInt64 endOffset = stream->GetCachedDataEnd(startOffset);
    if (endOffset == startOffset) {
      // Uncached at startOffset.
      endOffset = stream->GetNextCachedData(startOffset);
      if (endOffset == -1) {
        // Uncached at startOffset until endOffset of stream, or we're at
        // the end of stream.
        break;
      }
    } else {
      // Bytes [startOffset..endOffset] are cached.
      PRInt64 startTime = -1;
      PRInt64 endTime = -1;
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }
      FindStartTime(startOffset, startTime);
      if (startTime != -1 &&
          ((endTime = FindEndTime(endOffset)) != -1))
      {
        NS_ASSERTION(startOffset < endOffset,
                     "Start offset must be before end offset");
        NS_ASSERTION(startTime < endTime,
                     "Start time must be before end time");
        aRanges.AppendElement(ByteRange(startOffset,
                                        endOffset,
                                        startTime,
                                        endTime));
      }
    }
    startOffset = endOffset;
  }
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

ByteRange
nsBuiltinDecoderReader::GetSeekRange(const nsTArray<ByteRange>& ranges,
                                     PRInt64 aTarget,
                                     PRInt64 aStartTime,
                                     PRInt64 aEndTime,
                                     PRBool aExact)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");
  PRInt64 so = mDataOffset;
  PRInt64 eo = mDecoder->GetCurrentStream()->GetLength();
  PRInt64 st = aStartTime;
  PRInt64 et = aEndTime;
  for (PRUint32 i = 0; i < ranges.Length(); i++) {
    const ByteRange &r = ranges[i];
    if (r.mTimeStart < aTarget) {
      so = r.mOffsetStart;
      st = r.mTimeStart;
    }
    if (r.mTimeEnd >= aTarget && r.mTimeEnd < et) {
      eo = r.mOffsetEnd;
      et = r.mTimeEnd;
    }

    if (r.mTimeStart < aTarget && aTarget <= r.mTimeEnd) {
      // Target lies exactly in this range.
      return ranges[i];
    }
  }
  return aExact ? ByteRange() : ByteRange(so, eo, st, et);
}

VideoData* nsBuiltinDecoderReader::FindStartTime(PRInt64 aOffset,
                                                 PRInt64& aOutStartTime)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread(), "Should be on state machine thread.");

  if (NS_FAILED(ResetDecode())) {
    return nsnull;
  }

  // Extract the start times of the bitstreams in order to calculate
  // the duration.
  PRInt64 videoStartTime = PR_INT64_MAX;
  PRInt64 audioStartTime = PR_INT64_MAX;
  VideoData* videoData = nsnull;

  if (HasVideo()) {
    videoData = DecodeToFirstData(&nsBuiltinDecoderReader::DecodeVideoFrame,
                                  mVideoQueue);
    if (videoData) {
      videoStartTime = videoData->mTime;
    }
  }
  if (HasAudio()) {
    SoundData* soundData = DecodeToFirstData(&nsBuiltinDecoderReader::DecodeAudioData,
                                             mAudioQueue);
    if (soundData) {
      audioStartTime = soundData->mTime;
    }
  }

  PRInt64 startTime = PR_MIN(videoStartTime, audioStartTime);
  if (startTime != PR_INT64_MAX) {
    aOutStartTime = startTime;
  }

  return videoData;
}

PRInt64 nsBuiltinDecoderReader::FindEndTime(PRInt64 aEndOffset)
{
  return -1;
}

template<class Data>
Data* nsBuiltinDecoderReader::DecodeToFirstData(DecodeFn aDecodeFn,
                                                MediaQueue<Data>& aQueue)
{
  PRBool eof = PR_FALSE;
  while (!eof && aQueue.GetSize() == 0) {
    {
      MonitorAutoEnter decoderMon(mDecoder->GetMonitor());
      if (mDecoder->GetDecodeState() == nsDecoderStateMachine::DECODER_STATE_SHUTDOWN) {
        return nsnull;
      }
    }
    eof = !(this->*aDecodeFn)();
  }
  Data* d = nsnull;
  return (d = aQueue.PeekFront()) ? d : nsnull;
}

nsresult nsBuiltinDecoderReader::DecodeToTarget(PRInt64 aTarget)
{
  // Decode forward to the target frame. Start with video, if we have it.
  if (HasVideo()) {
    PRBool eof = PR_FALSE;
    PRInt64 startTime = -1;
    while (HasVideo() && !eof) {
      while (mVideoQueue.GetSize() == 0 && !eof) {
        PRBool skip = PR_FALSE;
        eof = !DecodeVideoFrame(skip, 0);
        {
          MonitorAutoExit exitReaderMon(mMonitor);
          MonitorAutoEnter decoderMon(mDecoder->GetMonitor());
          if (mDecoder->GetDecodeState() == nsBuiltinDecoderStateMachine::DECODER_STATE_SHUTDOWN) {
            return NS_ERROR_FAILURE;
          }
        }
      }
      if (mVideoQueue.GetSize() == 0) {
        break;
      }
      nsAutoPtr<VideoData> video(mVideoQueue.PeekFront());
      // If the frame end time is less than the seek target, we won't want
      // to display this frame after the seek, so discard it.
      if (video && video->mEndTime <= aTarget) {
        if (startTime == -1) {
          startTime = video->mTime;
        }
        mVideoQueue.PopFront();
        video = nsnull;
      } else {
        video.forget();
        break;
      }
    }
    {
      MonitorAutoExit exitReaderMon(mMonitor);
      MonitorAutoEnter decoderMon(mDecoder->GetMonitor());
      if (mDecoder->GetDecodeState() == nsBuiltinDecoderStateMachine::DECODER_STATE_SHUTDOWN) {
        return NS_ERROR_FAILURE;
      }
    }
    LOG(PR_LOG_DEBUG, ("First video frame after decode is %lld", startTime));
  }

  if (HasAudio()) {
    // Decode audio forward to the seek target.
    PRBool eof = PR_FALSE;
    while (HasAudio() && !eof) {
      while (!eof && mAudioQueue.GetSize() == 0) {
        eof = !DecodeAudioData();
        {
          MonitorAutoExit exitReaderMon(mMonitor);
          MonitorAutoEnter decoderMon(mDecoder->GetMonitor());
          if (mDecoder->GetDecodeState() == nsBuiltinDecoderStateMachine::DECODER_STATE_SHUTDOWN) {
            return NS_ERROR_FAILURE;
          }
        }
      }
      nsAutoPtr<SoundData> audio(mAudioQueue.PeekFront());
      if (audio && audio->mTime + audio->mDuration <= aTarget) {
        mAudioQueue.PopFront();
        audio = nsnull;
      } else {
        audio.forget();
        break;
      }
    }
  }
  return NS_OK;
}


