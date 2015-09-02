/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioSinkWrapper_h_
#define AudioSinkWrapper_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/nsRefPtr.h"
#include "mozilla/UniquePtr.h"

#include "MediaSink.h"

namespace mozilla {

class MediaData;
template <class T> class MediaQueue;

namespace media {

class AudioSink;

/**
 * A wrapper around AudioSink to provide the interface of MediaSink.
 */
class AudioSinkWrapper : public MediaSink {
  // An AudioSink factory.
  class Creator {
  public:
    virtual ~Creator() {}
    virtual AudioSink* Create() = 0;
  };

  // Wrap around a function object which creates AudioSinks.
  template <typename Function>
  class CreatorImpl : public Creator {
  public:
    explicit CreatorImpl(const Function& aFunc) : mFunction(aFunc) {}
    AudioSink* Create() override { return mFunction(); }
  private:
    Function mFunction;
  };

public:
  template <typename Function>
  AudioSinkWrapper(AbstractThread* aOwnerThread, const Function& aFunc)
    : mOwnerThread(aOwnerThread)
    , mCreator(new CreatorImpl<Function>(aFunc))
    , mIsStarted(false)
  {}

  const PlaybackParams& GetPlaybackParams() const override;
  void SetPlaybackParams(const PlaybackParams& aParams) override;

  nsRefPtr<GenericPromise> OnEnded(TrackType aType) override;
  int64_t GetEndTime(TrackType aType) const override;
  int64_t GetPosition() const override;
  bool HasUnplayedFrames(TrackType aType) const override;

  void SetVolume(double aVolume) override;
  void SetPlaybackRate(double aPlaybackRate) override;
  void SetPreservesPitch(bool aPreservesPitch) override;
  void SetPlaying(bool aPlaying) override;

  void Start(int64_t aStartTime, const MediaInfo& aInfo) override;
  void Stop() override;
  bool IsStarted() const override;

  void Shutdown() override;

private:
  virtual ~AudioSinkWrapper();

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  const nsRefPtr<AbstractThread> mOwnerThread;
  UniquePtr<Creator> mCreator;
  nsRefPtr<AudioSink> mAudioSink;
  nsRefPtr<GenericPromise> mEndPromise;

  bool mIsStarted;
  PlaybackParams mParams;
};

} // namespace media
} // namespace mozilla

#endif //AudioSinkWrapper_h_
