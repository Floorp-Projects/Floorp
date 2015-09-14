/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "nsTArray.h"
#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "MediaSink.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/nsRefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class DecodedStream;
class DecodedStreamData;
class MediaData;
class MediaInputPort;
class MediaStream;
class MediaStreamGraph;
class OutputStreamManager;
class ProcessedMediaStream;
class TimeStamp;

template <class T> class MediaQueue;

class OutputStreamData {
public:
  ~OutputStreamData();
  void Init(OutputStreamManager* aOwner, ProcessedMediaStream* aStream);

  // Connect mStream to the input stream.
  void Connect(MediaStream* aStream);
  // Disconnect mStream from its input stream.
  // Return false is mStream is already destroyed, otherwise true.
  bool Disconnect();
  // Return true if aStream points to the same object as mStream.
  // Used by OutputStreamManager to remove an output stream.
  bool Equals(MediaStream* aStream)
  {
    return mStream == aStream;
  }
  // Return the graph mStream belongs to.
  MediaStreamGraph* Graph() const;

private:
  OutputStreamManager* mOwner;
  nsRefPtr<ProcessedMediaStream> mStream;
  // mPort connects our mStream to an input stream.
  nsRefPtr<MediaInputPort> mPort;
};

class OutputStreamManager {
public:
  // Add the output stream to the collection.
  void Add(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  // Remove the output stream from the collection.
  void Remove(MediaStream* aStream);
  // Return true if the collection empty.
  bool IsEmpty() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mStreams.IsEmpty();
  }
  // Connect all output streams in the collection to the input stream.
  void Connect(MediaStream* aStream);
  // Disconnect all output streams from the input stream.
  void Disconnect();
  // Return the graph these streams belong to or null if empty.
  MediaStreamGraph* Graph() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return !IsEmpty() ? mStreams[0].Graph() : nullptr;
  }

private:
  // Keep the input stream so we can connect the output streams that
  // are added after Connect().
  nsRefPtr<MediaStream> mInputStream;
  nsTArray<OutputStreamData> mStreams;
};

class DecodedStream : public media::MediaSink {
  using media::MediaSink::PlaybackParams;

public:
  DecodedStream(AbstractThread* aOwnerThread,
                MediaQueue<MediaData>& aAudioQueue,
                MediaQueue<MediaData>& aVideoQueue);

  // MediaSink functions.
  const PlaybackParams& GetPlaybackParams() const override;
  void SetPlaybackParams(const PlaybackParams& aParams) override;

  nsRefPtr<GenericPromise> OnEnded(TrackType aType) override;
  int64_t GetEndTime(TrackType aType) const override;
  int64_t GetPosition(TimeStamp* aTimeStamp = nullptr) const override;
  bool HasUnplayedFrames(TrackType aType) const override
  {
    // TODO: implement this.
    return false;
  }

  void SetVolume(double aVolume) override;
  void SetPlaybackRate(double aPlaybackRate) override;
  void SetPreservesPitch(bool aPreservesPitch) override;
  void SetPlaying(bool aPlaying) override;

  void Start(int64_t aStartTime, const MediaInfo& aInfo) override;
  void Stop() override;
  bool IsStarted() const override;
  bool IsPlaying() const override;

  // TODO: fix these functions that don't fit into the interface of MediaSink.
  void BeginShutdown();
  void AddOutput(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  void RemoveOutput(MediaStream* aStream);
  void SetSameOrigin(bool aSameOrigin);
  bool IsFinished() const;
  bool HasConsumers() const;

protected:
  virtual ~DecodedStream();

private:
  void CreateData(MozPromiseHolder<GenericPromise>&& aPromise);
  void DestroyData(UniquePtr<DecodedStreamData> aData);
  void OnDataCreated(UniquePtr<DecodedStreamData> aData);
  void InitTracks();
  void AdvanceTracks();
  void SendAudio(double aVolume, bool aIsSameOrigin);
  void SendVideo(bool aIsSameOrigin);
  void SendData();

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  void ConnectListener();
  void DisconnectListener();

  const nsRefPtr<AbstractThread> mOwnerThread;

  /*
   * Main thread only members.
   */
  // Data about MediaStreams that are being fed by the decoder.
  OutputStreamManager mOutputStreamManager;
  // True if MDSM has begun shutdown.
  bool mShuttingDown;

  /*
   * Worker thread only members.
   */
  UniquePtr<DecodedStreamData> mData;
  nsRefPtr<GenericPromise> mFinishPromise;

  bool mPlaying;
  bool mSameOrigin;
  PlaybackParams mParams;

  Maybe<int64_t> mStartTime;
  MediaInfo mInfo;

  MediaQueue<MediaData>& mAudioQueue;
  MediaQueue<MediaData>& mVideoQueue;

  MediaEventListener mAudioPushListener;
  MediaEventListener mVideoPushListener;
  MediaEventListener mAudioFinishListener;
  MediaEventListener mVideoFinishListener;
};

} // namespace mozilla

#endif // DecodedStream_h_
