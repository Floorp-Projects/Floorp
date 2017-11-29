/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"
#include <stdio.h>
#include <algorithm>
#include "mozilla/Assertions.h"
#include "MediaTrackConstraints.h"
#include "mtransport/runnable_utils.h"
#include "nsAutoPtr.h"
#include "AudioConverter.h"

// scoped_ptr.h uses FF
#ifdef FF
#undef FF
#endif
#include "webrtc/modules/audio_device/opensl/single_rw_fifo.h"

#define CHANNELS 1
#define ENCODING "L16"
#define DEFAULT_PORT 5555

#define SAMPLE_RATE(freq) ((freq)*2*8) // bps, 16-bit samples
#define SAMPLE_LENGTH(freq) (((freq)*10)/1000)

// These are restrictions from the webrtc.org code
#define MAX_CHANNELS 2
#define MAX_SAMPLING_FREQ 48000 // Hz - multiple of 100

#define MAX_AEC_FIFO_DEPTH 200 // ms - multiple of 10
static_assert(!(MAX_AEC_FIFO_DEPTH % 10), "Invalid MAX_AEC_FIFO_DEPTH");

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOG_FRAMES(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

LogModule* AudioLogModule() {
  static mozilla::LazyLogModule log("AudioLatency");
  return static_cast<LogModule*>(log);
}

/**
 * Webrtc microphone source source.
 */
NS_IMPL_ISUPPORTS0(MediaEngineWebRTCMicrophoneSource)
NS_IMPL_ISUPPORTS0(MediaEngineWebRTCAudioCaptureSource)

int MediaEngineWebRTCMicrophoneSource::sChannelsOpen = 0;
ScopedCustomReleasePtr<webrtc::VoEBase> MediaEngineWebRTCMicrophoneSource::mVoEBase;
ScopedCustomReleasePtr<webrtc::VoEExternalMedia> MediaEngineWebRTCMicrophoneSource::mVoERender;
ScopedCustomReleasePtr<webrtc::VoENetwork> MediaEngineWebRTCMicrophoneSource::mVoENetwork;
ScopedCustomReleasePtr<webrtc::VoEAudioProcessing> MediaEngineWebRTCMicrophoneSource::mVoEProcessing;

AudioOutputObserver::AudioOutputObserver()
  : mPlayoutFreq(0)
  , mPlayoutChannels(0)
  , mChunkSize(0)
  , mSaved(nullptr)
  , mSamplesSaved(0)
  , mDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100)
{
  // Buffers of 10ms chunks
  mPlayoutFifo = new webrtc::SingleRwFifo(MAX_AEC_FIFO_DEPTH/10);
}

AudioOutputObserver::~AudioOutputObserver()
{
  Clear();
  free(mSaved);
  mSaved = nullptr;
}

void
AudioOutputObserver::Clear()
{
  while (mPlayoutFifo->size() > 0) {
    free(mPlayoutFifo->Pop());
  }
  // we'd like to touch mSaved here, but we can't if we might still be getting callbacks
}

FarEndAudioChunk *
AudioOutputObserver::Pop()
{
  return (FarEndAudioChunk *) mPlayoutFifo->Pop();
}

uint32_t
AudioOutputObserver::Size()
{
  return mPlayoutFifo->size();
}

// static
void
AudioOutputObserver::InsertFarEnd(const AudioDataValue *aBuffer, uint32_t aFrames, bool aOverran,
                                  int aFreq, int aChannels)
{
  // Prepare for downmix if needed
  int channels = aChannels;
  if (aChannels > MAX_CHANNELS) {
    channels = MAX_CHANNELS;
  }

  if (mPlayoutChannels != 0) {
    if (mPlayoutChannels != static_cast<uint32_t>(channels)) {
      MOZ_CRASH();
    }
  } else {
    MOZ_ASSERT(channels <= MAX_CHANNELS);
    mPlayoutChannels = static_cast<uint32_t>(channels);
  }
  if (mPlayoutFreq != 0) {
    if (mPlayoutFreq != static_cast<uint32_t>(aFreq)) {
      MOZ_CRASH();
    }
  } else {
    MOZ_ASSERT(aFreq <= MAX_SAMPLING_FREQ);
    MOZ_ASSERT(!(aFreq % 100), "Sampling rate for far end data should be multiple of 100.");
    mPlayoutFreq = aFreq;
    mChunkSize = aFreq/100; // 10ms
  }

#ifdef LOG_FAREND_INSERTION
  static FILE *fp = fopen("insertfarend.pcm","wb");
#endif

  if (mSaved) {
    // flag overrun as soon as possible, and only once
    mSaved->mOverrun = aOverran;
    aOverran = false;
  }
  // Rechunk to 10ms.
  // The AnalyzeReverseStream() and WebRtcAec_BufferFarend() functions insist on 10ms
  // samples per call.  Annoying...
  while (aFrames) {
    if (!mSaved) {
      mSaved = (FarEndAudioChunk *) moz_xmalloc(sizeof(FarEndAudioChunk) +
                                                (mChunkSize * channels - 1)*sizeof(int16_t));
      mSaved->mSamples = mChunkSize;
      mSaved->mOverrun = aOverran;
      aOverran = false;
    }
    uint32_t to_copy = mChunkSize - mSamplesSaved;
    if (to_copy > aFrames) {
      to_copy = aFrames;
    }

    int16_t* dest = &(mSaved->mData[mSamplesSaved * channels]);
    if (aChannels > MAX_CHANNELS) {
      AudioConverter converter(AudioConfig(aChannels, 0), AudioConfig(channels, 0));
      converter.Process(mDownmixBuffer, aBuffer, to_copy);
      ConvertAudioSamples(mDownmixBuffer.Data(), dest, to_copy * channels);
    } else {
      ConvertAudioSamples(aBuffer, dest, to_copy * channels);
    }

#ifdef LOG_FAREND_INSERTION
    if (fp) {
      fwrite(&(mSaved->mData[mSamplesSaved * aChannels]), to_copy * aChannels, sizeof(int16_t), fp);
    }
#endif
    aFrames -= to_copy;
    mSamplesSaved += to_copy;
    aBuffer += to_copy * aChannels;

    if (mSamplesSaved >= mChunkSize) {
      int free_slots = mPlayoutFifo->capacity() - mPlayoutFifo->size();
      if (free_slots <= 0) {
        // XXX We should flag an overrun for the reader.  We can't drop data from it due to
        // thread safety issues.
        break;
      } else {
        mPlayoutFifo->Push((int8_t *) mSaved); // takes ownership
        mSaved = nullptr;
        mSamplesSaved = 0;
      }
    }
  }
}

MediaEngineWebRTCMicrophoneSource::MediaEngineWebRTCMicrophoneSource(
    webrtc::VoiceEngine* aVoiceEnginePtr,
    mozilla::AudioInput* aAudioInput,
    int aIndex,
    const char* name,
    const char* uuid,
    bool aDelayAgnostic,
    bool aExtendedFilter)
  : MediaEngineAudioSource(kReleased)
  , mVoiceEngine(aVoiceEnginePtr)
  , mAudioInput(aAudioInput)
  , mMonitor("WebRTCMic.Monitor")
  , mCapIndex(aIndex)
  , mChannel(-1)
  , mDelayAgnostic(aDelayAgnostic)
  , mExtendedFilter(aExtendedFilter)
  , mTrackID(TRACK_NONE)
  , mStarted(false)
  , mSampleFrequency(MediaEngine::DEFAULT_SAMPLE_RATE)
  , mTotalFrames(0)
  , mLastLogFrames(0)
  , mNullTransport(nullptr)
  , mSkipProcessing(false)
  , mInputDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100)
{
  MOZ_ASSERT(aVoiceEnginePtr);
  MOZ_ASSERT(aAudioInput);
  mDeviceName.Assign(NS_ConvertUTF8toUTF16(name));
  mDeviceUUID.Assign(uuid);
  mListener = new mozilla::WebRTCAudioDataListener(this);
  mSettings->mEchoCancellation.Construct(0);
  mSettings->mAutoGainControl.Construct(0);
  mSettings->mNoiseSuppression.Construct(0);
  mSettings->mChannelCount.Construct(0);
  // We'll init lazily as needed
}

void
MediaEngineWebRTCMicrophoneSource::GetName(nsAString& aName) const
{
  aName.Assign(mDeviceName);
}

void
MediaEngineWebRTCMicrophoneSource::GetUUID(nsACString& aUUID) const
{
  aUUID.Assign(mDeviceUUID);
}

// GetBestFitnessDistance returns the best distance the capture device can offer
// as a whole, given an accumulated number of ConstraintSets.
// Ideal values are considered in the first ConstraintSet only.
// Plain values are treated as Ideal in the first ConstraintSet.
// Plain values are treated as Exact in subsequent ConstraintSets.
// Infinity = UINT32_MAX e.g. device cannot satisfy accumulated ConstraintSets.
// A finite result may be used to calculate this device's ranking as a choice.

uint32_t MediaEngineWebRTCMicrophoneSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const
{
  uint32_t distance = 0;

  for (const auto* cs : aConstraintSets) {
    distance = GetMinimumFitnessDistance(*cs, aDeviceId);
    break; // distance is read from first entry only
  }
  return distance;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Restart(AllocationHandle* aHandle,
                                           const dom::MediaTrackConstraints& aConstraints,
                                           const MediaEnginePrefs &aPrefs,
                                           const nsString& aDeviceId,
                                           const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aHandle);
  NormalizedConstraints constraints(aConstraints);
  return ReevaluateAllocation(aHandle, &constraints, aPrefs, aDeviceId,
                              aOutBadConstraint);
}

bool operator == (const MediaEnginePrefs& a, const MediaEnginePrefs& b)
{
  return !memcmp(&a, &b, sizeof(MediaEnginePrefs));
};

nsresult
MediaEngineWebRTCMicrophoneSource::UpdateSingleSource(
    const AllocationHandle* aHandle,
    const NormalizedConstraints& aNetConstraints,
    const MediaEnginePrefs& aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  FlattenedConstraints c(aNetConstraints);

  MediaEnginePrefs prefs = aPrefs;
  prefs.mAecOn = c.mEchoCancellation.Get(prefs.mAecOn);
  prefs.mAgcOn = c.mAutoGainControl.Get(prefs.mAgcOn);
  prefs.mNoiseOn = c.mNoiseSuppression.Get(prefs.mNoiseOn);
  uint32_t maxChannels = 1;
  if (mAudioInput->GetMaxAvailableChannels(maxChannels) != 0) {
    return NS_ERROR_FAILURE;
  }
  // Check channelCount violation
  if (static_cast<int32_t>(maxChannels) < c.mChannelCount.mMin ||
      static_cast<int32_t>(maxChannels) > c.mChannelCount.mMax) {
    *aOutBadConstraint = "channelCount";
    return NS_ERROR_FAILURE;
  }
  // Clamp channelCount to a valid value
  if (prefs.mChannels <= 0) {
    prefs.mChannels = static_cast<int32_t>(maxChannels);
  }
  prefs.mChannels = c.mChannelCount.Get(std::min(prefs.mChannels,
                                        static_cast<int32_t>(maxChannels)));
  // Clamp channelCount to a valid value
  prefs.mChannels = std::max(1, std::min(prefs.mChannels, static_cast<int32_t>(maxChannels)));

  LOG(("Audio config: aec: %d, agc: %d, noise: %d, channels: %d",
      prefs.mAecOn ? prefs.mAec : -1,
      prefs.mAgcOn ? prefs.mAgc : -1,
      prefs.mNoiseOn ? prefs.mNoise : -1,
      prefs.mChannels));

  switch (mState) {
    case kReleased:
      MOZ_ASSERT(aHandle);
      if (sChannelsOpen == 0) {
        if (!InitEngine()) {
          LOG(("Audio engine is not initalized"));
          return NS_ERROR_FAILURE;
        }
      } else {
        // Until we fix (or wallpaper) support for multiple mic input
        // (Bug 1238038) fail allocation for a second device
        return NS_ERROR_FAILURE;
      }
      if (mAudioInput->SetRecordingDevice(mCapIndex)) {
         return NS_ERROR_FAILURE;
      }
      mAudioInput->SetUserChannelCount(prefs.mChannels);
      if (!AllocChannel()) {
        FreeChannel();
        LOG(("Audio device is not initalized"));
        return NS_ERROR_FAILURE;
      }
      LOG(("Audio device %d allocated", mCapIndex));
      {
        // Update with the actual applied channelCount in order
        // to store it in settings.
        uint32_t channelCount = 0;
        mAudioInput->GetChannelCount(channelCount);
        MOZ_ASSERT(channelCount > 0);
        prefs.mChannels = channelCount;
      }
      break;

    case kStarted:
      if (prefs == mLastPrefs) {
        return NS_OK;
      }

      if (prefs.mChannels != mLastPrefs.mChannels) {
        MOZ_ASSERT(mSources.Length() > 0);
        auto& source = mSources.LastElement();
        mAudioInput->SetUserChannelCount(prefs.mChannels);
        // Get validated number of channel
        uint32_t channelCount = 0;
        mAudioInput->GetChannelCount(channelCount);
        MOZ_ASSERT(channelCount > 0 && mLastPrefs.mChannels > 0);
        // Check if new validated channels is the same as previous
        if (static_cast<uint32_t>(mLastPrefs.mChannels) != channelCount &&
            !source->OpenNewAudioCallbackDriver(mListener)) {
          return NS_ERROR_FAILURE;
        }
        // Update settings
        prefs.mChannels = channelCount;
      }

      if (MOZ_LOG_TEST(GetMediaManagerLog(), LogLevel::Debug)) {
        MonitorAutoLock lock(mMonitor);
        if (mSources.IsEmpty()) {
          LOG(("Audio device %d reallocated", mCapIndex));
        } else {
          LOG(("Audio device %d allocated shared", mCapIndex));
        }
      }
      break;

    default:
      LOG(("Audio device %d in ignored state %d", mCapIndex, mState));
      break;
  }

  if (sChannelsOpen > 0) {
    int error;

    error = mVoEProcessing->SetEcStatus(prefs.mAecOn, (webrtc::EcModes)prefs.mAec);
    if (error) {
      LOG(("%s Error setting Echo Status: %d ",__FUNCTION__, error));
      // Overhead of capturing all the time is very low (<0.1% of an audio only call)
      if (prefs.mAecOn) {
        error = mVoEProcessing->SetEcMetricsStatus(true);
        if (error) {
          LOG(("%s Error setting Echo Metrics: %d ",__FUNCTION__, error));
        }
      }
    }
    error = mVoEProcessing->SetAgcStatus(prefs.mAgcOn, (webrtc::AgcModes)prefs.mAgc);
    if (error) {
      LOG(("%s Error setting AGC Status: %d ",__FUNCTION__, error));
    }
    error = mVoEProcessing->SetNsStatus(prefs.mNoiseOn, (webrtc::NsModes)prefs.mNoise);
    if (error) {
      LOG(("%s Error setting NoiseSuppression Status: %d ",__FUNCTION__, error));
    }
  }

  // we don't allow switching from non-fast-path to fast-path on the fly yet
  if (mState != kStarted) {
    mSkipProcessing = !(prefs.mAecOn || prefs.mAgcOn || prefs.mNoiseOn);
    if (mSkipProcessing) {
      mSampleFrequency = MediaEngine::USE_GRAPH_RATE;
    } else {
      // make sure we route a copy of the mixed audio output of this MSG to the
      // AEC
      if (!mAudioOutputObserver) {
        mAudioOutputObserver = new AudioOutputObserver();
      }
    }
  }
  SetLastPrefs(prefs);
  return NS_OK;
}

void
MediaEngineWebRTCMicrophoneSource::SetLastPrefs(
    const MediaEnginePrefs& aPrefs)
{
  mLastPrefs = aPrefs;

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;

  NS_DispatchToMainThread(media::NewRunnableFrom([that, aPrefs]() mutable {
    that->mSettings->mEchoCancellation.Value() = aPrefs.mAecOn;
    that->mSettings->mAutoGainControl.Value() = aPrefs.mAgcOn;
    that->mSettings->mNoiseSuppression.Value() = aPrefs.mNoiseOn;
    that->mSettings->mChannelCount.Value() = aPrefs.mChannels;
    return NS_OK;
  }));
}


nsresult
MediaEngineWebRTCMicrophoneSource::Deallocate(AllocationHandle* aHandle)
{
  AssertIsOnOwningThread();

  Super::Deallocate(aHandle);

  if (!mRegisteredHandles.Length()) {
    // If empty, no callbacks to deliver data should be occuring
    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }

    FreeChannel();
    LOG(("Audio device %d deallocated", mCapIndex));
  } else {
    LOG(("Audio device %d deallocated but still in use", mCapIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Start(SourceMediaStream *aStream,
                                         TrackID aID,
                                         const PrincipalHandle& aPrincipalHandle)
{
  AssertIsOnOwningThread();
  if (sChannelsOpen == 0 || !aStream) {
    return NS_ERROR_FAILURE;
  }

  // Until we fix bug 1400488 we need to block a second tab (OuterWindow)
  // from opening an already-open device.  If it's the same tab, they
  // will share a Graph(), and we can allow it.
  if (!mSources.IsEmpty() && aStream->Graph() != mSources[0]->Graph()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  {
    MonitorAutoLock lock(mMonitor);
    mSources.AppendElement(aStream);
    mPrincipalHandles.AppendElement(aPrincipalHandle);
    MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());
  }

  AudioSegment* segment = new AudioSegment();
  if (mSampleFrequency == MediaEngine::USE_GRAPH_RATE) {
    mSampleFrequency = aStream->GraphRate();
  }
  aStream->AddAudioTrack(aID, mSampleFrequency, 0, segment, SourceMediaStream::ADDTRACK_QUEUED);

  // XXX Make this based on the pref.
  aStream->RegisterForAudioMixing();
  LOG(("Start audio for stream %p", aStream));

  if (!mListener) {
    mListener = new mozilla::WebRTCAudioDataListener(this);
  }
  if (mState == kStarted) {
    MOZ_ASSERT(aID == mTrackID);
    // Make sure we're associated with this stream
    mAudioInput->StartRecording(aStream, mListener);
    return NS_OK;
  }
  mState = kStarted;
  mTrackID = aID;

  // Make sure logger starts before capture
  AsyncLatencyLogger::Get(true);

  if (mAudioOutputObserver) {
    mAudioOutputObserver->Clear();
  }

  if (mVoEBase->StartReceive(mChannel)) {
    return NS_ERROR_FAILURE;
  }

  // Must be *before* StartSend() so it will notice we selected external input (full_duplex)
  mAudioInput->StartRecording(aStream, mListener);

  if (mVoEBase->StartSend(mChannel)) {
    return NS_ERROR_FAILURE;
  }

  // Attach external media processor, so this::Process will be called.
  mVoERender->RegisterExternalMediaProcessing(mChannel, webrtc::kRecordingPerChannel, *this);

  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Stop(SourceMediaStream *aSource, TrackID aID)
{
  AssertIsOnOwningThread();
  {
    MonitorAutoLock lock(mMonitor);

    size_t sourceIndex = mSources.IndexOf(aSource);
    if (sourceIndex == mSources.NoIndex) {
      // Already stopped - this is allowed
      return NS_OK;
    }
    mSources.RemoveElementAt(sourceIndex);
    mPrincipalHandles.RemoveElementAt(sourceIndex);
    MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());

    aSource->EndTrack(aID);

    if (!mSources.IsEmpty()) {
      mAudioInput->StopRecording(aSource);
      return NS_OK;
    }
    if (mState != kStarted) {
      return NS_ERROR_FAILURE;
    }
    if (!mVoEBase) {
      return NS_ERROR_FAILURE;
    }

    mState = kStopped;
  }
  if (mListener) {
    // breaks a cycle, since the WebRTCAudioDataListener has a RefPtr to us
    mListener->Shutdown();
    mListener = nullptr;
  }

  mAudioInput->StopRecording(aSource);

  mVoERender->DeRegisterExternalMediaProcessing(mChannel, webrtc::kRecordingPerChannel);

  if (mVoEBase->StopSend(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  if (mVoEBase->StopReceive(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
MediaEngineWebRTCMicrophoneSource::NotifyPull(MediaStreamGraph *aGraph,
                                              SourceMediaStream *aSource,
                                              TrackID aID,
                                              StreamTime aDesiredTime,
                                              const PrincipalHandle& aPrincipalHandle)
{
  // Ignore - we push audio data
  LOG_FRAMES(("NotifyPull, desired = %" PRId64, (int64_t) aDesiredTime));
}

void
MediaEngineWebRTCMicrophoneSource::NotifyOutputData(MediaStreamGraph* aGraph,
                                                    AudioDataValue* aBuffer,
                                                    size_t aFrames,
                                                    TrackRate aRate,
                                                    uint32_t aChannels)
{
  if (mAudioOutputObserver) {
    mAudioOutputObserver->InsertFarEnd(aBuffer, aFrames, false,
                                  aRate, aChannels);
  }
}

void
MediaEngineWebRTCMicrophoneSource::PacketizeAndProcess(MediaStreamGraph* aGraph,
                                                       const AudioDataValue* aBuffer,
                                                       size_t aFrames,
                                                       TrackRate aRate,
                                                       uint32_t aChannels)
{
  // This will call Process() with data coming out of the AEC/NS/AGC/etc chain
  if (!mPacketizer ||
      mPacketizer->PacketSize() != aRate/100u ||
      mPacketizer->Channels() != aChannels) {
    // It's ok to drop the audio still in the packetizer here.
    mPacketizer =
      new AudioPacketizer<AudioDataValue, int16_t>(aRate/100, aChannels);
  }

  mPacketizer->Input(aBuffer, static_cast<uint32_t>(aFrames));

  while (mPacketizer->PacketsAvailable()) {
    uint32_t samplesPerPacket = mPacketizer->PacketSize() *
      mPacketizer->Channels();
    if (mInputBuffer.Length() < samplesPerPacket) {
      mInputBuffer.SetLength(samplesPerPacket);
    }
    int16_t* packet = mInputBuffer.Elements();
    mPacketizer->Output(packet);

    if (aChannels > MAX_CHANNELS) {
      AudioConverter converter(AudioConfig(aChannels, 0, AudioConfig::FORMAT_S16),
                               AudioConfig(MAX_CHANNELS, 0, AudioConfig::FORMAT_S16));
      converter.Process(mInputDownmixBuffer, packet, mPacketizer->PacketSize());
      mVoERender->ExternalRecordingInsertData(mInputDownmixBuffer.Data(),
                                              mPacketizer->PacketSize() * MAX_CHANNELS,
                                              aRate, 0);
    } else {
      mVoERender->ExternalRecordingInsertData(packet, samplesPerPacket, aRate, 0);
    }
  }
}

template<typename T>
void
MediaEngineWebRTCMicrophoneSource::InsertInGraph(const T* aBuffer,
                                                 size_t aFrames,
                                                 uint32_t aChannels)
{
  MonitorAutoLock lock(mMonitor);
  if (mState != kStarted) {
    return;
  }

  if (MOZ_LOG_TEST(AudioLogModule(), LogLevel::Debug)) {
    mTotalFrames += aFrames;
    if (mTotalFrames > mLastLogFrames + mSampleFrequency) { // ~ 1 second
      MOZ_LOG(AudioLogModule(), LogLevel::Debug,
              ("%p: Inserting %zu samples into graph, total frames = %" PRIu64,
               (void*)this, aFrames, mTotalFrames));
      mLastLogFrames = mTotalFrames;
    }
  }

  size_t len = mSources.Length();
  for (size_t i = 0; i < len; i++) {
    if (!mSources[i]) {
      continue;
    }

    TimeStamp insertTime;
    // Make sure we include the stream and the track.
    // The 0:1 is a flag to note when we've done the final insert for a given input block.
    LogTime(AsyncLatencyLogger::AudioTrackInsertion,
            LATENCY_STREAM_ID(mSources[i].get(), mTrackID),
            (i+1 < len) ? 0 : 1, insertTime);

    // Bug 971528 - Support stereo capture in gUM
    MOZ_ASSERT(aChannels >= 1 && aChannels <= 8,
               "Support up to 8 channels");

    nsAutoPtr<AudioSegment> segment(new AudioSegment());
    RefPtr<SharedBuffer> buffer =
      SharedBuffer::Create(aFrames * aChannels * sizeof(T));
    AutoTArray<const T*, 8> channels;
    if (aChannels == 1) {
      PodCopy(static_cast<T*>(buffer->Data()), aBuffer, aFrames);
      channels.AppendElement(static_cast<T*>(buffer->Data()));
    } else {
      channels.SetLength(aChannels);
      AutoTArray<T*, 8> write_channels;
      write_channels.SetLength(aChannels);
      T * samples = static_cast<T*>(buffer->Data());

      size_t offset = 0;
      for(uint32_t i = 0; i < aChannels; ++i) {
        channels[i] = write_channels[i] = samples + offset;
        offset += aFrames;
      }

      DeinterleaveAndConvertBuffer(aBuffer,
                                   aFrames,
                                   aChannels,
                                   write_channels.Elements());
    }

    MOZ_ASSERT(aChannels == channels.Length());
    segment->AppendFrames(buffer.forget(), channels, aFrames,
                         mPrincipalHandles[i]);
    segment->GetStartTime(insertTime);

    mSources[i]->AppendToTrack(mTrackID, segment);
  }
}

// Called back on GraphDriver thread!
// Note this can be called back after ::Shutdown()
void
MediaEngineWebRTCMicrophoneSource::NotifyInputData(MediaStreamGraph* aGraph,
                                                   const AudioDataValue* aBuffer,
                                                   size_t aFrames,
                                                   TrackRate aRate,
                                                   uint32_t aChannels)
{
  // If some processing is necessary, packetize and insert in the WebRTC.org
  // code. Otherwise, directly insert the mic data in the MSG, bypassing all processing.
  if (PassThrough()) {
    InsertInGraph<AudioDataValue>(aBuffer, aFrames, aChannels);
  } else {
    PacketizeAndProcess(aGraph, aBuffer, aFrames, aRate, aChannels);
  }
}

#define ResetProcessingIfNeeded(_processing)                        \
do {                                                                \
  webrtc::_processing##Modes mode;                                  \
  int rv = mVoEProcessing->Get##_processing##Status(enabled, mode); \
  if (rv) {                                                         \
    NS_WARNING("Could not get the status of the "                   \
     #_processing " on device change.");                            \
    return;                                                         \
  }                                                                 \
                                                                    \
  if (enabled) {                                                    \
    rv = mVoEProcessing->Set##_processing##Status(!enabled);        \
    if (rv) {                                                       \
      NS_WARNING("Could not reset the status of the "               \
      #_processing " on device change.");                           \
      return;                                                       \
    }                                                               \
                                                                    \
    rv = mVoEProcessing->Set##_processing##Status(enabled);         \
    if (rv) {                                                       \
      NS_WARNING("Could not reset the status of the "               \
      #_processing " on device change.");                           \
      return;                                                       \
    }                                                               \
  }                                                                 \
}  while(0)

void
MediaEngineWebRTCMicrophoneSource::DeviceChanged() {
  // Reset some processing
  bool enabled;
  ResetProcessingIfNeeded(Agc);
  ResetProcessingIfNeeded(Ec);
  ResetProcessingIfNeeded(Ns);
}

bool
MediaEngineWebRTCMicrophoneSource::InitEngine()
{
  MOZ_ASSERT(!mVoEBase);
  mVoEBase = webrtc::VoEBase::GetInterface(mVoiceEngine);

  mVoEBase->Init();
  webrtc::Config config;
  config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(mExtendedFilter));
  config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(mDelayAgnostic));
  mVoEBase->audio_processing()->SetExtraOptions(config);

  mVoERender = webrtc::VoEExternalMedia::GetInterface(mVoiceEngine);
  if (mVoERender) {
    mVoENetwork = webrtc::VoENetwork::GetInterface(mVoiceEngine);
    if (mVoENetwork) {
      mVoEProcessing = webrtc::VoEAudioProcessing::GetInterface(mVoiceEngine);
      if (mVoEProcessing) {
        mNullTransport = new NullTransport();
        return true;
      }
    }
  }
  DeInitEngine();
  return false;
}

// This shuts down the engine when no channel is open
void
MediaEngineWebRTCMicrophoneSource::DeInitEngine()
{
  if (mVoEBase) {
    mVoEBase->Terminate();
    delete mNullTransport;
    mNullTransport = nullptr;

    mVoEProcessing = nullptr;
    mVoENetwork = nullptr;
    mVoERender = nullptr;
    mVoEBase = nullptr;
  }
}

// This shuts down the engine when no channel is open.
// mState records if a channel is allocated (slightly redundantly to mChannel)
void
MediaEngineWebRTCMicrophoneSource::FreeChannel()
{
  if (mState != kReleased) {
    if (mChannel != -1) {
      MOZ_ASSERT(mVoENetwork && mVoEBase);
      if (mVoENetwork) {
        mVoENetwork->DeRegisterExternalTransport(mChannel);
      }
      if (mVoEBase) {
        mVoEBase->DeleteChannel(mChannel);
      }
      mChannel = -1;
    }
    mState = kReleased;

    MOZ_ASSERT(sChannelsOpen > 0);
    if (--sChannelsOpen == 0) {
      DeInitEngine();
    }
  }
}

bool
MediaEngineWebRTCMicrophoneSource::AllocChannel()
{
  MOZ_ASSERT(mVoEBase);

  mChannel = mVoEBase->CreateChannel();
  if (mChannel >= 0) {
    if (!mVoENetwork->RegisterExternalTransport(mChannel, *mNullTransport)) {
      mSampleFrequency = MediaEngine::DEFAULT_SAMPLE_RATE;
      LOG(("%s: sampling rate %u", __FUNCTION__, mSampleFrequency));

      // Check for availability.
      if (!mAudioInput->SetRecordingDevice(mCapIndex)) {
        // Because of the permission mechanism of B2G, we need to skip the status
        // check here.
        bool avail = false;
        mAudioInput->GetRecordingDeviceStatus(avail);
        if (!avail) {
          if (sChannelsOpen == 0) {
            DeInitEngine();
          }
          return false;
        }

        // Set "codec" to PCM, 32kHz on device's channels
        ScopedCustomReleasePtr<webrtc::VoECodec> ptrVoECodec(webrtc::VoECodec::GetInterface(mVoiceEngine));
        if (ptrVoECodec) {
          webrtc::CodecInst codec;
          strcpy(codec.plname, ENCODING);
          codec.channels = CHANNELS;
          uint32_t maxChannels = 0;
          if (mAudioInput->GetMaxAvailableChannels(maxChannels) == 0) {
            MOZ_ASSERT(maxChannels);
            codec.channels = std::min<uint32_t>(maxChannels, MAX_CHANNELS);
          }
          MOZ_ASSERT(mSampleFrequency == 16000 || mSampleFrequency == 32000);
          codec.rate = SAMPLE_RATE(mSampleFrequency);
          codec.plfreq = mSampleFrequency;
          codec.pacsize = SAMPLE_LENGTH(mSampleFrequency);
          codec.pltype = 0; // Default payload type

          if (!ptrVoECodec->SetSendCodec(mChannel, codec)) {
            mState = kAllocated;
            sChannelsOpen++;
            return true;
          }
        }
      }
    }
  }
  mVoEBase->DeleteChannel(mChannel);
  mChannel = -1;
  if (sChannelsOpen == 0) {
    DeInitEngine();
  }
  return false;
}

void
MediaEngineWebRTCMicrophoneSource::Shutdown()
{
  Super::Shutdown();
  if (mListener) {
    // breaks a cycle, since the WebRTCAudioDataListener has a RefPtr to us
    mListener->Shutdown();
    // Don't release the webrtc.org pointers yet until the Listener is (async) shutdown
    mListener = nullptr;
  }

  if (mState == kStarted) {
    SourceMediaStream *source;
    bool empty;

    while (1) {
      {
        MonitorAutoLock lock(mMonitor);
        empty = mSources.IsEmpty();
        if (empty) {
          break;
        }
        source = mSources[0];
      }
      Stop(source, kAudioTrack); // XXX change to support multiple tracks
    }
    MOZ_ASSERT(mState == kStopped);
  }

  while (mRegisteredHandles.Length()) {
    MOZ_ASSERT(mState == kAllocated || mState == kStopped);
    // on last Deallocate(), FreeChannel()s and DeInit()s if all channels are released
    Deallocate(mRegisteredHandles[0].get());
  }
  MOZ_ASSERT(mState == kReleased);

  mAudioInput = nullptr;
}

typedef int16_t sample;

void
MediaEngineWebRTCMicrophoneSource::Process(int channel,
                                           webrtc::ProcessingTypes type,
                                           sample *audio10ms, size_t length,
                                           int samplingFreq, bool isStereo)
{
  MOZ_ASSERT(!PassThrough(), "This should be bypassed when in PassThrough mode.");
  // On initial capture, throw away all far-end data except the most recent sample
  // since it's already irrelevant and we want to keep avoid confusing the AEC far-end
  // input code with "old" audio.
  if (!mStarted) {
    mStarted  = true;
    while (mAudioOutputObserver->Size() > 1) {
      free(mAudioOutputObserver->Pop()); // only call if size() > 0
    }
  }

  while (mAudioOutputObserver->Size() > 0) {
    FarEndAudioChunk *buffer = mAudioOutputObserver->Pop(); // only call if size() > 0
    if (buffer) {
      int length = buffer->mSamples;
      int res = mVoERender->ExternalPlayoutData(buffer->mData,
                                                mAudioOutputObserver->PlayoutFrequency(),
                                                mAudioOutputObserver->PlayoutChannels(),
                                                length);
      free(buffer);
      if (res == -1) {
        return;
      }
    }
  }

  uint32_t channels = isStereo ? 2 : 1;
  InsertInGraph<int16_t>(audio10ms, length, channels);
}

void
MediaEngineWebRTCAudioCaptureSource::GetName(nsAString &aName) const
{
  aName.AssignLiteral("AudioCapture");
}

void
MediaEngineWebRTCAudioCaptureSource::GetUUID(nsACString &aUUID) const
{
  nsID uuid;
  char uuidBuffer[NSID_LENGTH];
  nsCString asciiString;
  ErrorResult rv;

  rv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (rv.Failed()) {
    aUUID.AssignLiteral("");
    return;
  }


  uuid.ToProvidedString(uuidBuffer);
  asciiString.AssignASCII(uuidBuffer);

  // Remove {} and the null terminator
  aUUID.Assign(Substring(asciiString, 1, NSID_LENGTH - 3));
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Start(SourceMediaStream *aMediaStream,
                                           TrackID aId,
                                           const PrincipalHandle& aPrincipalHandle)
{
  AssertIsOnOwningThread();
  aMediaStream->AddTrack(aId, 0, new AudioSegment());
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Stop(SourceMediaStream *aMediaStream,
                                          TrackID aId)
{
  AssertIsOnOwningThread();
  aMediaStream->EndAllTrackAndFinish();
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Restart(
    AllocationHandle* aHandle,
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  MOZ_ASSERT(!aHandle);
  return NS_OK;
}

uint32_t
MediaEngineWebRTCAudioCaptureSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const
{
  // There is only one way of capturing audio for now, and it's always adequate.
  return 0;
}

}
