/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaOmxReader.h"

#include "MediaDecoderStateMachine.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/TimeRanges.h"
#include "MediaResource.h"
#include "VideoUtils.h"
#include "MediaOmxDecoder.h"
#include "AbstractMediaDecoder.h"
#include "AudioChannelService.h"
#include "OmxDecoder.h"
#include "MPAPI.h"
#include "gfx2DGlue.h"
#include "MediaStreamSource.h"

#ifdef MOZ_AUDIO_OFFLOAD
#include <stagefright/Utils.h>
#include <cutils/properties.h>
#include <stagefright/MetaData.h>
#endif

#define MAX_DROPPED_FRAMES 25
// Try not to spend more than this much time in a single call to DecodeVideoFrame.
#define MAX_VIDEO_DECODE_SECONDS 0.1

using namespace mozilla::gfx;
using namespace android;

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define DECODER_LOG(type, msg)
#endif

MediaOmxReader::MediaOmxReader(AbstractMediaDecoder *aDecoder)
  : MediaDecoderReader(aDecoder)
  , mHasVideo(false)
  , mHasAudio(false)
  , mVideoSeekTimeUs(-1)
  , mAudioSeekTimeUs(-1)
  , mSkipCount(0)
{
#ifdef PR_LOGGING
  if (!gMediaDecoderLog) {
    gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  }
#endif

  mAudioChannel = dom::AudioChannelService::GetDefaultAudioChannel();
}

MediaOmxReader::~MediaOmxReader()
{
}

nsresult MediaOmxReader::Init(MediaDecoderReader* aCloneDonor)
{
  return NS_OK;
}

void MediaOmxReader::ReleaseDecoder()
{
  if (mOmxDecoder.get()) {
    mOmxDecoder->ReleaseDecoder();
  }
  mOmxDecoder.clear();
}

void MediaOmxReader::Shutdown()
{
  ReleaseMediaResources();
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &MediaOmxReader::ReleaseDecoder);
  NS_DispatchToMainThread(event);
}

bool MediaOmxReader::IsWaitingMediaResources()
{
  if (!mOmxDecoder.get()) {
    return false;
  }
  return mOmxDecoder->IsWaitingMediaResources();
}

bool MediaOmxReader::IsDormantNeeded()
{
  if (!mOmxDecoder.get()) {
    return false;
  }
  return mOmxDecoder->IsDormantNeeded();
}

void MediaOmxReader::ReleaseMediaResources()
{
  ResetDecode();
  // Before freeing a video codec, all video buffers needed to be released
  // even from graphics pipeline.
  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  if (container) {
    container->ClearCurrentFrame();
  }
  if (mOmxDecoder.get()) {
    mOmxDecoder->ReleaseMediaResources();
  }
}

nsresult MediaOmxReader::InitOmxDecoder()
{
  if (!mOmxDecoder.get()) {
    //register sniffers, if they are not registered in this process.
    DataSource::RegisterDefaultSniffers();
    mDecoder->GetResource()->SetReadMode(MediaCacheStream::MODE_METADATA);

    sp<DataSource> dataSource = new MediaStreamSource(mDecoder->GetResource());
    dataSource->initCheck();

    mExtractor = MediaExtractor::Create(dataSource);
    if (!mExtractor.get()) {
      return NS_ERROR_FAILURE;
    }
    mOmxDecoder = new OmxDecoder(mDecoder->GetResource(), mDecoder);
    if (!mOmxDecoder->Init(mExtractor)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult MediaOmxReader::ReadMetadata(MediaInfo* aInfo,
                                      MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  EnsureActive();

  *aTags = nullptr;

  // Initialize the internal OMX Decoder.
  nsresult rv = InitOmxDecoder();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mOmxDecoder->TryLoad()) {
    return NS_ERROR_FAILURE;
  }

#ifdef MOZ_AUDIO_OFFLOAD
  CheckAudioOffload();
#endif

  if (IsWaitingMediaResources()) {
    return NS_OK;
  }

  // Set the total duration (the max of the audio and video track).
  int64_t durationUs;
  mOmxDecoder->GetDuration(&durationUs);
  if (durationUs) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(durationUs);
  }

  if (mOmxDecoder->HasVideo()) {
    int32_t displayWidth, displayHeight, width, height;
    mOmxDecoder->GetVideoParameters(&displayWidth, &displayHeight,
                                    &width, &height);
    nsIntRect pictureRect(0, 0, width, height);

    // Validate the container-reported frame and pictureRect sizes. This ensures
    // that our video frame creation code doesn't overflow.
    nsIntSize displaySize(displayWidth, displayHeight);
    nsIntSize frameSize(width, height);
    if (!IsValidVideoRegion(frameSize, pictureRect, displaySize)) {
      return NS_ERROR_FAILURE;
    }

    // Video track's frame sizes will not overflow. Activate the video track.
    mHasVideo = mInfo.mVideo.mHasVideo = true;
    mInfo.mVideo.mDisplay = displaySize;
    mPicture = pictureRect;
    mInitialFrame = frameSize;
    VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
    if (container) {
      container->SetCurrentFrame(gfxIntSize(displaySize.width, displaySize.height),
                                 nullptr,
                                 mozilla::TimeStamp::Now());
    }
  }

  if (mOmxDecoder->HasAudio()) {
    int32_t numChannels, sampleRate;
    mOmxDecoder->GetAudioParameters(&numChannels, &sampleRate);
    mHasAudio = mInfo.mAudio.mHasAudio = true;
    mInfo.mAudio.mChannels = numChannels;
    mInfo.mAudio.mRate = sampleRate;
  }

 *aInfo = mInfo;

  return NS_OK;
}

bool
MediaOmxReader::IsMediaSeekable()
{
  // Check the MediaExtract flag if the source is seekable.
  return (mExtractor->flags() & MediaExtractor::CAN_SEEK);
}

bool MediaOmxReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                      int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  EnsureActive();

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  bool doSeek = mVideoSeekTimeUs != -1;
  if (doSeek) {
    aTimeThreshold = mVideoSeekTimeUs;
  }

  TimeStamp start = TimeStamp::Now();

  // Read next frame. Don't let this loop run for too long.
  while ((TimeStamp::Now() - start) < TimeDuration::FromSeconds(MAX_VIDEO_DECODE_SECONDS)) {
    MPAPI::VideoFrame frame;
    frame.mGraphicBuffer = nullptr;
    frame.mShouldSkip = false;
    if (!mOmxDecoder->ReadVideo(&frame, aTimeThreshold, aKeyframeSkip, doSeek)) {
      return false;
    }
    doSeek = false;

    // Ignore empty buffer which stagefright media read will sporadically return
    if (frame.mSize == 0 && !frame.mGraphicBuffer) {
      continue;
    }

    parsed++;
    if (frame.mShouldSkip && mSkipCount < MAX_DROPPED_FRAMES) {
      mSkipCount++;
      continue;
    }

    mSkipCount = 0;

    mVideoSeekTimeUs = -1;
    aKeyframeSkip = false;

    IntRect picture = ToIntRect(mPicture);
    if (frame.Y.mWidth != mInitialFrame.width ||
        frame.Y.mHeight != mInitialFrame.height) {

      // Frame size is different from what the container reports. This is legal,
      // and we will preserve the ratio of the crop rectangle as it
      // was reported relative to the picture size reported by the container.
      picture.x = (mPicture.x * frame.Y.mWidth) / mInitialFrame.width;
      picture.y = (mPicture.y * frame.Y.mHeight) / mInitialFrame.height;
      picture.width = (frame.Y.mWidth * mPicture.width) / mInitialFrame.width;
      picture.height = (frame.Y.mHeight * mPicture.height) / mInitialFrame.height;
    }

    // This is the approximate byte position in the stream.
    int64_t pos = mDecoder->GetResource()->Tell();

    VideoData *v;
    if (!frame.mGraphicBuffer) {

      VideoData::YCbCrBuffer b;
      b.mPlanes[0].mData = static_cast<uint8_t *>(frame.Y.mData);
      b.mPlanes[0].mStride = frame.Y.mStride;
      b.mPlanes[0].mHeight = frame.Y.mHeight;
      b.mPlanes[0].mWidth = frame.Y.mWidth;
      b.mPlanes[0].mOffset = frame.Y.mOffset;
      b.mPlanes[0].mSkip = frame.Y.mSkip;

      b.mPlanes[1].mData = static_cast<uint8_t *>(frame.Cb.mData);
      b.mPlanes[1].mStride = frame.Cb.mStride;
      b.mPlanes[1].mHeight = frame.Cb.mHeight;
      b.mPlanes[1].mWidth = frame.Cb.mWidth;
      b.mPlanes[1].mOffset = frame.Cb.mOffset;
      b.mPlanes[1].mSkip = frame.Cb.mSkip;

      b.mPlanes[2].mData = static_cast<uint8_t *>(frame.Cr.mData);
      b.mPlanes[2].mStride = frame.Cr.mStride;
      b.mPlanes[2].mHeight = frame.Cr.mHeight;
      b.mPlanes[2].mWidth = frame.Cr.mWidth;
      b.mPlanes[2].mOffset = frame.Cr.mOffset;
      b.mPlanes[2].mSkip = frame.Cr.mSkip;

      v = VideoData::Create(mInfo.mVideo,
                            mDecoder->GetImageContainer(),
                            pos,
                            frame.mTimeUs,
                            1, // We don't know the duration.
                            b,
                            frame.mKeyFrame,
                            -1,
                            picture);
    } else {
      v = VideoData::Create(mInfo.mVideo,
                            mDecoder->GetImageContainer(),
                            pos,
                            frame.mTimeUs,
                            1, // We don't know the duration.
                            frame.mGraphicBuffer,
                            frame.mKeyFrame,
                            -1,
                            picture);
    }

    if (!v) {
      NS_WARNING("Unable to create VideoData");
      return false;
    }

    decoded++;
    NS_ASSERTION(decoded <= parsed, "Expect to decode fewer frames than parsed in OMX decoder...");

    mVideoQueue.Push(v);

    break;
  }

  return true;
}

void MediaOmxReader::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  android::OmxDecoder *omxDecoder = mOmxDecoder.get();

  if (omxDecoder) {
    omxDecoder->NotifyDataArrived(aBuffer, aLength, aOffset);
  }
}

bool MediaOmxReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  EnsureActive();

  // This is the approximate byte position in the stream.
  int64_t pos = mDecoder->GetResource()->Tell();

  // Read next frame
  MPAPI::AudioFrame source;
  if (!mOmxDecoder->ReadAudio(&source, mAudioSeekTimeUs)) {
    return false;
  }
  mAudioSeekTimeUs = -1;

  // Ignore empty buffer which stagefright media read will sporadically return
  if (source.mSize == 0) {
    return true;
  }

  uint32_t frames = source.mSize / (source.mAudioChannels *
                                    sizeof(AudioDataValue));

  typedef AudioCompactor::NativeCopy OmxCopy;
  return mAudioCompactor.Push(pos,
                              source.mTimeUs,
                              source.mAudioSampleRate,
                              frames,
                              source.mAudioChannels,
                              OmxCopy(static_cast<uint8_t *>(source.mData),
                                      source.mSize,
                                      source.mAudioChannels));
}

nsresult MediaOmxReader::Seek(int64_t aTarget, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  EnsureActive();

  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  if (container && container->GetImageContainer()) {
    container->GetImageContainer()->ClearAllImagesExceptFront();
  }

  if (mHasAudio && mHasVideo) {
    // The OMXDecoder seeks/demuxes audio and video streams separately. So if
    // we seek both audio and video to aTarget, the audio stream can typically
    // seek closer to the seek target, since typically every audio block is
    // a sync point, whereas for video there are only keyframes once every few
    // seconds. So if we have both audio and video, we must seek the video
    // stream to the preceeding keyframe first, get the stream time, and then
    // seek the audio stream to match the video stream's time. Otherwise, the
    // audio and video streams won't be in sync after the seek.
    mVideoSeekTimeUs = aTarget;
    const VideoData* v = DecodeToFirstVideoData();
    mAudioSeekTimeUs = v ? v->mTime : aTarget;
  } else {
    mAudioSeekTimeUs = mVideoSeekTimeUs = aTarget;
  }

  return NS_OK;
}

void MediaOmxReader::SetIdle() {
  if (!mOmxDecoder.get()) {
    return;
  }
  mOmxDecoder->Pause();
}

void MediaOmxReader::EnsureActive() {
  if (!mOmxDecoder.get()) {
    return;
  }
  DebugOnly<nsresult> result = mOmxDecoder->Play();
  NS_ASSERTION(result == NS_OK, "OmxDecoder should be in play state to continue decoding");
}

#ifdef MOZ_AUDIO_OFFLOAD
void MediaOmxReader::CheckAudioOffload()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  char offloadProp[128];
  property_get("audio.offload.disable", offloadProp, "0");
  bool offloadDisable =  atoi(offloadProp) != 0;
  if (offloadDisable) {
    return;
  }

  mAudioOffloadTrack = mOmxDecoder->GetAudioOffloadTrack();
  sp<MetaData> meta = (mAudioOffloadTrack.get()) ?
      mAudioOffloadTrack->getFormat() : nullptr;

  // Supporting audio offload only when there is no video, no streaming
  bool hasNoVideo = !mOmxDecoder->HasVideo();
  bool isNotStreaming
      = mDecoder->GetResource()->IsDataCachedToEndOfResource(0);

  // Not much benefit in trying to offload other channel types. Most of them
  // aren't supported and also duration would be less than a minute
  bool isTypeMusic = mAudioChannel == dom::AudioChannel::Content;

  DECODER_LOG(PR_LOG_DEBUG, ("%s meta %p, no video %d, no streaming %d,"
      " channel type %d", __FUNCTION__, meta.get(), hasNoVideo,
      isNotStreaming, mAudioChannel));

  if ((meta.get()) && hasNoVideo && isNotStreaming && isTypeMusic &&
      canOffloadStream(meta, false, false, AUDIO_STREAM_MUSIC)) {
    DECODER_LOG(PR_LOG_DEBUG, ("Can offload this audio stream"));
    mDecoder->SetCanOffloadAudio(true);
  }
}
#endif

} // namespace mozilla
