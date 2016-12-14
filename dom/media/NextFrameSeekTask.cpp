/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NextFrameSeekTask.h"
#include "MediaDecoderReaderWrapper.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "nsPrintfCString.h"

namespace mozilla {

extern LazyLogModule gMediaSampleLog;

#define SAMPLE_LOG(x, ...) MOZ_LOG(gMediaSampleLog, LogLevel::Debug, \
  ("[NextFrameSeekTask] Decoder=%p " x, mDecoderID, ##__VA_ARGS__))

namespace media {

NextFrameSeekTask::NextFrameSeekTask(const void* aDecoderID,
                                     AbstractThread* aThread,
                                     MediaDecoderReaderWrapper* aReader,
                                     const SeekTarget& aTarget,
                                     const MediaInfo& aInfo,
                                     const media::TimeUnit& aDuration,
                                     int64_t aCurrentTime,
                                     MediaQueue<MediaData>& aAudioQueue,
                                     MediaQueue<MediaData>& aVideoQueue)
  : SeekTask(aDecoderID, aThread, aReader, aTarget)
{
  AssertOwnerThread();
  MOZ_ASSERT(aInfo.HasVideo());
}

NextFrameSeekTask::~NextFrameSeekTask()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsDiscarded);
}

void
NextFrameSeekTask::Discard()
{
  AssertOwnerThread();

  // Disconnect MDSM.
  RejectIfExist(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  mIsDiscarded = true;
}

int64_t
NextFrameSeekTask::CalculateNewCurrentTime() const
{
  AssertOwnerThread();
  return 0;
}

void
NextFrameSeekTask::HandleAudioDecoded(MediaData* aAudio)
{
}

void
NextFrameSeekTask::HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart)
{
}

void
NextFrameSeekTask::HandleNotDecoded(MediaData::Type aType, const MediaResult& aError)
{
}

void
NextFrameSeekTask::HandleAudioWaited(MediaData::Type aType)
{
}

void
NextFrameSeekTask::HandleVideoWaited(MediaData::Type aType)
{
}

void
NextFrameSeekTask::HandleNotWaited(const WaitForDataRejectValue& aRejection)
{
}

RefPtr<NextFrameSeekTask::SeekTaskPromise>
NextFrameSeekTask::Seek(const media::TimeUnit&)
{
  AssertOwnerThread();

  RefPtr<SeekTaskPromise> promise = mSeekTaskPromise.Ensure(__func__);

  return promise;
}

} // namespace media
} // namespace mozilla
