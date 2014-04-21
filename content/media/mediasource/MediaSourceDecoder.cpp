/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceResource.h"
#include "MediaSourceDecoder.h"

#include "AbstractMediaDecoder.h"
#include "MediaDecoderReader.h"
#include "MediaDecoderStateMachine.h"
#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/mozalloc.h"
#include "nsISupports.h"
#include "nsIThread.h"
#include "prlog.h"
#include "MediaSource.h"
#include "SubBufferDecoder.h"
#include "SourceBufferResource.h"
#include "VideoUtils.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define MSE_DEBUG(...) PR_LOG(gMediaSourceLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#endif

namespace mozilla {

namespace dom {

class TimeRanges;

} // namespace dom

class MediaSourceReader : public MediaDecoderReader
{
public:
  MediaSourceReader(MediaSourceDecoder* aDecoder)
    : MediaDecoderReader(aDecoder)
    , mActiveVideoReader(-1)
    , mActiveAudioReader(-1)
  {
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSourceReader)

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE
  {
    // Although we technically don't implement anything here, we return NS_OK
    // so that when the state machine initializes and calls this function
    // we don't return an error code back to the media element.
    return NS_OK;
  }

  bool DecodeAudioData() MOZ_OVERRIDE
  {
    if (mActiveAudioReader == -1) {
      MSE_DEBUG("%p DecodeAudioFrame called with no audio reader", this);
      MOZ_ASSERT(mPendingDecoders.IsEmpty());
      return false;
    }
    return mAudioReaders[mActiveAudioReader]->DecodeAudioData();
  }

  bool DecodeVideoFrame(bool& aKeyFrameSkip, int64_t aTimeThreshold) MOZ_OVERRIDE
  {
    if (mActiveVideoReader == -1) {
      MSE_DEBUG("%p DecodeVideoFrame called with no video reader", this);
      MOZ_ASSERT(mPendingDecoders.IsEmpty());
      return false;
    }
    bool rv = mVideoReaders[mActiveVideoReader]->DecodeVideoFrame(aKeyFrameSkip, aTimeThreshold);
    if (rv) {
      return true;
    }
    MSE_DEBUG("%p MSR::DecodeVF %d (%p) returned false (readers=%u)",
              this, mActiveVideoReader, mVideoReaders[mActiveVideoReader], mVideoReaders.Length());
    if (SwitchVideoReaders(aTimeThreshold)) {
      return mVideoReaders[mActiveVideoReader]->DecodeVideoFrame(aKeyFrameSkip, aTimeThreshold);
    }
    return false;
  }

  bool HasVideo() MOZ_OVERRIDE
  {
    return mInfo.HasVideo();
  }

  bool HasAudio() MOZ_OVERRIDE
  {
    return mInfo.HasAudio();
  }

  nsresult ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags) MOZ_OVERRIDE;

  nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                int64_t aCurrentTime) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsresult GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime) MOZ_OVERRIDE
  {
    for (uint32_t i = 0; i < mVideoReaders.Length(); ++i) {
      nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
      mVideoReaders[i]->GetBuffered(r, aStartTime);
      aBuffered->Add(r->GetStartTime(), r->GetEndTime());
    }
    for (uint32_t i = 0; i < mAudioReaders.Length(); ++i) {
      nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
      mAudioReaders[i]->GetBuffered(r, aStartTime);
      aBuffered->Add(r->GetStartTime(), r->GetEndTime());
    }
    aBuffered->Normalize();
    return NS_OK;
  }

  MediaQueue<AudioData>& AudioQueue() MOZ_OVERRIDE
  {
    // TODO: Share AudioQueue with SubReaders.
    for (uint32_t i = 0; i < mAudioReaders.Length(); ++i) {
      MediaQueue<AudioData>& audioQueue = mAudioReaders[i]->AudioQueue();
      // Empty existing queues in order.
      if (audioQueue.GetSize() > 0) {
        return audioQueue;
      }
    }
    return MediaDecoderReader::AudioQueue();
  }

  MediaQueue<VideoData>& VideoQueue() MOZ_OVERRIDE
  {
    // TODO: Share VideoQueue with SubReaders.
    for (uint32_t i = 0; i < mVideoReaders.Length(); ++i) {
      MediaQueue<VideoData>& videoQueue = mVideoReaders[i]->VideoQueue();
      // Empty existing queues in order.
      if (videoQueue.GetSize() > 0) {
        return videoQueue;
      }
    }
    return MediaDecoderReader::VideoQueue();
  }

  already_AddRefed<SubBufferDecoder> CreateSubDecoder(const nsACString& aType,
                                                      MediaSourceDecoder* aParentDecoder);

private:
  bool SwitchVideoReaders(int64_t aTimeThreshold) {
    MOZ_ASSERT(mActiveVideoReader != -1);
    // XXX: We switch when the first reader is depleted, but it might be
    // better to switch as soon as the next reader is ready to decode and
    // has data for the current media time.
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

    WaitForPendingDecoders();

    if (mVideoReaders.Length() > uint32_t(mActiveVideoReader + 1)) {
      mActiveVideoReader += 1;
      MSE_DEBUG("%p MSR::DecodeVF switching to %d", this, mActiveVideoReader);

      MOZ_ASSERT(mVideoReaders[mActiveVideoReader]->GetMediaInfo().HasVideo());
      mVideoReaders[mActiveVideoReader]->SetActive();
      mVideoReaders[mActiveVideoReader]->DecodeToTarget(aTimeThreshold);

      return true;
    }
    return false;
  }

  bool EnsureWorkQueueInitialized();
  nsresult EnqueueDecoderInitialization();
  void CallDecoderInitialization();
  void WaitForPendingDecoders();

  nsTArray<nsRefPtr<SubBufferDecoder>> mPendingDecoders;
  nsTArray<nsRefPtr<SubBufferDecoder>> mDecoders;

  nsTArray<MediaDecoderReader*> mVideoReaders;
  nsTArray<MediaDecoderReader*> mAudioReaders;
  int32_t mActiveVideoReader;
  int32_t mActiveAudioReader;

  nsCOMPtr<nsIThread> mWorkQueue;
};

MediaSourceDecoder::MediaSourceDecoder(dom::HTMLMediaElement* aElement)
  : mMediaSource(nullptr)
{
  Init(aElement);
}

MediaDecoder*
MediaSourceDecoder::Clone()
{
  // TODO: Sort out cloning.
  return nullptr;
}

MediaDecoderStateMachine*
MediaSourceDecoder::CreateStateMachine()
{
  // XXX: Find a cleaner way to retain a reference to our reader.
  mReader = new MediaSourceReader(this);
  return new MediaDecoderStateMachine(this, mReader);
}

nsresult
MediaSourceDecoder::Load(nsIStreamListener**, MediaDecoder*)
{
  MOZ_ASSERT(!mDecoderStateMachine);
  mDecoderStateMachine = CreateStateMachine();
  if (!mDecoderStateMachine) {
    NS_WARNING("Failed to create state machine!");
    return NS_ERROR_FAILURE;
  }

  return mDecoderStateMachine->Init(nullptr);
}

nsresult
MediaSourceDecoder::GetSeekable(dom::TimeRanges* aSeekable)
{
  double duration = mMediaSource->Duration();
  if (IsNaN(duration)) {
    // Return empty range.
  } else if (duration > 0 && mozilla::IsInfinite(duration)) {
    nsRefPtr<dom::TimeRanges> bufferedRanges = new dom::TimeRanges();
    GetBuffered(bufferedRanges);
    aSeekable->Add(bufferedRanges->GetStartTime(), bufferedRanges->GetEndTime());
  } else {
    aSeekable->Add(0, duration);
  }
  return NS_OK;
}

/*static*/
already_AddRefed<MediaResource>
MediaSourceDecoder::CreateResource()
{
  return nsRefPtr<MediaResource>(new MediaSourceResource()).forget();
}

void
MediaSourceDecoder::AttachMediaSource(dom::MediaSource* aMediaSource)
{
  MOZ_ASSERT(!mMediaSource && !mDecoderStateMachine);
  mMediaSource = aMediaSource;
}

void
MediaSourceDecoder::DetachMediaSource()
{
  mMediaSource = nullptr;
}

already_AddRefed<SubBufferDecoder>
MediaSourceDecoder::CreateSubDecoder(const nsACString& aType)
{
  return mReader->CreateSubDecoder(aType, this);
}

bool
MediaSourceReader::EnsureWorkQueueInitialized()
{
  // TODO: Use a global threadpool rather than a thread per MediaSource.
  MOZ_ASSERT(NS_IsMainThread());
  if (!mWorkQueue &&
      NS_FAILED(NS_NewNamedThread("MediaSource",
                                  getter_AddRefs(mWorkQueue),
                                  nullptr,
                                  MEDIA_THREAD_STACK_SIZE))) {
    return false;
  }
  return true;
}

nsresult
MediaSourceReader::EnqueueDecoderInitialization()
{
  if (!EnsureWorkQueueInitialized()) {
    return NS_ERROR_FAILURE;
  }
  return mWorkQueue->Dispatch(NS_NewRunnableMethod(this, &MediaSourceReader::CallDecoderInitialization), NS_DISPATCH_NORMAL);
}

void
MediaSourceReader::CallDecoderInitialization()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mPendingDecoders.Length(); ++i) {
    nsRefPtr<SubBufferDecoder> decoder = mPendingDecoders[i];
    MediaDecoderReader* reader = decoder->GetReader();
    MSE_DEBUG("%p: Initializating subdecoder %p reader %p", this, decoder.get(), reader);

    reader->SetActive();
    MediaInfo mi;
    nsAutoPtr<MetadataTags> tags; // TODO: Handle metadata.
    nsresult rv;
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      rv = reader->ReadMetadata(&mi, getter_Transfers(tags));
    }
    reader->SetIdle();
    if (NS_FAILED(rv)) {
      // XXX: Need to signal error back to owning SourceBuffer.
      MSE_DEBUG("%p: Reader %p failed to initialize, rv=%x", this, reader, rv);
      continue;
    }

    bool active = false;
    if (mi.HasVideo()) {
      MSE_DEBUG("%p: Reader %p has video track", this, reader);
      mVideoReaders.AppendElement(reader);
      active = true;
    }
    if (mi.HasAudio()) {
      MSE_DEBUG("%p: Reader %p has audio track", this, reader);
      mAudioReaders.AppendElement(reader);
      active = true;
    }

    if (active) {
      mDecoders.AppendElement(decoder);
    } else {
      MSE_DEBUG("%p: Reader %p not activated", this, reader);
    }
  }
  mPendingDecoders.Clear();
  mon.NotifyAll();
}

void
MediaSourceReader::WaitForPendingDecoders()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  while (!mPendingDecoders.IsEmpty()) {
    mon.Wait();
  }
}

already_AddRefed<SubBufferDecoder>
MediaSourceReader::CreateSubDecoder(const nsACString& aType, MediaSourceDecoder* aParentDecoder)
{
  nsRefPtr<SubBufferDecoder> decoder =
    new SubBufferDecoder(new SourceBufferResource(nullptr, aType), aParentDecoder);
  nsAutoPtr<MediaDecoderReader> reader(DecoderTraits::CreateReader(aType, decoder));
  if (!reader) {
    return nullptr;
  }
  reader->Init(nullptr);
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MSE_DEBUG("Registered subdecoder %p subreader %p", decoder.get(), reader.get());
  decoder->SetReader(reader.forget());
  mPendingDecoders.AppendElement(decoder);
  EnqueueDecoderInitialization();
  return decoder.forget();
}

nsresult
MediaSourceReader::ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  mDecoder->SetMediaSeekable(true);
  mDecoder->SetTransportSeekable(false);

  WaitForPendingDecoders();

  // XXX: Make subdecoder setup async, so that use cases like bug 989888 can
  // work.  This will require teaching the state machine about dynamic track
  // changes (and multiple tracks).
  // Shorter term, make this block until we've got at least one video track
  // and lie about having an audio track, then resample/remix as necessary
  // to match any audio track added later to fit the format we lied about
  // now.  For now we just configure what we've got and cross our fingers.
  int64_t maxDuration = -1;
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    MediaDecoderReader* reader = mDecoders[i]->GetReader();

    reader->SetActive(); // XXX check where this should be called

    MediaInfo mi = reader->GetMediaInfo();

    if (mi.HasVideo() && !mInfo.HasVideo()) {
      MOZ_ASSERT(mActiveVideoReader == -1);
      mActiveVideoReader = i;
      mInfo.mVideo = mi.mVideo;
      maxDuration = std::max(maxDuration, mDecoders[i]->GetMediaDuration());
    }
    if (mi.HasAudio() && !mInfo.HasAudio()) {
      MOZ_ASSERT(mActiveAudioReader == -1);
      mActiveAudioReader = i;
      mInfo.mAudio = mi.mAudio;
      maxDuration = std::max(maxDuration, mDecoders[i]->GetMediaDuration());
    }
  }
  *aInfo = mInfo;

  if (maxDuration != -1) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(maxDuration);
  }

  return NS_OK;
}

} // namespace mozilla
