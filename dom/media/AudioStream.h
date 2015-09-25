/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioStream_h_)
#define AudioStream_h_

#include "AudioSampleFormat.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "CubebUtils.h"
#include "soundtouch/SoundTouchFactory.h"

namespace mozilla {

struct CubebDestroyPolicy
{
  void operator()(cubeb_stream* aStream) const {
    cubeb_stream_destroy(aStream);
  }
};

class AudioStream;
class FrameHistory;

class AudioClock
{
public:
  explicit AudioClock(AudioStream* aStream);
  // Initialize the clock with the current AudioStream. Need to be called
  // before querying the clock. Called on the audio thread.
  void Init();
  // Update the number of samples that has been written in the audio backend.
  // Called on the state machine thread.
  void UpdateFrameHistory(uint32_t aServiced, uint32_t aUnderrun);
  // Get the read position of the stream, in microseconds.
  // Called on the state machine thead.
  // Assumes the AudioStream lock is held and thus calls Unlocked versions
  // of AudioStream funcs.
  int64_t GetPositionUnlocked() const;
  // Get the read position of the stream, in frames.
  // Called on the state machine thead.
  int64_t GetPositionInFrames() const;
  // Set the playback rate.
  // Called on the audio thread.
  // Assumes the AudioStream lock is held and thus calls Unlocked versions
  // of AudioStream funcs.
  void SetPlaybackRateUnlocked(double aPlaybackRate);
  // Get the current playback rate.
  // Called on the audio thread.
  double GetPlaybackRate() const;
  // Set if we are preserving the pitch.
  // Called on the audio thread.
  void SetPreservesPitch(bool aPreservesPitch);
  // Get the current pitch preservation state.
  // Called on the audio thread.
  bool GetPreservesPitch() const;
private:
  // This AudioStream holds a strong reference to this AudioClock. This
  // pointer is garanteed to always be valid.
  AudioStream* const mAudioStream;
  // Output rate in Hz (characteristic of the playback rate)
  int mOutRate;
  // Input rate in Hz (characteristic of the media being played)
  int mInRate;
  // True if the we are timestretching, false if we are resampling.
  bool mPreservesPitch;
  // The history of frames sent to the audio engine in each DataCallback.
  const nsAutoPtr<FrameHistory> mFrameHistory;
};

class CircularByteBuffer
{
public:
  CircularByteBuffer()
    : mBuffer(nullptr), mCapacity(0), mStart(0), mCount(0)
  {}

  // Set the capacity of the buffer in bytes.  Must be called before any
  // call to append or pop elements.
  void SetCapacity(uint32_t aCapacity) {
    MOZ_ASSERT(!mBuffer, "Buffer allocated.");
    mCapacity = aCapacity;
    mBuffer = new uint8_t[mCapacity];
  }

  uint32_t Length() {
    return mCount;
  }

  uint32_t Capacity() {
    return mCapacity;
  }

  uint32_t Available() {
    return Capacity() - Length();
  }

  // Append aLength bytes from aSrc to the buffer.  Caller must check that
  // sufficient space is available.
  void AppendElements(const uint8_t* aSrc, uint32_t aLength) {
    MOZ_ASSERT(mBuffer && mCapacity, "Buffer not initialized.");
    MOZ_ASSERT(aLength <= Available(), "Buffer full.");

    uint32_t end = (mStart + mCount) % mCapacity;

    uint32_t toCopy = std::min(mCapacity - end, aLength);
    memcpy(&mBuffer[end], aSrc, toCopy);
    memcpy(&mBuffer[0], aSrc + toCopy, aLength - toCopy);
    mCount += aLength;
  }

  // Remove aSize bytes from the buffer.  Caller must check returned size in
  // aSize{1,2} before using the pointer returned in aData{1,2}.  Caller
  // must not specify an aSize larger than Length().
  void PopElements(uint32_t aSize, void** aData1, uint32_t* aSize1,
                   void** aData2, uint32_t* aSize2) {
    MOZ_ASSERT(mBuffer && mCapacity, "Buffer not initialized.");
    MOZ_ASSERT(aSize <= Length(), "Request too large.");

    *aData1 = &mBuffer[mStart];
    *aSize1 = std::min(mCapacity - mStart, aSize);
    *aData2 = &mBuffer[0];
    *aSize2 = aSize - *aSize1;
    mCount -= *aSize1 + *aSize2;
    mStart += *aSize1 + *aSize2;
    mStart %= mCapacity;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t amount = 0;
    amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

private:
  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mCapacity;
  uint32_t mStart;
  uint32_t mCount;
};

// Access to a single instance of this class must be synchronized by
// callers, or made from a single thread.  One exception is that access to
// GetPosition, GetPositionInFrames, SetVolume, and Get{Rate,Channels},
// SetMicrophoneActive is thread-safe without external synchronization.
class AudioStream final
{
  virtual ~AudioStream();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioStream)
  AudioStream();

  // Initialize the audio stream. aNumChannels is the number of audio
  // channels (1 for mono, 2 for stereo, etc) and aRate is the sample rate
  // (22050Hz, 44100Hz, etc).
  nsresult Init(int32_t aNumChannels, int32_t aRate,
                const dom::AudioChannel aAudioStreamChannel);

  // Closes the stream. All future use of the stream is an error.
  void Shutdown();

  void Reset();

  // Write audio data to the audio hardware.  aBuf is an array of AudioDataValues
  // AudioDataValue of length aFrames*mChannels.  If aFrames is larger
  // than the result of Available(), the write will block until sufficient
  // buffer space is available.
  nsresult Write(const AudioDataValue* aBuf, uint32_t aFrames);

  // Return the number of audio frames that can be written without blocking.
  uint32_t Available();

  // Set the current volume of the audio playback. This is a value from
  // 0 (meaning muted) to 1 (meaning full volume).  Thread-safe.
  void SetVolume(double aVolume);

  // Block until buffered audio data has been consumed.
  void Drain();

  // Break any blocking operation and set the stream to shutdown.
  void Cancel();

  // Start the stream.
  void Start();

  // Return the number of frames written so far in the stream. This allow the
  // caller to check if it is safe to start the stream, if needed.
  int64_t GetWritten();

  // Pause audio playback.
  void Pause();

  // Resume audio playback.
  void Resume();

  // Return the position in microseconds of the audio frame being played by
  // the audio hardware, compensated for playback rate change. Thread-safe.
  int64_t GetPosition();

  // Return the position, measured in audio frames played since the stream
  // was opened, of the audio hardware.  Thread-safe.
  int64_t GetPositionInFrames();

  // Returns true when the audio stream is paused.
  bool IsPaused();

  int GetRate() { return mOutRate; }
  int GetChannels() { return mChannels; }
  int GetOutChannels() { return mOutChannels; }

  // Set playback rate as a multiple of the intrinsic playback rate. This is to
  // be called only with aPlaybackRate > 0.0.
  nsresult SetPlaybackRate(double aPlaybackRate);
  // Switch between resampling (if false) and time stretching (if true, default).
  nsresult SetPreservesPitch(bool aPreservesPitch);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

protected:
  friend class AudioClock;

  // Return the position, measured in audio frames played since the stream was
  // opened, of the audio hardware, not adjusted for the changes of playback
  // rate or underrun frames.
  // Caller must own the monitor.
  int64_t GetPositionInFramesUnlocked();

private:
  nsresult OpenCubeb(cubeb_stream_params &aParams);

  static long DataCallback_S(cubeb_stream*, void* aThis, void* aBuffer, long aFrames)
  {
    return static_cast<AudioStream*>(aThis)->DataCallback(aBuffer, aFrames);
  }

  static void StateCallback_S(cubeb_stream*, void* aThis, cubeb_state aState)
  {
    static_cast<AudioStream*>(aThis)->StateCallback(aState);
  }


  long DataCallback(void* aBuffer, long aFrames);
  void StateCallback(cubeb_state aState);

  nsresult EnsureTimeStretcherInitializedUnlocked();

  long GetUnprocessed(void* aBuffer, long aFrames);
  long GetTimeStretched(void* aBuffer, long aFrames);

  void StartUnlocked();

  // The monitor is held to protect all access to member variables.  Write()
  // waits while mBuffer is full; DataCallback() notifies as it consumes
  // data from mBuffer.  Drain() waits while mState is DRAINING;
  // StateCallback() notifies when mState is DRAINED.
  Monitor mMonitor;

  // Input rate in Hz (characteristic of the media being played)
  int mInRate;
  // Output rate in Hz (characteristic of the playback rate)
  int mOutRate;
  int mChannels;
  int mOutChannels;
#if defined(__ANDROID__)
  dom::AudioChannel mAudioChannel;
#endif
  // Number of frames written to the buffers.
  int64_t mWritten;
  AudioClock mAudioClock;
  soundtouch::SoundTouch* mTimeStretcher;

  // Stream start time for stream open delay telemetry.
  TimeStamp mStartTime;

  // Output file for dumping audio
  FILE* mDumpFile;

  // Temporary audio buffer.  Filled by Write() and consumed by
  // DataCallback().  Once mBuffer is full, Write() blocks until sufficient
  // space becomes available in mBuffer.  mBuffer is sized in bytes, not
  // frames.
  CircularByteBuffer mBuffer;

  // Owning reference to a cubeb_stream.
  UniquePtr<cubeb_stream, CubebDestroyPolicy> mCubebStream;

  uint32_t mBytesPerFrame;

  uint32_t BytesToFrames(uint32_t aBytes) {
    NS_ASSERTION(aBytes % mBytesPerFrame == 0,
                 "Byte count not aligned on frames size.");
    return aBytes / mBytesPerFrame;
  }

  uint32_t FramesToBytes(uint32_t aFrames) {
    return aFrames * mBytesPerFrame;
  }

  enum StreamState {
    INITIALIZED, // Initialized, playback has not begun.
    STARTED,     // cubeb started, but callbacks haven't started
    RUNNING,     // DataCallbacks have started after STARTED, or after Resume().
    STOPPED,     // Stopped by a call to Pause().
    DRAINING,    // Drain requested.  DataCallback will indicate end of stream
                 // once the remaining contents of mBuffer are requested by
                 // cubeb, after which StateCallback will indicate drain
                 // completion.
    DRAINED,     // StateCallback has indicated that the drain is complete.
    ERRORED,     // Stream disabled due to an internal error.
    SHUTDOWN     // Shutdown has been called
  };

  StreamState mState;
  bool mIsFirst;
  // The last good position returned by cubeb_stream_get_position(). Used to
  // check if the cubeb position is going backward.
  uint64_t mLastGoodPosition;
  // Get this value from the preferece, if true, we would downmix the stereo.
  bool mIsMonoAudioEnabled;
};

} // namespace mozilla

#endif
