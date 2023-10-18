/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioDecoderInputTrack_h
#define AudioDecoderInputTrack_h

#include "AudioSegment.h"
#include "MediaEventSource.h"
#include "MediaTimer.h"
#include "MediaTrackGraph.h"
#include "MediaSegment.h"
#include "TimeUnits.h"
#include "mozilla/SPSCQueue.h"
#include "mozilla/StateMirroring.h"
#include "nsISerialEventTarget.h"

namespace mozilla {

class AudioData;
class AudioInfo;
class RLBoxSoundTouch;

/**
 * AudioDecoderInputTrack is used as a source for the audio decoder data, which
 * supports adjusting playback rate and preserve pitch.
 * The owner of this track would be responsible to push audio data via
 * `AppendData()` into a SPSC queue, which is a thread-safe queue between the
 * decoder thread (producer) and the graph thread (consumer). MediaTrackGraph
 * requires data via `ProcessInput()`, then AudioDecoderInputTrack would convert
 * (based on sample rate and playback rate) and append the amount of needed
 * audio frames onto the output segment that would be used by MediaTrackGraph.
 */
class AudioDecoderInputTrack final : public ProcessedMediaTrack {
 public:
  static AudioDecoderInputTrack* Create(MediaTrackGraph* aGraph,
                                        nsISerialEventTarget* aDecoderThread,
                                        const AudioInfo& aInfo,
                                        float aPlaybackRate, float aVolume,
                                        bool aPreservesPitch);

  // SPSCData suppports filling different supported type variants, and is used
  // to achieve a thread-safe information exchange between the decoder thread
  // and the graph thread.
  struct SPSCData final {
    struct Empty {};
    struct ClearFutureData {};
    struct DecodedData {
      DecodedData()
          : mStartTime(media::TimeUnit::Invalid()),
            mEndTime(media::TimeUnit::Invalid()) {}
      DecodedData(DecodedData&& aDecodedData)
          : mSegment(std::move(aDecodedData.mSegment)) {
        mStartTime = aDecodedData.mStartTime;
        mEndTime = aDecodedData.mEndTime;
        aDecodedData.Clear();
      }
      DecodedData(media::TimeUnit aStartTime, media::TimeUnit aEndTime)
          : mStartTime(aStartTime), mEndTime(aEndTime) {}
      DecodedData(const DecodedData&) = delete;
      DecodedData& operator=(const DecodedData&) = delete;
      void Clear() {
        mSegment.Clear();
        mStartTime = media::TimeUnit::Invalid();
        mEndTime = media::TimeUnit::Invalid();
      }
      AudioSegment mSegment;
      media::TimeUnit mStartTime;
      media::TimeUnit mEndTime;
    };
    struct EOS {};

    SPSCData() : mData(Empty()){};
    explicit SPSCData(ClearFutureData&& aArg) : mData(std::move(aArg)){};
    explicit SPSCData(DecodedData&& aArg) : mData(std::move(aArg)){};
    explicit SPSCData(EOS&& aArg) : mData(std::move(aArg)){};

    bool HasData() const { return !mData.is<Empty>(); }
    bool IsClearFutureData() const { return mData.is<ClearFutureData>(); }
    bool IsDecodedData() const { return mData.is<DecodedData>(); }
    bool IsEOS() const { return mData.is<EOS>(); }

    DecodedData* AsDecodedData() {
      return IsDecodedData() ? &mData.as<DecodedData>() : nullptr;
    }

    Variant<Empty, ClearFutureData, DecodedData, EOS> mData;
  };

  // Decoder thread API
  void AppendData(AudioData* aAudio, const PrincipalHandle& aPrincipalHandle);
  void AppendData(nsTArray<RefPtr<AudioData>>& aAudioArray,
                  const PrincipalHandle& aPrincipalHandle);
  void NotifyEndOfStream();
  void ClearFutureData();
  void SetVolume(float aVolume);
  void SetPlaybackRate(float aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);
  // After calling this, the track are not expected to receive any new data.
  void Close();
  bool HasBatchedData() const;

  MediaEventSource<int64_t>& OnOutput() { return mOnOutput; }
  MediaEventSource<void>& OnEnd() { return mOnEnd; }

  // Graph Thread API
  void DestroyImpl() override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  uint32_t NumberOfChannels() const override;

  // The functions below are only used for testing.
  TrackTime WrittenFrames() const {
    AssertOnGraphThread();
    return mWrittenFrames;
  }
  float Volume() const {
    AssertOnGraphThread();
    return mVolume;
  }
  float PlaybackRate() const {
    AssertOnGraphThread();
    return mPlaybackRate;
  }

 protected:
  ~AudioDecoderInputTrack();

 private:
  AudioDecoderInputTrack(nsISerialEventTarget* aDecoderThread,
                         TrackRate aGraphRate, const AudioInfo& aInfo,
                         float aPlaybackRate, float aVolume,
                         bool aPreservesPitch);

  // Return false if the converted segment contains zero duration.
  bool ConvertAudioDataToSegment(AudioData* aAudio, AudioSegment& aSegment,
                                 const PrincipalHandle& aPrincipalHandle);

  void HandleSPSCData(SPSCData& aData);

  // These methods would return the total frames that we consumed from
  // `mBufferedData`.
  TrackTime AppendBufferedDataToOutput(TrackTime aExpectedDuration);
  TrackTime FillDataToTimeStretcher(TrackTime aExpectedDuration);
  TrackTime AppendTimeStretchedDataToSegment(TrackTime aExpectedDuration,
                                             AudioSegment& aOutput);
  TrackTime AppendUnstretchedDataToSegment(TrackTime aExpectedDuration,
                                           AudioSegment& aOutput);

  // Return the total frames that we retrieve from the time stretcher.
  TrackTime DrainStretchedDataIfNeeded(TrackTime aExpectedDuration,
                                       AudioSegment& aOutput);
  TrackTime GetDataFromTimeStretcher(TrackTime aExpectedDuration,
                                     AudioSegment& aOutput);
  void NotifyInTheEndOfProcessInput(TrackTime aFillDuration);

  bool HasSentAllData() const;

  bool ShouldBatchData() const;
  void BatchData(AudioData* aAudio, const PrincipalHandle& aPrincipalHandle);
  void DispatchPushBatchedDataIfNeeded();
  void PushBatchedDataIfNeeded();
  void PushDataToSPSCQueue(SPSCData& data);

  void SetVolumeImpl(float aVolume);
  void SetPlaybackRateImpl(float aPlaybackRate);
  void SetPreservesPitchImpl(bool aPreservesPitch);

  void EnsureTimeStretcher();
  void SetTempoAndRateForTimeStretcher();
  uint32_t GetChannelCountForTimeStretcher() const;

  inline void AssertOnDecoderThread() const {
    MOZ_ASSERT(mDecoderThread->IsOnCurrentThread());
  }

  const RefPtr<nsISerialEventTarget> mDecoderThread;

  // Notify the amount of audio frames which have been sent to the track.
  MediaEventProducer<int64_t> mOnOutput;
  // Notify when the track is ended.
  MediaEventProducer<void> mOnEnd;

  // These variables are ONLY used in the decoder thread.
  nsAutoRef<SpeexResamplerState> mResampler;
  uint32_t mResamplerChannelCount;
  const uint32_t mInitialInputChannels;
  TrackRate mInputSampleRate;
  DelayedScheduler mDelayedScheduler;
  bool mShutdownSPSCQueue = false;

  // These attributes are ONLY used in the graph thread.
  bool mReceivedEOS = false;
  TrackTime mWrittenFrames = 0;
  float mPlaybackRate;
  float mVolume;
  bool mPreservesPitch;

  // A thread-safe queue shared by the decoder thread and the graph thread.
  // The decoder thread is the producer side, and the graph thread is the
  // consumer side. This queue should NEVER get full. In order to achieve that,
  // we would batch input samples when SPSC queue doesn't have many available
  // capacity.
  // In addition, as the media track isn't guaranteed to be destroyed on the
  // graph thread (it could be destroyed on the main thread as well) so we might
  // not clear all data in SPSC queue when the track's `DestroyImpl()` gets
  // called. We leave to destroy the queue later when the track gets destroyed.
  SPSCQueue<SPSCData> mSPSCQueue{40};

  // When the graph requires the less amount of audio frames than the amount of
  // frames an audio data has, then the remaining part of frames would be stored
  // and used in next iteration.
  // This is ONLY used in the graph thread.
  AudioSegment mBufferedData;

  // In order to prevent SPSC queue from being full, we want to batch multiple
  // data into one to control the density of SPSC queue, the length of batched
  // data would be dynamically adjusted by queue's available capacity.
  // This is ONLY used in the decoder thread.
  SPSCData::DecodedData mBatchedData;

  // True if we've sent all data to the graph, then the track will be marked as
  // ended in the next iteration.
  bool mSentAllData = false;

  // This is used to adjust the playback rate and pitch.
  RLBoxSoundTouch* mTimeStretcher = nullptr;

  // Buffers that would be used for the time stretching.
  AutoTArray<AudioDataValue, 2> mInterleavedBuffer;
};

}  // namespace mozilla

#endif  // AudioDecoderInputTrack_h
