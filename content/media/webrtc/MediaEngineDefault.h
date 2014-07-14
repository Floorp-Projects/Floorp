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
  ~MediaEngineDefaultVideoSource();

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);

  virtual nsresult Allocate(const VideoTrackConstraintsN &aConstraints,
                            const MediaEnginePrefs &aPrefs);
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop(SourceMediaStream*, TrackID);
  virtual void SetDirectListeners(bool aHasDirectListeners) {};
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) { return NS_OK; };
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime,
                          TrackTicks &aLastEndTime);

  virtual bool IsFake() {
    return true;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
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
  ~MediaEngineDefaultAudioSource();

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);

  virtual nsresult Allocate(const AudioTrackConstraintsN &aConstraints,
                            const MediaEnginePrefs &aPrefs);
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop(SourceMediaStream*, TrackID);
  virtual void SetDirectListeners(bool aHasDirectListeners) {};
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) { return NS_OK; };
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime,
                          TrackTicks &aLastEndTime) {}

  virtual bool IsFake() {
    return true;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;

  SourceMediaStream* mSource;
  nsAutoPtr<SineWaveGenerator> mSineGenerator;
};


class MediaEngineDefault : public MediaEngine
{
public:
  MediaEngineDefault()
  : mMutex("mozilla::MediaEngineDefault")
  {}

  virtual void EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >*);
  virtual void EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >*);

private:
  ~MediaEngineDefault() {}

  Mutex mMutex;
  // protected with mMutex:

  nsTArray<nsRefPtr<MediaEngineVideoSource> > mVSources;
  nsTArray<nsRefPtr<MediaEngineAudioSource> > mASources;
};

}

#endif /* NSMEDIAENGINEDEFAULT_H_ */
