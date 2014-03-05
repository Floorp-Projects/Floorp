/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  {
  }

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE
  {
    // Although we technically don't implement anything here, we return NS_OK
    // so that when the state machine initializes and calls this function
    // we don't return an error code back to the media element.
    return NS_OK;
  }

  bool DecodeAudioData() MOZ_OVERRIDE
  {
    if (GetAudioReader()) {
      return GetAudioReader()->DecodeAudioData();
    }
    return false;
  }

  bool DecodeVideoFrame(bool& aKeyFrameSkip, int64_t aTimeThreshold) MOZ_OVERRIDE
  {
    if (GetVideoReader()) {
      return GetVideoReader()->DecodeVideoFrame(aKeyFrameSkip, aTimeThreshold);
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
    // XXX: Merge result with audio reader.
    MediaDecoderReader* reader = GetVideoReader() ? GetVideoReader() : GetAudioReader();
    if (reader) {
      return reader->GetBuffered(aBuffered, aStartTime);
    }
    return NS_OK;
  }

  MediaQueue<AudioData>& AudioQueue() MOZ_OVERRIDE
  {
    // TODO: Share AudioQueue with SubReaders.
    if (GetAudioReader()) {
      return GetAudioReader()->AudioQueue();
    }
    return MediaDecoderReader::AudioQueue();
  }

  MediaQueue<VideoData>& VideoQueue() MOZ_OVERRIDE
  {
    // TODO: Share VideoQueue with SubReaders.
    if (GetVideoReader()) {
      return GetVideoReader()->VideoQueue();
    }
    return MediaDecoderReader::VideoQueue();
  }

private:
  MediaDecoderReader* GetVideoReader()
  {
    MediaSourceDecoder* decoder = static_cast<MediaSourceDecoder*>(mDecoder);
    return decoder->GetVideoReader();
  }

  MediaDecoderReader* GetAudioReader()
  {
    MediaSourceDecoder* decoder = static_cast<MediaSourceDecoder*>(mDecoder);
    return decoder->GetAudioReader();
  }
};

MediaSourceDecoder::MediaSourceDecoder(dom::HTMLMediaElement* aElement)
  : mMediaSource(nullptr)
  , mVideoReader(nullptr),
    mAudioReader(nullptr)
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
  return new MediaDecoderStateMachine(this, new MediaSourceReader(this));
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
    aSeekable->Add(0, bufferedRanges->GetFinalEndTime());
  } else {
    aSeekable->Add(0, duration);
  }
  return NS_OK;
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

SubBufferDecoder*
MediaSourceDecoder::CreateSubDecoder(const nsACString& aType)
{
  MediaResource* resource = new SourceBufferResource(nullptr, aType);
  nsRefPtr<SubBufferDecoder> decoder = new SubBufferDecoder(resource, this);
  nsAutoPtr<MediaDecoderReader> reader(DecoderTraits::CreateReader(aType, decoder));
  reader->Init(nullptr);

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mDecoders.AppendElement(decoder);
  mReaders.AppendElement(reader);
  MSE_DEBUG("Registered subdecoder %p subreader %p", decoder.get(), reader.get());
  mon.NotifyAll();

  decoder->SetReader(reader.forget());
  return decoder;
}

nsresult
MediaSourceReader::ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  mDecoder->SetMediaSeekable(true);
  mDecoder->SetTransportSeekable(false);

  MediaSourceDecoder* decoder = static_cast<MediaSourceDecoder*>(mDecoder);
  const nsTArray<MediaDecoderReader*>& readers = decoder->GetReaders();
  for (uint32_t i = 0; i < readers.Length(); ++i) {
    MediaDecoderReader* reader = readers[i];
    MediaInfo mi;
    nsresult rv = reader->ReadMetadata(&mi, aTags);
    MSE_DEBUG("ReadMetadata on SB reader %p", reader);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (mi.HasVideo() && !mInfo.HasVideo()) {
      mInfo.mVideo = mi.mVideo;
      decoder->SetVideoReader(reader);
    }
    if (mi.HasAudio() && !mInfo.HasAudio()) {
      mInfo.mAudio = mi.mAudio;
      decoder->SetAudioReader(reader);
    }
  }
  *aInfo = mInfo;

  return NS_OK;
}

double
MediaSourceDecoder::GetMediaSourceDuration()
{
  return mMediaSource ?
           mMediaSource->Duration() :
           mDuration / static_cast<double>(USECS_PER_S);
}

} // namespace mozilla
