/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioStream_h_)
#define AudioStream_h_

#include "AudioSampleFormat.h"
#include "AudioChannelCommon.h"
#include "nsAutoPtr.h"
#include "nsAutoRef.h"
#include "nsCOMPtr.h"
#include "Latency.h"
#include "mozilla/StaticMutex.h"

#include "cubeb/cubeb.h"

template <>
class nsAutoRefTraits<cubeb_stream> : public nsPointerRefTraits<cubeb_stream>
{
public:
  static void Release(cubeb_stream* aStream) { cubeb_stream_destroy(aStream); }
};

namespace soundtouch {
class SoundTouch;
}

namespace mozilla {

class AudioStream;

class AudioClock
{
public:
  AudioClock(AudioStream* aStream);
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

class CircularByteBuffer
{
public:
  CircularByteBuffer()
    : mBuffer(nullptr), mCapacity(0), mStart(0), mCount(0)
  {}

  // Set the capacity of the buffer in bytes.  Must be called before any
  // call to append or pop elements.
  void SetCapacity(uint32_t aCapacity) {
    NS_ABORT_IF_FALSE(!mBuffer, "Buffer allocated.");
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
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    NS_ABORT_IF_FALSE(aLength <= Available(), "Buffer full.");

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
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    NS_ABORT_IF_FALSE(aSize <= Length(), "Request too large.");

    *aData1 = &mBuffer[mStart];
    *aSize1 = std::min(mCapacity - mStart, aSize);
    *aData2 = &mBuffer[0];
    *aSize2 = aSize - *aSize1;
    mCount -= *aSize1 + *aSize2;
    mStart += *aSize1 + *aSize2;
    mStart %= mCapacity;
  }

private:
  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mCapacity;
  uint32_t mStart;
  uint32_t mCount;
};

// Access to a single instance of this class must be synchronized by
// callers, or made from a single thread.  One exception is that access to
// GetPosition, GetPositionInFrames, SetVolume, and Get{Rate,Channels}
// is thread-safe without external synchronization.
class AudioStream MOZ_FINAL
{
public:
  // Initialize Audio Library. Some Audio backends require initializing the
  // library before using it.
  static void InitLibrary();

  // Shutdown Audio Library. Some Audio backends require shutting down the
  // library after using it.
  static void ShutdownLibrary();

  // Returns the maximum number of channels supported by the audio hardware.
  static int MaxNumberOfChannels();

  // Returns the samplerate the systems prefer, because it is the
  // samplerate the hardware/mixer supports.
  static int PreferredSampleRate();

  AudioStream();
  ~AudioStream();

  enum LatencyRequest {
    HighLatency,
    LowLatency
  };

  // Initialize the audio stream. aNumChannels is the number of audio
  // channels (1 for mono, 2 for stereo, etc) and aRate is the sample rate
  // (22050Hz, 44100Hz, etc).
  nsresult Init(int32_t aNumChannels, int32_t aRate,
                const dom::AudioChannelType aAudioStreamType,
                LatencyRequest aLatencyRequest);

  // Closes the stream. All future use of the stream is an error.
  void Shutdown();

  // Write audio data to the audio hardware.  aBuf is an array of AudioDataValues
  // AudioDataValue of length aFrames*mChannels.  If aFrames is larger
  // than the result of Available(), the write will block until sufficient
  // buffer space is available.  aTime is the time in ms associated with the first sample
  // for latency calculations
  nsresult Write(const AudioDataValue* aBuf, uint32_t aFrames, TimeStamp* aTime = nullptr);

  // Return the number of audio frames that can be written without blocking.
  uint32_t Available();

  // Set the current volume of the audio playback. This is a value from
  // 0 (meaning muted) to 1 (meaning full volume).  Thread-safe.
  void SetVolume(double aVolume);

  // Block until buffered audio data has been consumed.
  void Drain();

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

  // Return the position, measured in audio framed played since the stream was
  // opened, of the audio hardware, not adjusted for the changes of playback
  // rate.
  int64_t GetPositionInFramesInternal();

  // Returns true when the audio stream is paused.
  bool IsPaused();

  int GetRate() { return mOutRate; }
  int GetChannels() { return mChannels; }

  // This should be called before attempting to use the time stretcher.
  nsresult EnsureTimeStretcherInitialized();
  // Set playback rate as a multiple of the intrinsic playback rate. This is to
  // be called only with aPlaybackRate > 0.0.
  nsresult SetPlaybackRate(double aPlaybackRate);
  // Switch between resampling (if false) and time stretching (if true, default).
  nsresult SetPreservesPitch(bool aPreservesPitch);

private:
  static int PrefChanged(const char* aPref, void* aClosure);
  static double GetVolumeScale();
  static cubeb* GetCubebContext();
  static cubeb* GetCubebContextUnlocked();
  static uint32_t GetCubebLatency();
  static bool CubebLatencyPrefSet();

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

  // aTime is the time in ms the samples were inserted into MediaStreamGraph
  long GetUnprocessed(void* aBuffer, long aFrames, int64_t &aTime);
  long GetTimeStretched(void* aBuffer, long aFrames, int64_t &aTime);
  long GetUnprocessedWithSilencePadding(void* aBuffer, long aFrames, int64_t &aTime);

  // Shared implementation of underflow adjusted position calculation.
  // Caller must own the monitor.
  int64_t GetPositionInFramesUnlocked();

  int64_t GetLatencyInFrames();
  void GetBufferInsertTime(int64_t &aTimeMs);

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
  // Number of frames written to the buffers.
  int64_t mWritten;
  AudioClock mAudioClock;
  nsAutoPtr<soundtouch::SoundTouch> mTimeStretcher;
  nsRefPtr<AsyncLatencyLogger> mLatencyLog;

  // copy of Latency logger's starting time for offset calculations
  TimeStamp mStartTime;
  // Whether we are playing a low latency stream, or a normal stream.
  LatencyRequest mLatencyRequest;
  // Where in the current mInserts[0] block cubeb has read to
  int64_t mReadPoint;
  // Keep track of each inserted block of samples and the time it was inserted
  // so we can estimate the clock time for a specific sample's insertion (for when
  // we send data to cubeb).  Blocks are aged out as needed.
  struct Inserts {
    int64_t mTimeMs;
    int64_t mFrames;
  };
  nsAutoTArray<Inserts, 8> mInserts;

  // Sum of silent frames written when DataCallback requests more frames
  // than are available in mBuffer.
  uint64_t mLostFrames;

  // Output file for dumping audio
  FILE* mDumpFile;

  // Temporary audio buffer.  Filled by Write() and consumed by
  // DataCallback().  Once mBuffer is full, Write() blocks until sufficient
  // space becomes available in mBuffer.  mBuffer is sized in bytes, not
  // frames.
  CircularByteBuffer mBuffer;

  // Software volume level.  Applied during the servicing of DataCallback().
  double mVolume;

  // Owning reference to a cubeb_stream.  cubeb_stream_destroy is called by
  // nsAutoRef's destructor.
  nsAutoRef<cubeb_stream> mCubebStream;

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
    STARTED,     // Started by a call to Write() (iff INITIALIZED) or Resume().
    STOPPED,     // Stopped by a call to Pause().
    DRAINING,    // Drain requested.  DataCallback will indicate end of stream
                 // once the remaining contents of mBuffer are requested by
                 // cubeb, after which StateCallback will indicate drain
                 // completion.
    DRAINED,     // StateCallback has indicated that the drain is complete.
    ERRORED      // Stream disabled due to an internal error.
  };

  StreamState mState;

  // This mutex protects the static members below.
  static StaticMutex sMutex;
  static cubeb* sCubebContext;

  // Prefered samplerate, in Hz (characteristic of the
  // hardware/mixer/platform/API used).
  static uint32_t sPreferredSampleRate;

  static double sVolumeScale;
  static uint32_t sCubebLatency;
  static bool sCubebLatencyPrefSet;
};

} // namespace mozilla

#endif
