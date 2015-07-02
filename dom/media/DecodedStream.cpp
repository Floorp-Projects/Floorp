/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodedStream.h"
#include "MediaStreamGraph.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

class DecodedStreamGraphListener : public MediaStreamListener {
  typedef MediaStreamListener::MediaStreamGraphEvent MediaStreamGraphEvent;
public:
  explicit DecodedStreamGraphListener(MediaStream* aStream)
    : mMutex("DecodedStreamGraphListener::mMutex")
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
    MutexAutoLock lock(mMutex);
    mStream = nullptr;
  }

  bool IsFinishedOnMainThread()
  {
    MutexAutoLock lock(mMutex);
    return mStreamFinishedOnMainThread;
  }

private:
  Mutex mMutex;
  // Members below are protected by mMutex.
  nsRefPtr<MediaStream> mStream;
  int64_t mLastOutputTime; // microseconds
  bool mStreamFinishedOnMainThread;
};

static void
UpdateStreamBlocking(MediaStream* aStream, bool aBlocking)
{
  int32_t delta = aBlocking ? 1 : -1;
  if (NS_IsMainThread()) {
    aStream->ChangeExplicitBlockerCount(delta);
  } else {
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<int32_t>(
      aStream, &MediaStream::ChangeExplicitBlockerCount, delta);
    AbstractThread::MainThread()->Dispatch(r.forget());
  }
}

DecodedStreamData::DecodedStreamData(SourceMediaStream* aStream)
  : mAudioFramesWritten(0)
  , mNextVideoTime(-1)
  , mNextAudioTime(-1)
  , mStreamInitialized(false)
  , mHaveSentFinish(false)
  , mHaveSentFinishAudio(false)
  , mHaveSentFinishVideo(false)
  , mStream(aStream)
  , mPlaying(false)
  , mEOSVideoCompensation(false)
{
  mListener = new DecodedStreamGraphListener(mStream);
  mStream->AddListener(mListener);
  // Block the stream as mPlaying is initially false.
  UpdateStreamBlocking(mStream, true);
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
DecodedStreamData::GetPosition() const
{
  return mListener->GetLastOutputTime();
}

void
DecodedStreamData::SetPlaying(bool aPlaying)
{
  if (mPlaying != aPlaying) {
    mPlaying = aPlaying;
    UpdateStreamBlocking(mStream, !mPlaying);
  }
}

class OutputStreamListener : public MediaStreamListener {
  typedef MediaStreamListener::MediaStreamGraphEvent MediaStreamGraphEvent;
public:
  OutputStreamListener(DecodedStream* aDecodedStream, MediaStream* aStream)
    : mDecodedStream(aDecodedStream), mStream(aStream) {}

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
    mDecodedStream = nullptr;
  }

private:
  void DoNotifyFinished()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mDecodedStream) {
      return;
    }

    // Remove the finished stream so it won't block the decoded stream.
    ReentrantMonitorAutoEnter mon(mDecodedStream->GetReentrantMonitor());
    auto& streams = mDecodedStream->OutputStreams();
    // Don't read |mDecodedStream| in the loop since removing the element will lead
    // to ~OutputStreamData() which will call Forget() to reset |mDecodedStream|.
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
  DecodedStream* mDecodedStream;
  nsRefPtr<MediaStream> mStream;
};

OutputStreamData::~OutputStreamData()
{
  mListener->Forget();
}

void
OutputStreamData::Init(DecodedStream* aDecodedStream, ProcessedMediaStream* aStream)
{
  mStream = aStream;
  mListener = new OutputStreamListener(aDecodedStream, aStream);
  aStream->AddListener(mListener);
}

DecodedStream::DecodedStream()
  : mMonitor("DecodedStream::mMonitor")
{
  //
}

DecodedStreamData*
DecodedStream::GetData() const
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return mData.get();
}

void
DecodedStream::DestroyData()
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  // Avoid the redundant blocking to output stream.
  if (!mData) {
    return;
  }

  // All streams are having their SourceMediaStream disconnected, so they
  // need to be explicitly blocked again.
  auto& outputStreams = OutputStreams();
  for (int32_t i = outputStreams.Length() - 1; i >= 0; --i) {
    OutputStreamData& os = outputStreams[i];
    // Explicitly remove all existing ports.
    // This is not strictly necessary but it's good form.
    MOZ_ASSERT(os.mPort, "Double-delete of the ports!");
    os.mPort->Destroy();
    os.mPort = nullptr;
    // During cycle collection, nsDOMMediaStream can be destroyed and send
    // its Destroy message before this decoder is destroyed. So we have to
    // be careful not to send any messages after the Destroy().
    if (os.mStream->IsDestroyed()) {
      // Probably the DOM MediaStream was GCed. Clean up.
      outputStreams.RemoveElementAt(i);
    } else {
      os.mStream->ChangeExplicitBlockerCount(1);
    }
  }

  mData = nullptr;
}

void
DecodedStream::RecreateData(MediaStreamGraph* aGraph)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT((aGraph && !mData && OutputStreams().IsEmpty()) || // first time
             (!aGraph && mData)); // 2nd time and later

  if (!aGraph) {
    aGraph = mData->mStream->Graph();
  }
  auto source = aGraph->CreateSourceStream(nullptr);
  DestroyData();
  mData.reset(new DecodedStreamData(source));

  // Note that the delay between removing ports in DestroyDecodedStream
  // and adding new ones won't cause a glitch since all graph operations
  // between main-thread stable states take effect atomically.
  auto& outputStreams = OutputStreams();
  for (int32_t i = outputStreams.Length() - 1; i >= 0; --i) {
    OutputStreamData& os = outputStreams[i];
    MOZ_ASSERT(!os.mStream->IsDestroyed(), "Should've been removed in DestroyData()");
    Connect(&os);
  }
}

nsTArray<OutputStreamData>&
DecodedStream::OutputStreams()
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  return mOutputStreams;
}

ReentrantMonitor&
DecodedStream::GetReentrantMonitor() const
{
  return mMonitor;
}

void
DecodedStream::Connect(OutputStreamData* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(!aStream->mPort, "Already connected?");

  // The output stream must stay in sync with the decoded stream, so if
  // either stream is blocked, we block the other.
  aStream->mPort = aStream->mStream->AllocateInputPort(mData->mStream,
      MediaInputPort::FLAG_BLOCK_INPUT | MediaInputPort::FLAG_BLOCK_OUTPUT);
  // Unblock the output stream now. While it's connected to DecodedStream,
  // DecodedStream is responsible for controlling blocking.
  aStream->mStream->ChangeExplicitBlockerCount(-1);
}

void
DecodedStream::Connect(ProcessedMediaStream* aStream, bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  OutputStreamData* os = OutputStreams().AppendElement();
  os->Init(this, aStream);
  Connect(os);
  if (aFinishWhenEnded) {
    // Ensure that aStream finishes the moment mDecodedStream does.
    aStream->SetAutofinish(true);
  }
}

void
DecodedStream::SetPlaying(bool aPlaying)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(mData);
  mData->SetPlaying(aPlaying);
}

} // namespace mozilla
