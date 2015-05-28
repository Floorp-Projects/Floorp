/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaOmxReader.h"

#include "MediaDecoderStateMachine.h"
#include "mozilla/TimeStamp.h"
#include "MediaResource.h"
#include "VideoUtils.h"
#include "MediaOmxDecoder.h"
#include "AbstractMediaDecoder.h"
#include "AudioChannelService.h"
#include "OmxDecoder.h"
#include "MPAPI.h"
#include "gfx2DGlue.h"
#include "MediaStreamSource.h"

#define MAX_DROPPED_FRAMES 25
// Try not to spend more than this much time in a single call to DecodeVideoFrame.
#define MAX_VIDEO_DECODE_SECONDS 0.1

using namespace mozilla::gfx;
using namespace android;

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)

class MediaOmxReader::ProcessCachedDataTask : public Task
{
public:
  ProcessCachedDataTask(MediaOmxReader* aOmxReader, int64_t aOffset)
  : mOmxReader(aOmxReader),
    mOffset(aOffset)
  { }

  void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mOmxReader.get());
    mOmxReader->ProcessCachedData(mOffset, false);
  }

private:
  nsRefPtr<MediaOmxReader> mOmxReader;
  int64_t                  mOffset;
};

// When loading an MP3 stream from a file, we need to parse the file's
// content to find its duration. Reading files of 100 MiB or more can
// delay the player app noticably, so the file is read and decoded in
// smaller chunks.
//
// We first read on the decode thread, but parsing must be done on the
// main thread. After we read the file's initial MiBs in the decode
// thread, an instance of this class is scheduled to the main thread for
// parsing the MP3 stream. The decode thread waits until it has finished.
//
// If there is more data available from the file, the runnable dispatches
// a task to the IO thread for retrieving the next chunk of data, and
// the IO task dispatches a runnable to the main thread for parsing the
// data. This goes on until all of the MP3 file has been parsed.

class MediaOmxReader::NotifyDataArrivedRunnable : public nsRunnable
{
public:
  NotifyDataArrivedRunnable(MediaOmxReader* aOmxReader,
                                     const char* aBuffer, uint64_t aLength,
                                     int64_t aOffset, uint64_t aFullLength)
  : mOmxReader(aOmxReader),
    mBuffer(aBuffer),
    mLength(aLength),
    mOffset(aOffset),
    mFullLength(aFullLength)
  {
    MOZ_ASSERT(mOmxReader.get());
    MOZ_ASSERT(mBuffer.get() || !mLength);
  }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

    NotifyDataArrived();

    return NS_OK;
  }

private:
  void NotifyDataArrived()
  {
    if (mOmxReader->IsShutdown()) {
      return;
    }
    const char* buffer = mBuffer.get();

    while (mLength) {
      uint32_t length = std::min<uint64_t>(mLength, UINT32_MAX);
      mOmxReader->NotifyDataArrived(buffer, length,
                                    mOffset);
      buffer  += length;
      mLength -= length;
      mOffset += length;
    }

    if (static_cast<uint64_t>(mOffset) < mFullLength) {
      // We cannot read data in the main thread because it
      // might block for too long. Instead we post an IO task
      // to the IO thread if there is more data available.
      XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
          new ProcessCachedDataTask(mOmxReader.get(), mOffset));
    }
  }

  nsRefPtr<MediaOmxReader>         mOmxReader;
  nsAutoArrayPtr<const char>       mBuffer;
  uint64_t                         mLength;
  int64_t                          mOffset;
  uint64_t                         mFullLength;
};

MediaOmxReader::MediaOmxReader(AbstractMediaDecoder *aDecoder)
  : MediaOmxCommonReader(aDecoder)
  , mShutdownMutex("MediaOmxReader.Shutdown")
  , mHasVideo(false)
  , mHasAudio(false)
  , mVideoSeekTimeUs(-1)
  , mAudioSeekTimeUs(-1)
  , mLastParserDuration(-1)
  , mSkipCount(0)
  , mIsShutdown(false)
  , mMP3FrameParser(-1)
{
  if (!gMediaDecoderLog) {
    gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  }

  mAudioChannel = dom::AudioChannelService::GetDefaultAudioChannel();
}

MediaOmxReader::~MediaOmxReader()
{
}

nsresult MediaOmxReader::Init(MediaDecoderReader* aCloneDonor)
{
  return NS_OK;
}

already_AddRefed<AbstractMediaDecoder>
MediaOmxReader::SafeGetDecoder() {
  nsRefPtr<AbstractMediaDecoder> decoder;
  MutexAutoLock lock(mShutdownMutex);
  if (!mIsShutdown) {
    decoder = mDecoder;
  }
  return decoder.forget();
}

void MediaOmxReader::ReleaseDecoder()
{
  if (mOmxDecoder.get()) {
    mOmxDecoder->ReleaseDecoder();
  }
  mOmxDecoder.clear();
}

nsRefPtr<ShutdownPromise>
MediaOmxReader::Shutdown()
{
  {
    MutexAutoLock lock(mShutdownMutex);
    mIsShutdown = true;
  }

  nsRefPtr<ShutdownPromise> p = MediaDecoderReader::Shutdown();

  // Wait for the superclass to finish tearing things down before releasing
  // the decoder on the main thread.
  p->Then(AbstractThread::MainThread(), __func__, this, &MediaOmxReader::ReleaseDecoder, &MediaOmxReader::ReleaseDecoder);

  return p;
}

void MediaOmxReader::ReleaseMediaResources()
{
  mMediaResourceRequest.DisconnectIfExists();
  mMetadataPromise.RejectIfExists(ReadMetadataFailureReason::METADATA_ERROR, __func__);

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

nsRefPtr<MediaDecoderReader::MetadataPromise>
MediaOmxReader::AsyncReadMetadata()
{
  MOZ_ASSERT(OnTaskQueue());
  EnsureActive();

  // Initialize the internal OMX Decoder.
  nsresult rv = InitOmxDecoder();
  if (NS_FAILED(rv)) {
    return MediaDecoderReader::MetadataPromise::CreateAndReject(
             ReadMetadataFailureReason::METADATA_ERROR, __func__);
  }

  bool isMP3 = mDecoder->GetResource()->GetContentType().EqualsASCII(AUDIO_MP3);
  if (isMP3) {
    // When read sdcard's file on b2g platform at constructor,
    // the mDecoder->GetResource()->GetLength() would return -1.
    // Delay set the total duration on this function.
    mMP3FrameParser.SetLength(mDecoder->GetResource()->GetLength());
    ProcessCachedData(0, true);
  }

  nsRefPtr<MediaDecoderReader::MetadataPromise> p = mMetadataPromise.Ensure(__func__);

  nsRefPtr<MediaOmxReader> self = this;
  mMediaResourceRequest.Begin(mOmxDecoder->AllocateMediaResources()
    ->RefableThen(GetTaskQueue(), __func__,
      [self] (bool) -> void {
        self->mMediaResourceRequest.Complete();
        self->HandleResourceAllocated();
      }, [self] (bool) -> void {
        self->mMediaResourceRequest.Complete();
        self->mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
      }));

  return p;
}

void MediaOmxReader::HandleResourceAllocated()
{
  EnsureActive();

  // After resources are available, set the metadata.
  if (!mOmxDecoder->EnsureMetadata()) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    return;
  }

  bool isMP3 = mDecoder->GetResource()->GetContentType().EqualsASCII(AUDIO_MP3);
  if (isMP3 && mMP3FrameParser.IsMP3()) {
    // Check if the MP3 frame parser found a duration.
    mLastParserDuration = mMP3FrameParser.GetDuration();
  }

  if (mLastParserDuration >= 0) {
    // Prefer the parser duration if we have it.
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(mLastParserDuration);
  } else {
    // MP3 parser failed to find a duration.
    // Set the total duration (the max of the audio and video track).
    int64_t durationUs;
    mOmxDecoder->GetDuration(&durationUs);
    if (durationUs) {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mDecoder->SetMediaDuration(durationUs);
    }
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
      mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
      return;
    }

    // Video track's frame sizes will not overflow. Activate the video track.
    mHasVideo = true;
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
    mHasAudio = true;
    mInfo.mAudio.mChannels = numChannels;
    mInfo.mAudio.mRate = sampleRate;
  }

  nsRefPtr<MetadataHolder> metadata = new MetadataHolder();
  metadata->mInfo = mInfo;
  metadata->mTags = nullptr;

#ifdef MOZ_AUDIO_OFFLOAD
  CheckAudioOffload();
#endif

  mMetadataPromise.Resolve(metadata, __func__);
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
  MOZ_ASSERT(OnTaskQueue());
  EnsureActive();

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  AbstractMediaDecoder::AutoNotifyDecoded a(mDecoder);

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
    mVideoSeekTimeUs = -1;

    // Ignore empty buffer which stagefright media read will sporadically return
    if (frame.mSize == 0 && !frame.mGraphicBuffer) {
      continue;
    }

    a.mParsed++;
    if (frame.mShouldSkip && mSkipCount < MAX_DROPPED_FRAMES) {
      mSkipCount++;
      continue;
    }

    mSkipCount = 0;

    aKeyframeSkip = false;

    IntRect picture = mPicture;
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

    nsRefPtr<VideoData> v;
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

    a.mDecoded++;
    NS_ASSERTION(a.mDecoded <= a.mParsed, "Expect to decode fewer frames than parsed in OMX decoder...");

    mVideoQueue.Push(v);

    break;
  }

  return true;
}

void MediaOmxReader::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsRefPtr<AbstractMediaDecoder> decoder = SafeGetDecoder();
  if (!decoder) { // reader has shut down
    return;
  }
  if (HasVideo()) {
    return;
  }
  if (!mMP3FrameParser.NeedsData()) {
    return;
  }

  mMP3FrameParser.Parse(aBuffer, aLength, aOffset);
  if (!mMP3FrameParser.IsMP3()) {
    return;
  }

  int64_t duration = mMP3FrameParser.GetDuration();
  if (duration != mLastParserDuration) {
    ReentrantMonitorAutoEnter mon(decoder->GetReentrantMonitor());
    mLastParserDuration = duration;
    decoder->UpdateEstimatedMediaDuration(mLastParserDuration);
  }
}

bool MediaOmxReader::DecodeAudioData()
{
  MOZ_ASSERT(OnTaskQueue());
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

nsRefPtr<MediaDecoderReader::SeekPromise>
MediaOmxReader::Seek(int64_t aTarget, int64_t aEndTime)
{
  MOZ_ASSERT(OnTaskQueue());
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

  return SeekPromise::CreateAndResolve(mAudioSeekTimeUs, __func__);
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

int64_t MediaOmxReader::ProcessCachedData(int64_t aOffset, bool aWaitForCompletion)
{
  // Could run on decoder thread or IO thread.
  nsRefPtr<AbstractMediaDecoder> decoder = SafeGetDecoder();
  if (!decoder) { // reader has shut down
    return -1;
  }
  // We read data in chunks of 32 KiB. We can reduce this
  // value if media, such as sdcards, is too slow.
  // Because of SD card's slowness, need to keep sReadSize to small size.
  // See Bug 914870.
  static const int64_t sReadSize = 32 * 1024;

  NS_ASSERTION(!NS_IsMainThread(), "Should not be on main thread.");

  MOZ_ASSERT(decoder->GetResource());
  int64_t resourceLength = decoder->GetResource()->GetCachedDataEnd(0);
  NS_ENSURE_TRUE(resourceLength >= 0, -1);

  if (aOffset >= resourceLength) {
    return 0; // Cache is empty, nothing to do
  }

  int64_t bufferLength = std::min<int64_t>(resourceLength-aOffset, sReadSize);

  nsAutoArrayPtr<char> buffer(new char[bufferLength]);

  nsresult rv = decoder->GetResource()->ReadFromCache(buffer.get(),
                                                       aOffset, bufferLength);
  NS_ENSURE_SUCCESS(rv, -1);

  nsRefPtr<NotifyDataArrivedRunnable> runnable(
    new NotifyDataArrivedRunnable(this, buffer.forget(), bufferLength,
                                  aOffset, resourceLength));
  if (aWaitForCompletion) {
    rv = NS_DispatchToMainThread(runnable.get(), NS_DISPATCH_SYNC);
  } else {
    rv = NS_DispatchToMainThread(runnable.get());
  }
  NS_ENSURE_SUCCESS(rv, -1);

  return resourceLength - aOffset - bufferLength;
}

android::sp<android::MediaSource> MediaOmxReader::GetAudioOffloadTrack()
{
  if (!mOmxDecoder.get()) {
    return nullptr;
  }
  return mOmxDecoder->GetAudioOffloadTrack();
}

} // namespace mozilla
