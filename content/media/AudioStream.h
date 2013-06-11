/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioStream_h_)
#define AudioStream_h_

#include "nscore.h"
#include "AudioSampleFormat.h"
#include "AudioChannelCommon.h"
#include "soundtouch/SoundTouch.h"
#include "nsAutoPtr.h"

namespace mozilla {

class AudioStream;

class AudioClock
{
  public:
    AudioClock(mozilla::AudioStream* aStream);
    // Initialize the clock with the current AudioStream. Need to be called
    // before querying the clock. Called on the audio thread.
    void Init();
    // Update the number of samples that has been written in the audio backend.
    // Called on the state machine thread.
    void UpdateWritePosition(uint32_t aCount);
    // Get the read position of the stream, in microseconds.
    // Called on the state machine thead.
    uint64_t GetPosition();
    // Get the read position of the stream, in frames.
    // Called on the state machine thead.
    uint64_t GetPositionInFrames();
    // Set the playback rate.
    // Called on the audio thread.
    void SetPlaybackRate(double aPlaybackRate);
    // Get the current playback rate.
    // Called on the audio thread.
    double GetPlaybackRate();
    // Set if we are preserving the pitch.
    // Called on the audio thread.
    void SetPreservesPitch(bool aPreservesPitch);
    // Get the current pitch preservation state.
    // Called on the audio thread.
    bool GetPreservesPitch();
    // Get the number of frames written to the backend.
    int64_t GetWritten();
  private:
    // This AudioStream holds a strong reference to this AudioClock. This
    // pointer is garanteed to always be valid.
    AudioStream* mAudioStream;
    // The old output rate, to compensate audio latency for the period inbetween
    // the moment resampled buffers are pushed to the hardware and the moment the
    // clock should take the new rate into account for A/V sync.
    int mOldOutRate;
    // Position at which the last playback rate change occured
    int64_t mBasePosition;
    // Offset, in frames, at which the last playback rate change occured
    int64_t mBaseOffset;
    // Old base offset (number of samples), used when changing rate to compute the
    // position in the stream.
    int64_t mOldBaseOffset;
    // Old base position (number of microseconds), when changing rate. This is the
    // time in the media, not wall clock position.
    int64_t mOldBasePosition;
    // Write position at which the playbackRate change occured.
    int64_t mPlaybackRateChangeOffset;
    // The previous position reached in the media, used when compensating
    // latency, to have the position at which the playbackRate change occured.
    int64_t mPreviousPosition;
    // Number of samples effectivelly written in backend, i.e. write position.
    int64_t mWritten;
    // Output rate in Hz (characteristic of the playback rate)
    int mOutRate;
    // Input rate in Hz (characteristic of the media being played)
    int mInRate;
    // True if the we are timestretching, false if we are resampling.
    bool mPreservesPitch;
    // True if we are playing at the old playbackRate after it has been changed.
    bool mCompensatingLatency;
};

// Access to a single instance of this class must be synchronized by
// callers, or made from a single thread.  One exception is that access to
// GetPosition, GetPositionInFrames, SetVolume, and Get{Rate,Channels}
// is thread-safe without external synchronization.
class AudioStream
{
public:
  AudioStream();

  virtual ~AudioStream();

  // Initialize Audio Library. Some Audio backends require initializing the
  // library before using it.
  static void InitLibrary();

  // Shutdown Audio Library. Some Audio backends require shutting down the
  // library after using it.
  static void ShutdownLibrary();

  // AllocateStream will return either a local stream or a remoted stream
  // depending on where you call it from.  If you call this from a child process,
  // you may receive an implementation which forwards to a compositing process.
  static AudioStream* AllocateStream();

  // Returns the maximum number of channels supported by the audio hardware.
  static int MaxNumberOfChannels();

  // Initialize the audio stream. aNumChannels is the number of audio
  // channels (1 for mono, 2 for stereo, etc) and aRate is the sample rate
  // (22050Hz, 44100Hz, etc).
  virtual nsresult Init(int32_t aNumChannels, int32_t aRate,
                        const dom::AudioChannelType aAudioStreamType) = 0;

  // Closes the stream. All future use of the stream is an error.
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
  virtual void Drain() = 0;

  // Start the stream.
  virtual void Start() = 0;

  // Return the number of frames written so far in the stream. This allow the
  // caller to check if it is safe to start the stream, if needed.
  virtual int64_t GetWritten();

  // Pause audio playback.
  virtual void Pause() = 0;

  // Resume audio playback.
  virtual void Resume() = 0;

  // Return the position in microseconds of the audio frame being played by
  // the audio hardware, compensated for playback rate change. Thread-safe.
  virtual int64_t GetPosition() = 0;

  // Return the position, measured in audio frames played since the stream
  // was opened, of the audio hardware.  Thread-safe.
  virtual int64_t GetPositionInFrames() = 0;

  // Return the position, measured in audio framed played since the stream was
  // opened, of the audio hardware, not adjusted for the changes of playback
  // rate.
  virtual int64_t GetPositionInFramesInternal() = 0;

  // Returns true when the audio stream is paused.
  virtual bool IsPaused() = 0;

  int GetRate() { return mOutRate; }
  int GetChannels() { return mChannels; }

  // This should be called before attempting to use the time stretcher.
  virtual nsresult EnsureTimeStretcherInitialized();
  // Set playback rate as a multiple of the intrinsic playback rate. This is to
  // be called only with aPlaybackRate > 0.0.
  virtual nsresult SetPlaybackRate(double aPlaybackRate);
  // Switch between resampling (if false) and time stretching (if true, default).
  virtual nsresult SetPreservesPitch(bool aPreservesPitch);

protected:
  // Input rate in Hz (characteristic of the media being played)
  int mInRate;
  // Output rate in Hz (characteristic of the playback rate)
  int mOutRate;
  int mChannels;
  // Number of frames written to the buffers.
  int64_t mWritten;
  AudioClock mAudioClock;
  nsAutoPtr<soundtouch::SoundTouch> mTimeStretcher;
};

} // namespace mozilla

#endif
