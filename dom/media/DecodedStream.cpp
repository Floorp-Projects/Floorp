/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodedStream.h"
#include "MediaStreamGraph.h"
#include "MediaDecoder.h"

namespace mozilla {

class DecodedStreamGraphListener : public MediaStreamListener {
  typedef MediaStreamListener::MediaStreamGraphEvent MediaStreamGraphEvent;
public:
  DecodedStreamGraphListener(MediaStream* aStream, DecodedStreamData* aData)
    : mData(aData)
    , mMutex("DecodedStreamGraphListener::mMutex")
    , mStream(aStream)
    , mLastOutputTime(aStream->StreamTimeToMicroseconds(aStream->GetCurrentTime()))
    , mStreamFinishedOnMainThread(false) {}

  void NotifyOutput(MediaStreamGraph* aGraph, GraphTime aCurrentTime) override
  {
    MutexAutoLock lock(mMutex);
    if (mStream) {
      mLastOutputTime = mStream->StreamTimeToMicroseconds(mStream->GraphTimeToStreamTime(aCurrentTime));
    }
  }

  void NotifyEvent(MediaStreamGraph* aGraph, MediaStreamGraphEvent event) override
  {
    if (event == EVENT_FINISHED) {
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(this, &DecodedStreamGraphListener::DoNotifyFinished);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
    }
  }

  void DoNotifyFinished()
  {
    MutexAutoLock lock(mMutex);
    mStreamFinishedOnMainThread = true;
  }

  int64_t GetLastOutputTime()
  {
    MutexAutoLock lock(mMutex);
    return mLastOutputTime;
  }

  void Forget()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mData = nullptr;
    MutexAutoLock lock(mMutex);
    mStream = nullptr;
  }

  bool IsFinishedOnMainThread()
  {
    MutexAutoLock lock(mMutex);
    return mStreamFinishedOnMainThread;
  }

private:
  // Main thread only
  DecodedStreamData* mData;

  Mutex mMutex;
  // Members below are protected by mMutex.
  nsRefPtr<MediaStream> mStream;
  int64_t mLastOutputTime; // microseconds
  bool mStreamFinishedOnMainThread;
};

DecodedStreamData::DecodedStreamData(MediaDecoder* aDecoder,
                                     int64_t aInitialTime,
                                     SourceMediaStream* aStream)
  : mAudioFramesWritten(0)
  , mInitialTime(aInitialTime)
  , mNextVideoTime(-1)
  , mNextAudioTime(-1)
  , mDecoder(aDecoder)
  , mStreamInitialized(false)
  , mHaveSentFinish(false)
  , mHaveSentFinishAudio(false)
  , mHaveSentFinishVideo(false)
  , mStream(aStream)
  , mHaveBlockedForPlayState(false)
  , mHaveBlockedForStateMachineNotPlaying(false)
  , mEOSVideoCompensation(false)
{
  mListener = new DecodedStreamGraphListener(mStream, this);
  mStream->AddListener(mListener);
}

DecodedStreamData::~DecodedStreamData()
{
  mListener->Forget();
  mStream->Destroy();
}

bool
DecodedStreamData::IsFinished() const
{
  return mListener->IsFinishedOnMainThread();
}

int64_t
DecodedStreamData::GetClock() const
{
  return mInitialTime + mListener->GetLastOutputTime();
}

class OutputStreamListener : public MediaStreamListener {
  typedef MediaStreamListener::MediaStreamGraphEvent MediaStreamGraphEvent;
public:
  OutputStreamListener(MediaDecoder* aDecoder, MediaStream* aStream)
    : mDecoder(aDecoder), mStream(aStream) {}

  void NotifyEvent(MediaStreamGraph* aGraph, MediaStreamGraphEvent event) override
  {
    if (event == EVENT_FINISHED) {
      nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &OutputStreamListener::DoNotifyFinished);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(r.forget());
    }
  }

  void Forget()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mDecoder = nullptr;
  }

private:
  void DoNotifyFinished()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mDecoder) {
      return;
    }

    // Remove the finished stream so it won't block the decoded stream.
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    auto& streams = mDecoder->OutputStreams();
    // Don't read |mDecoder| in the loop since removing the element will lead
    // to ~OutputStreamData() which will call Forget() to reset |mDecoder|.
    for (int32_t i = streams.Length() - 1; i >= 0; --i) {
      auto& os = streams[i];
      MediaStream* p = os.mStream.get();
      if (p == mStream.get()) {
        if (os.mPort) {
          os.mPort->Destroy();
          os.mPort = nullptr;
        }
        streams.RemoveElementAt(i);
        break;
      }
    }
  }

  // Main thread only
  MediaDecoder* mDecoder;
  nsRefPtr<MediaStream> mStream;
};

OutputStreamData::OutputStreamData()
{
  //
}

OutputStreamData::~OutputStreamData()
{
  mListener->Forget();
}

void
OutputStreamData::Init(MediaDecoder* aDecoder, ProcessedMediaStream* aStream)
{
  mStream = aStream;
  mListener = new OutputStreamListener(aDecoder, aStream);
  aStream->AddListener(mListener);
}

} // namespace mozilla
