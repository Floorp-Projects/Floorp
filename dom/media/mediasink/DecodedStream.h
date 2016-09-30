/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "MediaSink.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class DecodedStreamData;
class MediaData;
class MediaStream;
class OutputStreamManager;
struct PlaybackInfoInit;
class ProcessedMediaStream;
class TimeStamp;

template <class T> class MediaQueue;

class DecodedStream : public media::MediaSink {
  using media::MediaSink::PlaybackParams;

public:
  DecodedStream(AbstractThread* aOwnerThread,
                MediaQueue<MediaData>& aAudioQueue,
                MediaQueue<MediaData>& aVideoQueue,
                OutputStreamManager* aOutputStreamManager,
                const bool& aSameOrigin,
                const PrincipalHandle& aPrincipalHandle);

  // MediaSink functions.
  const PlaybackParams& GetPlaybackParams() const override;
  void SetPlaybackParams(const PlaybackParams& aParams) override;

  RefPtr<GenericPromise> OnEnded(TrackType aType) override;
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

protected:
  virtual ~DecodedStream();

private:
  void DestroyData(UniquePtr<DecodedStreamData> aData);
  void AdvanceTracks();
  void SendAudio(double aVolume, bool aIsSameOrigin, const PrincipalHandle& aPrincipalHandle);
  void SendVideo(bool aIsSameOrigin, const PrincipalHandle& aPrincipalHandle);
  void SendData();
  void NotifyOutput(int64_t aTime);

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  void ConnectListener();
  void DisconnectListener();

  const RefPtr<AbstractThread> mOwnerThread;

  /*
   * Main thread only members.
   */
  // Data about MediaStreams that are being fed by the decoder.
  const RefPtr<OutputStreamManager> mOutputStreamManager;

  /*
   * Worker thread only members.
   */
  UniquePtr<DecodedStreamData> mData;
  RefPtr<GenericPromise> mFinishPromise;

  bool mPlaying;
  const bool& mSameOrigin; // valid until Shutdown() is called.
  const PrincipalHandle& mPrincipalHandle; // valid until Shutdown() is called.

  PlaybackParams mParams;

  Maybe<int64_t> mStartTime;
  int64_t mLastOutputTime = 0; // microseconds
  MediaInfo mInfo;

  MediaQueue<MediaData>& mAudioQueue;
  MediaQueue<MediaData>& mVideoQueue;

  MediaEventListener mAudioPushListener;
  MediaEventListener mVideoPushListener;
  MediaEventListener mAudioFinishListener;
  MediaEventListener mVideoFinishListener;
  MediaEventListener mOutputListener;
};

} // namespace mozilla

#endif // DecodedStream_h_
