/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Matthew Gregan <kinetik@flim.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#if !defined(nsWaveDecoder_h_)
#define nsWaveDecoder_h_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsMediaDecoder.h"
#include "nsMediaStream.h"

/*
  nsWaveDecoder provides an implementation of the abstract nsMediaDecoder
  class that supports parsing and playback of Waveform Audio (WAVE) chunks
  embedded in Resource Interchange File Format (RIFF) bitstreams as
  specified by the Multimedia Programming Interface and Data Specification
  1.0.

  Each decoder instance starts one thread (the playback thread).  A single
  nsWaveStateMachine event is dispatched to this thread to start the
  thread's state machine running.  The Run method of the event is a loop
  that executes the current state.  The state can be changed by the state
  machine, or from the main thread via threadsafe methods on the event.
  During playback, the playback thread reads data from the network and
  writes it to the audio backend, attempting to keep the backend's audio
  buffers full.  It is also responsible for seeking, buffering, and
  pausing/resuming audio.

  The decoder also owns an nsMediaStream instance that provides a threadsafe
  blocking interface to read from network channels.  The state machine is
  the primary user of this stream and holds a weak (raw) pointer to it as
  the thread, state machine, and stream's lifetimes are all managed by the
  decoder.

  nsWaveStateMachine has the following states:

  LOADING_METADATA
    RIFF/WAVE chunks are being read from the stream, the metadata describing
    the audio data is parsed.

  BUFFERING
    Playback is paused while waiting for additional data.

  PLAYING
    If data is available in the stream and the audio backend can consume
    more data, it is read from the stream and written to the audio backend.
    Sleep until approximately half of the backend's buffers have drained.

  SEEKING
    Decoder is seeking to a specified time in the media.

  PAUSED
    Pause the audio backend, then wait for a state transition.

  ENDED
    Expected PCM data (or stream EOF) reached, wait for the audio backend to
    play any buffered data, then wait for shutdown.

  ERROR
    Metadata loading/parsing failed, wait for shutdown.

  SHUTDOWN
    Close the audio backend and return from the run loop.

  State transitions within the state machine are:

  LOADING_METADATA -> PLAYING
                   -> PAUSED
                   -> ERROR

  BUFFERING        -> PLAYING
                   -> PAUSED

  PLAYING          -> BUFFERING
                   -> ENDED

  SEEKING          -> PLAYING
                   -> PAUSED

  PAUSED           -> waits for caller to play, seek, or shutdown

  ENDED            -> waits for caller to shutdown

  ERROR            -> waits for caller to shutdown

  SHUTDOWN         -> exits state machine

  In addition, the following methods cause state transitions:

  Shutdown(), Play(), Pause(), Seek(float)

  The decoder implementation is currently limited to Linear PCM encoded
  audio data with one or two channels of 8- or 16-bit samples at sample
  rates from 100 Hz to 96 kHz.  The number of channels is limited by what
  the audio backend (sydneyaudio via nsAudioStream) currently supports.  The
  supported sample rate is artificially limited to arbitrarily selected sane
  values.  Support for additional channels (and other new features) would
  require extending nsWaveDecoder to support parsing the newer
  WAVE_FORMAT_EXTENSIBLE chunk format.
 */

class nsWaveStateMachine;

class nsWaveDecoder : public nsMediaDecoder
{
  friend class nsWaveStateMachine;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 public:
  nsWaveDecoder();
  ~nsWaveDecoder();

  virtual nsMediaDecoder* Clone() { return new nsWaveDecoder(); }

  virtual PRBool Init(nsHTMLMediaElement* aElement);

  virtual nsMediaStream* GetCurrentStream();
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  // Return the current playback position in the media in seconds.
  virtual float GetCurrentTime();

  // Return the total playback length of the media in seconds.
  virtual float GetDuration();

  // Set the audio playback volume; must be in range [0.0, 1.0].
  virtual void SetVolume(float aVolume);

  virtual nsresult Play();
  virtual void Pause();

  // Set the current time of the media to aTime.  This may cause mStream to
  // create a new channel to fetch data from the appropriate position in the
  // stream.
  virtual nsresult Seek(float aTime);

  // Report whether the decoder is currently seeking.
  virtual PRBool IsSeeking() const;

  // Report whether the decoder has reached end of playback.
  virtual PRBool IsEnded() const;

  // Start downloading the media at the specified URI.  The media's metadata
  // will be parsed and made available as the load progresses.
  virtual nsresult Load(nsMediaStream* aStream,
                        nsIStreamListener** aStreamListener);

  // Called by mStream (and possibly the nsChannelToPipeListener used
  // internally by mStream) when the stream has completed loading.
  virtual void ResourceLoaded();

  // Called by mStream (and possibly the nsChannelToPipeListener used
  // internally by mStream) if the stream encounters a network error.
  virtual void NetworkError();

  // Element is notifying us that the requested playback rate has changed.
  virtual nsresult PlaybackRateChanged();

  virtual void NotifySuspendedStatusChanged();
  virtual void NotifyBytesDownloaded();
  virtual void NotifyDownloadEnded(nsresult aStatus);

  virtual Statistics GetStatistics();

  void PlaybackPositionChanged();

  // Setter for the duration. This is ignored by the wave decoder since it can
  // compute the duration directly from the wave data.
  virtual void SetDuration(PRInt64 aDuration);

  // Getter/setter for mSeekable.
  virtual void SetSeekable(PRBool aSeekable);
  virtual PRBool GetSeekable();

  // Must be called by the owning object before disposing the decoder.
  virtual void Shutdown();

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache. Call on the main
  // thread only.
  virtual void Suspend();

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache. Call on the
  // main thread only.
  virtual void Resume();

  // Change the element's ready state as necessary. Main thread only.
  void UpdateReadyStateForData();

  // Tells mStream to put all loads in the background.
  virtual void MoveLoadsToBackground();

  // Called asynchronously to shut down the decoder
  void Stop();

private:
  // Notifies the element that seeking has started.
  void SeekingStarted();

  // Notifies the element that seeking has completed.
  void SeekingStopped();

  // Notifies the element that metadata loading has completed.  Only fired
  // if metadata is valid.
  void MetadataLoaded();

  // Notifies the element that playback has completed.
  void PlaybackEnded();

  // Notifies the element that decoding has failed.
  void DecodeError();

  void RegisterShutdownObserver();
  void UnregisterShutdownObserver();

  // Volume that the audio backend will be initialized with.
  float mInitialVolume;

  // Thread that handles audio playback, including data download.
  nsCOMPtr<nsIThread> mPlaybackThread;

  // State machine that runs on mPlaybackThread.  Methods on this object are
  // safe to call from any thread.
  nsCOMPtr<nsWaveStateMachine> mPlaybackStateMachine;

  // Threadsafe wrapper around channels that provides seeking based on the
  // underlying channel type.
  nsAutoPtr<nsMediaStream> mStream;

  // The current playback position of the media resource in units of
  // seconds. This is updated every time a block of audio is passed to the
  // backend (unless an prior update is still pending).  It is read and
  // written from the main thread only.
  float mCurrentTime;

  // Copy of the duration and ended state when the state machine was
  // disposed.  Used to respond to duration and ended queries with sensible
  // values after the state machine has been destroyed.
  float mEndedDuration;
  PRPackedBool mEnded;

  // True if the media resource is seekable.
  PRPackedBool mSeekable;

  // True when the media resource has completely loaded. Accessed on
  // the main thread only.
  PRPackedBool mResourceLoaded;

  // True if MetadataLoaded has been reported to the element.
  PRPackedBool mMetadataLoadedReported;

  // True if ResourceLoaded has been reported to the element.
  PRPackedBool mResourceLoadedReported;
};

#endif
