/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_H_
#define MEDIAENGINE_H_

#include "nsIDOMFile.h"
#include "nsDOMMediaStream.h"
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

class MediaEngine
{
public:
  virtual ~MediaEngine() {};

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
  virtual ~MediaEngineSource() {};

  /* Populate the human readable name of this device in the nsAString */
  virtual void GetName(nsAString&) = 0;

  /* Populate the UUID of this device in the nsAString */
  virtual void GetUUID(nsAString&) = 0;

  /* This call reserves but does not start the device. */
  virtual already_AddRefed<nsDOMMediaStream> Allocate() = 0;

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
  virtual nsresult Snapshot(PRUint32 aDuration, nsIDOMFile** aFile) = 0;

  /* Stop the device and release the corresponding MediaStream */
  virtual nsresult Stop() = 0;

  /* It is an error to call Start() before an Allocate(), and Stop() before
   * a Start(). Only Allocate() may be called after a Deallocate(). */
};

/**
 * Video source and friends.
 */
enum MediaEngineVideoCodecType {
  kVideoCodecH263,
  kVideoCodecVP8,
  kVideoCodecI420
};

struct MediaEngineVideoOptions {
  PRUint32 mWidth;
  PRUint32 mHeight;
  PRUint32 mMaxFPS;
  MediaEngineVideoCodecType codecType;
};

class MediaEngineVideoSource : public MediaEngineSource
{
public:
  virtual ~MediaEngineVideoSource() {};

  /* Return a MediaEngineVideoOptions struct with appropriate values for all
   * fields. */
  virtual MediaEngineVideoOptions GetOptions() = 0;
};

/**
 * Audio source and friends.
 */
class MediaEngineAudioSource : public MediaEngineSource
{
public:
  virtual ~MediaEngineAudioSource() {};
};

}

#endif /* MEDIAENGINE_H_ */
