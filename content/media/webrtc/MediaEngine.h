/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_H_
#define MEDIAENGINE_H_

#include "mozilla/RefPtr.h"
#include "nsIDOMFile.h"
#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"

namespace mozilla {

struct VideoTrackConstraintsN;
struct AudioTrackConstraintsN;

/**
 * Abstract interface for managing audio and video devices. Each platform
 * must implement a concrete class that will map these classes and methods
 * to the appropriate backend. For example, on Desktop platforms, these will
 * correspond to equivalent webrtc (GIPS) calls, and on B2G they will map to
 * a Gonk interface.
 */
class MediaEngineVideoSource;
class MediaEngineAudioSource;
class MediaEnginePrefs;

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

// includes everything from dom::MediaSourceEnum (really video sources), plus audio sources
enum MediaSourceType {
  Camera = (int) dom::MediaSourceEnum::Camera,
  Screen = (int) dom::MediaSourceEnum::Screen,
  Application = (int) dom::MediaSourceEnum::Application,
  Window, // = (int) dom::MediaSourceEnum::Window, // XXX bug 1038926
  //Browser = (int) dom::MediaSourceEnum::Browser, // proposed in WG, unclear if it's useful
  Microphone
};

class MediaEngine
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEngine)

  static const int DEFAULT_VIDEO_FPS = 30;
  static const int DEFAULT_VIDEO_MIN_FPS = 10;
  static const int DEFAULT_43_VIDEO_WIDTH = 640;
  static const int DEFAULT_43_VIDEO_HEIGHT = 480;
  static const int DEFAULT_169_VIDEO_WIDTH = 1280;
  static const int DEFAULT_169_VIDEO_HEIGHT = 720;
  static const int DEFAULT_AUDIO_TIMER_MS = 10;

  /* Populate an array of video sources in the nsTArray. Also include devices
   * that are currently unavailable. */
  virtual void EnumerateVideoDevices(MediaSourceType,
                                     nsTArray<nsRefPtr<MediaEngineVideoSource> >*) = 0;

  /* Populate an array of audio sources in the nsTArray. Also include devices
   * that are currently unavailable. */
  virtual void EnumerateAudioDevices(MediaSourceType,
                                     nsTArray<nsRefPtr<MediaEngineAudioSource> >*) = 0;

protected:
  virtual ~MediaEngine() {}
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

  /* Release the device back to the system. */
  virtual nsresult Deallocate() = 0;

  /* Start the device and add the track to the provided SourceMediaStream, with
   * the provided TrackID. You may start appending data to the track
   * immediately after. */
  virtual nsresult Start(SourceMediaStream*, TrackID) = 0;

  /* tell the source if there are any direct listeners attached */
  virtual void SetDirectListeners(bool) = 0;

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
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) = 0;

  /* Returns true if a source represents a fake capture device and
   * false otherwise
   */
  virtual bool IsFake() = 0;

  /* Returns the type of media source (camera, microphone, screen, window, etc) */
  virtual const MediaSourceType GetMediaSource() = 0;

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
class MediaEnginePrefs {
public:
  int32_t mWidth;
  int32_t mHeight;
  int32_t mFPS;
  int32_t mMinFPS;

  // mWidth and/or mHeight may be zero (=adaptive default), so use functions.

  int32_t GetWidth(bool aHD = false) const {
    return mWidth? mWidth : (mHeight?
                             (mHeight * GetDefWidth(aHD)) / GetDefHeight(aHD) :
                             GetDefWidth(aHD));
  }

  int32_t GetHeight(bool aHD = false) const {
    return mHeight? mHeight : (mWidth?
                               (mWidth * GetDefHeight(aHD)) / GetDefWidth(aHD) :
                               GetDefHeight(aHD));
  }
private:
  static int32_t GetDefWidth(bool aHD = false) {
    // It'd be nice if we could use the ternary operator here, but we can't
    // because of bug 1002729.
    if (aHD) {
      return MediaEngine::DEFAULT_169_VIDEO_WIDTH;
    }

    return MediaEngine::DEFAULT_43_VIDEO_WIDTH;
  }

  static int32_t GetDefHeight(bool aHD = false) {
    // It'd be nice if we could use the ternary operator here, but we can't
    // because of bug 1002729.
    if (aHD) {
      return MediaEngine::DEFAULT_169_VIDEO_HEIGHT;
    }

    return MediaEngine::DEFAULT_43_VIDEO_HEIGHT;
  }
};

class MediaEngineVideoSource : public MediaEngineSource
{
public:
  virtual ~MediaEngineVideoSource() {}

  virtual const MediaSourceType GetMediaSource() {
      return MediaSourceType::Camera;
  }
  /* This call reserves but does not start the device. */
  virtual nsresult Allocate(const VideoTrackConstraintsN &aConstraints,
                            const MediaEnginePrefs &aPrefs) = 0;
};

/**
 * Audio source and friends.
 */
class MediaEngineAudioSource : public MediaEngineSource
{
public:
  virtual ~MediaEngineAudioSource() {}

  /* This call reserves but does not start the device. */
  virtual nsresult Allocate(const AudioTrackConstraintsN &aConstraints,
                            const MediaEnginePrefs &aPrefs) = 0;

};

}

#endif /* MEDIAENGINE_H_ */
