/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is 
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brad Lassey <blassey@mozilla.com>
 *  Kyle Huey <me@kylehuey.com>
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

#include "nsBuiltinDecoderStateMachine.h"
#include "nsBuiltinDecoder.h"
#include "nsRawReader.h"
#include "nsRawDecoder.h"
#include "VideoUtils.h"

#define RAW_ID 0x595556

nsRawReader::nsRawReader(nsBuiltinDecoder* aDecoder)
  : nsBuiltinDecoderReader(aDecoder),
    mCurrentFrame(0), mFrameSize(0)
{
  MOZ_COUNT_CTOR(nsRawReader);
}

nsRawReader::~nsRawReader()
{
  MOZ_COUNT_DTOR(nsRawReader);
}

nsresult nsRawReader::Init(nsBuiltinDecoderReader* aCloneDonor)
{
  return NS_OK;
}

nsresult nsRawReader::ResetDecode()
{
  mCurrentFrame = 0;
  return nsBuiltinDecoderReader::ResetDecode();
}

nsresult nsRawReader::ReadMetadata()
{
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");
  mozilla::MonitorAutoEnter autoEnter(mMonitor);

  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ASSERTION(stream, "Decoder has no media stream");

  if (!ReadFromStream(stream, reinterpret_cast<PRUint8*>(&mMetadata),
                      sizeof(mMetadata)))
    return NS_ERROR_FAILURE;

  // Validate the header
  if (!(mMetadata.headerPacketID == 0 /* Packet ID of 0 for the header*/ &&
        mMetadata.codecID == RAW_ID /* "YUV" */ &&
        mMetadata.majorVersion == 0 &&
        mMetadata.minorVersion == 1))
    return NS_ERROR_FAILURE;

  PRUint32 dummy;
  if (!MulOverflow32(mMetadata.frameWidth, mMetadata.frameHeight, dummy))
    return NS_ERROR_FAILURE;

  mInfo.mHasVideo = PR_TRUE;
  mInfo.mPicture.x = 0;
  mInfo.mPicture.y = 0;
  mInfo.mPicture.width = mMetadata.frameWidth;
  mInfo.mPicture.height = mMetadata.frameHeight;
  mInfo.mFrame.width = mMetadata.frameWidth;
  mInfo.mFrame.height = mMetadata.frameHeight;
  if (mMetadata.aspectDenominator == 0 ||
      mMetadata.framerateDenominator == 0)
    return NS_ERROR_FAILURE; // Invalid data
  mInfo.mPixelAspectRatio = static_cast<float>(mMetadata.aspectNumerator) / 
                            mMetadata.aspectDenominator;
  mInfo.mDataOffset = sizeof(nsRawVideoHeader) + 1;
  mInfo.mHasAudio = PR_FALSE;

  mFrameRate = static_cast<float>(mMetadata.framerateNumerator) /
               mMetadata.framerateDenominator;

  // Make some sanity checks
  if (mFrameRate > 45 ||
      mFrameRate == 0 ||
      mInfo.mPixelAspectRatio == 0 ||
      mMetadata.frameWidth > 2000 ||
      mMetadata.frameHeight > 2000 ||
      mMetadata.chromaChannelBpp != 4 ||
      mMetadata.lumaChannelBpp != 8 ||
      mMetadata.colorspace != 1 /* 4:2:0 */)
    return NS_ERROR_FAILURE;

  mFrameSize = mMetadata.frameWidth * mMetadata.frameHeight *
    (mMetadata.lumaChannelBpp + mMetadata.chromaChannelBpp) / 8.0 +
    sizeof(nsRawPacketHeader);

  PRInt64 length = stream->GetLength();
  if (length != -1) {
    mozilla::MonitorAutoExit autoExitMonitor(mMonitor);
    mozilla::MonitorAutoEnter autoMonitor(mDecoder->GetMonitor());
    mDecoder->GetStateMachine()->SetDuration(1000 *
                                           (length - sizeof(nsRawVideoHeader)) /
                                           (mFrameSize * mFrameRate));
  }

  return NS_OK;
}

 PRBool nsRawReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on state machine thread or decode thread.");
  return PR_FALSE;
}

// Helper method that either reads until it gets aLength bytes 
// or returns PR_FALSE
PRBool nsRawReader::ReadFromStream(nsMediaStream *aStream, PRUint8* aBuf,
                                   PRUint32 aLength)
{
  while (aLength > 0) {
    PRUint32 bytesRead = 0;
    nsresult rv;

    rv = aStream->Read(reinterpret_cast<char*>(aBuf), aLength, &bytesRead);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (bytesRead == 0) {
      return PR_FALSE;
    }

    aLength -= bytesRead;
    aBuf += bytesRead;
  }

  return PR_TRUE;
}

PRBool nsRawReader::DecodeVideoFrame(PRBool &aKeyframeSkip,
                                     PRInt64 aTimeThreshold)
{
  mozilla::MonitorAutoEnter autoEnter(mMonitor);
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on state machine thread or decode thread.");

  if (!mFrameSize)
    return PR_FALSE; // Metadata read failed.  We should refuse to play.

  PRInt64 currentFrameTime = 1000 * mCurrentFrame / mFrameRate;
  PRUint32 length = mFrameSize - sizeof(nsRawPacketHeader);

  nsAutoPtr<PRUint8> buffer(new PRUint8[length]);
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ASSERTION(stream, "Decoder has no media stream");

  // We're always decoding one frame when called
  while(true) {
    nsRawPacketHeader header;

    // Read in a packet header and validate
    if (!(ReadFromStream(stream, reinterpret_cast<PRUint8*>(&header),
                         sizeof(header))) ||
        !(header.packetID == 0xFF && header.codecID == RAW_ID /* "YUV" */)) {
      return PR_FALSE;
    }

    if (!ReadFromStream(stream, buffer, length)) {
      return PR_FALSE;
    }

    if (currentFrameTime >= aTimeThreshold)
      break;

    mCurrentFrame++;
    currentFrameTime += 1000.0 / mFrameRate;
  }

  VideoData::YCbCrBuffer b;
  b.mPlanes[0].mData = buffer;
  b.mPlanes[0].mStride = mMetadata.frameWidth * mMetadata.lumaChannelBpp / 8.0;
  b.mPlanes[0].mHeight = mMetadata.frameHeight;
  b.mPlanes[0].mWidth = mMetadata.frameWidth;

  PRUint32 cbcrStride = mMetadata.frameWidth * mMetadata.chromaChannelBpp / 8.0;

  b.mPlanes[1].mData = buffer + mMetadata.frameHeight * b.mPlanes[0].mStride;
  b.mPlanes[1].mStride = cbcrStride;
  b.mPlanes[1].mHeight = mMetadata.frameHeight / 2;
  b.mPlanes[1].mWidth = mMetadata.frameWidth / 2;

  b.mPlanes[2].mData = b.mPlanes[1].mData + mMetadata.frameHeight * cbcrStride / 2;
  b.mPlanes[2].mStride = cbcrStride;
  b.mPlanes[2].mHeight = mMetadata.frameHeight / 2;
  b.mPlanes[2].mWidth = mMetadata.frameWidth / 2;

  VideoData *v = VideoData::Create(mInfo,
                                   mDecoder->GetImageContainer(),
                                   -1,
                                   currentFrameTime,
                                   currentFrameTime + (1000 / mFrameRate),
                                   b,
                                   1, // In raw video every frame is a keyframe
                                   -1);
  if (!v)
    return PR_FALSE;

  mVideoQueue.Push(v);
  mCurrentFrame++;
  currentFrameTime += 1000 / mFrameRate;

  return PR_TRUE;
}

nsresult nsRawReader::Seek(PRInt64 aTime, PRInt64 aStartTime, PRInt64 aEndTime, PRInt64 aCurrentTime)
{
  mozilla::MonitorAutoEnter autoEnter(mMonitor);
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");

  nsMediaStream *stream = mDecoder->GetCurrentStream();
  NS_ASSERTION(stream, "Decoder has no media stream");

  PRUint32 frame = mCurrentFrame;
  if (aTime >= UINT_MAX)
    return NS_ERROR_FAILURE;
  mCurrentFrame = aTime * mFrameRate / 1000;

  PRUint32 offset;
  if (!MulOverflow32(mCurrentFrame, mFrameSize, offset))
    return NS_ERROR_FAILURE;

  offset += sizeof(nsRawVideoHeader);

  nsresult rv = stream->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  NS_ENSURE_SUCCESS(rv, rv);

  mVideoQueue.Erase();

  while(mVideoQueue.GetSize() == 0) {
    PRBool keyframeSkip = PR_FALSE;
    if (!DecodeVideoFrame(keyframeSkip, 0)) {
      mCurrentFrame = frame;
      return NS_ERROR_FAILURE;
    }

    {
      mozilla::MonitorAutoExit autoMonitorExit(mMonitor);
      mozilla::MonitorAutoEnter autoMonitor(mDecoder->GetMonitor());
      if (mDecoder->GetDecodeState() ==
          nsBuiltinDecoderStateMachine::DECODER_STATE_SHUTDOWN) {
        mCurrentFrame = frame;
        return NS_ERROR_FAILURE;
      }
    }

    nsAutoPtr<VideoData> video(mVideoQueue.PeekFront());
    if (video && video->mEndTime < aTime) {
      mVideoQueue.PopFront();
      video = nsnull;
    } else {
      video.forget();
    }
  }

  return NS_OK;
}

PRInt64 nsRawReader::FindEndTime(PRInt64 aEndTime)
{
  return -1;
}

nsresult nsRawReader::GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime)
{
  return NS_OK;
}
