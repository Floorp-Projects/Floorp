/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SEEK_TASK_H
#define SEEK_TASK_H

#include "mozilla/MozPromise.h"
#include "MediaResult.h"
#include "SeekTarget.h"

namespace mozilla {

class AbstractThread;
class MediaData;
class MediaDecoderReaderWrapper;

namespace media {
class TimeUnit;
}

struct SeekTaskResolveValue
{
  RefPtr<MediaData> mSeekedAudioData;
  RefPtr<MediaData> mSeekedVideoData;
  bool mIsAudioQueueFinished;
  bool mIsVideoQueueFinished;
};

struct SeekTaskRejectValue
{
  SeekTaskRejectValue()
    : mIsAudioQueueFinished(false)
    , mIsVideoQueueFinished(false)
    , mError(NS_ERROR_DOM_MEDIA_FATAL_ERR)
  {
  }
  bool mIsAudioQueueFinished;
  bool mIsVideoQueueFinished;
  MediaResult mError;
};

class SeekTask {

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SeekTask)

public:
  static const bool IsExclusive = true;

  using SeekTaskPromise =
    MozPromise<SeekTaskResolveValue, SeekTaskRejectValue, IsExclusive>;

  virtual void Discard() = 0;

  virtual RefPtr<SeekTaskPromise> Seek(const media::TimeUnit& aDuration) = 0;

  virtual bool NeedToResetMDSM() const = 0;

  virtual int64_t CalculateNewCurrentTime() const = 0;

  const SeekTarget& GetSeekTarget();

protected:
  SeekTask(const void* aDecoderID,
           AbstractThread* aThread,
           MediaDecoderReaderWrapper* aReader,
           const SeekTarget& aTarget);

  virtual ~SeekTask();

  void Resolve(const char* aCallSite);

  void RejectIfExist(const MediaResult& aError, const char* aCallSite);

  void AssertOwnerThread() const;

  AbstractThread* OwnerThread() const;

  /*
   * Data shared with MDSM.
   */
  const void* mDecoderID; // For logging.
  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<MediaDecoderReaderWrapper> mReader;

  /*
   * Internal state.
   */
  SeekTarget mTarget;
  MozPromiseHolder<SeekTaskPromise> mSeekTaskPromise;
  bool mIsDiscarded;

  /*
   * Information which are going to be returned to MDSM.
   */
  RefPtr<MediaData> mSeekedAudioData;
  RefPtr<MediaData> mSeekedVideoData;
  bool mIsAudioQueueFinished;
  bool mIsVideoQueueFinished;
};

} // namespace mozilla

#endif /* SEEK_TASK_H */
