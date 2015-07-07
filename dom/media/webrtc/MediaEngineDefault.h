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
}

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

  virtual void Shutdown() override {};

  virtual void GetName(nsAString&) override;
  virtual void GetUUID(nsACString&) override;

  virtual nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                            const MediaEnginePrefs &aPrefs,
                            const nsString& aDeviceId) override;
  virtual nsresult Deallocate() override;
  virtual nsresult Start(SourceMediaStream*, TrackID) override;
  virtual nsresult Stop(SourceMediaStream*, TrackID) override;
  virtual void SetDirectListeners(bool aHasDirectListeners) override {};
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) override { return NS_OK; };
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime) override;
  virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override;

  virtual bool IsFake() override {
    return true;
  }

  virtual const dom::MediaSourceEnum GetMediaSource() override {
    return dom::MediaSourceEnum::Camera;
  }

  virtual nsresult TakePhoto(PhotoCallback* aCallback) override
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
  nsRefPtr<layers::Image> mImage;

  nsRefPtr<layers::ImageContainer> mImageContainer;

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

  virtual void Shutdown() override {};

  virtual void GetName(nsAString&) override;
  virtual void GetUUID(nsACString&) override;

  virtual nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                            const MediaEnginePrefs &aPrefs,
                            const nsString& aDeviceId) override;
  virtual nsresult Deallocate() override;
  virtual nsresult Start(SourceMediaStream*, TrackID) override;
  virtual nsresult Stop(SourceMediaStream*, TrackID) override;
  virtual void SetDirectListeners(bool aHasDirectListeners) override {};
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) override { return NS_OK; };
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime) override {}

  virtual bool IsFake() override {
    return true;
  }

  virtual const dom::MediaSourceEnum GetMediaSource() override {
    return dom::MediaSourceEnum::Microphone;
  }

  virtual nsresult TakePhoto(PhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  ~MediaEngineDefaultAudioSource();

  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;

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

  virtual void EnumerateVideoDevices(dom::MediaSourceEnum,
                                     nsTArray<nsRefPtr<MediaEngineVideoSource> >*) override;
  virtual void EnumerateAudioDevices(dom::MediaSourceEnum,
                                     nsTArray<nsRefPtr<MediaEngineAudioSource> >*) override;
  virtual void Shutdown() override {
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

  nsTArray<nsRefPtr<MediaEngineVideoSource> > mVSources;
  nsTArray<nsRefPtr<MediaEngineAudioSource> > mASources;
};

}

#endif /* NSMEDIAENGINEDEFAULT_H_ */
