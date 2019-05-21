/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioStream_h_)
#  define AudioStream_h_

#  include "AudioSampleFormat.h"
#  include "CubebUtils.h"
#  include "MediaInfo.h"
#  include "mozilla/Monitor.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/TimeStamp.h"
#  include "mozilla/UniquePtr.h"
#  include "nsAutoPtr.h"
#  include "nsCOMPtr.h"
#  include "nsThreadUtils.h"
#  include "soundtouch/SoundTouchFactory.h"
#  include "WavDumper.h"

#  if defined(XP_WIN)
#    include "mozilla/audio/AudioNotificationReceiver.h"
#  endif

namespace mozilla {

struct CubebDestroyPolicy {
  void operator()(cubeb_stream* aStream) const {
    cubeb_stream_destroy(aStream);
  }
};

class AudioStream;
class FrameHistory;
class AudioConfig;

class AudioClock {
 public:
  AudioClock();

  // Initialize the clock with the current sampling rate.
  // Need to be called before querying the clock.
  void Init(uint32_t aRate);

  // Update the number of samples that has been written in the audio backend.
  // Called on the state machine thread.
  void UpdateFrameHistory(uint32_t aServiced, uint32_t aUnderrun);

  /**
   * @param aFrames The playback position in frames of the audio engine.
   * @return The playback position in frames of the stream,
   *         adjusted by playback rate changes and underrun frames.
   */
  int64_t GetPositionInFrames(int64_t aFrames) const;

  /**
   * @param frames The playback position in frames of the audio engine.
   * @return The playback position in microseconds of the stream,
   *         adjusted by playback rate changes and underrun frames.
   */
  int64_t GetPosition(int64_t frames) const;

  // Set the playback rate.
  // Called on the audio thread.
  void SetPlaybackRate(double aPlaybackRate);
  // Get the current playback rate.
  // Called on the audio thread.
  double GetPlaybackRate() const;
  // Set if we are preserving the pitch.
  // Called on the audio thread.
  void SetPreservesPitch(bool aPreservesPitch);
  // Get the current pitch preservation state.
  // Called on the audio thread.
  bool GetPreservesPitch() const;

  uint32_t GetInputRate() const { return mInRate; }
  uint32_t GetOutputRate() const { return mOutRate; }

 private:
  // Output rate in Hz (characteristic of the playback rate)
  uint32_t mOutRate;
  // Input rate in Hz (characteristic of the media being played)
  uint32_t mInRate;
  // True if the we are timestretching, false if we are resampling.
  bool mPreservesPitch;
  // The history of frames sent to the audio engine in each DataCallback.
  const nsAutoPtr<FrameHistory> mFrameHistory;
};

/*
 * A bookkeeping class to track the read/write position of an audio buffer.
 */
class AudioBufferCursor {
 public:
  AudioBufferCursor(Span<AudioDataValue> aSpan, uint32_t aChannels,
                    uint32_t aFrames)
      : mChannels(aChannels), mSpan(aSpan), mFrames(aFrames) {}

  // Advance the cursor to account for frames that are consumed.
  uint32_t Advance(uint32_t aFrames) {
    MOZ_DIAGNOSTIC_ASSERT(Contains(aFrames));
    MOZ_ASSERT(mFrames >= aFrames);
    mFrames -= aFrames;
    mOffset += mChannels * aFrames;
    return aFrames;
  }

  // The number of frames available for read/write in this buffer.
  uint32_t Available() const { return mFrames; }

  // Return a pointer where read/write should begin.
  AudioDataValue* Ptr() const {
    MOZ_DIAGNOSTIC_ASSERT(mOffset <= mSpan.Length());
    return mSpan.Elements() + mOffset;
  }

 protected:
  bool Contains(uint32_t aFrames) const {
    return mSpan.Length() >= mOffset + mChannels * aFrames;
  }
  const uint32_t mChannels;

 private:
  const Span<AudioDataValue> mSpan;
  size_t mOffset = 0;
  uint32_t mFrames;
};

/*
 * A helper class to encapsulate pointer arithmetic and provide means to modify
 * the underlying audio buffer.
 */
class AudioBufferWriter : private AudioBufferCursor {
 public:
  AudioBufferWriter(Span<AudioDataValue> aSpan, uint32_t aChannels,
                    uint32_t aFrames)
      : AudioBufferCursor(aSpan, aChannels, aFrames) {}

  uint32_t WriteZeros(uint32_t aFrames) {
    MOZ_DIAGNOSTIC_ASSERT(Contains(aFrames));
    memset(Ptr(), 0, sizeof(AudioDataValue) * mChannels * aFrames);
    return Advance(aFrames);
  }

  uint32_t Write(const AudioDataValue* aPtr, uint32_t aFrames) {
    MOZ_DIAGNOSTIC_ASSERT(Contains(aFrames));
    memcpy(Ptr(), aPtr, sizeof(AudioDataValue) * mChannels * aFrames);
    return Advance(aFrames);
  }

  // Provide a write fuction to update the audio buffer with the following
  // signature: uint32_t(const AudioDataValue* aPtr, uint32_t aFrames)
  // aPtr: Pointer to the audio buffer.
  // aFrames: The number of frames available in the buffer.
  // return: The number of frames actually written by the function.
  template <typename Function>
  uint32_t Write(const Function& aFunction, uint32_t aFrames) {
    MOZ_DIAGNOSTIC_ASSERT(Contains(aFrames));
    return Advance(aFunction(Ptr(), aFrames));
  }

  using AudioBufferCursor::Available;
};

// Access to a single instance of this class must be synchronized by
// callers, or made from a single thread.  One exception is that access to
// GetPosition, GetPositionInFrames, SetVolume, and Get{Rate,Channels},
// SetMicrophoneActive is thread-safe without external synchronization.
class AudioStream final
#  if defined(XP_WIN)
    : public audio::DeviceChangeListener
#  endif
{
  virtual ~AudioStream();

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioStream)

  class Chunk {
   public:
    // Return a pointer to the audio data.
    virtual const AudioDataValue* Data() const = 0;
    // Return the number of frames in this chunk.
    virtual uint32_t Frames() const = 0;
    // Return the number of audio channels.
    virtual uint32_t Channels() const = 0;
    // Return the sample rate of this chunk.
    virtual uint32_t Rate() const = 0;
    // Return a writable pointer for downmixing.
    virtual AudioDataValue* GetWritable() const = 0;
    virtual ~Chunk() {}
  };

  class DataSource {
   public:
    // Return a chunk which contains at most aFrames frames or zero if no
    // frames in the source at all.
    virtual UniquePtr<Chunk> PopFrames(uint32_t aFrames) = 0;
    // Return true if no more data will be added to the source.
    virtual bool Ended() const = 0;
    // Notify that all data is drained by the AudioStream.
    virtual void Drained() = 0;

   protected:
    virtual ~DataSource() = default;
  };

  explicit AudioStream(DataSource& aSource);

  // Initialize the audio stream. aNumChannels is the number of audio
  // channels (1 for mono, 2 for stereo, etc), aChannelMap is the indicator for
  // channel layout(mono, stereo, 5.1 or 7.1 ) and aRate is the sample rate
  // (22050Hz, 44100Hz, etc).
  nsresult Init(uint32_t aNumChannels,
                AudioConfig::ChannelLayout::ChannelMap aChannelMap,
                uint32_t aRate, AudioDeviceInfo* aSinkInfo);

  // Closes the stream. All future use of the stream is an error.
  void Shutdown();

  void Reset();

  // Set the current volume of the audio playback. This is a value from
  // 0 (meaning muted) to 1 (meaning full volume).  Thread-safe.
  void SetVolume(double aVolume);

  // Start the stream.
  nsresult Start();

  // Pause audio playback.
  void Pause();

  // Resume audio playback.
  void Resume();

#  if defined(XP_WIN)
  // Reset stream to the default device.
  void ResetDefaultDevice() override;
#  endif

  // Return the position in microseconds of the audio frame being played by
  // the audio hardware, compensated for playback rate change. Thread-safe.
  int64_t GetPosition();

  // Return the position, measured in audio frames played since the stream
  // was opened, of the audio hardware.  Thread-safe.
  int64_t GetPositionInFrames();

  static uint32_t GetPreferredRate() {
    return CubebUtils::PreferredSampleRate();
  }

  uint32_t GetOutChannels() { return mOutChannels; }

  // Set playback rate as a multiple of the intrinsic playback rate. This is to
  // be called only with aPlaybackRate > 0.0.
  nsresult SetPlaybackRate(double aPlaybackRate);
  // Switch between resampling (if false) and time stretching (if true,
  // default).
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
  nsresult OpenCubeb(cubeb* aContext, cubeb_stream_params& aParams,
                     TimeStamp aStartTime, bool aIsFirst);

  static long DataCallback_S(cubeb_stream*, void* aThis,
                             const void* /* aInputBuffer */,
                             void* aOutputBuffer, long aFrames) {
    return static_cast<AudioStream*>(aThis)->DataCallback(aOutputBuffer,
                                                          aFrames);
  }

  static void StateCallback_S(cubeb_stream*, void* aThis, cubeb_state aState) {
    static_cast<AudioStream*>(aThis)->StateCallback(aState);
  }

  long DataCallback(void* aBuffer, long aFrames);
  void StateCallback(cubeb_state aState);

  nsresult EnsureTimeStretcherInitializedUnlocked();

  // Return true if audio frames are valid (correct sampling rate and valid
  // channel count) otherwise false.
  bool IsValidAudioFormat(Chunk* aChunk);

  void GetUnprocessed(AudioBufferWriter& aWriter);
  void GetTimeStretched(AudioBufferWriter& aWriter);

  template <typename Function, typename... Args>
  int InvokeCubeb(Function aFunction, Args&&... aArgs);

  // The monitor is held to protect all access to member variables.
  Monitor mMonitor;

  uint32_t mChannels;
  uint32_t mOutChannels;
  AudioClock mAudioClock;
  soundtouch::SoundTouch* mTimeStretcher;

  WavDumper mDumpFile;

  // Owning reference to a cubeb_stream.
  UniquePtr<cubeb_stream, CubebDestroyPolicy> mCubebStream;

  enum StreamState {
    INITIALIZED,  // Initialized, playback has not begun.
    STARTED,      // cubeb started.
    STOPPED,      // Stopped by a call to Pause().
    DRAINED,      // StateCallback has indicated that the drain is complete.
    ERRORED,      // Stream disabled due to an internal error.
    SHUTDOWN      // Shutdown has been called
  };

  StreamState mState;

  DataSource& mDataSource;

  bool mPrefillQuirk;

  // The device info of the current sink. If null
  // the default device is used. It is set
  // during the Init() in decoder thread.
  RefPtr<AudioDeviceInfo> mSinkInfo;
};

}  // namespace mozilla

#endif
