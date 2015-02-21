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

namespace mozilla {

namespace layers {
class ImageContainer;
class PlanarYCbCrImage;
}

class MediaEngineDefault;

/**
 * The default implementation of the MediaEngine interface.
 */
class MediaEngineDefaultVideoSource : public nsITimerCallback,
                                      public MediaEngineVideoSource
{
public:
  MediaEngineDefaultVideoSource();

  virtual void GetName(nsAString&) MOZ_OVERRIDE;
  virtual void GetUUID(nsAString&) MOZ_OVERRIDE;

  virtual nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                            const MediaEnginePrefs &aPrefs) MOZ_OVERRIDE;
  virtual nsresult Deallocate() MOZ_OVERRIDE;
  virtual nsresult Start(SourceMediaStream*, TrackID) MOZ_OVERRIDE;
  virtual nsresult Stop(SourceMediaStream*, TrackID) MOZ_OVERRIDE;
  virtual void SetDirectListeners(bool aHasDirectListeners) MOZ_OVERRIDE {};
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) MOZ_OVERRIDE { return NS_OK; };
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime) MOZ_OVERRIDE;
  virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets) MOZ_OVERRIDE
  {
    return true;
  }

  virtual bool IsFake() MOZ_OVERRIDE {
    return true;
  }

  virtual const dom::MediaSourceEnum GetMediaSource() MOZ_OVERRIDE {
    return dom::MediaSourceEnum::Camera;
  }

  virtual nsresult TakePhoto(PhotoCallback* aCallback) MOZ_OVERRIDE
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
                                      public MediaEngineAudioSource
{
public:
  MediaEngineDefaultAudioSource();

  virtual void GetName(nsAString&) MOZ_OVERRIDE;
  virtual void GetUUID(nsAString&) MOZ_OVERRIDE;

  virtual nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                            const MediaEnginePrefs &aPrefs) MOZ_OVERRIDE;
  virtual nsresult Deallocate() MOZ_OVERRIDE;
  virtual nsresult Start(SourceMediaStream*, TrackID) MOZ_OVERRIDE;
  virtual nsresult Stop(SourceMediaStream*, TrackID) MOZ_OVERRIDE;
  virtual void SetDirectListeners(bool aHasDirectListeners) MOZ_OVERRIDE {};
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) MOZ_OVERRIDE { return NS_OK; };
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime) MOZ_OVERRIDE {}

  virtual bool IsFake() MOZ_OVERRIDE {
    return true;
  }

  virtual const dom::MediaSourceEnum GetMediaSource() MOZ_OVERRIDE {
    return dom::MediaSourceEnum::Microphone;
  }

  virtual nsresult TakePhoto(PhotoCallback* aCallback) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

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
                                     nsTArray<nsRefPtr<MediaEngineVideoSource> >*);
  virtual void EnumerateAudioDevices(dom::MediaSourceEnum,
                                     nsTArray<nsRefPtr<MediaEngineAudioSource> >*);

protected:
  bool mHasFakeTracks;

private:
  ~MediaEngineDefault() {}

  Mutex mMutex;
  // protected with mMutex:

  nsTArray<nsRefPtr<MediaEngineVideoSource> > mVSources;
  nsTArray<nsRefPtr<MediaEngineAudioSource> > mASources;
};

}

#endif /* NSMEDIAENGINEDEFAULT_H_ */
