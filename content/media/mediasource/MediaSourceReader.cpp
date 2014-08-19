/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceReader.h"

#include "mozilla/dom/TimeRanges.h"
#include "DecoderTraits.h"
#include "MediaDataDecodedListener.h"
#include "MediaDecoderOwner.h"
#include "MediaSource.h"
#include "MediaSourceDecoder.h"
#include "SubBufferDecoder.h"

#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#include "MP4Reader.h"
#endif

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_DEBUG(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define MSE_DEBUGV(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG+1, (__VA_ARGS__))
#define MSE_API(...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#define MSE_DEBUGV(...)
#define MSE_API(...)
#endif

namespace mozilla {

MediaSourceReader::MediaSourceReader(MediaSourceDecoder* aDecoder, dom::MediaSource* aSource)
  : MediaDecoderReader(aDecoder)
  , mTimeThreshold(-1)
  , mDropVideoBeforeThreshold(false)
  , mActiveVideoDecoder(-1)
  , mActiveAudioDecoder(-1)
  , mMediaSource(aSource)
{
}

bool
MediaSourceReader::IsWaitingMediaResources()
{
  return mDecoders.IsEmpty() && mPendingDecoders.IsEmpty();
}

void
MediaSourceReader::RequestAudioData()
{
  if (!GetAudioReader()) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestAudioData called with no audio reader", this);
    MOZ_ASSERT(mPendingDecoders.IsEmpty());
    GetCallback()->OnDecodeError();
    return;
  }
  GetAudioReader()->RequestAudioData();
}

void
MediaSourceReader::OnAudioDecoded(AudioData* aSample)
{
  GetCallback()->OnAudioDecoded(aSample);
}

void
MediaSourceReader::OnAudioEOS()
{
  MSE_DEBUG("MediaSourceReader(%p)::OnAudioEOS %d (%p) EOS (readers=%u)",
            this, mActiveAudioDecoder, mDecoders[mActiveAudioDecoder].get(), mDecoders.Length());
  GetCallback()->OnAudioEOS();
}

void
MediaSourceReader::RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold)
{
  if (!GetVideoReader()) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestVideoData called with no video reader", this);
    MOZ_ASSERT(mPendingDecoders.IsEmpty());
    GetCallback()->OnDecodeError();
    return;
  }
  mTimeThreshold = aTimeThreshold;
  SwitchVideoReaders(SWITCH_OPTIONAL);
  GetVideoReader()->RequestVideoData(aSkipToNextKeyframe, aTimeThreshold);
}

void
MediaSourceReader::OnVideoDecoded(VideoData* aSample)
{
  if (mDropVideoBeforeThreshold) {
    if (aSample->mTime < mTimeThreshold) {
      MSE_DEBUG("MediaSourceReader(%p)::OnVideoDecoded mTime=%lld < mTimeThreshold=%lld",
                this, aSample->mTime, mTimeThreshold);
      delete aSample;
      GetVideoReader()->RequestVideoData(false, mTimeThreshold);
      return;
    }
    mDropVideoBeforeThreshold = false;
  }
  GetCallback()->OnVideoDecoded(aSample);
}

void
MediaSourceReader::OnVideoEOS()
{
  // End of stream. See if we can switch to another video decoder.
  MSE_DEBUG("MediaSourceReader(%p)::OnVideoEOS %d (%p) (readers=%u)",
            this, mActiveVideoDecoder, mDecoders[mActiveVideoDecoder].get(), mDecoders.Length());
  if (SwitchVideoReaders(SWITCH_FORCED)) {
    // Success! Resume decoding with next video decoder.
    RequestVideoData(false, mTimeThreshold);
  } else {
    // End of stream.
    MSE_DEBUG("MediaSourceReader(%p)::OnVideoEOS %d (%p) EOS (readers=%u)",
              this, mActiveVideoDecoder, mDecoders[mActiveVideoDecoder].get(), mDecoders.Length());
    GetCallback()->OnVideoEOS();
  }
}

void
MediaSourceReader::OnDecodeError()
{
  GetCallback()->OnDecodeError();
}

void
MediaSourceReader::Shutdown()
{
  MediaDecoderReader::Shutdown();
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->Shutdown();
  }
}

void
MediaSourceReader::BreakCycles()
{
  MediaDecoderReader::BreakCycles();
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->BreakCycles();
  }
}

bool
MediaSourceReader::SwitchVideoReaders(SwitchType aType)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MOZ_ASSERT(mActiveVideoDecoder != -1);

  InitializePendingDecoders();

  for (uint32_t i = mActiveVideoDecoder + 1; i < mDecoders.Length(); ++i) {
    SubBufferDecoder* decoder = mDecoders[i];

    nsRefPtr<dom::TimeRanges> ranges = new dom::TimeRanges();
    decoder->GetBuffered(ranges);

    MSE_DEBUGV("MediaDecoderReader(%p)::SwitchVideoReaders(%d) decoder=%u (%p) discarded=%d"
               " hasVideo=%d timeThreshold=%f startTime=%f endTime=%f length=%u",
               this, aType, i, decoder, decoder->IsDiscarded(),
               decoder->GetReader()->GetMediaInfo().HasVideo(),
               double(mTimeThreshold) / USECS_PER_S,
               ranges->GetStartTime(), ranges->GetEndTime(), ranges->Length());

    if (decoder->IsDiscarded() ||
        !decoder->GetReader()->GetMediaInfo().HasVideo() ||
        ranges->Length() == 0) {
      continue;
    }

    if (aType == SWITCH_FORCED ||
        ranges->Find(double(mTimeThreshold) / USECS_PER_S) != dom::TimeRanges::NoIndex) {
      GetVideoReader()->SetIdle();

      mActiveVideoDecoder = i;
      mDropVideoBeforeThreshold = true;
      MSE_DEBUG("MediaDecoderReader(%p)::SwitchVideoReaders(%d) switching to %d (%p)",
                this, aType, mActiveVideoDecoder, mDecoders[mActiveVideoDecoder].get());
      return true;
    }
  }

  return false;
}

MediaDecoderReader*
MediaSourceReader::GetAudioReader()
{
  if (mActiveAudioDecoder == -1) {
    return nullptr;
  }
  return mDecoders[mActiveAudioDecoder]->GetReader();
}

MediaDecoderReader*
MediaSourceReader::GetVideoReader()
{
  if (mActiveVideoDecoder == -1) {
    return nullptr;
  }
  return mDecoders[mActiveVideoDecoder]->GetReader();
}

void
MediaSourceReader::SetMediaSourceDuration(double aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());
  ErrorResult dummy;
  mMediaSource->SetDuration(aDuration, dummy);
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
    MSE_DEBUG("MediaSourceReader(%p): Initializing subdecoder %p reader %p",
              this, decoder.get(), reader);

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
      MSE_DEBUG("MediaSourceReader(%p): Reader %p failed to initialize rv=%x", this, reader, rv);
      continue;
    }

    bool active = false;
    if (mi.HasVideo() || mi.HasAudio()) {
      MSE_DEBUG("MediaSourceReader(%p): Reader %p has video=%d audio=%d",
                this, reader, mi.HasVideo(), mi.HasAudio());
      if (mi.HasVideo()) {
        MSE_DEBUG("MediaSourceReader(%p): Reader %p video resolution=%dx%d",
                  this, reader, mi.mVideo.mDisplay.width, mi.mVideo.mDisplay.height);
      }
      if (mi.HasAudio()) {
        MSE_DEBUG("MediaSourceReader(%p): Reader %p audio sampleRate=%d channels=%d",
                  this, reader, mi.mAudio.mRate, mi.mAudio.mChannels);
      }
      active = true;
    }

    if (active) {
      mDecoders.AppendElement(decoder);
    } else {
      MSE_DEBUG("MediaSourceReader(%p): Reader %p not activated", this, reader);
    }
  }
  NS_DispatchToMainThread(new ReleaseDecodersTask(mPendingDecoders));
  MOZ_ASSERT(mPendingDecoders.IsEmpty());
  mDecoder->NotifyWaitingForResourcesStatusChanged();
}

MediaDecoderReader*
CreateReaderForType(const nsACString& aType, AbstractMediaDecoder* aDecoder)
{
#ifdef MOZ_FMP4
  // The MP4Reader that supports fragmented MP4 and uses
  // PlatformDecoderModules is hidden behind prefs for regular video
  // elements, but we always want to use it for MSE, so instantiate it
  // directly here.
  if ((aType.LowerCaseEqualsLiteral("video/mp4") ||
       aType.LowerCaseEqualsLiteral("audio/mp4")) &&
      MP4Decoder::IsEnabled()) {
    return new MP4Reader(aDecoder);
  }
#endif
  return DecoderTraits::CreateReader(aType, aDecoder);
}

already_AddRefed<SubBufferDecoder>
MediaSourceReader::CreateSubDecoder(const nsACString& aType,
                                    MediaSourceDecoder* aParentDecoder)
{
  // XXX: Why/when is mDecoder null here, since it should be equal to aParentDecoder?!
  nsRefPtr<SubBufferDecoder> decoder =
    new SubBufferDecoder(new SourceBufferResource(nullptr, aType), aParentDecoder);
  nsRefPtr<MediaDecoderReader> reader(CreateReaderForType(aType, decoder));
  if (!reader) {
    return nullptr;
  }
  // Set a callback on the subreader that forwards calls to this reader.
  // This reader will then forward them onto the state machine via this
  // reader's callback.
  RefPtr<MediaDataDecodedListener<MediaSourceReader>> callback =
    new MediaDataDecodedListener<MediaSourceReader>(this, GetTaskQueue());
  reader->SetCallback(callback);
  reader->SetTaskQueue(GetTaskQueue());
  reader->Init(nullptr);
  ReentrantMonitorAutoEnter mon(aParentDecoder->GetReentrantMonitor());
  MSE_DEBUG("MediaSourceReader(%p)::CreateSubDecoder subdecoder %p subreader %p",
            this, decoder.get(), reader.get());
  decoder->SetReader(reader);
  mPendingDecoders.AppendElement(decoder);
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethod(this, &MediaSourceReader::InitializePendingDecoders);
  if (NS_FAILED(GetTaskQueue()->Dispatch(task))) {
    MSE_DEBUG("MediaSourceReader(%p): Failed to enqueue decoder initialization task", this);
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

bool
MediaSourceReader::DecodersContainTime(double aTime)
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    if (!mDecoders[i]->IsDiscarded() && mDecoders[i]->ContainsTime(aTime)) {
      return true;
    }
  }
  return false;
}

nsresult
MediaSourceReader::Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                        int64_t aCurrentTime)
{
  MSE_DEBUG("MediaSourceReader(%p)::Seek(aTime=%lld, aStart=%lld, aEnd=%lld, aCurrent=%lld)",
            this, aTime, aStartTime, aEndTime, aCurrentTime);
  double target = static_cast<double>(aTime) / USECS_PER_S;
  if (!DecodersContainTime(target)) {
    MSE_DEBUG("MediaSourceReader(%p)::Seek no active buffer contains target=%f", this, target);
    NS_DispatchToMainThread(new ChangeToHaveMetadata(mDecoder));
  }

  // Loop until we have the requested time range in the source buffers.
  // This is a workaround for our lack of async functionality in the
  // MediaDecoderStateMachine. Bug 979104 implements what we need and
  // we'll remove this for an async approach based on that in bug XXXXXXX.
  while (!DecodersContainTime(target)
         && !IsShutdown()) {
    MSE_DEBUG("MediaSourceReader(%p)::Seek waiting for target=%f", this, target);
    mMediaSource->WaitForData();
    SwitchVideoReaders(SWITCH_FORCED);
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
MediaSourceReader::ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  InitializePendingDecoders();

  MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata decoders=%u", this, mDecoders.Length());

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
      MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata video decoder=%u maxDuration=%lld",
                this, i, maxDuration);
    }
    if (mi.HasAudio() && !mInfo.HasAudio()) {
      MOZ_ASSERT(mActiveAudioDecoder == -1);
      mActiveAudioDecoder = i;
      mInfo.mAudio = mi.mAudio;
      maxDuration = std::max(maxDuration, mDecoders[i]->GetMediaDuration());
      MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata audio decoder=%u maxDuration=%lld",
                this, i, maxDuration);
    }
  }

  if (maxDuration != -1) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(maxDuration);
    nsRefPtr<nsIRunnable> task (
      NS_NewRunnableMethodWithArg<double>(this, &MediaSourceReader::SetMediaSourceDuration,
                                          static_cast<double>(maxDuration) / USECS_PER_S));
    NS_DispatchToMainThread(task);
  }

  *aInfo = mInfo;
  *aTags = nullptr; // TODO: Handle metadata.

  return NS_OK;
}

} // namespace mozilla
