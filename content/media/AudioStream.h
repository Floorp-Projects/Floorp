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
#include "Latency.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "CubebUtils.h"

namespace soundtouch {
class SoundTouch;
}

namespace mozilla {

template<>
struct DefaultDelete<cubeb_stream>
{
  void operator()(cubeb_stream* aStream) const
  {
    cubeb_stream_destroy(aStream);
  }
};

class AudioStream;
class FrameHistory;

class AudioClock
{
public:
  AudioClock(AudioStream* aStream);
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
  // The history of frames sent to the audio engine in each Datacallback.
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

  // Throw away all but aSize bytes from the buffer.  Returns new size, which
  // may be less than aSize
  uint32_t ContractTo(uint32_t aSize) {
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    if (aSize >= mCount) {
      return mCount;
    }
    mStart += (mCount - aSize);
    mCount = aSize;
    mStart %= mCapacity;
    return mCount;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t amount = 0;
    amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  void Reset()
  {
    mBuffer = nullptr;
    mCapacity = 0;
    mStart = 0;
    mCount = 0;
  }

private:
  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mCapacity;
  uint32_t mStart;
  uint32_t mCount;
};

class AudioInitTask;

// Access to a single instance of this class must be synchronized by
// callers, or made from a single thread.  One exception is that access to
// GetPosition, GetPositionInFrames, SetVolume, and Get{Rate,Channels},
// SetMicrophoneActive is thread-safe without external synchronization.
class AudioStream MOZ_FINAL
{
  virtual ~AudioStream();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioStream)
  AudioStream();

  enum LatencyRequest {
    HighLatency,
    LowLatency
  };

  // Initialize the audio stream. aNumChannels is the number of audio
  // channels (1 for mono, 2 for stereo, etc) and aRate is the sample rate
  // (22050Hz, 44100Hz, etc).
  nsresult Init(int32_t aNumChannels, int32_t aRate,
                const dom::AudioChannel aAudioStreamChannel,
                LatencyRequest aLatencyRequest);

  // Closes the stream. All future use of the stream is an error.
  void Shutdown();

  void Reset();

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

  // Informs the AudioStream that a microphone is being used by someone in the
  // application.
  void SetMicrophoneActive(bool aActive);
  void PanOutputIfNeeded(bool aMicrophoneActive);
  void ResetStreamIfNeeded();

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
  friend class AudioInitTask;

  // So we can call it asynchronously from AudioInitTask
  nsresult OpenCubeb(cubeb_stream_params &aParams,
                     LatencyRequest aLatencyRequest);
  void AudioInitTaskFinished();

  void CheckForStart();

  static long DataCallback_S(cubeb_stream*, void* aThis, void* aBuffer, long aFrames)
  {
    return static_cast<AudioStream*>(aThis)->DataCallback(aBuffer, aFrames);
  }

  static void StateCallback_S(cubeb_stream*, void* aThis, cubeb_state aState)
  {
    static_cast<AudioStream*>(aThis)->StateCallback(aState);
  }


  static void DeviceChangedCallback_s(void * aThis) {
    static_cast<AudioStream*>(aThis)->DeviceChangedCallback();
  }

  long DataCallback(void* aBuffer, long aFrames);
  void StateCallback(cubeb_state aState);
  void DeviceChangedCallback();

  nsresult EnsureTimeStretcherInitializedUnlocked();

  // aTime is the time in ms the samples were inserted into MediaStreamGraph
  long GetUnprocessed(void* aBuffer, long aFrames, int64_t &aTime);
  long GetTimeStretched(void* aBuffer, long aFrames, int64_t &aTime);
  long GetUnprocessedWithSilencePadding(void* aBuffer, long aFrames, int64_t &aTime);

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
  int mOutChannels;
#if defined(__ANDROID__)
  dom::AudioChannel mAudioChannel;
#endif
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

  // Output file for dumping audio
  FILE* mDumpFile;

  // Temporary audio buffer.  Filled by Write() and consumed by
  // DataCallback().  Once mBuffer is full, Write() blocks until sufficient
  // space becomes available in mBuffer.  mBuffer is sized in bytes, not
  // frames.
  CircularByteBuffer mBuffer;

  // Owning reference to a cubeb_stream.
  UniquePtr<cubeb_stream> mCubebStream;

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
  bool mNeedsStart; // needed in case Start() is called before cubeb is open
  bool mIsFirst;
  // True if a microphone is active.
  bool mMicrophoneActive;
  // When we are in the process of changing the output device, and the callback
  // is not going to be called for a little while, simply drop incoming frames.
  // This is only on OSX for now, because other systems handle this gracefully.
  bool mShouldDropFrames;
  // True if there is a pending AudioInitTask. Shutdown() will wait until the
  // pending AudioInitTask is finished.
  bool mPendingAudioInitTask;
};

class AudioInitTask : public nsRunnable
{
public:
  AudioInitTask(AudioStream *aStream,
                AudioStream::LatencyRequest aLatencyRequest,
                const cubeb_stream_params &aParams)
    : mAudioStream(aStream)
    , mLatencyRequest(aLatencyRequest)
    , mParams(aParams)
  {}

  nsresult Dispatch()
  {
    // Can't add 'this' as the event to run, since mThread may not be set yet
    nsresult rv = NS_NewNamedThread("CubebInit", getter_AddRefs(mThread));
    if (NS_SUCCEEDED(rv)) {
      // Note: event must not null out mThread!
      rv = mThread->Dispatch(this, NS_DISPATCH_NORMAL);
    }
    return rv;
  }

protected:
  virtual ~AudioInitTask() {};

private:
  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL;

  RefPtr<AudioStream> mAudioStream;
  AudioStream::LatencyRequest mLatencyRequest;
  cubeb_stream_params mParams;

  nsCOMPtr<nsIThread> mThread;
};

} // namespace mozilla

#endif
