/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEDEFAULT_H_
#define MEDIAENGINEDEFAULT_H_

#include "prmem.h"
#include "nsITimer.h"

#include "nsCOMPtr.h"
#include "nsDOMMediaStream.h"
#include "nsComponentManagerUtils.h"

#include "Layers.h"
#include "VideoUtils.h"
#include "MediaEngine.h"
#include "ImageLayers.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamBuffer.h"
#include "MediaStreamGraph.h"

namespace mozilla {

/**
 * The default implementation of the MediaEngine interface.
 */

enum DefaultEngineState {
  kAllocated,
  kStarted,
  kStopped,
  kReleased
};

class MediaEngineDefaultVideoSource : public nsITimerCallback,
                                      public MediaEngineVideoSource
{
public:
  MediaEngineDefaultVideoSource() : mTimer(nsnull), mState(kReleased) {}
  ~MediaEngineDefaultVideoSource(){};

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);

  virtual MediaEngineVideoOptions GetOptions();
  virtual nsresult Allocate();

  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(PRUint32 aDuration, nsIDOMFile** aFile);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<layers::ImageContainer> mImageContainer;

  DefaultEngineState mState;
  SourceMediaStream* mSource;
  layers::PlanarYCbCrImage* mImage;
};

class MediaEngineDefaultAudioSource : public nsITimerCallback,
                                      public MediaEngineAudioSource
{
public:
  MediaEngineDefaultAudioSource() : mTimer(nsnull), mState(kReleased) {}
  ~MediaEngineDefaultAudioSource(){};

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);

  virtual nsresult Allocate();

  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(PRUint32 aDuration, nsIDOMFile** aFile);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;

  DefaultEngineState mState;
  SourceMediaStream* mSource;
};

class MediaEngineDefault : public MediaEngine
{
public:
  MediaEngineDefault() {
    mVSource = new MediaEngineDefaultVideoSource();
    mASource = new MediaEngineDefaultAudioSource();
  }
  ~MediaEngineDefault() {}

  virtual void EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >*);
  virtual void EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >*);

private:
  nsRefPtr<MediaEngineVideoSource> mVSource;
  nsRefPtr<MediaEngineAudioSource> mASource;
};

}

#endif /* NSMEDIAENGINEDEFAULT_H_ */
