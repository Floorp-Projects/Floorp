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
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/mozalloc.h"
#include "nsISupports.h"
#include "prlog.h"
#include "SubBufferDecoder.h"
#include "SourceBufferResource.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define LOG(type, msg) PR_LOG(gMediaSourceLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {

namespace dom {

class TimeRanges;

} // namespace dom

class MediaSourceReader : public MediaDecoderReader
{
public:
  MediaSourceReader(AbstractMediaDecoder* aDecoder)
    : MediaDecoderReader(aDecoder)
  {
  }

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_IMPLEMENTED;
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
    return mInfo.mHasVideo;
  }

  bool HasAudio() MOZ_OVERRIDE
  {
    return mInfo.mHasAudio;
  }

  nsresult ReadMetadata(VideoInfo* aInfo, MetadataTags** aTags) MOZ_OVERRIDE;

  nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                int64_t aCurrentTime) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsresult GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime) MOZ_OVERRIDE
  {
    // XXX: Merge result with audio reader.
    return GetVideoReader()->GetBuffered(aBuffered, aStartTime);
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

MediaSourceDecoder::MediaSourceDecoder(HTMLMediaElement* aElement)
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
  return NS_OK;
}

void
MediaSourceDecoder::AttachMediaSource(MediaSource* aMediaSource)
{
  MOZ_ASSERT(!mMediaSource && !mDecoderStateMachine);
  mMediaSource = aMediaSource;
  mDecoderStateMachine = CreateStateMachine();
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
  LOG(PR_LOG_DEBUG, ("Registered subdecoder %p subreader %p", decoder.get(), reader.get()));
  mon.NotifyAll();

  decoder->SetReader(reader.forget());
  return decoder;
}

nsresult
MediaSourceReader::ReadMetadata(VideoInfo* aInfo, MetadataTags** aTags)
{
  mDecoder->SetMediaSeekable(true);
  mDecoder->SetTransportSeekable(false);

  MediaSourceDecoder* decoder = static_cast<MediaSourceDecoder*>(mDecoder);
  const nsTArray<MediaDecoderReader*>& readers = decoder->GetReaders();
  bool gotVideo = false;
  bool gotAudio = false;
  for (uint32_t i = 0; i < readers.Length(); ++i) {
    MediaDecoderReader* reader = readers[i];
    VideoInfo vi;
    nsresult rv = reader->ReadMetadata(&vi, aTags);
    LOG(PR_LOG_DEBUG, ("ReadMetadata on SB reader %p", reader));
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (vi.mHasVideo && !gotVideo) {
      mInfo = vi;
      decoder->SetVideoReader(reader);
      gotVideo = true;
    }
    if (vi.mHasAudio && !gotAudio) {
      mInfo.mAudioRate = vi.mAudioRate;
      mInfo.mAudioChannels = vi.mAudioChannels;
      mInfo.mHasAudio = true;
      decoder->SetAudioReader(reader);
      gotAudio = true;
    }
  }
  *aInfo = mInfo;

  return NS_OK;
}

} // namespace mozilla
