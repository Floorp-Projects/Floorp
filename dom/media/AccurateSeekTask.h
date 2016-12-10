/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ACCURATE_SEEK_TASK_H
#define ACCURATE_SEEK_TASK_H

#include "SeekTask.h"
#include "MediaDecoderReader.h"
#include "SeekJob.h"

namespace mozilla {

class AccurateSeekTask final : public SeekTask {
public:
  AccurateSeekTask(const void* aDecoderID,
                   AbstractThread* aThread,
                   MediaDecoderReaderWrapper* aReader,
                   const SeekTarget& aTarget,
                   const MediaInfo& aInfo,
                   const media::TimeUnit& aEnd,
                   int64_t aCurrentMediaTime);

  void Discard() override;

  RefPtr<SeekTaskPromise> Seek(const media::TimeUnit& aDuration) override;

  int64_t CalculateNewCurrentTime() const override;

  void HandleAudioDecoded(MediaData* aAudio) override;

  void HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart) override;

  void HandleNotDecoded(MediaData::Type aType, const MediaResult& aError) override;

  void HandleAudioWaited(MediaData::Type aType) override;

  void HandleVideoWaited(MediaData::Type aType) override;

  void HandleNotWaited(const WaitForDataRejectValue& aRejection) override;

  ~AccurateSeekTask();
};

} // namespace mozilla

#endif /* ACCURATE_SEEK_TASK_H */
