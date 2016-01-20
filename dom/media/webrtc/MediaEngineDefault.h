/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEDEFAULT_H_
#define MEDIAENGINEDEFAULT_H_

#include "nsITimer.h"

#include "nsCOMPtr.h"
#include "DOMMediaStream.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Monitor.h"

#include "VideoUtils.h"
#include "MediaEngine.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamBuffer.h"
#include "MediaStreamGraph.h"
#include "MediaTrackConstraints.h"

namespace mozilla {

namespace layers {
class ImageContainer;
} // namespace layers

class MediaEngineDefault;

/**
 * The default implementation of the MediaEngine interface.
 */
class MediaEngineDefaultVideoSource : public nsITimerCallback,
                                      public MediaEngineVideoSource,
                                      private MediaConstraintsHelper
{
public:
  MediaEngineDefaultVideoSource();

  void Shutdown() override {};

  void GetName(nsAString&) override;
  void GetUUID(nsACString&) override;

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId) override;
  nsresult Deallocate() override;
  nsresult Start(SourceMediaStream*, TrackID) override;
  nsresult Stop(SourceMediaStream*, TrackID) override;
  nsresult Restart(const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId) override;
  void SetDirectListeners(bool aHasDirectListeners) override {};
  nsresult Config(bool aEchoOn, uint32_t aEcho,
                  bool aAgcOn, uint32_t aAGC,
                  bool aNoiseOn, uint32_t aNoise,
                  int32_t aPlayoutDelay) override { return NS_OK; };
  void NotifyPull(MediaStreamGraph* aGraph,
                  SourceMediaStream *aSource,
                  TrackID aId,
                  StreamTime aDesiredTime) override;
  uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override;

  bool IsFake() override {
    return true;
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Camera;
  }

  nsresult TakePhoto(PhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  ~MediaEngineDefaultVideoSource();

  friend class MediaEngineDefault;

  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;
  // mMonitor protects mImage access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack() and
  // image changes).  Note that mSources is not accessed from other threads
  // for video and is not protected.
  Monitor mMonitor;
  RefPtr<layers::Image> mImage;

  RefPtr<layers::ImageContainer> mImageContainer;

  MediaEnginePrefs mOpts;
  int mCb;
  int mCr;
};

class SineWaveGenerator;

class MediaEngineDefaultAudioSource : public nsITimerCallback,
                                      public MediaEngineAudioSource,
                                      private MediaConstraintsHelper
{
public:
  MediaEngineDefaultAudioSource();

  void Shutdown() override {};

  void GetName(nsAString&) override;
  void GetUUID(nsACString&) override;

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId) override;
  nsresult Deallocate() override;
  nsresult Start(SourceMediaStream*, TrackID) override;
  nsresult Stop(SourceMediaStream*, TrackID) override;
  nsresult Restart(const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId) override;
  void SetDirectListeners(bool aHasDirectListeners) override {};
  nsresult Config(bool aEchoOn, uint32_t aEcho,
                  bool aAgcOn, uint32_t aAGC,
                  bool aNoiseOn, uint32_t aNoise,
                  int32_t aPlayoutDelay) override { return NS_OK; };
  void AppendToSegment(AudioSegment& aSegment, TrackTicks aSamples);
  void NotifyPull(MediaStreamGraph* aGraph,
                  SourceMediaStream *aSource,
                  TrackID aId,
                  StreamTime aDesiredTime) override
  {
#ifdef DEBUG
    StreamBuffer::Track* data = aSource->FindTrack(aId);
    NS_WARN_IF_FALSE(!data || data->IsEnded() ||
                     aDesiredTime <= aSource->GetEndOfAppendedData(aId),
                     "MediaEngineDefaultAudioSource data underrun");
#endif
  }

  bool IsFake() override {
    return true;
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Microphone;
  }

  nsresult TakePhoto(PhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  ~MediaEngineDefaultAudioSource();

  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;

  TimeStamp mLastNotify;
  TrackTicks mBufferSize;

  SourceMediaStream* mSource;
  nsAutoPtr<SineWaveGenerator> mSineGenerator;
};


class MediaEngineDefault : public MediaEngine
{
public:
  explicit MediaEngineDefault(bool aHasFakeTracks = false)
    : mHasFakeTracks(aHasFakeTracks)
    , mMutex("mozilla::MediaEngineDefault")
  {}

  void EnumerateVideoDevices(dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaEngineVideoSource> >*) override;
  void EnumerateAudioDevices(dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaEngineAudioSource> >*) override;
  void Shutdown() override {
    MutexAutoLock lock(mMutex);

    mVSources.Clear();
    mASources.Clear();
  };

protected:
  bool mHasFakeTracks;

private:
  ~MediaEngineDefault() {
    Shutdown();
  }

  Mutex mMutex;
  // protected with mMutex:

  nsTArray<RefPtr<MediaEngineVideoSource> > mVSources;
  nsTArray<RefPtr<MediaEngineAudioSource> > mASources;
};

} // namespace mozilla

#endif /* NSMEDIAENGINEDEFAULT_H_ */
