/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AudioChild.h"

namespace mozilla {
namespace dom {
NS_IMPL_THREADSAFE_ADDREF(AudioChild);
NS_IMPL_THREADSAFE_RELEASE(AudioChild);

AudioChild::AudioChild()
  : mLastPosition(-1),
    mLastPositionTimestamp(0),
    mWriteCounter(0),
    mMinWriteSize(-2),// Initial value, -2, error on -1
    mAudioReentrantMonitor("AudioChild.mReentrantMonitor"),
    mIPCOpen(true),
    mDrained(false)
{
  MOZ_COUNT_CTOR(AudioChild);
}

AudioChild::~AudioChild()
{
  MOZ_COUNT_DTOR(AudioChild);
}

void
AudioChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mIPCOpen = false;
}

bool
AudioChild::RecvPositionInFramesUpdate(const PRInt64& position,
                                       const PRInt64& time)
{
  mLastPosition = position;
  mLastPositionTimestamp = time;
  return true;
}

bool
AudioChild::RecvDrainDone()
{
  ReentrantMonitorAutoEnter mon(mAudioReentrantMonitor);
  mDrained = true;
  mAudioReentrantMonitor.NotifyAll();
  return true;
}

PRInt32
AudioChild::WaitForMinWriteSize()
{
  ReentrantMonitorAutoEnter mon(mAudioReentrantMonitor);
  // -2 : initial value
  while (mMinWriteSize == -2 && mIPCOpen) {
    mAudioReentrantMonitor.Wait();
  }
  return mMinWriteSize;
}

bool
AudioChild::RecvMinWriteSizeDone(const PRInt32& minFrames)
{
  ReentrantMonitorAutoEnter mon(mAudioReentrantMonitor);
  mMinWriteSize = minFrames;
  mAudioReentrantMonitor.NotifyAll();
  return true;
}

void
AudioChild::WaitForDrain()
{
  ReentrantMonitorAutoEnter mon(mAudioReentrantMonitor);
  while (!mDrained && mIPCOpen) {
    mAudioReentrantMonitor.Wait();
  }
}

bool
AudioChild::RecvWriteDone()
{
  ReentrantMonitorAutoEnter mon(mAudioReentrantMonitor);
  mWriteCounter += 1;
  mAudioReentrantMonitor.NotifyAll();
  return true;
}

void
AudioChild::WaitForWrite()
{
  ReentrantMonitorAutoEnter mon(mAudioReentrantMonitor);
  PRUint64 writeCounter = mWriteCounter;
  while (mWriteCounter == writeCounter && mIPCOpen) {
    mAudioReentrantMonitor.Wait();
  }
}

PRInt64
AudioChild::GetLastKnownPosition()
{
  return mLastPosition;
}

PRInt64
AudioChild::GetLastKnownPositionTimestamp()
{
  return mLastPositionTimestamp;
}

} // namespace dom
} // namespace mozilla
