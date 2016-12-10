/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccurateSeekTask.h"
#include "MediaDecoderReaderWrapper.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "nsPrintfCString.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
extern LazyLogModule gMediaSampleLog;

// avoid redefined macro in unified build
#undef FMT
#undef DECODER_LOG
#undef SAMPLE_LOG
#undef DECODER_WARN

#define FMT(x, ...) "[AccurateSeekTask] Decoder=%p " x, mDecoderID, ##__VA_ARGS__
#define DECODER_LOG(...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(__VA_ARGS__)))
#define SAMPLE_LOG(...)  MOZ_LOG(gMediaSampleLog,  LogLevel::Debug,   (FMT(__VA_ARGS__)))
#define DECODER_WARN(...) NS_WARNING(nsPrintfCString(FMT(__VA_ARGS__)).get())

AccurateSeekTask::AccurateSeekTask(const void* aDecoderID,
                                   AbstractThread* aThread,
                                   MediaDecoderReaderWrapper* aReader,
                                   const SeekTarget& aTarget,
                                   const MediaInfo& aInfo,
                                   const media::TimeUnit& aEnd,
                                   int64_t aCurrentMediaTime)
  : SeekTask(aDecoderID, aThread, aReader, aTarget)
{
  AssertOwnerThread();
}

AccurateSeekTask::~AccurateSeekTask()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsDiscarded);
}

void
AccurateSeekTask::Discard()
{
  AssertOwnerThread();

  // Disconnect MDSM.
  RejectIfExist(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  mIsDiscarded = true;
}

int64_t
AccurateSeekTask::CalculateNewCurrentTime() const
{
  AssertOwnerThread();
  return 0;
}

void
AccurateSeekTask::HandleAudioDecoded(MediaData* aAudio)
{
}

void
AccurateSeekTask::HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart)
{
}

void
AccurateSeekTask::HandleNotDecoded(MediaData::Type aType, const MediaResult& aError)
{
}

void
AccurateSeekTask::HandleAudioWaited(MediaData::Type aType)
{
}

void
AccurateSeekTask::HandleVideoWaited(MediaData::Type aType)
{
}

void
AccurateSeekTask::HandleNotWaited(const WaitForDataRejectValue& aRejection)
{
}

RefPtr<AccurateSeekTask::SeekTaskPromise>
AccurateSeekTask::Seek(const media::TimeUnit& aDuration)
{
  AssertOwnerThread();

  return mSeekTaskPromise.Ensure(__func__);
}

} // namespace mozilla
