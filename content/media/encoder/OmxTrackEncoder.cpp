/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxTrackEncoder.h"
#include "OMXCodecWrapper.h"
#include "VideoUtils.h"
#include "ISOTrackMetadata.h"

#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define OMX_LOG(args...)                                                       \
  do {                                                                         \
    __android_log_print(ANDROID_LOG_INFO, "OmxTrackEncoder", ##args);          \
  } while (0)
#else
#define OMX_LOG(args, ...)
#endif

using namespace android;

namespace mozilla {

#define ENCODER_CONFIG_FRAME_RATE 30 // fps
#define GET_ENCODED_VIDEO_FRAME_TIMEOUT 100000 // microseconds

nsresult
OmxVideoTrackEncoder::Init(int aWidth, int aHeight, int aDisplayWidth,
                           int aDisplayHeight, TrackRate aTrackRate)
{
  mFrameWidth = aWidth;
  mFrameHeight = aHeight;
  mTrackRate = aTrackRate;
  mDisplayWidth = aDisplayWidth;
  mDisplayHeight = aDisplayHeight;

  mEncoder = OMXCodecWrapper::CreateAVCEncoder();
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_FAILURE);

  nsresult rv = mEncoder->Configure(mFrameWidth, mFrameHeight,
                                    ENCODER_CONFIG_FRAME_RATE);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mInitialized = (rv == NS_OK);

  mReentrantMonitor.NotifyAll();

  return rv;
}

already_AddRefed<TrackMetadataBase>
OmxVideoTrackEncoder::GetMetadata()
{
  {
    // Wait if mEncoder is not initialized nor is being canceled.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mReentrantMonitor.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  nsRefPtr<AVCTrackMetadata> meta = new AVCTrackMetadata();
  meta->mWidth = mFrameWidth;
  meta->mHeight = mFrameHeight;
  meta->mDisplayWidth = mDisplayWidth;
  meta->mDisplayHeight = mDisplayHeight;
  meta->mFrameRate = ENCODER_CONFIG_FRAME_RATE;
  return meta.forget();
}

nsresult
OmxVideoTrackEncoder::GetEncodedTrack(EncodedFrameContainer& aData)
{
  VideoSegment segment;
  {
    // Move all the samples from mRawSegment to segment. We only hold the
    // monitor in this block.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Wait if mEncoder is not initialized nor is being canceled.
    while (!mCanceled && (!mInitialized ||
          (mRawSegment.GetDuration() == 0 && !mEndOfStream))) {
      mReentrantMonitor.Wait();
    }

    if (mCanceled || mEncodingComplete) {
      return NS_ERROR_FAILURE;
    }

    segment.AppendFrom(&mRawSegment);
  }

  // Start queuing raw frames to the input buffers of OMXCodecWrapper.
  VideoSegment::ChunkIterator iter(segment);
  while (!iter.IsEnded()) {
    VideoChunk chunk = *iter;

    // Send only the unique video frames to OMXCodecWrapper.
    if (mLastFrame != chunk.mFrame) {
      uint64_t totalDurationUs = mTotalFrameDuration * USECS_PER_S / mTrackRate;
      layers::Image* img = (chunk.IsNull() || chunk.mFrame.GetForceBlack()) ?
                           nullptr : chunk.mFrame.GetImage();
      mEncoder->Encode(img, mFrameWidth, mFrameHeight, totalDurationUs);
    }

    mLastFrame.TakeFrom(&chunk.mFrame);
    mTotalFrameDuration += chunk.GetDuration();

    iter.Next();
  }

  // Send the EOS signal to OMXCodecWrapper.
  if (mEndOfStream && iter.IsEnded() && !mEosSetInEncoder) {
    uint64_t totalDurationUs = mTotalFrameDuration * USECS_PER_S / mTrackRate;
    layers::Image* img = (!mLastFrame.GetImage() || mLastFrame.GetForceBlack())
                         ? nullptr : mLastFrame.GetImage();
    nsresult result = mEncoder->Encode(img, mFrameWidth, mFrameHeight,
                                       totalDurationUs,
                                       OMXCodecWrapper::BUFFER_EOS);
    // Keep sending EOS signal until OMXVideoEncoder gets it.
    if (result == NS_OK) {
      mEosSetInEncoder = true;
    }
  }

  // Dequeue an encoded frame from the output buffers of OMXCodecWrapper.
  nsresult rv;
  nsTArray<uint8_t> buffer;
  int outFlags = 0;
  int64_t outTimeStampUs = 0;
  mEncoder->GetNextEncodedFrame(&buffer, &outTimeStampUs, &outFlags,
                                GET_ENCODED_VIDEO_FRAME_TIMEOUT);
  if (!buffer.IsEmpty()) {
    nsRefPtr<EncodedFrame> videoData = new EncodedFrame();
    if (outFlags & OMXCodecWrapper::BUFFER_CODEC_CONFIG) {
      videoData->SetFrameType(EncodedFrame::AVC_CSD);
    } else {
      videoData->SetFrameType((outFlags & OMXCodecWrapper::BUFFER_SYNC_FRAME) ?
                              EncodedFrame::AVC_I_FRAME : EncodedFrame::AVC_P_FRAME);
    }
    rv = videoData->SwapInFrameData(buffer);
    NS_ENSURE_SUCCESS(rv, rv);
    videoData->SetTimeStamp(outTimeStampUs);
    aData.AppendEncodedFrame(videoData);
  }

  if (outFlags & OMXCodecWrapper::BUFFER_EOS) {
    mEncodingComplete = true;
    OMX_LOG("Done encoding video.");
  }

  return NS_OK;
}

nsresult
OmxAudioTrackEncoder::AppendEncodedFrames(EncodedFrameContainer& aContainer)
{
  nsTArray<uint8_t> frameData;
  int outFlags = 0;
  int64_t outTimeUs = -1;

  nsresult rv = mEncoder->GetNextEncodedFrame(&frameData, &outTimeUs, &outFlags,
                                              3000); // wait up to 3ms
  NS_ENSURE_SUCCESS(rv, rv);

  if (!frameData.IsEmpty()) {
    bool isCSD = false;
    if (outFlags & OMXCodecWrapper::BUFFER_CODEC_CONFIG) { // codec specific data
      isCSD = true;
    } else if (outFlags & OMXCodecWrapper::BUFFER_EOS) { // last frame
      mEncodingComplete = true;
    }

    nsRefPtr<EncodedFrame> audiodata = new EncodedFrame();
    if (mEncoder->GetCodecType() == OMXCodecWrapper::AAC_ENC) {
      audiodata->SetFrameType(isCSD ?
        EncodedFrame::AAC_CSD : EncodedFrame::AAC_AUDIO_FRAME);
    } else if (mEncoder->GetCodecType() == OMXCodecWrapper::AMR_NB_ENC){
      audiodata->SetFrameType(isCSD ?
        EncodedFrame::AMR_AUDIO_CSD : EncodedFrame::AMR_AUDIO_FRAME);
    } else {
      MOZ_ASSERT("audio codec not supported");
    }
    audiodata->SetTimeStamp(outTimeUs);
    rv = audiodata->SwapInFrameData(frameData);
    NS_ENSURE_SUCCESS(rv, rv);
    aContainer.AppendEncodedFrame(audiodata);
  }

  return NS_OK;
}

nsresult
OmxAudioTrackEncoder::GetEncodedTrack(EncodedFrameContainer& aData)
{
  AudioSegment segment;
  // Move all the samples from mRawSegment to segment. We only hold
  // the monitor in this block.
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Wait if mEncoder is not initialized nor canceled.
    while (!mInitialized && !mCanceled) {
      mReentrantMonitor.Wait();
    }

    if (mCanceled || mEncodingComplete) {
      return NS_ERROR_FAILURE;
    }

    segment.AppendFrom(&mRawSegment);
  }

  nsresult rv;
  if (segment.GetDuration() == 0) {
    // Notify EOS at least once, even if segment is empty.
    if (mEndOfStream && !mEosSetInEncoder) {
      mEosSetInEncoder = true;
      rv = mEncoder->Encode(segment, OMXCodecWrapper::BUFFER_EOS);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Nothing to encode but encoder could still have encoded data for earlier
    // input.
    return AppendEncodedFrames(aData);
  }

  // OMX encoder has limited input buffers only so we have to feed input and get
  // output more than once if there are too many samples pending in segment.
  while (segment.GetDuration() > 0) {
    rv = mEncoder->Encode(segment,
                          mEndOfStream ? OMXCodecWrapper::BUFFER_EOS : 0);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AppendEncodedFrames(aData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
OmxAACAudioTrackEncoder::Init(int aChannels, int aSamplingRate)
{
  mChannels = aChannels;
  mSamplingRate = aSamplingRate;

  mEncoder = OMXCodecWrapper::CreateAACEncoder();
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_FAILURE);

  nsresult rv = mEncoder->Configure(mChannels, mSamplingRate, mSamplingRate);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mInitialized = (rv == NS_OK);

  mReentrantMonitor.NotifyAll();

  return NS_OK;
}

already_AddRefed<TrackMetadataBase>
OmxAACAudioTrackEncoder::GetMetadata()
{
  {
    // Wait if mEncoder is not initialized nor is being canceled.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mReentrantMonitor.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }
  nsRefPtr<AACTrackMetadata> meta = new AACTrackMetadata();
  meta->mChannels = mChannels;
  meta->mSampleRate = mSamplingRate;
  meta->mFrameSize = OMXCodecWrapper::kAACFrameSize;
  meta->mFrameDuration = OMXCodecWrapper::kAACFrameDuration;
  return meta.forget();
}

nsresult
OmxAMRAudioTrackEncoder::Init(int aChannels, int aSamplingRate)
{
  mChannels = aChannels;
  mSamplingRate = aSamplingRate;

  mEncoder = OMXCodecWrapper::CreateAMRNBEncoder();
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_FAILURE);

  nsresult rv = mEncoder->Configure(mChannels, mSamplingRate, AMR_NB_SAMPLERATE);
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mInitialized = (rv == NS_OK);

  mReentrantMonitor.NotifyAll();

  return NS_OK;
}

already_AddRefed<TrackMetadataBase>
OmxAMRAudioTrackEncoder::GetMetadata()
{
  {
    // Wait if mEncoder is not initialized nor is being canceled.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mReentrantMonitor.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  nsRefPtr<AMRTrackMetadata> meta = new AMRTrackMetadata();
  return meta.forget();
}

}
