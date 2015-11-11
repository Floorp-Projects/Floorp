/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCodecReader.h"

#include <OMX_IVCommon.h>

#include <gui/Surface.h>
#include <ICrypto.h>

#include "GonkNativeWindow.h"

#include <stagefright/foundation/ABuffer.h>
#include <stagefright/foundation/ADebug.h>
#include <stagefright/foundation/ALooper.h>
#include <stagefright/foundation/AMessage.h>
#include <stagefright/MediaBuffer.h>
#include <stagefright/MediaCodec.h>
#include <stagefright/MediaDefs.h>
#include <stagefright/MediaExtractor.h>
#include <stagefright/MediaSource.h>
#include <stagefright/MetaData.h>
#include <stagefright/Utils.h>

#include "mozilla/TaskQueue.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/layers/GrallocTextureClient.h"

#include "gfx2DGlue.h"

#include "MediaStreamSource.h"
#include "MP3FrameParser.h"
#include "nsMimeTypes.h"
#include "nsThreadUtils.h"
#include "ImageContainer.h"
#include "mozilla/SharedThreadPool.h"
#include "VideoFrameContainer.h"
#include "VideoUtils.h"

using namespace android;
using namespace mozilla::layers;
using namespace mozilla::media;

namespace mozilla {

static const int64_t sInvalidDurationUs = INT64_C(-1);
static const int64_t sInvalidTimestampUs = INT64_C(-1);

// Try not to spend more than this much time (in seconds) in a single call
// to GetCodecOutputData.
static const double sMaxAudioDecodeDurationS = 0.1;
static const double sMaxVideoDecodeDurationS = 0.1;

static CheckedUint32 sInvalidInputIndex = INT32_C(-1);

inline bool
IsValidDurationUs(int64_t aDuration)
{
  return aDuration >= INT64_C(0);
}

inline bool
IsValidTimestampUs(int64_t aTimestamp)
{
  return aTimestamp >= INT64_C(0);
}

MediaCodecReader::TrackInputCopier::~TrackInputCopier()
{
}

bool
MediaCodecReader::TrackInputCopier::Copy(MediaBuffer* aSourceBuffer,
                                         sp<ABuffer> aCodecBuffer)
{
  if (aSourceBuffer == nullptr ||
      aCodecBuffer == nullptr ||
      aSourceBuffer->range_length() > aCodecBuffer->capacity()) {
    return false;
  }

  aCodecBuffer->setRange(0, aSourceBuffer->range_length());
  memcpy(aCodecBuffer->data(),
         (uint8_t*)aSourceBuffer->data() + aSourceBuffer->range_offset(),
         aSourceBuffer->range_length());

  return true;
}

MediaCodecReader::Track::Track(Type type)
  : mType(type)
  , mSourceIsStopped(true)
  , mDurationUs(INT64_C(0))
  , mInputIndex(sInvalidInputIndex)
  , mInputEndOfStream(false)
  , mOutputEndOfStream(false)
  , mSeekTimeUs(sInvalidTimestampUs)
  , mFlushed(false)
  , mDiscontinuity(false)
  , mTaskQueue(nullptr)
  , mTrackMonitor("MediaCodecReader::mTrackMonitor")
{
  MOZ_ASSERT(mType != kUnknown, "Should have a valid Track::Type");
}

// Append the value of |kKeyValidSamples| to the end of each vorbis buffer.
// https://github.com/mozilla-b2g/platform_frameworks_av/blob/master/media/libstagefright/OMXCodec.cpp#L3128
// https://github.com/mozilla-b2g/platform_frameworks_av/blob/master/media/libstagefright/NuMediaExtractor.cpp#L472
bool
MediaCodecReader::VorbisInputCopier::Copy(MediaBuffer* aSourceBuffer,
                                          sp<ABuffer> aCodecBuffer)
{
  if (aSourceBuffer == nullptr ||
      aCodecBuffer == nullptr ||
      aSourceBuffer->range_length() + sizeof(int32_t) > aCodecBuffer->capacity()) {
    return false;
  }

  int32_t numPageSamples = 0;
  if (!aSourceBuffer->meta_data()->findInt32(kKeyValidSamples, &numPageSamples)) {
    numPageSamples = -1;
  }

  aCodecBuffer->setRange(0, aSourceBuffer->range_length() + sizeof(int32_t));
  memcpy(aCodecBuffer->data(),
         (uint8_t*)aSourceBuffer->data() + aSourceBuffer->range_offset(),
         aSourceBuffer->range_length());
  memcpy(aCodecBuffer->data() + aSourceBuffer->range_length(),
         &numPageSamples, sizeof(numPageSamples));

  return true;
}

MediaCodecReader::AudioTrack::AudioTrack()
  : Track(kAudio)
{
  mAudioPromise.SetMonitor(&mTrackMonitor);
}

MediaCodecReader::VideoTrack::VideoTrack()
  : Track(kVideo)
  , mWidth(0)
  , mHeight(0)
  , mStride(0)
  , mSliceHeight(0)
  , mColorFormat(0)
  , mRotation(0)
{
  mVideoPromise.SetMonitor(&mTrackMonitor);
}

MediaCodecReader::CodecBufferInfo::CodecBufferInfo()
  : mIndex(0)
  , mOffset(0)
  , mSize(0)
  , mTimeUs(0)
  , mFlags(0)
{
}

MediaCodecReader::SignalObject::SignalObject(const char* aName)
  : mMonitor(aName)
  , mSignaled(false)
{
}

MediaCodecReader::SignalObject::~SignalObject()
{
}

void
MediaCodecReader::SignalObject::Wait()
{
  MonitorAutoLock al(mMonitor);
  if (!mSignaled) {
    mMonitor.Wait();
  }
}

void
MediaCodecReader::SignalObject::Signal()
{
  MonitorAutoLock al(mMonitor);
  mSignaled = true;
  mMonitor.Notify();
}

MediaCodecReader::ParseCachedDataRunnable::ParseCachedDataRunnable(RefPtr<MediaCodecReader> aReader,
                                                                   const char* aBuffer,
                                                                   uint32_t aLength,
                                                                   int64_t aOffset,
                                                                   RefPtr<SignalObject> aSignal)
  : mReader(aReader)
  , mBuffer(aBuffer)
  , mLength(aLength)
  , mOffset(aOffset)
  , mSignal(aSignal)
{
  MOZ_ASSERT(mReader, "Should have a valid MediaCodecReader.");
  MOZ_ASSERT(mBuffer, "Should have a valid buffer.");
  MOZ_ASSERT(mOffset >= INT64_C(0), "Should have a valid offset.");
}

NS_IMETHODIMP
MediaCodecReader::ParseCachedDataRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (mReader->ParseDataSegment(mBuffer, mLength, mOffset)) {
    MonitorAutoLock monLock(mReader->mParserMonitor);
    if (mReader->mNextParserPosition >= mOffset + mLength &&
        mReader->mParsedDataLength < mOffset + mLength) {
      mReader->mParsedDataLength = mOffset + mLength;
    }
  }

  if (mSignal != nullptr) {
    mSignal->Signal();
  }

  return NS_OK;
}

MediaCodecReader::ProcessCachedDataTask::ProcessCachedDataTask(RefPtr<MediaCodecReader> aReader,
                                                               int64_t aOffset)
  : mReader(aReader)
  , mOffset(aOffset)
{
  MOZ_ASSERT(mReader, "Should have a valid MediaCodecReader.");
  MOZ_ASSERT(mOffset >= INT64_C(0), "Should have a valid offset.");
}

void
MediaCodecReader::ProcessCachedDataTask::Run()
{
  mReader->ProcessCachedData(mOffset, nullptr);
}

MediaCodecReader::MediaCodecReader(AbstractMediaDecoder* aDecoder)
  : MediaOmxCommonReader(aDecoder)
  , mExtractor(nullptr)
  , mTextureClientIndexesLock("MediaCodecReader::mTextureClientIndexesLock")
  , mColorConverterBufferSize(0)
  , mParserMonitor("MediaCodecReader::mParserMonitor")
  , mParseDataFromCache(true)
  , mNextParserPosition(INT64_C(0))
  , mParsedDataLength(INT64_C(0))
{
}

MediaCodecReader::~MediaCodecReader()
{
}

void
MediaCodecReader::ReleaseMediaResources()
{
  // Stop the mSource because we are in the dormant state and the stop function
  // will rewind the mSource to the beginning of the stream.
  if (mVideoTrack.mSource != nullptr && !mVideoTrack.mSourceIsStopped) {
    mVideoTrack.mSource->stop();
    mVideoTrack.mSourceIsStopped = true;
  }
  if (mAudioTrack.mSource != nullptr && !mAudioTrack.mSourceIsStopped) {
    mAudioTrack.mSource->stop();
    mAudioTrack.mSourceIsStopped = true;
  }
  ReleaseCriticalResources();
}

RefPtr<ShutdownPromise>
MediaCodecReader::Shutdown()
{
  MOZ_ASSERT(mAudioTrack.mAudioPromise.IsEmpty());
  MOZ_ASSERT(mVideoTrack.mVideoPromise.IsEmpty());
  ReleaseResources();
  return MediaDecoderReader::Shutdown();
}

void
MediaCodecReader::DispatchAudioTask()
{
  if (mAudioTrack.mTaskQueue) {
    RefPtr<nsIRunnable> task =
      NS_NewRunnableMethod(this,
                           &MediaCodecReader::DecodeAudioDataTask);
    mAudioTrack.mTaskQueue->Dispatch(task.forget());
  }
}

void
MediaCodecReader::DispatchVideoTask(int64_t aTimeThreshold)
{
  if (mVideoTrack.mTaskQueue) {
    RefPtr<nsIRunnable> task =
      NS_NewRunnableMethodWithArg<int64_t>(this,
                                           &MediaCodecReader::DecodeVideoFrameTask,
                                           aTimeThreshold);
    mVideoTrack.mTaskQueue->Dispatch(task.forget());
  }
}

RefPtr<MediaDecoderReader::AudioDataPromise>
MediaCodecReader::RequestAudioData()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasAudio());

  MonitorAutoLock al(mAudioTrack.mTrackMonitor);
  if (CheckAudioResources()) {
    DispatchAudioTask();
  }
  MOZ_ASSERT(mAudioTrack.mAudioPromise.IsEmpty());
  return mAudioTrack.mAudioPromise.Ensure(__func__);
}

RefPtr<MediaDecoderReader::VideoDataPromise>
MediaCodecReader::RequestVideoData(bool aSkipToNextKeyframe,
                                   int64_t aTimeThreshold)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasVideo());

  int64_t threshold = sInvalidTimestampUs;
  if (aSkipToNextKeyframe && IsValidTimestampUs(aTimeThreshold)) {
    threshold = aTimeThreshold;
  }

  MonitorAutoLock al(mVideoTrack.mTrackMonitor);
  if (CheckVideoResources()) {
    DispatchVideoTask(threshold);
  }
  MOZ_ASSERT(mVideoTrack.mVideoPromise.IsEmpty());
  return mVideoTrack.mVideoPromise.Ensure(__func__);
}

void
MediaCodecReader::DecodeAudioDataSync()
{
  if (mAudioTrack.mCodec == nullptr || !mAudioTrack.mCodec->allocated() ||
      mAudioTrack.mOutputEndOfStream) {
    return;
  }

  // Get one audio output data from MediaCodec
  CodecBufferInfo bufferInfo;
  status_t status;
  TimeStamp timeout = TimeStamp::Now() +
                      TimeDuration::FromSeconds(sMaxAudioDecodeDurationS);
  while (true) {
    // Try to fill more input buffers and then get one output buffer.
    // FIXME: use callback from MediaCodec
    FillCodecInputData(mAudioTrack);

    status = GetCodecOutputData(mAudioTrack, bufferInfo, sInvalidTimestampUs,
                                timeout);
    if (status == OK || status == ERROR_END_OF_STREAM) {
      break;
    } else if (status == -EAGAIN) {
      if (TimeStamp::Now() > timeout) {
        // Don't let this loop run for too long. Try it again later.
        return;
      }
      continue; // Try it again now.
    } else if (status == INFO_FORMAT_CHANGED) {
      if (UpdateAudioInfo()) {
        continue; // Try it again now.
      } else {
        return;
      }
    } else {
      return;
    }
  }

  if ((bufferInfo.mFlags & MediaCodec::BUFFER_FLAG_EOS) ||
      (status == ERROR_END_OF_STREAM)) {
    AudioQueue().Finish();
  } else if (bufferInfo.mBuffer != nullptr && bufferInfo.mSize > 0 &&
             bufferInfo.mBuffer->data() != nullptr) {
    MOZ_ASSERT(mStreamSource);
    // This is the approximate byte position in the stream.
    int64_t pos = mStreamSource->Tell();

    uint32_t frames = bufferInfo.mSize /
                      (mInfo.mAudio.mChannels * sizeof(AudioDataValue));

    mAudioCompactor.Push(
      pos,
      bufferInfo.mTimeUs,
      mInfo.mAudio.mRate,
      frames,
      mInfo.mAudio.mChannels,
      AudioCompactor::NativeCopy(
        bufferInfo.mBuffer->data() + bufferInfo.mOffset,
        bufferInfo.mSize,
        mInfo.mAudio.mChannels));
  }
  mAudioTrack.mCodec->releaseOutputBuffer(bufferInfo.mIndex);
}

void
MediaCodecReader::DecodeAudioDataTask()
{
  MOZ_ASSERT(mAudioTrack.mTaskQueue->IsCurrentThreadIn());
  MonitorAutoLock al(mAudioTrack.mTrackMonitor);
  if (mAudioTrack.mAudioPromise.IsEmpty()) {
    // Clear the data in queue because the promise might be canceled by
    // ResetDecode().
    AudioQueue().Reset();
    return;
  }
  if (AudioQueue().GetSize() == 0 && !AudioQueue().IsFinished()) {
    MonitorAutoUnlock ul(mAudioTrack.mTrackMonitor);
    DecodeAudioDataSync();
  }
  // Since we unlock the monitor above, we should check the promise again
  // because the promise might be canceled by ResetDecode().
  if (mAudioTrack.mAudioPromise.IsEmpty()) {
    AudioQueue().Reset();
    return;
  }
  if (AudioQueue().GetSize() > 0) {
    RefPtr<AudioData> a = AudioQueue().PopFront();
    if (a) {
      if (mAudioTrack.mDiscontinuity) {
        a->mDiscontinuity = true;
        mAudioTrack.mDiscontinuity = false;
      }
      mAudioTrack.mAudioPromise.Resolve(a, __func__);
    }
  } else if (AudioQueue().AtEndOfStream()) {
    mAudioTrack.mAudioPromise.Reject(END_OF_STREAM, __func__);
  } else if (AudioQueue().GetSize() == 0) {
    DispatchAudioTask();
  }
}

void
MediaCodecReader::DecodeVideoFrameTask(int64_t aTimeThreshold)
{
  MOZ_ASSERT(mVideoTrack.mTaskQueue->IsCurrentThreadIn());
  MonitorAutoLock al(mVideoTrack.mTrackMonitor);
  if (mVideoTrack.mVideoPromise.IsEmpty()) {
    // Clear the data in queue because the promise might be canceled by
    // ResetDecode().
    VideoQueue().Reset();
    return;
  }
  {
    MonitorAutoUnlock ul(mVideoTrack.mTrackMonitor);
    DecodeVideoFrameSync(aTimeThreshold);
  }
  // Since we unlock the monitor above, we should check the promise again
  // because the promise might be canceled by ResetDecode().
  if (mVideoTrack.mVideoPromise.IsEmpty()) {
    VideoQueue().Reset();
    return;
  }
  if (VideoQueue().GetSize() > 0) {
    RefPtr<VideoData> v = VideoQueue().PopFront();
    if (v) {
      if (mVideoTrack.mDiscontinuity) {
        v->mDiscontinuity = true;
        mVideoTrack.mDiscontinuity = false;
      }
      mVideoTrack.mVideoPromise.Resolve(v, __func__);
    }
  } else if (VideoQueue().AtEndOfStream()) {
    mVideoTrack.mVideoPromise.Reject(END_OF_STREAM, __func__);
  } else if (VideoQueue().GetSize() == 0) {
    DispatchVideoTask(aTimeThreshold);
  }
}

bool
MediaCodecReader::HasAudio()
{
  return mInfo.HasAudio();
}

bool
MediaCodecReader::HasVideo()
{
  return mInfo.HasVideo();
}

void
MediaCodecReader::NotifyDataArrivedInternal()
{
  AutoPinned<MediaResource> resource(mDecoder->GetResource());
  nsTArray<MediaByteRange> byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);

  if (NS_FAILED(rv)) {
    return;
  }

  IntervalSet<int64_t> intervals;
  for (auto& range : byteRanges) {
    intervals += mFilter.NotifyDataArrived(range.Length(), range.mStart);
  }
  for (const auto& interval : intervals) {
    RefPtr<MediaByteBuffer> bytes =
      resource->MediaReadAt(interval.mStart, interval.Length());
    MonitorAutoLock monLock(mParserMonitor);
    if (mNextParserPosition == mParsedDataLength &&
        mNextParserPosition >= interval.mStart &&
        mNextParserPosition <= interval.mEnd) {
      // No pending parsing runnable currently. And available data are adjacent to
      // parsed data.
      int64_t shift = mNextParserPosition - interval.mStart;
      const char* buffer = reinterpret_cast<const char*>(bytes->Elements()) + shift;
      uint32_t length = interval.Length() - shift;
      int64_t offset = mNextParserPosition;
      if (length > 0) {
        MonitorAutoUnlock monUnlock(mParserMonitor);
        ParseDataSegment(buffer, length, offset);
      }
      mParseDataFromCache = false;
      mParsedDataLength = offset + length;
      mNextParserPosition = mParsedDataLength;
    }
  }
}

int64_t
MediaCodecReader::ProcessCachedData(int64_t aOffset,
                                    RefPtr<SignalObject> aSignal)
{
  // We read data in chunks of 32 KiB. We can reduce this
  // value if media, such as sdcards, is too slow.
  // Because of SD card's slowness, need to keep sReadSize to small size.
  // See Bug 914870.
  static const int64_t sReadSize = 32 * 1024;

  MOZ_ASSERT(!NS_IsMainThread(), "Should not be on main thread.");

  {
    MonitorAutoLock monLock(mParserMonitor);
    if (!mParseDataFromCache) {
      // Skip cache processing since data can be continuously be parsed by
      // ParseDataSegment() from NotifyDataArrived() directly.
      return INT64_C(0);
    }
  }

  MediaResource *resource = mDecoder->GetResource();
  MOZ_ASSERT(resource);

  int64_t resourceLength = resource->GetCachedDataEnd(0);
  NS_ENSURE_TRUE(resourceLength >= 0, INT64_C(-1));

  if (aOffset >= resourceLength) {
    return INT64_C(0); // Cache is empty, nothing to do
  }

  int64_t bufferLength = std::min<int64_t>(resourceLength - aOffset, sReadSize);

  nsAutoArrayPtr<char> buffer(new char[bufferLength]);

  nsresult rv = resource->ReadFromCache(buffer.get(), aOffset, bufferLength);
  NS_ENSURE_SUCCESS(rv, INT64_C(-1));

  MonitorAutoLock monLock(mParserMonitor);
  if (mParseDataFromCache) {
    RefPtr<ParseCachedDataRunnable> runnable(
      new ParseCachedDataRunnable(this,
                                  buffer.forget(),
                                  bufferLength,
                                  aOffset,
                                  aSignal));

    rv = NS_DispatchToMainThread(runnable.get());
    NS_ENSURE_SUCCESS(rv, INT64_C(-1));

    mNextParserPosition = aOffset + bufferLength;
    if (mNextParserPosition < resource->GetCachedDataEnd(0)) {
      // We cannot read data in the main thread because it
      // might block for too long. Instead we post an IO task
      // to the IO thread if there is more data available.
      XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
          new ProcessCachedDataTask(this, mNextParserPosition));
    }
  }

  return bufferLength;
}

bool
MediaCodecReader::ParseDataSegment(const char* aBuffer,
                                   uint32_t aLength,
                                   int64_t aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  int64_t duration = INT64_C(-1);

  {
    MonitorAutoLock monLock(mParserMonitor);

    // currently only mp3 files are supported for incremental parsing
    if (mMP3FrameParser == nullptr) {
      return false;
    }

    if (!mMP3FrameParser->IsMP3()) {
      return true; // NO-OP
    }

    mMP3FrameParser->Parse(reinterpret_cast<const uint8_t*>(aBuffer), aLength, aOffset);

    duration = mMP3FrameParser->GetDuration();
  }

  bool durationUpdateRequired = false;

  {
    MonitorAutoLock al(mAudioTrack.mTrackMonitor);
    if (duration > mAudioTrack.mDurationUs) {
      mAudioTrack.mDurationUs = duration;
      durationUpdateRequired = true;
    }
  }

  if (durationUpdateRequired && HasVideo()) {
    MonitorAutoLock al(mVideoTrack.mTrackMonitor);
    durationUpdateRequired = duration > mVideoTrack.mDurationUs;
  }

  if (durationUpdateRequired) {
    MOZ_ASSERT(mDecoder);
    mDecoder->DispatchUpdateEstimatedMediaDuration(duration);
  }

  return true;
}

RefPtr<MediaDecoderReader::MetadataPromise>
MediaCodecReader::AsyncReadMetadata()
{
  MOZ_ASSERT(OnTaskQueue());

  if (!ReallocateExtractorResources()) {
    return MediaDecoderReader::MetadataPromise::CreateAndReject(
             ReadMetadataFailureReason::METADATA_ERROR, __func__);
  }

  bool incrementalParserNeeded =
    mDecoder->GetResource()->GetContentType().EqualsASCII(AUDIO_MP3);
  if (incrementalParserNeeded && !TriggerIncrementalParser()) {
    return MediaDecoderReader::MetadataPromise::CreateAndReject(
             ReadMetadataFailureReason::METADATA_ERROR, __func__);
  }

  RefPtr<MediaDecoderReader::MetadataPromise> p = mMetadataPromise.Ensure(__func__);

  RefPtr<MediaCodecReader> self = this;
  mMediaResourceRequest.Begin(CreateMediaCodecs()
    ->Then(OwnerThread(), __func__,
      [self] (bool) -> void {
        self->mMediaResourceRequest.Complete();
        self->HandleResourceAllocated();
      }, [self] (bool) -> void {
        self->mMediaResourceRequest.Complete();
        self->mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
      }));

  return p;
}

void
MediaCodecReader::HandleResourceAllocated()
{
  // Configure video codec after the codecReserved.
  if (mVideoTrack.mSource != nullptr) {
    if (!ConfigureMediaCodec(mVideoTrack)) {
      DestroyMediaCodec(mVideoTrack);
      mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
      return;
    }
  }

  // TODO: start streaming

  if (!UpdateDuration()) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    return;
  }

  if (!UpdateAudioInfo()) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    return;
  }

  if (!UpdateVideoInfo()) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    return;
  }

  // Set the total duration (the max of the audio and video track).
  int64_t audioDuration = INT64_C(-1);
  {
    MonitorAutoLock al(mAudioTrack.mTrackMonitor);
    audioDuration = mAudioTrack.mDurationUs;
  }
  int64_t videoDuration = INT64_C(-1);
  {
    MonitorAutoLock al(mVideoTrack.mTrackMonitor);
    videoDuration = mVideoTrack.mDurationUs;
  }
  int64_t duration = audioDuration > videoDuration ? audioDuration : videoDuration;
  if (duration >= INT64_C(0)) {
    mInfo.mMetadataDuration = Some(TimeUnit::FromMicroseconds(duration));
  }

  // Video track's frame sizes will not overflow. Activate the video track.
  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  if (container) {
    container->ClearCurrentFrame(
      gfx::IntSize(mInfo.mVideo.mDisplay.width, mInfo.mVideo.mDisplay.height));
  }

  RefPtr<MetadataHolder> metadata = new MetadataHolder();
  metadata->mInfo = mInfo;
  metadata->mTags = nullptr;

#ifdef MOZ_AUDIO_OFFLOAD
  CheckAudioOffload();
#endif

  mMetadataPromise.Resolve(metadata, __func__);
}

nsresult
MediaCodecReader::ResetDecode()
{
  if (CheckAudioResources()) {
    MonitorAutoLock al(mAudioTrack.mTrackMonitor);
    if (!mAudioTrack.mAudioPromise.IsEmpty()) {
      mAudioTrack.mAudioPromise.Reject(CANCELED, __func__);
    }
    FlushCodecData(mAudioTrack);
    mAudioTrack.mDiscontinuity = true;
  }
  if (CheckVideoResources()) {
    MonitorAutoLock al(mVideoTrack.mTrackMonitor);
    if (!mVideoTrack.mVideoPromise.IsEmpty()) {
      mVideoTrack.mVideoPromise.Reject(CANCELED, __func__);
    }
    FlushCodecData(mVideoTrack);
    mVideoTrack.mDiscontinuity = true;
  }

  return MediaDecoderReader::ResetDecode();
}

void
MediaCodecReader::TextureClientRecycleCallback(TextureClient* aClient,
                                               void* aClosure)
{
  RefPtr<MediaCodecReader> reader = static_cast<MediaCodecReader*>(aClosure);
  MOZ_ASSERT(reader, "reader should not be nullptr in TextureClientRecycleCallback()");

  reader->TextureClientRecycleCallback(aClient);
}

void
MediaCodecReader::TextureClientRecycleCallback(TextureClient* aClient)
{
  MOZ_ASSERT(aClient, "aClient should not be nullptr in RecycleCallback()");
  MOZ_ASSERT(!aClient->IsDead());
  size_t index = 0;

  {
    MutexAutoLock al(mTextureClientIndexesLock);

    aClient->ClearRecycleCallback();

    // aClient has been removed from mTextureClientIndexes by
    // ReleaseAllTextureClients() on another thread.
    if (!mTextureClientIndexes.Get(aClient, &index)) {
      return;
    }

    FenceHandle handle = aClient->GetAndResetReleaseFenceHandle();
    mPendingReleaseItems.AppendElement(ReleaseItem(index, handle));

    mTextureClientIndexes.Remove(aClient);
  }

  if (mVideoTrack.mReleaseBufferTaskQueue->IsEmpty()) {
    RefPtr<nsIRunnable> task =
      NS_NewRunnableMethod(this,
                           &MediaCodecReader::WaitFenceAndReleaseOutputBuffer);
    mVideoTrack.mReleaseBufferTaskQueue->Dispatch(task.forget());
  }
}

void
MediaCodecReader::WaitFenceAndReleaseOutputBuffer()
{
  nsTArray<ReleaseItem> releasingItems;
  {
    MutexAutoLock autoLock(mTextureClientIndexesLock);
    releasingItems.AppendElements(mPendingReleaseItems);
    mPendingReleaseItems.Clear();
  }

  for (size_t i = 0; i < releasingItems.Length(); i++) {
    if (releasingItems[i].mReleaseFence.IsValid()) {
#if MOZ_WIDGET_GONK && ANDROID_VERSION >= 17
      RefPtr<FenceHandle::FdObj> fdObj = releasingItems[i].mReleaseFence.GetAndResetFdObj();
      sp<Fence> fence = new Fence(fdObj->GetAndResetFd());
      fence->waitForever("MediaCodecReader");
#endif
    }
    if (mVideoTrack.mCodec != nullptr) {
      mVideoTrack.mCodec->releaseOutputBuffer(releasingItems[i].mReleaseIndex);
    }
  }
}

void
MediaCodecReader::ReleaseAllTextureClients()
{
  MutexAutoLock al(mTextureClientIndexesLock);
  MOZ_ASSERT(mTextureClientIndexes.Count(), "All TextureClients should be released already");

  if (mTextureClientIndexes.Count() == 0) {
    return;
  }
  printf_stderr("All TextureClients should be released already");

  for (auto iter = mTextureClientIndexes.Iter(); !iter.Done(); iter.Next()) {
    TextureClient* client = iter.Key();
    size_t& index = iter.Data();

    client->ClearRecycleCallback();

    if (mVideoTrack.mCodec != nullptr) {
      mVideoTrack.mCodec->releaseOutputBuffer(index);
    }
    iter.Remove();
  }
  mTextureClientIndexes.Clear();
}

void
MediaCodecReader::DecodeVideoFrameSync(int64_t aTimeThreshold)
{
  if (mVideoTrack.mCodec == nullptr || !mVideoTrack.mCodec->allocated() ||
      mVideoTrack.mOutputEndOfStream) {
    return;
  }

  // Get one video output data from MediaCodec
  CodecBufferInfo bufferInfo;
  status_t status;
  TimeStamp timeout = TimeStamp::Now() +
                      TimeDuration::FromSeconds(sMaxVideoDecodeDurationS);
  while (true) {
    // Try to fill more input buffers and then get one output buffer.
    // FIXME: use callback from MediaCodec
    FillCodecInputData(mVideoTrack);

    status = GetCodecOutputData(mVideoTrack, bufferInfo, aTimeThreshold,
                                timeout);
    if (status == OK || status == ERROR_END_OF_STREAM) {
      break;
    } else if (status == -EAGAIN) {
      if (TimeStamp::Now() > timeout) {
        // Don't let this loop run for too long. Try it again later.
        return;
      }
      continue; // Try it again now.
    } else if (status == INFO_FORMAT_CHANGED) {
      if (UpdateVideoInfo()) {
        continue; // Try it again now.
      } else {
        return;
      }
    } else {
      return;
    }
  }

  if ((bufferInfo.mFlags & MediaCodec::BUFFER_FLAG_EOS) ||
      (status == ERROR_END_OF_STREAM)) {
    VideoQueue().Finish();
    mVideoTrack.mCodec->releaseOutputBuffer(bufferInfo.mIndex);
    return;
  }

  RefPtr<VideoData> v;
  RefPtr<TextureClient> textureClient;
  sp<GraphicBuffer> graphicBuffer;
  if (bufferInfo.mBuffer != nullptr) {
    MOZ_ASSERT(mStreamSource);
    // This is the approximate byte position in the stream.
    int64_t pos = mStreamSource->Tell();

    if (mVideoTrack.mNativeWindow != nullptr &&
        mVideoTrack.mCodec->getOutputGraphicBufferFromIndex(bufferInfo.mIndex, &graphicBuffer) == OK &&
        graphicBuffer != nullptr) {
      textureClient = mVideoTrack.mNativeWindow->getTextureClientFromBuffer(graphicBuffer.get());
      v = VideoData::Create(mInfo.mVideo,
                            mDecoder->GetImageContainer(),
                            pos,
                            bufferInfo.mTimeUs,
                            1, // We don't know the duration.
                            textureClient,
                            bufferInfo.mFlags & MediaCodec::BUFFER_FLAG_SYNCFRAME,
                            -1,
                            mVideoTrack.mRelativePictureRect);
    } else if (bufferInfo.mSize > 0 &&
        bufferInfo.mBuffer->data() != nullptr) {
      uint8_t* yuv420p_buffer = bufferInfo.mBuffer->data();
      int32_t stride = mVideoTrack.mStride;
      int32_t slice_height = mVideoTrack.mSliceHeight;

      // Converts to OMX_COLOR_FormatYUV420Planar
      if (mVideoTrack.mColorFormat != OMX_COLOR_FormatYUV420Planar) {
        ARect crop;
        crop.top = 0;
        crop.bottom = mVideoTrack.mHeight;
        crop.left = 0;
        crop.right = mVideoTrack.mWidth;

        yuv420p_buffer = GetColorConverterBuffer(mVideoTrack.mWidth,
                                                 mVideoTrack.mHeight);
        if (mColorConverter.convertDecoderOutputToI420(
              bufferInfo.mBuffer->data(), mVideoTrack.mWidth, mVideoTrack.mHeight,
              crop, yuv420p_buffer) != OK) {
          mVideoTrack.mCodec->releaseOutputBuffer(bufferInfo.mIndex);
          NS_WARNING("Unable to convert color format");
          return;
        }

        stride = mVideoTrack.mWidth;
        slice_height = mVideoTrack.mHeight;
      }

      size_t yuv420p_y_size = stride * slice_height;
      size_t yuv420p_u_size = ((stride + 1) / 2) * ((slice_height + 1) / 2);
      uint8_t* yuv420p_y = yuv420p_buffer;
      uint8_t* yuv420p_u = yuv420p_y + yuv420p_y_size;
      uint8_t* yuv420p_v = yuv420p_u + yuv420p_u_size;

      VideoData::YCbCrBuffer b;
      b.mPlanes[0].mData = yuv420p_y;
      b.mPlanes[0].mWidth = mVideoTrack.mWidth;
      b.mPlanes[0].mHeight = mVideoTrack.mHeight;
      b.mPlanes[0].mStride = stride;
      b.mPlanes[0].mOffset = 0;
      b.mPlanes[0].mSkip = 0;

      b.mPlanes[1].mData = yuv420p_u;
      b.mPlanes[1].mWidth = (mVideoTrack.mWidth + 1) / 2;
      b.mPlanes[1].mHeight = (mVideoTrack.mHeight + 1) / 2;
      b.mPlanes[1].mStride = (stride + 1) / 2;
      b.mPlanes[1].mOffset = 0;
      b.mPlanes[1].mSkip = 0;

      b.mPlanes[2].mData = yuv420p_v;
      b.mPlanes[2].mWidth =(mVideoTrack.mWidth + 1) / 2;
      b.mPlanes[2].mHeight = (mVideoTrack.mHeight + 1) / 2;
      b.mPlanes[2].mStride = (stride + 1) / 2;
      b.mPlanes[2].mOffset = 0;
      b.mPlanes[2].mSkip = 0;

      v = VideoData::Create(mInfo.mVideo,
                            mDecoder->GetImageContainer(),
                            pos,
                            bufferInfo.mTimeUs,
                            1, // We don't know the duration.
                            b,
                            bufferInfo.mFlags & MediaCodec::BUFFER_FLAG_SYNCFRAME,
                            -1,
                            mVideoTrack.mRelativePictureRect);
    }

    if (v) {
      // Notify mDecoder that we have decoded a video frame.
      mDecoder->NotifyDecodedFrames(0, 1, 0);
      VideoQueue().Push(v);
    } else {
      NS_WARNING("Unable to create VideoData");
    }
  }

  if (v != nullptr && textureClient != nullptr && graphicBuffer != nullptr) {
    MutexAutoLock al(mTextureClientIndexesLock);
    mTextureClientIndexes.Put(textureClient.get(), bufferInfo.mIndex);
    textureClient->SetRecycleCallback(MediaCodecReader::TextureClientRecycleCallback, this);
  } else {
    mVideoTrack.mCodec->releaseOutputBuffer(bufferInfo.mIndex);
  }
}

RefPtr<MediaDecoderReader::SeekPromise>
MediaCodecReader::Seek(int64_t aTime, int64_t aEndTime)
{
  MOZ_ASSERT(OnTaskQueue());

  int64_t timestamp = sInvalidTimestampUs;

  if (CheckVideoResources()) {
    MonitorAutoLock al(mVideoTrack.mTrackMonitor);
    mVideoTrack.mSeekTimeUs = aTime;
    mVideoTrack.mInputEndOfStream = false;
    mVideoTrack.mOutputEndOfStream = false;
    mVideoTrack.mFlushed = false;

    MediaBuffer* source_buffer = nullptr;
    MediaSource::ReadOptions options;
    options.setSeekTo(aTime, MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC);
    if (mVideoTrack.mSource->read(&source_buffer, &options) != OK ||
        source_buffer == nullptr) {
      return SeekPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }
    sp<MetaData> format = source_buffer->meta_data();
    if (format != nullptr) {
      if (format->findInt64(kKeyTime, &timestamp) &&
          IsValidTimestampUs(timestamp)) {
        mVideoTrack.mSeekTimeUs = timestamp;
      }
      format = nullptr;
    }
    source_buffer->release();
  }

  {
    MonitorAutoLock al(mAudioTrack.mTrackMonitor);
    mAudioTrack.mInputEndOfStream = false;
    mAudioTrack.mOutputEndOfStream = false;
    mAudioTrack.mFlushed = false;

    if (IsValidTimestampUs(timestamp)) {
      mAudioTrack.mSeekTimeUs = timestamp;
    } else {
      mAudioTrack.mSeekTimeUs = aTime;
    }
  }

  return SeekPromise::CreateAndResolve(aTime, __func__);
}

bool
MediaCodecReader::IsMediaSeekable()
{
  // Check the MediaExtract flag if the source is seekable.
  return (mExtractor != nullptr) &&
         (mExtractor->flags() & MediaExtractor::CAN_SEEK);
}

sp<MediaSource>
MediaCodecReader::GetAudioOffloadTrack()
{
  return mAudioOffloadTrack.mSource;
}

bool
MediaCodecReader::ReallocateExtractorResources()
{
  if (CreateLooper() &&
      CreateExtractor() &&
      CreateMediaSources() &&
      CreateTaskQueues()) {
    return true;
  }

  ReleaseResources();
  return false;
}

void
MediaCodecReader::ReleaseCriticalResources()
{
  mMediaResourceRequest.DisconnectIfExists();
  mMediaResourcePromise.RejectIfExists(true, __func__);
  mMetadataPromise.RejectIfExists(ReadMetadataFailureReason::METADATA_ERROR, __func__);
  mVideoCodecRequest.DisconnectIfExists();

  ResetDecode();
  // Before freeing a video codec, all video buffers needed to be released
  // even from graphics pipeline.
  VideoFrameContainer* videoframe = mDecoder->GetVideoFrameContainer();
  if (videoframe) {
    videoframe->ClearCurrentFrame();
  }
  ReleaseAllTextureClients();

  DestroyMediaCodecs();

  ClearColorConverterBuffer();
}

void
MediaCodecReader::ReleaseResources()
{
  ReleaseCriticalResources();
  DestroyMediaSources();
  DestroyExtractor();
  DestroyLooper();
  ShutdownTaskQueues();
}

bool
MediaCodecReader::CreateLooper()
{
  if (mLooper != nullptr) {
    return true;
  }

  // Create ALooper
  sp<ALooper> looper = new ALooper;
  looper->setName("MediaCodecReader::mLooper");

  // Start ALooper thread.
  if (looper->start() != OK) {
    return false;
  }

  mLooper = looper;

  return true;
}

void
MediaCodecReader::DestroyLooper()
{
  if (mLooper == nullptr) {
    return;
  }

  // Stop ALooper thread.
  mLooper->stop();

  // Clear ALooper
  mLooper = nullptr;
}

bool
MediaCodecReader::CreateExtractor()
{
  if (mExtractor != nullptr) {
    return true;
  }

  //register sniffers, if they are not registered in this process.
  DataSource::RegisterDefaultSniffers();

  if (mExtractor == nullptr) {
    sp<DataSource> dataSource = new MediaStreamSource(mDecoder->GetResource());

    if (dataSource->initCheck() != OK) {
      return false;
    }
    mStreamSource = static_cast<MediaStreamSource*>(dataSource.get());
    mExtractor = MediaExtractor::Create(dataSource);
  }

  return mExtractor != nullptr;
}

void
MediaCodecReader::DestroyExtractor()
{
  mExtractor = nullptr;
}

bool
MediaCodecReader::CreateMediaSources()
{
  if (mExtractor == nullptr) {
    return false;
  }

  mMetaData = mExtractor->getMetaData();

  const ssize_t invalidTrackIndex = -1;
  ssize_t audioTrackIndex = invalidTrackIndex;
  ssize_t videoTrackIndex = invalidTrackIndex;

  for (size_t i = 0; i < mExtractor->countTracks(); ++i) {
    sp<MetaData> trackFormat = mExtractor->getTrackMetaData(i);

    const char* mime;
    if (!trackFormat->findCString(kKeyMIMEType, &mime)) {
      continue;
    }

    if (audioTrackIndex == invalidTrackIndex &&
        !strncasecmp(mime, "audio/", 6)) {
      audioTrackIndex = i;
    } else if (videoTrackIndex == invalidTrackIndex &&
               !strncasecmp(mime, "video/", 6)) {
      videoTrackIndex = i;
    }
  }

  if (audioTrackIndex == invalidTrackIndex &&
      videoTrackIndex == invalidTrackIndex) {
    NS_WARNING("OMX decoder could not find audio or video tracks");
    return false;
  }

  if (audioTrackIndex != invalidTrackIndex && mAudioTrack.mSource == nullptr) {
    sp<MediaSource> audioSource = mExtractor->getTrack(audioTrackIndex);
    if (audioSource != nullptr && audioSource->start() == OK) {
      mAudioTrack.mSource = audioSource;
      mAudioTrack.mSourceIsStopped = false;
    }
    // Get one another track instance for audio offload playback.
    mAudioOffloadTrack.mSource = mExtractor->getTrack(audioTrackIndex);
  }

  if (videoTrackIndex != invalidTrackIndex && mVideoTrack.mSource == nullptr &&
      mDecoder->GetImageContainer()) {
    sp<MediaSource> videoSource = mExtractor->getTrack(videoTrackIndex);
    if (videoSource != nullptr && videoSource->start() == OK) {
      mVideoTrack.mSource = videoSource;
      mVideoTrack.mSourceIsStopped = false;
    }
  }

  return
    (audioTrackIndex == invalidTrackIndex || mAudioTrack.mSource != nullptr) &&
    (videoTrackIndex == invalidTrackIndex || mVideoTrack.mSource != nullptr);
}

void
MediaCodecReader::DestroyMediaSources()
{
  mAudioTrack.mSource = nullptr;
  mVideoTrack.mSource = nullptr;
#if ANDROID_VERSION >= 21
  mAudioOffloadTrack.mSource = nullptr;
#endif
}

void
MediaCodecReader::ShutdownTaskQueues()
{
  if (mAudioTrack.mTaskQueue) {
    mAudioTrack.mTaskQueue->BeginShutdown();
    mAudioTrack.mTaskQueue->AwaitShutdownAndIdle();
    mAudioTrack.mTaskQueue = nullptr;
  }
  if (mVideoTrack.mTaskQueue) {
    mVideoTrack.mTaskQueue->BeginShutdown();
    mVideoTrack.mTaskQueue->AwaitShutdownAndIdle();
    mVideoTrack.mTaskQueue = nullptr;
  }
  if (mVideoTrack.mReleaseBufferTaskQueue) {
    mVideoTrack.mReleaseBufferTaskQueue->BeginShutdown();
    mVideoTrack.mReleaseBufferTaskQueue->AwaitShutdownAndIdle();
    mVideoTrack.mReleaseBufferTaskQueue = nullptr;
  }
}

bool
MediaCodecReader::CreateTaskQueues()
{
  if (mAudioTrack.mSource != nullptr && !mAudioTrack.mTaskQueue) {
    mAudioTrack.mTaskQueue = CreateMediaDecodeTaskQueue();
    NS_ENSURE_TRUE(mAudioTrack.mTaskQueue, false);
  }
  if (mVideoTrack.mSource != nullptr && !mVideoTrack.mTaskQueue) {
    mVideoTrack.mTaskQueue = CreateMediaDecodeTaskQueue();
    NS_ENSURE_TRUE(mVideoTrack.mTaskQueue, false);
    mVideoTrack.mReleaseBufferTaskQueue = CreateMediaDecodeTaskQueue();
    NS_ENSURE_TRUE(mVideoTrack.mReleaseBufferTaskQueue, false);
  }

  return true;
}

RefPtr<MediaOmxCommonReader::MediaResourcePromise>
MediaCodecReader::CreateMediaCodecs()
{
  bool isWaiting = false;
  RefPtr<MediaResourcePromise> p = mMediaResourcePromise.Ensure(__func__);

  if (!CreateMediaCodec(mLooper, mAudioTrack, isWaiting)) {
    mMediaResourcePromise.Reject(true, __func__);
    return p;
  }

  if (!CreateMediaCodec(mLooper, mVideoTrack, isWaiting)) {
    mMediaResourcePromise.Reject(true, __func__);
    return p;
  }

  if (!isWaiting) {
    // No MediaCodec allocation wait.
    mMediaResourcePromise.Resolve(true, __func__);
  }

  return p;
}

bool
MediaCodecReader::CreateMediaCodec(sp<ALooper>& aLooper,
                                   Track& aTrack,
                                   bool& aIsWaiting)
{
  if (aTrack.mSource != nullptr && aTrack.mCodec == nullptr) {
    sp<MetaData> sourceFormat = aTrack.mSource->getFormat();

    const char* mime;
    if (sourceFormat->findCString(kKeyMIMEType, &mime)) {
      aTrack.mCodec = MediaCodecProxy::CreateByType(aLooper, mime, false);
    }

    if (aTrack.mCodec == nullptr) {
      NS_WARNING("Couldn't create MediaCodecProxy");
      return false;
    }

    if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_VORBIS)) {
      aTrack.mInputCopier = new VorbisInputCopier;
    } else {
      aTrack.mInputCopier = new TrackInputCopier;
    }

    uint32_t capability = MediaCodecProxy::kEmptyCapability;
    if (aTrack.mType == Track::kVideo &&
        aTrack.mCodec->getCapability(&capability) == OK &&
        (capability & MediaCodecProxy::kCanExposeGraphicBuffer) == MediaCodecProxy::kCanExposeGraphicBuffer) {
#if ANDROID_VERSION >= 21
      android::sp<android::IGraphicBufferProducer> producer;
      android::sp<android::IGonkGraphicBufferConsumer> consumer;
      GonkBufferQueue::createBufferQueue(&producer, &consumer);
      aTrack.mNativeWindow = new GonkNativeWindow(consumer);
      aTrack.mGraphicBufferProducer = producer;
#else
      aTrack.mNativeWindow = new GonkNativeWindow();
#endif
    }

    if (aTrack.mType == Track::kAudio && aTrack.mCodec->AllocateAudioMediaCodec()) {
      // Pending configure() and start() to codecReserved() if the creation
      // should be asynchronous.
      if (!aTrack.mCodec->allocated() || !ConfigureMediaCodec(aTrack)){
        NS_WARNING("Couldn't create and configure MediaCodec synchronously");
        DestroyMediaCodec(aTrack);
        return false;
      }
    } else if (aTrack.mType == Track::kVideo) {
      aIsWaiting = true;
      RefPtr<MediaCodecReader> self = this;
      mVideoCodecRequest.Begin(aTrack.mCodec->AsyncAllocateVideoMediaCodec()
        ->Then(OwnerThread(), __func__,
          [self] (bool) -> void {
            self->mVideoCodecRequest.Complete();
            self->mMediaResourcePromise.ResolveIfExists(true, __func__);
          }, [self] (bool) -> void {
            self->mVideoCodecRequest.Complete();
            self->mMediaResourcePromise.RejectIfExists(true, __func__);
          }));
    }
  }

  return true;
}

bool
MediaCodecReader::ConfigureMediaCodec(Track& aTrack)
{
  if (aTrack.mSource != nullptr && aTrack.mCodec != nullptr) {
    if (!aTrack.mCodec->allocated()) {
      return false;
    }

    sp<Surface> surface;
    if (aTrack.mNativeWindow != nullptr) {
#if ANDROID_VERSION >= 21
      surface = new Surface(aTrack.mGraphicBufferProducer);
#else
      surface = new Surface(aTrack.mNativeWindow->getBufferQueue());
#endif
    }

    sp<MetaData> sourceFormat = aTrack.mSource->getFormat();
    sp<AMessage> codecFormat;
    convertMetaDataToMessage(sourceFormat, &codecFormat);

    bool allpass = true;
    if (allpass && aTrack.mCodec->configure(codecFormat, surface, nullptr, 0) != OK) {
      NS_WARNING("Couldn't configure MediaCodec");
      allpass = false;
    }
    if (allpass && aTrack.mCodec->start() != OK) {
      NS_WARNING("Couldn't start MediaCodec");
      allpass = false;
    }
    if (allpass && aTrack.mCodec->getInputBuffers(&aTrack.mInputBuffers) != OK) {
      NS_WARNING("Couldn't get input buffers from MediaCodec");
      allpass = false;
    }
    if (allpass && aTrack.mCodec->getOutputBuffers(&aTrack.mOutputBuffers) != OK) {
      NS_WARNING("Couldn't get output buffers from MediaCodec");
      allpass = false;
    }
    if (!allpass) {
      DestroyMediaCodec(aTrack);
      return false;
    }
  }

  return true;
}

void
MediaCodecReader::DestroyMediaCodecs()
{
  DestroyMediaCodec(mAudioTrack);
  DestroyMediaCodec(mVideoTrack);
}

void
MediaCodecReader::DestroyMediaCodec(Track& aTrack)
{
  aTrack.mCodec = nullptr;
  aTrack.mNativeWindow = nullptr;
#if ANDROID_VERSION >= 21
  aTrack.mGraphicBufferProducer = nullptr;
#endif
}

bool
MediaCodecReader::TriggerIncrementalParser()
{
  if (mMetaData == nullptr) {
    return false;
  }

  int64_t duration = INT64_C(-1);

  {
    MonitorAutoLock monLock(mParserMonitor);

    // only support incremental parsing for mp3 currently.
    if (mMP3FrameParser != nullptr) {
      return true;
    }

    mParseDataFromCache = true;
    mNextParserPosition = INT64_C(0);
    mParsedDataLength = INT64_C(0);

    // MP3 file duration
    const char* mime = nullptr;
    if (mMetaData->findCString(kKeyMIMEType, &mime) &&
        !strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_MPEG)) {
      mMP3FrameParser = new MP3FrameParser(mDecoder->GetResource()->GetLength());
      {
        MonitorAutoUnlock monUnlock(mParserMonitor);
        // trigger parsing logic and wait for finishing parsing data in the beginning.
        RefPtr<SignalObject> signalObject = new SignalObject("MediaCodecReader::UpdateDuration()");
        if (ProcessCachedData(INT64_C(0), signalObject) > INT64_C(0)) {
          signalObject->Wait();
        }
      }
      duration = mMP3FrameParser->GetDuration();
    }
  }

  {
    MonitorAutoLock al(mAudioTrack.mTrackMonitor);
    if (duration > mAudioTrack.mDurationUs) {
      mAudioTrack.mDurationUs = duration;
    }
  }

  return true;
}

bool
MediaCodecReader::UpdateDuration()
{
  // read audio duration
  if (mAudioTrack.mSource != nullptr) {
    sp<MetaData> audioFormat = mAudioTrack.mSource->getFormat();
    if (audioFormat != nullptr) {
      int64_t duration = INT64_C(0);
      if (audioFormat->findInt64(kKeyDuration, &duration)) {
        MonitorAutoLock al(mAudioTrack.mTrackMonitor);
        if (duration > mAudioTrack.mDurationUs) {
          mAudioTrack.mDurationUs = duration;
        }
      }
    }
  }

  // read video duration
  if (mVideoTrack.mSource != nullptr) {
    sp<MetaData> videoFormat = mVideoTrack.mSource->getFormat();
    if (videoFormat != nullptr) {
      int64_t duration = INT64_C(0);
      if (videoFormat->findInt64(kKeyDuration, &duration)) {
        MonitorAutoLock al(mVideoTrack.mTrackMonitor);
        if (duration > mVideoTrack.mDurationUs) {
          mVideoTrack.mDurationUs = duration;
        }
      }
    }
  }

  return true;
}

bool
MediaCodecReader::UpdateAudioInfo()
{
  if (mAudioTrack.mSource == nullptr && mAudioTrack.mCodec == nullptr) {
    // No needs to update AudioInfo if no audio streams.
    return true;
  }

  if (mAudioTrack.mSource == nullptr || mAudioTrack.mCodec == nullptr ||
      !mAudioTrack.mCodec->allocated()) {
    // Something wrong.
    MOZ_ASSERT(mAudioTrack.mSource != nullptr, "mAudioTrack.mSource should not be nullptr");
    MOZ_ASSERT(mAudioTrack.mCodec != nullptr, "mAudioTrack.mCodec should not be nullptr");
    MOZ_ASSERT(mAudioTrack.mCodec->allocated(), "mAudioTrack.mCodec->allocated() should not be false");
    return false;
  }

  // read audio metadata from MediaSource
  sp<MetaData> audioSourceFormat = mAudioTrack.mSource->getFormat();
  if (audioSourceFormat == nullptr) {
    return false;
  }

  // ensure audio metadata from MediaCodec has been parsed
  if (!EnsureCodecFormatParsed(mAudioTrack)){
    return false;
  }

  // read audio metadata from MediaCodec
  sp<AMessage> audioCodecFormat;
  if (mAudioTrack.mCodec->getOutputFormat(&audioCodecFormat) != OK ||
      audioCodecFormat == nullptr) {
    return false;
  }

  AString codec_mime;
  int32_t codec_channel_count = 0;
  int32_t codec_sample_rate = 0;
  if (!audioCodecFormat->findString("mime", &codec_mime) ||
      !audioCodecFormat->findInt32("channel-count", &codec_channel_count) ||
      !audioCodecFormat->findInt32("sample-rate", &codec_sample_rate)) {
    return false;
  }

  // Update AudioInfo
  mInfo.mAudio.mChannels = codec_channel_count;
  mInfo.mAudio.mRate = codec_sample_rate;

  return true;
}

bool
MediaCodecReader::UpdateVideoInfo()
{
  if (mVideoTrack.mSource == nullptr && mVideoTrack.mCodec == nullptr) {
    // No needs to update VideoInfo if no video streams.
    return true;
  }

  if (mVideoTrack.mSource == nullptr || mVideoTrack.mCodec == nullptr ||
      !mVideoTrack.mCodec->allocated()) {
    // Something wrong.
    MOZ_ASSERT(mVideoTrack.mSource != nullptr, "mVideoTrack.mSource should not be nullptr");
    MOZ_ASSERT(mVideoTrack.mCodec != nullptr, "mVideoTrack.mCodec should not be nullptr");
    MOZ_ASSERT(mVideoTrack.mCodec->allocated(), "mVideoTrack.mCodec->allocated() should not be false");
    return false;
  }

  // read video metadata from MediaSource
  sp<MetaData> videoSourceFormat = mVideoTrack.mSource->getFormat();
  if (videoSourceFormat == nullptr) {
    return false;
  }
  int32_t container_width = 0;
  int32_t container_height = 0;
  int32_t container_rotation = 0;
  if (!videoSourceFormat->findInt32(kKeyWidth, &container_width) ||
      !videoSourceFormat->findInt32(kKeyHeight, &container_height)) {
    return false;
  }
  mVideoTrack.mFrameSize = nsIntSize(container_width, container_height);
  if (videoSourceFormat->findInt32(kKeyRotation, &container_rotation)) {
    mVideoTrack.mRotation = container_rotation;
  }

  // ensure video metadata from MediaCodec has been parsed
  if (!EnsureCodecFormatParsed(mVideoTrack)){
    return false;
  }

  // read video metadata from MediaCodec
  sp<AMessage> videoCodecFormat;
  if (mVideoTrack.mCodec->getOutputFormat(&videoCodecFormat) != OK ||
      videoCodecFormat == nullptr) {
    return false;
  }
  AString codec_mime;
  int32_t codec_width = 0;
  int32_t codec_height = 0;
  int32_t codec_stride = 0;
  int32_t codec_slice_height = 0;
  int32_t codec_color_format = 0;
  int32_t codec_crop_left = 0;
  int32_t codec_crop_top = 0;
  int32_t codec_crop_right = 0;
  int32_t codec_crop_bottom = 0;
  if (!videoCodecFormat->findString("mime", &codec_mime) ||
      !videoCodecFormat->findInt32("width", &codec_width) ||
      !videoCodecFormat->findInt32("height", &codec_height) ||
      !videoCodecFormat->findInt32("stride", &codec_stride) ||
      !videoCodecFormat->findInt32("slice-height", &codec_slice_height) ||
      !videoCodecFormat->findInt32("color-format", &codec_color_format) ||
      !videoCodecFormat->findRect("crop", &codec_crop_left, &codec_crop_top,
                                  &codec_crop_right, &codec_crop_bottom)) {
    return false;
  }

  mVideoTrack.mWidth = codec_width;
  mVideoTrack.mHeight = codec_height;
  mVideoTrack.mStride = codec_stride;
  mVideoTrack.mSliceHeight = codec_slice_height;
  mVideoTrack.mColorFormat = codec_color_format;

  // Validate the container-reported frame and pictureRect sizes. This ensures
  // that our video frame creation code doesn't overflow.
  int32_t display_width = codec_crop_right - codec_crop_left + 1;
  int32_t display_height = codec_crop_bottom - codec_crop_top + 1;
  nsIntRect picture_rect(0, 0, mVideoTrack.mWidth, mVideoTrack.mHeight);
  nsIntSize display_size(display_width, display_height);
  if (!IsValidVideoRegion(mVideoTrack.mFrameSize, picture_rect, display_size)) {
    return false;
  }

  // Relative picture size
  gfx::IntRect relative_picture_rect = picture_rect;
  if (mVideoTrack.mWidth != mVideoTrack.mFrameSize.width ||
      mVideoTrack.mHeight != mVideoTrack.mFrameSize.height) {
    // Frame size is different from what the container reports. This is legal,
    // and we will preserve the ratio of the crop rectangle as it
    // was reported relative to the picture size reported by the container.
    relative_picture_rect.x = (picture_rect.x * mVideoTrack.mWidth) /
                              mVideoTrack.mFrameSize.width;
    relative_picture_rect.y = (picture_rect.y * mVideoTrack.mHeight) /
                              mVideoTrack.mFrameSize.height;
    relative_picture_rect.width = (picture_rect.width * mVideoTrack.mWidth) /
                                  mVideoTrack.mFrameSize.width;
    relative_picture_rect.height = (picture_rect.height * mVideoTrack.mHeight) /
                                   mVideoTrack.mFrameSize.height;
  }

  // Update VideoInfo
  mVideoTrack.mPictureRect = picture_rect;
  mInfo.mVideo.mDisplay = display_size;
  mVideoTrack.mRelativePictureRect = relative_picture_rect;

  return true;
}

status_t
MediaCodecReader::FlushCodecData(Track& aTrack)
{
  if (aTrack.mType == Track::kVideo) {
    // TODO: if we do release TextureClient on a separate thread in the future,
    // we will have to explicitly cleanup TextureClients which have been
    // recycled through TextureClient::mRecycleCallback.
    // Just NO-OP for now.
  }

  if (aTrack.mSource == nullptr || aTrack.mCodec == nullptr ||
      !aTrack.mCodec->allocated()) {
    return UNKNOWN_ERROR;
  }

  status_t status = aTrack.mCodec->flush();
  aTrack.mFlushed = (status == OK);
  if (aTrack.mFlushed) {
    aTrack.mInputIndex = sInvalidInputIndex;
  }

  return status;
}

// Keep filling data if there are available buffers.
// FIXME: change to non-blocking read
status_t
MediaCodecReader::FillCodecInputData(Track& aTrack)
{
  if (aTrack.mSource == nullptr || aTrack.mCodec == nullptr ||
      !aTrack.mCodec->allocated()) {
    return UNKNOWN_ERROR;
  }

  if (aTrack.mInputEndOfStream) {
    return ERROR_END_OF_STREAM;
  }

  if (IsValidTimestampUs(aTrack.mSeekTimeUs) && !aTrack.mFlushed) {
    FlushCodecData(aTrack);
  }

  size_t index = 0;
  while (aTrack.mInputIndex.isValid() ||
         aTrack.mCodec->dequeueInputBuffer(&index) == OK) {
    if (!aTrack.mInputIndex.isValid()) {
      aTrack.mInputIndex = index;
    }
    MOZ_ASSERT(aTrack.mInputIndex.isValid(), "aElement.mInputIndex should be valid");

    // Start the mSource before we read it.
    if (aTrack.mSourceIsStopped) {
      if (aTrack.mSource->start() == OK) {
        aTrack.mSourceIsStopped = false;
      } else {
        return UNKNOWN_ERROR;
      }
    }
    MediaBuffer* source_buffer = nullptr;
    status_t status = OK;
    if (IsValidTimestampUs(aTrack.mSeekTimeUs)) {
      MediaSource::ReadOptions options;
      options.setSeekTo(aTrack.mSeekTimeUs);
      status = aTrack.mSource->read(&source_buffer, &options);
    } else {
      status = aTrack.mSource->read(&source_buffer);
    }

    // read() fails
    if (status == INFO_FORMAT_CHANGED) {
      return INFO_FORMAT_CHANGED;
    } else if (status == ERROR_END_OF_STREAM) {
      aTrack.mInputEndOfStream = true;
      aTrack.mCodec->queueInputBuffer(aTrack.mInputIndex.value(),
                                      0, 0, 0,
                                      MediaCodec::BUFFER_FLAG_EOS);
      return ERROR_END_OF_STREAM;
    } else if (status == -ETIMEDOUT) {
      return OK; // try it later
    } else if (status != OK) {
      return status;
    } else if (source_buffer == nullptr) {
      return UNKNOWN_ERROR;
    }

    // read() successes
    aTrack.mInputEndOfStream = false;
    aTrack.mSeekTimeUs = sInvalidTimestampUs;

    sp<ABuffer> input_buffer = nullptr;
    if (aTrack.mInputIndex.value() < aTrack.mInputBuffers.size()) {
      input_buffer = aTrack.mInputBuffers[aTrack.mInputIndex.value()];
    }
    if (input_buffer != nullptr &&
        aTrack.mInputCopier != nullptr &&
        aTrack.mInputCopier->Copy(source_buffer, input_buffer)) {
      int64_t timestamp = sInvalidTimestampUs;
      sp<MetaData> codec_format = source_buffer->meta_data();
      if (codec_format != nullptr) {
        codec_format->findInt64(kKeyTime, &timestamp);
      }

      status = aTrack.mCodec->queueInputBuffer(
        aTrack.mInputIndex.value(), input_buffer->offset(),
        input_buffer->size(), timestamp, 0);
      if (status == OK) {
        aTrack.mInputIndex = sInvalidInputIndex;
      }
    }
    source_buffer->release();

    if (status != OK) {
      return status;
    }
  }

  return OK;
}

status_t
MediaCodecReader::GetCodecOutputData(Track& aTrack,
                                     CodecBufferInfo& aBuffer,
                                     int64_t aThreshold,
                                     const TimeStamp& aTimeout)
{
  // Read next frame.
  CodecBufferInfo info;
  status_t status = OK;
  while (status == OK || status == INFO_OUTPUT_BUFFERS_CHANGED ||
         status == -EAGAIN) {

    int64_t duration = (int64_t)(aTimeout - TimeStamp::Now()).ToMicroseconds();
    if (!IsValidDurationUs(duration)) {
      return -EAGAIN;
    }

    status = aTrack.mCodec->dequeueOutputBuffer(&info.mIndex, &info.mOffset,
      &info.mSize, &info.mTimeUs, &info.mFlags, duration);
    // Check EOS first.
    if (status == ERROR_END_OF_STREAM ||
        (info.mFlags & MediaCodec::BUFFER_FLAG_EOS)) {
      aBuffer = info;
      aBuffer.mBuffer = aTrack.mOutputBuffers[info.mIndex];
      aTrack.mOutputEndOfStream = true;
      return ERROR_END_OF_STREAM;
    }

    if (status == OK) {
      // Notify mDecoder that we have parsed a video frame.
      if (aTrack.mType == Track::kVideo) {
        mDecoder->NotifyDecodedFrames(1, 0, 0);
      }
      if (!IsValidTimestampUs(aThreshold) || info.mTimeUs >= aThreshold) {
        // Get a valid output buffer.
        break;
      } else {
        aTrack.mCodec->releaseOutputBuffer(info.mIndex);
      }
    } else if (status == INFO_OUTPUT_BUFFERS_CHANGED) {
      // Update output buffers of MediaCodec.
      if (aTrack.mCodec->getOutputBuffers(&aTrack.mOutputBuffers) != OK) {
        NS_WARNING("Couldn't get output buffers from MediaCodec");
        return UNKNOWN_ERROR;
      }
    }

    if (TimeStamp::Now() > aTimeout) {
      // Don't let this loop run for too long. Try it again later.
      return -EAGAIN;
    }
  }

  if (status != OK) {
    // Something wrong.
    return status;
  }

  if (info.mIndex >= aTrack.mOutputBuffers.size()) {
    NS_WARNING("Couldn't get proper index of output buffers from MediaCodec");
    aTrack.mCodec->releaseOutputBuffer(info.mIndex);
    return UNKNOWN_ERROR;
  }

  aBuffer = info;
  aBuffer.mBuffer = aTrack.mOutputBuffers[info.mIndex];

  return OK;
}

bool
MediaCodecReader::EnsureCodecFormatParsed(Track& aTrack)
{
  if (aTrack.mSource == nullptr || aTrack.mCodec == nullptr ||
      !aTrack.mCodec->allocated()) {
    return false;
  }

  sp<AMessage> format;
  if (aTrack.mCodec->getOutputFormat(&format) == OK) {
    return true;
  }

  status_t status = OK;
  size_t index = 0;
  size_t offset = 0;
  size_t size = 0;
  int64_t timeUs = INT64_C(0);
  uint32_t flags = 0;
  FillCodecInputData(aTrack);
  while ((status = aTrack.mCodec->dequeueOutputBuffer(&index, &offset, &size,
                     &timeUs, &flags)) != INFO_FORMAT_CHANGED) {
    if (status == OK) {
      aTrack.mCodec->releaseOutputBuffer(index);
    } else if (status == INFO_OUTPUT_BUFFERS_CHANGED) {
      // Update output buffers of MediaCodec.
      if (aTrack.mCodec->getOutputBuffers(&aTrack.mOutputBuffers) != OK) {
        NS_WARNING("Couldn't get output buffers from MediaCodec");
        return false;
      }
    } else if (status != -EAGAIN) {
      return false; // something wrong!!!
    }
    FillCodecInputData(aTrack);
  }
  aTrack.mCodec->releaseOutputBuffer(index);
  return aTrack.mCodec->getOutputFormat(&format) == OK;
}

uint8_t*
MediaCodecReader::GetColorConverterBuffer(int32_t aWidth, int32_t aHeight)
{
  // Allocate a temporary YUV420Planer buffer.
  size_t yuv420p_y_size = aWidth * aHeight;
  size_t yuv420p_u_size = ((aWidth + 1) / 2) * ((aHeight + 1) / 2);
  size_t yuv420p_v_size = yuv420p_u_size;
  size_t yuv420p_size = yuv420p_y_size + yuv420p_u_size + yuv420p_v_size;
  if (mColorConverterBufferSize != yuv420p_size) {
    mColorConverterBuffer = nullptr; // release the previous buffer first
    mColorConverterBuffer = new uint8_t[yuv420p_size];
    mColorConverterBufferSize = yuv420p_size;
  }
  return mColorConverterBuffer.get();
}

void
MediaCodecReader::ClearColorConverterBuffer()
{
  mColorConverterBuffer = nullptr;
  mColorConverterBufferSize = 0;
}


} // namespace mozilla
