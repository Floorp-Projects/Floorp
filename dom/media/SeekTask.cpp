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
                   const SeekTarget& aTarget)
  : mDecoderID(aDecoderID)
  , mOwnerThread(aThread)
  , mReader(aReader)
  , mTarget(aTarget)
  , mIsDiscarded(false)
  , mIsAudioQueueFinished(false)
  , mIsVideoQueueFinished(false)
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

  mSeekTaskPromise.Resolve(val, aCallSite);
}

void
SeekTask::RejectIfExist(const char* aCallSite)
{
  AssertOwnerThread();

  SeekTaskRejectValue val;
  val.mIsAudioQueueFinished = mIsAudioQueueFinished;
  val.mIsVideoQueueFinished = mIsVideoQueueFinished;

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

const SeekTarget&
SeekTask::GetSeekTarget()
{
  AssertOwnerThread();
  return mTarget;
}

} // namespace mozilla
