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
#include "SourceBufferList.h"
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
  MediaSourceReader(MediaSourceDecoder* aDecoder, dom::MediaSource* aSource)
    : MediaDecoderReader(aDecoder)
    , mActiveVideoDecoder(-1)
    , mActiveAudioDecoder(-1)
    , mMediaSource(aSource)
  {
  }

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE
  {
    // Although we technically don't implement anything here, we return NS_OK
    // so that when the state machine initializes and calls this function
    // we don't return an error code back to the media element.
    return NS_OK;
  }

  bool IsWaitingMediaResources() MOZ_OVERRIDE
  {
    return mDecoders.IsEmpty() && mPendingDecoders.IsEmpty();
  }

  bool DecodeAudioData() MOZ_OVERRIDE
  {
    if (!GetAudioReader()) {
      MSE_DEBUG("%p DecodeAudioFrame called with no audio reader", this);
      MOZ_ASSERT(mPendingDecoders.IsEmpty());
      return false;
    }
    bool rv = GetAudioReader()->DecodeAudioData();

    nsAutoTArray<AudioData*, 10> audio;
    GetAudioReader()->AudioQueue().GetElementsAfter(-1, &audio);
    for (uint32_t i = 0; i < audio.Length(); ++i) {
      AudioQueue().Push(audio[i]);
    }
    GetAudioReader()->AudioQueue().Empty();

    return rv;
  }

  bool DecodeVideoFrame(bool& aKeyFrameSkip, int64_t aTimeThreshold) MOZ_OVERRIDE
  {
    if (!GetVideoReader()) {
      MSE_DEBUG("%p DecodeVideoFrame called with no video reader", this);
      MOZ_ASSERT(mPendingDecoders.IsEmpty());
      return false;
    }

    if (MaybeSwitchVideoReaders(aTimeThreshold)) {
      GetVideoReader()->DecodeToTarget(aTimeThreshold);
    }

    bool rv = GetVideoReader()->DecodeVideoFrame(aKeyFrameSkip, aTimeThreshold);

    nsAutoTArray<VideoData*, 10> video;
    GetVideoReader()->VideoQueue().GetElementsAfter(-1, &video);
    for (uint32_t i = 0; i < video.Length(); ++i) {
      VideoQueue().Push(video[i]);
    }
    GetVideoReader()->VideoQueue().Empty();

    if (rv) {
      return true;
    }

    MSE_DEBUG("%p MSR::DecodeVF %d (%p) returned false (readers=%u)",
              this, mActiveVideoDecoder, mDecoders[mActiveVideoDecoder].get(), mDecoders.Length());
    return rv;
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
                int64_t aCurrentTime) MOZ_OVERRIDE;
  nsresult GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime) MOZ_OVERRIDE;
  already_AddRefed<SubBufferDecoder> CreateSubDecoder(const nsACString& aType,
                                                      MediaSourceDecoder* aParentDecoder);

  void InitializePendingDecoders();

  bool IsShutdown() {
    ReentrantMonitorAutoEnter decoderMon(mDecoder->GetReentrantMonitor());
    return mDecoder->IsShutdown();
  }

private:
  bool MaybeSwitchVideoReaders(int64_t aTimeThreshold) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    MOZ_ASSERT(mActiveVideoDecoder != -1);

    InitializePendingDecoders();

    for (uint32_t i = mActiveVideoDecoder + 1; i < mDecoders.Length(); ++i) {
      if (!mDecoders[i]->GetReader()->GetMediaInfo().HasVideo()) {
        continue;
      }
      if (aTimeThreshold >= mDecoders[i]->GetMediaStartTime()) {
        GetVideoReader()->SetIdle();

        mActiveVideoDecoder = i;
        MSE_DEBUG("%p MSR::DecodeVF switching to %d", this, mActiveVideoDecoder);

        return true;
      }
    }

    return false;
  }

  MediaDecoderReader* GetAudioReader() {
    if (mActiveAudioDecoder == -1) {
      return nullptr;
    }
    return mDecoders[mActiveAudioDecoder]->GetReader();
  }

  MediaDecoderReader* GetVideoReader() {
    if (mActiveVideoDecoder == -1) {
      return nullptr;
    }
    return mDecoders[mActiveVideoDecoder]->GetReader();
  }

  nsTArray<nsRefPtr<SubBufferDecoder>> mPendingDecoders;
  nsTArray<nsRefPtr<SubBufferDecoder>> mDecoders;

  int32_t mActiveVideoDecoder;
  int32_t mActiveAudioDecoder;
  dom::MediaSource* mMediaSource;
};

class MediaSourceStateMachine : public MediaDecoderStateMachine
{
public:
  MediaSourceStateMachine(MediaDecoder* aDecoder,
                          MediaDecoderReader* aReader,
                          bool aRealTime = false)
    : MediaDecoderStateMachine(aDecoder, aReader, aRealTime)
  {
  }

  already_AddRefed<SubBufferDecoder> CreateSubDecoder(const nsACString& aType,
                                                      MediaSourceDecoder* aParentDecoder) {
    if (!mReader) {
      return nullptr;
    }
    return static_cast<MediaSourceReader*>(mReader.get())->CreateSubDecoder(aType, aParentDecoder);
  }

  nsresult EnqueueDecoderInitialization() {
    AssertCurrentThreadInMonitor();
    if (!mReader) {
      return NS_ERROR_FAILURE;
    }
    return mDecodeTaskQueue->Dispatch(NS_NewRunnableMethod(this,
                                                           &MediaSourceStateMachine::InitializePendingDecoders));
  }

private:
  void InitializePendingDecoders() {
    if (!mReader) {
      return;
    }
    static_cast<MediaSourceReader*>(mReader.get())->InitializePendingDecoders();
  }
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
  return new MediaSourceStateMachine(this, new MediaSourceReader(this, mMediaSource));
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
  if (!mDecoderStateMachine) {
    return nullptr;
  }
  return static_cast<MediaSourceStateMachine*>(mDecoderStateMachine.get())->CreateSubDecoder(aType, this);
}

nsresult
MediaSourceDecoder::EnqueueDecoderInitialization()
{
  if (!mDecoderStateMachine) {
    return NS_ERROR_FAILURE;
  }
  return static_cast<MediaSourceStateMachine*>(mDecoderStateMachine.get())->EnqueueDecoderInitialization();
}

class ReleaseDecodersTask : public nsRunnable {
public:
  ReleaseDecodersTask(nsTArray<nsRefPtr<SubBufferDecoder>>& aDecoders)
  {
    mDecoders.SwapElements(aDecoders);
  }

  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL {
    mDecoders.Clear();
    return NS_OK;
  }

private:
  nsTArray<nsRefPtr<SubBufferDecoder>> mDecoders;
};

void
MediaSourceReader::InitializePendingDecoders()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mPendingDecoders.Length(); ++i) {
    nsRefPtr<SubBufferDecoder> decoder = mPendingDecoders[i];
    MediaDecoderReader* reader = decoder->GetReader();
    MSE_DEBUG("%p: Initializating subdecoder %p reader %p", this, decoder.get(), reader);

    MediaInfo mi;
    nsAutoPtr<MetadataTags> tags; // TODO: Handle metadata.
    nsresult rv;
    int64_t startTime = 0;
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      rv = reader->ReadMetadata(&mi, getter_Transfers(tags));
      if (NS_SUCCEEDED(rv)) {
        reader->FindStartTime(startTime);
      }
    }
    reader->SetIdle();
    if (NS_FAILED(rv)) {
      // XXX: Need to signal error back to owning SourceBuffer.
      MSE_DEBUG("%p: Reader %p failed to initialize, rv=%x", this, reader, rv);
      continue;
    }
    decoder->SetMediaStartTime(startTime);

    bool active = false;
    if (mi.HasVideo() || mi.HasAudio()) {
      MSE_DEBUG("%p: Reader %p has video=%d audio=%d startTime=%lld",
                this, reader, mi.HasVideo(), mi.HasAudio(), startTime);
      active = true;
    }

    if (active) {
      mDecoders.AppendElement(decoder);
    } else {
      MSE_DEBUG("%p: Reader %p not activated", this, reader);
    }
  }
  NS_DispatchToMainThread(new ReleaseDecodersTask(mPendingDecoders));
  MOZ_ASSERT(mPendingDecoders.IsEmpty());
  mDecoder->NotifyWaitingForResourcesStatusChanged();
}

already_AddRefed<SubBufferDecoder>
MediaSourceReader::CreateSubDecoder(const nsACString& aType, MediaSourceDecoder* aParentDecoder)
{
  // XXX: Why/when is mDecoder null here, since it should be equal to aParentDecoder?!
  nsRefPtr<SubBufferDecoder> decoder =
    new SubBufferDecoder(new SourceBufferResource(nullptr, aType), aParentDecoder);
  nsAutoPtr<MediaDecoderReader> reader(DecoderTraits::CreateReader(aType, decoder));
  if (!reader) {
    return nullptr;
  }
  reader->Init(nullptr);
  ReentrantMonitorAutoEnter mon(aParentDecoder->GetReentrantMonitor());
  MSE_DEBUG("Registered subdecoder %p subreader %p", decoder.get(), reader.get());
  decoder->SetReader(reader.forget());
  mPendingDecoders.AppendElement(decoder);
  if (NS_FAILED(static_cast<MediaSourceDecoder*>(mDecoder)->EnqueueDecoderInitialization())) {
    MSE_DEBUG("%p: Failed to enqueue decoder initialization task", this);
    return nullptr;
  }
  mDecoder->NotifyWaitingForResourcesStatusChanged();
  return decoder.forget();
}

namespace {
class ChangeToHaveMetadata : public nsRunnable {
public:
  ChangeToHaveMetadata(AbstractMediaDecoder* aDecoder) :
    mDecoder(aDecoder)
  {
  }

  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL {
    auto owner = mDecoder->GetOwner();
    if (owner) {
      owner->UpdateReadyStateForData(MediaDecoderOwner::NEXT_FRAME_WAIT_FOR_MSE_DATA);
    }
    return NS_OK;
  }

private:
  nsRefPtr<AbstractMediaDecoder> mDecoder;
};
}

nsresult
MediaSourceReader::Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                        int64_t aCurrentTime)
{
  if (!mMediaSource->ActiveSourceBuffers()->AllContainsTime (aTime / USECS_PER_S)) {
    NS_DispatchToMainThread(new ChangeToHaveMetadata(mDecoder));
  }

  // Loop until we have the requested time range in the source buffers.
  // This is a workaround for our lack of async functionality in the
  // MediaDecoderStateMachine. Bug 979104 implements what we need and
  // we'll remove this for an async approach based on that in bug XXXXXXX.
  while (!mMediaSource->ActiveSourceBuffers()->AllContainsTime (aTime / USECS_PER_S)
         && !IsShutdown()) {
    mMediaSource->WaitForData();
    MaybeSwitchVideoReaders(aTime);
  }

  if (IsShutdown()) {
    return NS_OK;
  }

  ResetDecode();
  if (GetAudioReader()) {
    nsresult rv = GetAudioReader()->Seek(aTime, aStartTime, aEndTime, aCurrentTime);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  if (GetVideoReader()) {
    nsresult rv = GetVideoReader()->Seek(aTime, aStartTime, aEndTime, aCurrentTime);
     if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
MediaSourceReader::GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime)
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
    mDecoders[i]->GetBuffered(r);
    aBuffered->Add(r->GetStartTime(), r->GetEndTime());
  }
  aBuffered->Normalize();
  return NS_OK;
}

nsresult
MediaSourceReader::ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  mDecoder->SetMediaSeekable(true);
  mDecoder->SetTransportSeekable(false);

  MSE_DEBUG("%p: MSR::ReadMetadata pending=%u", this, mPendingDecoders.Length());

  InitializePendingDecoders();

  MSE_DEBUG("%p: MSR::ReadMetadata decoders=%u", this, mDecoders.Length());

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

    MediaInfo mi = reader->GetMediaInfo();

    if (mi.HasVideo() && !mInfo.HasVideo()) {
      MOZ_ASSERT(mActiveVideoDecoder == -1);
      mActiveVideoDecoder = i;
      mInfo.mVideo = mi.mVideo;
      maxDuration = std::max(maxDuration, mDecoders[i]->GetMediaDuration());
      MSE_DEBUG("%p: MSR::ReadMetadata video decoder=%u maxDuration=%lld", this, i, maxDuration);
    }
    if (mi.HasAudio() && !mInfo.HasAudio()) {
      MOZ_ASSERT(mActiveAudioDecoder == -1);
      mActiveAudioDecoder = i;
      mInfo.mAudio = mi.mAudio;
      maxDuration = std::max(maxDuration, mDecoders[i]->GetMediaDuration());
      MSE_DEBUG("%p: MSR::ReadMetadata audio decoder=%u maxDuration=%lld", this, i, maxDuration);
    }
  }

  if (maxDuration != -1) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(maxDuration);
    ErrorResult dummy;
    mMediaSource->SetDuration(maxDuration, dummy);
  }

  *aInfo = mInfo;
  *aTags = nullptr; // TODO: Handle metadata.

  return NS_OK;
}

} // namespace mozilla
