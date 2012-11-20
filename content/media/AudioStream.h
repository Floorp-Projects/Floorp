/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioStream_h_)
#define AudioStream_h_

#include "nscore.h"
#include "nsISupportsImpl.h"
#include "nsIThread.h"
#include "nsAutoPtr.h"
#include "AudioSampleFormat.h"
#include "AudioChannelCommon.h"

namespace mozilla {

// Access to a single instance of this class must be synchronized by
// callers, or made from a single thread.  One exception is that access to
// GetPosition, GetPositionInFrames, SetVolume, and Get{Rate,Channels}
// is thread-safe without external synchronization.
class AudioStream : public nsISupports
{
public:
  AudioStream()
    : mRate(0),
      mChannels(0)
  {}

  virtual ~AudioStream();

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
  static AudioStream* AllocateStream();

  // Initialize the audio stream. aNumChannels is the number of audio
  // channels (1 for mono, 2 for stereo, etc) and aRate is the sample rate
  // (22050Hz, 44100Hz, etc).
  // Unsafe to call with a monitor held due to synchronous event execution
  // on the main thread, which may attempt to acquire any held monitor.
  virtual nsresult Init(int32_t aNumChannels, int32_t aRate,
                        const mozilla::dom::AudioChannelType aAudioStreamType) = 0;

  // Closes the stream. All future use of the stream is an error.
  // Unsafe to call with a monitor held due to synchronous event execution
  // on the main thread, which may attempt to acquire any held monitor.
  virtual void Shutdown() = 0;

  // Write audio data to the audio hardware.  aBuf is an array of AudioDataValues
  // AudioDataValue of length aFrames*mChannels.  If aFrames is larger
  // than the result of Available(), the write will block until sufficient
  // buffer space is available.
  virtual nsresult Write(const mozilla::AudioDataValue* aBuf, uint32_t aFrames) = 0;

  // Return the number of audio frames that can be written without blocking.
  virtual uint32_t Available() = 0;

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
  virtual int64_t GetPosition() = 0;

  // Return the position, measured in audio frames played since the stream
  // was opened, of the audio hardware.  Thread-safe.
  virtual int64_t GetPositionInFrames() = 0;

  // Returns true when the audio stream is paused.
  virtual bool IsPaused() = 0;

  // Returns the minimum number of audio frames which must be written before
  // you can be sure that something will be played.
  // Unsafe to call with a monitor held due to synchronous event execution
  // on the main thread, which may attempt to acquire any held monitor.
  virtual int32_t GetMinWriteSize() = 0;

  int GetRate() { return mRate; }
  int GetChannels() { return mChannels; }

protected:
  nsCOMPtr<nsIThread> mAudioPlaybackThread;
  int mRate;
  int mChannels;
};

} // namespace mozilla

#endif
