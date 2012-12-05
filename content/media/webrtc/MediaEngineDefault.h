/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEDEFAULT_H_
#define MEDIAENGINEDEFAULT_H_

#include "nsITimer.h"

#include "nsCOMPtr.h"
#include "nsDOMMediaStream.h"
#include "nsComponentManagerUtils.h"

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

  virtual const MediaEngineVideoOptions *GetOptions();
  virtual nsresult Allocate();
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  // Need something better...
  static const int DEFAULT_WIDTH=640;
  static const int DEFAULT_HEIGHT=480;
  static const int DEFAULT_FPS=30;

protected:
  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<layers::ImageContainer> mImageContainer;

  SourceMediaStream* mSource;
  layers::PlanarYCbCrImage* mImage;
  static const MediaEngineVideoOptions mOpts;
  int mCb;
  int mCr;
};

class MediaEngineDefaultAudioSource : public nsITimerCallback,
                                      public MediaEngineAudioSource
{
public:
  MediaEngineDefaultAudioSource();
  ~MediaEngineDefaultAudioSource();

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);

  virtual nsresult Allocate();
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;

  SourceMediaStream* mSource;
};

class MediaEngineDefault : public MediaEngine
{
public:
  MediaEngineDefault()
  : mMutex("mozilla::MediaEngineDefault")
  {}
  ~MediaEngineDefault() {}

  virtual void EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >*);
  virtual void EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >*);

private:
  Mutex mMutex;
  // protected with mMutex:

  nsTArray<nsRefPtr<MediaEngineVideoSource> > mVSources;
  nsTArray<nsRefPtr<MediaEngineAudioSource> > mASources;
};

}

#endif /* NSMEDIAENGINEDEFAULT_H_ */
