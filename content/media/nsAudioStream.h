/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsAudioStream_h_)
#define nsAudioStream_h_

#include "nscore.h"
#include "nsISupportsImpl.h"
#include "nsIThread.h"
#include "nsAutoPtr.h"

// Access to a single instance of this class must be synchronized by
// callers, or made from a single thread.  One exception is that access to
// GetPosition, GetPositionInFrames, SetVolume, and Get{Rate,Channels,Format}
// is thread-safe without external synchronization.
class nsAudioStream : public nsISupports
{
public:

  enum SampleFormat
  {
    FORMAT_U8,
    FORMAT_S16_LE,
    FORMAT_FLOAT32
  };

  nsAudioStream()
    : mRate(0),
      mChannels(0),
      mFormat(FORMAT_S16_LE)
  {}

  virtual ~nsAudioStream();

  // Initialize Audio Library. Some Audio backends require initializing the
  // library before using it.
  static void InitLibrary();

  // Shutdown Audio Library. Some Audio backends require shutting down the
  // library after using it.
  static void ShutdownLibrary();

  // Thread that is shared between audio streams.
  // This may return null in the child process
  nsIThread *GetThread();

  // AllocateStream will return either a local stream or a remoted stream
  // depending on where you call it from.  If you call this from a child process,
  // you may receive an implementation which forwards to a compositing process.
  static nsAudioStream* AllocateStream();

  // Initialize the audio stream. aNumChannels is the number of audio
  // channels (1 for mono, 2 for stereo, etc) and aRate is the sample rate
  // (22050Hz, 44100Hz, etc).
  // Unsafe to call with a monitor held due to synchronous event execution
  // on the main thread, which may attempt to acquire any held monitor.
  virtual nsresult Init(PRInt32 aNumChannels, PRInt32 aRate, SampleFormat aFormat) = 0;

  // Closes the stream. All future use of the stream is an error.
  // Unsafe to call with a monitor held due to synchronous event execution
  // on the main thread, which may attempt to acquire any held monitor.
  virtual void Shutdown() = 0;

  // Write audio data to the audio hardware.  aBuf is an array of frames in
  // the format specified by mFormat of length aCount.  If aFrames is larger
  // than the result of Available(), the write will block until sufficient
  // buffer space is available.
  virtual nsresult Write(const void* aBuf, PRUint32 aFrames) = 0;

  // Return the number of audio frames that can be written without blocking.
  virtual PRUint32 Available() = 0;

  // Set the current volume of the audio playback. This is a value from
  // 0 (meaning muted) to 1 (meaning full volume).  Thread-safe.
  virtual void SetVolume(double aVolume) = 0;

  // Block until buffered audio data has been consumed.
  // Unsafe to call with a monitor held due to synchronous event execution
  // on the main thread, which may attempt to acquire any held monitor.
  virtual void Drain() = 0;

  // Pause audio playback
  virtual void Pause() = 0;

  // Resume audio playback
  virtual void Resume() = 0;

  // Return the position in microseconds of the audio frame being played by
  // the audio hardware.  Thread-safe.
  virtual PRInt64 GetPosition() = 0;

  // Return the position, measured in audio frames played since the stream
  // was opened, of the audio hardware.  Thread-safe.
  virtual PRInt64 GetPositionInFrames() = 0;

  // Returns true when the audio stream is paused.
  virtual bool IsPaused() = 0;

  // Returns the minimum number of audio frames which must be written before
  // you can be sure that something will be played.
  // Unsafe to call with a monitor held due to synchronous event execution
  // on the main thread, which may attempt to acquire any held monitor.
  virtual PRInt32 GetMinWriteSize() = 0;

  int GetRate() { return mRate; }
  int GetChannels() { return mChannels; }
  SampleFormat GetFormat() { return mFormat; }

protected:
  nsCOMPtr<nsIThread> mAudioPlaybackThread;
  int mRate;
  int mChannels;
  SampleFormat mFormat;
};

#endif
