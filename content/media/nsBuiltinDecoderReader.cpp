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

// 32 bit integer multiplication with overflow checking. Returns PR_TRUE
// if the multiplication was successful, or PR_FALSE if the operation resulted
// in an integer overflow.
PRBool MulOverflow32(PRUint32 a, PRUint32 b, PRUint32& aResult) {
  PRUint64 a64 = a;
  PRUint64 b64 = b;
  PRUint64 r64 = a64 * b64;
  if (r64 > PR_UINT32_MAX)
    return PR_FALSE;
  aResult = static_cast<PRUint32>(r64);
  return PR_TRUE;
}

VideoData* VideoData::Create(PRInt64 aOffset,
                             PRInt64 aTime,
                             const YCbCrBuffer &aBuffer,
                             PRBool aKeyframe,
                             PRInt64 aTimecode)
{
  nsAutoPtr<VideoData> v(new VideoData(aOffset, aTime, aKeyframe, aTimecode));
  for (PRUint32 i=0; i < 3; ++i) {
    PRUint32 size = 0;
    if (!MulOverflow32(PR_ABS(aBuffer.mPlanes[i].mHeight),
                       PR_ABS(aBuffer.mPlanes[i].mStride),
                       size))
    {
      // Invalid frame size. Skip this plane. The plane will have 0
      // dimensions, thanks to our constructor.
      continue;
    }
    unsigned char* p = static_cast<unsigned char*>(moz_xmalloc(size));
    if (!p) {
      NS_WARNING("Failed to allocate memory for video frame");
      return nsnull;
    }
    v->mBuffer.mPlanes[i].mData = p;
    v->mBuffer.mPlanes[i].mWidth = aBuffer.mPlanes[i].mWidth;
    v->mBuffer.mPlanes[i].mHeight = aBuffer.mPlanes[i].mHeight;
    v->mBuffer.mPlanes[i].mStride = aBuffer.mPlanes[i].mStride;
    memcpy(v->mBuffer.mPlanes[i].mData, aBuffer.mPlanes[i].mData, size);
  }
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
          (endTime = FindEndTime(endOffset) != -1))
      {
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

  nsMediaStream* stream = mDecoder->GetCurrentStream();

  stream->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
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


