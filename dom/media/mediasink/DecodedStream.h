/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "MediaSegment.h"
#include "MediaSink.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class DecodedStreamData;
class AudioData;
class VideoData;
class MediaStream;
class OutputStreamManager;
struct PlaybackInfoInit;
class ProcessedMediaStream;
class TimeStamp;

template <class T>
class MediaQueue;

class DecodedStream : public MediaSink {
  using MediaSink::PlaybackParams;

 public:
  DecodedStream(AbstractThread* aOwnerThread, AbstractThread* aMainThread,
                MediaQueue<AudioData>& aAudioQueue,
                MediaQueue<VideoData>& aVideoQueue,
                OutputStreamManager* aOutputStreamManager,
                const bool& aSameOrigin);

  // MediaSink functions.
  const PlaybackParams& GetPlaybackParams() const override;
  void SetPlaybackParams(const PlaybackParams& aParams) override;

  RefPtr<EndedPromise> OnEnded(TrackType aType) override;
  media::TimeUnit GetEndTime(TrackType aType) const override;
  media::TimeUnit GetPosition(TimeStamp* aTimeStamp = nullptr) const override;
  bool HasUnplayedFrames(TrackType aType) const override {
    // TODO: implement this.
    return false;
  }

  void SetVolume(double aVolume) override;
  void SetPlaybackRate(double aPlaybackRate) override;
  void SetPreservesPitch(bool aPreservesPitch) override;
  void SetPlaying(bool aPlaying) override;

  nsresult Start(const media::TimeUnit& aStartTime,
                 const MediaInfo& aInfo) override;
  void Stop() override;
  bool IsStarted() const override;
  bool IsPlaying() const override;
  void Shutdown() override;

  nsCString GetDebugInfo() override;

 protected:
  virtual ~DecodedStream();

 private:
  void DestroyData(UniquePtr<DecodedStreamData>&& aData);
  void SendAudio(double aVolume, bool aIsSameOrigin,
                 const PrincipalHandle& aPrincipalHandle);
  void SendVideo(bool aIsSameOrigin, const PrincipalHandle& aPrincipalHandle);
  void ResetVideo(const PrincipalHandle& aPrincipalHandle);
  StreamTime SentDuration();
  void SendData();
  void NotifyOutput(int64_t aTime);

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  void PlayingChanged();

  void ConnectListener();
  void DisconnectListener();

  const RefPtr<AbstractThread> mOwnerThread;

  const RefPtr<AbstractThread> mAbstractMainThread;

  /*
   * Main thread only members.
   */
  // Data about MediaStreams that are being fed by the decoder.
  const RefPtr<OutputStreamManager> mOutputStreamManager;

  /*
   * Worker thread only members.
   */
  WatchManager<DecodedStream> mWatchManager;
  UniquePtr<DecodedStreamData> mData;
  RefPtr<EndedPromise> mAudioEndedPromise;
  RefPtr<EndedPromise> mVideoEndedPromise;

  Watchable<bool> mPlaying;
  const bool& mSameOrigin;  // valid until Shutdown() is called.
  Mirror<PrincipalHandle> mPrincipalHandle;

  PlaybackParams mParams;

  media::NullableTimeUnit mStartTime;
  media::TimeUnit mLastOutputTime;
  StreamTime mStreamTimeOffset = 0;
  MediaInfo mInfo;

  MediaQueue<AudioData>& mAudioQueue;
  MediaQueue<VideoData>& mVideoQueue;

  MediaEventListener mAudioPushListener;
  MediaEventListener mVideoPushListener;
  MediaEventListener mAudioFinishListener;
  MediaEventListener mVideoFinishListener;
  MediaEventListener mOutputListener;
};

}  // namespace mozilla

#endif  // DecodedStream_h_
