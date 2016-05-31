/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SeekTask.h"
#include "MediaDecoderReaderWrapper.h"
#include "mozilla/AbstractThread.h"

namespace mozilla {

SeekTask::SeekTask(const void* aDecoderID,
                   AbstractThread* aThread,
                   MediaDecoderReaderWrapper* aReader,
                   SeekJob&& aSeekJob)
  : mDecoderID(aDecoderID)
  , mOwnerThread(aThread)
  , mReader(aReader)
  , mSeekJob(Move(aSeekJob))
  , mIsDiscarded(false)
  , mIsAudioQueueFinished(false)
  , mIsVideoQueueFinished(false)
  , mNeedToStopPrerollingAudio(false)
  , mNeedToStopPrerollingVideo(false)
{
  AssertOwnerThread();
}

SeekTask::~SeekTask()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsDiscarded);
}

void
SeekTask::Resolve(const char* aCallSite)
{
  AssertOwnerThread();

  SeekTaskResolveValue val;
  val.mSeekedAudioData = mSeekedAudioData;
  val.mSeekedVideoData = mSeekedVideoData;
  val.mIsAudioQueueFinished = mIsAudioQueueFinished;
  val.mIsVideoQueueFinished = mIsVideoQueueFinished;
  val.mNeedToStopPrerollingAudio = mNeedToStopPrerollingAudio;
  val.mNeedToStopPrerollingVideo = mNeedToStopPrerollingVideo;

  mSeekTaskPromise.Resolve(val, aCallSite);
}

void
SeekTask::RejectIfExist(const char* aCallSite)
{
  AssertOwnerThread();

  SeekTaskRejectValue val;
  val.mIsAudioQueueFinished = mIsAudioQueueFinished;
  val.mIsVideoQueueFinished = mIsVideoQueueFinished;
  val.mNeedToStopPrerollingAudio = mNeedToStopPrerollingAudio;
  val.mNeedToStopPrerollingVideo = mNeedToStopPrerollingVideo;

  mSeekTaskPromise.RejectIfExists(val, aCallSite);
}

void
SeekTask::AssertOwnerThread() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
}

AbstractThread*
SeekTask::OwnerThread() const
{
  AssertOwnerThread();
  return mOwnerThread;
}

SeekJob&
SeekTask::GetSeekJob()
{
  AssertOwnerThread();
  return mSeekJob;
}

bool
SeekTask::Exists() const
{
  AssertOwnerThread();

  // mSeekTaskPromise communicates SeekTask and MDSM;
  // mSeekJob communicates MDSM and MediaDecoder;
  // Either one exists means the current seek task has yet finished.
  return !mSeekTaskPromise.IsEmpty() || mSeekJob.Exists();
}

} // namespace mozilla
