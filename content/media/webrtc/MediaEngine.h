/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_H_
#define MEDIAENGINE_H_

#include "mozilla/RefPtr.h"
#include "nsIDOMFile.h"
#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"

namespace mozilla {

/**
 * Abstract interface for managing audio and video devices. Each platform
 * must implement a concrete class that will map these classes and methods
 * to the appropriate backend. For example, on Desktop platforms, these will
 * correspond to equivalent webrtc (GIPS) calls, and on B2G they will map to
 * a Gonk interface.
 */
class MediaEngineVideoSource;
class MediaEngineAudioSource;
struct MediaEnginePrefs;

enum MediaEngineState {
  kAllocated,
  kStarted,
  kStopped,
  kReleased
};

// We only support 1 audio and 1 video track for now.
enum {
  kVideoTrack = 1,
  kAudioTrack = 2
};

class MediaEngine : public RefCounted<MediaEngine>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(MediaEngine)
  virtual ~MediaEngine() {}

  static const int DEFAULT_VIDEO_FPS = 30;
  static const int DEFAULT_VIDEO_MIN_FPS = 10;
  static const int DEFAULT_VIDEO_WIDTH = 640;
  static const int DEFAULT_VIDEO_HEIGHT = 480;
  static const int DEFAULT_AUDIO_TIMER_MS = 10;

  /* Populate an array of video sources in the nsTArray. Also include devices
   * that are currently unavailable. */
  virtual void EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >*) = 0;

  /* Populate an array of audio sources in the nsTArray. Also include devices
   * that are currently unavailable. */
  virtual void EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >*) = 0;
};

/**
 * Common abstract base class for audio and video sources.
 */
class MediaEngineSource : public nsISupports
{
public:
  virtual ~MediaEngineSource() {}

  /* Populate the human readable name of this device in the nsAString */
  virtual void GetName(nsAString&) = 0;

  /* Populate the UUID of this device in the nsAString */
  virtual void GetUUID(nsAString&) = 0;

  /* This call reserves but does not start the device. */
  virtual nsresult Allocate(const MediaEnginePrefs &aPrefs) = 0;

  /* Release the device back to the system. */
  virtual nsresult Deallocate() = 0;

  /* Start the device and add the track to the provided SourceMediaStream, with
   * the provided TrackID. You may start appending data to the track
   * immediately after. */
  virtual nsresult Start(SourceMediaStream*, TrackID) = 0;

  /* Take a snapshot from this source. In the case of video this is a single
   * image, and for audio, it is a snippet lasting aDuration milliseconds. The
   * duration argument is ignored for a MediaEngineVideoSource.
   */
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile) = 0;

  /* Called when the stream wants more data */
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime,
                          TrackTicks &aLastEndTime) = 0;

  /* Stop the device and release the corresponding MediaStream */
  virtual nsresult Stop(SourceMediaStream *aSource, TrackID aID) = 0;

  /* Change device configuration.  */
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise) = 0;

  /* Returns true if a source represents a fake capture device and
   * false otherwise
   */
  virtual bool IsFake() = 0;

  /* Return false if device is currently allocated or started */
  bool IsAvailable() {
    if (mState == kAllocated || mState == kStarted) {
      return false;
    } else {
      return true;
    }
  }

  /* It is an error to call Start() before an Allocate(), and Stop() before
   * a Start(). Only Allocate() may be called after a Deallocate(). */

protected:
  MediaEngineState mState;
};

/**
 * Video source and friends.
 */
struct MediaEnginePrefs {
  int32_t mWidth;
  int32_t mHeight;
  int32_t mFPS;
  int32_t mMinFPS;
};

class MediaEngineVideoSource : public MediaEngineSource
{
public:
  virtual ~MediaEngineVideoSource() {}
};

/**
 * Audio source and friends.
 */
class MediaEngineAudioSource : public MediaEngineSource
{
public:
  virtual ~MediaEngineAudioSource() {}
};

}

#endif /* MEDIAENGINE_H_ */
