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
#include "MediaStreamGraphImpl.h"

// scoped_ptr.h uses FF
#ifdef FF
#undef FF
#endif
#include "webrtc/modules/audio_device/opensl/single_rw_fifo.h"
#include "webrtc/voice_engine/voice_engine_defines.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/common_audio/include/audio_util.h"

using namespace webrtc;

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

AudioOutputObserver::AudioOutputObserver()
  : mPlayoutFreq(0)
  , mPlayoutChannels(0)
  , mChunkSize(0)
  , mSaved(nullptr)
  , mSamplesSaved(0)
  , mDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100)
{
  // Buffers of 10ms chunks
  mPlayoutFifo = new SingleRwFifo(MAX_AEC_FIFO_DEPTH/10);
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
                                                (mChunkSize * channels - 1)*sizeof(AudioDataValue));
      mSaved->mSamples = mChunkSize;
      mSaved->mOverrun = aOverran;
      aOverran = false;
    }
    uint32_t to_copy = mChunkSize - mSamplesSaved;
    if (to_copy > aFrames) {
      to_copy = aFrames;
    }

    AudioDataValue* dest = &(mSaved->mData[mSamplesSaved * channels]);
    if (aChannels > MAX_CHANNELS) {
      AudioConverter converter(AudioConfig(aChannels, 0), AudioConfig(channels, 0));
      converter.Process(mDownmixBuffer, aBuffer, to_copy);
      ConvertAudioSamples(mDownmixBuffer.Data(), dest, to_copy * channels);
    } else {
      ConvertAudioSamples(aBuffer, dest, to_copy * channels);
    }

#ifdef LOG_FAREND_INSERTION
    if (fp) {
      fwrite(&(mSaved->mData[mSamplesSaved * aChannels]), to_copy * aChannels, sizeof(AudioDataValue), fp);
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
    mozilla::AudioInput* aAudioInput,
    int aIndex,
    const char* name,
    const char* uuid,
    bool aDelayAgnostic,
    bool aExtendedFilter)
  : MediaEngineAudioSource(kReleased)
  , mAudioInput(aAudioInput)
  , mAudioProcessing(AudioProcessing::Create())
  , mAudioOutputObserver(new AudioOutputObserver())
  , mMonitor("WebRTCMic.Monitor")
  , mCapIndex(aIndex)
  , mDelayAgnostic(aDelayAgnostic)
  , mExtendedFilter(aExtendedFilter)
  , mTrackID(TRACK_NONE)
  , mStarted(false)
  , mSampleFrequency(MediaEngine::USE_GRAPH_RATE)
  , mTotalFrames(0)
  , mLastLogFrames(0)
  , mSkipProcessing(false)
  , mInputDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100)
{
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

// This does an early return in case of error.
#define HANDLE_APM_ERROR(fn)                                \
do {                                                        \
  int rv = fn;                                              \
  if (rv != AudioProcessing::kNoError) {                    \
    MOZ_ASSERT_UNREACHABLE("APM error in " #fn);            \
    return;                                                 \
  }                                                         \
} while(0);

void MediaEngineWebRTCMicrophoneSource::UpdateAECSettingsIfNeeded(bool aEnable, EcModes aMode)
{
  using webrtc::EcModes;

  EchoCancellation::SuppressionLevel level;

  switch(aMode) {
    case EcModes::kEcUnchanged:
      level = mAudioProcessing->echo_cancellation()->suppression_level();
      break;
    case EcModes::kEcConference:
      level = EchoCancellation::kHighSuppression;
      break;
    case EcModes::kEcDefault:
      level = EchoCancellation::kModerateSuppression;
      break;
    case EcModes::kEcAec:
      level = EchoCancellation::kModerateSuppression;
      break;
    case EcModes::kEcAecm:
      // No suppression level to set for the mobile echo canceller
      break;
    default:
      MOZ_LOG(GetMediaManagerLog(), LogLevel::Error, ("Bad EcMode value"));
      MOZ_ASSERT_UNREACHABLE("Bad pref set in all.js or in about:config"
                             " for the echo cancelation mode.");
      // fall back to something sensible in release
      level = EchoCancellation::kModerateSuppression;
      break;
  }

  // AECm and AEC are mutually exclusive.
  if (aMode == EcModes::kEcAecm) {
    HANDLE_APM_ERROR(mAudioProcessing->echo_cancellation()->Enable(false));
    HANDLE_APM_ERROR(mAudioProcessing->echo_control_mobile()->Enable(aEnable));
  } else {
    HANDLE_APM_ERROR(mAudioProcessing->echo_control_mobile()->Enable(false));
    HANDLE_APM_ERROR(mAudioProcessing->echo_cancellation()->Enable(aEnable));
    HANDLE_APM_ERROR(mAudioProcessing->echo_cancellation()->set_suppression_level(level));
  }
}

void
MediaEngineWebRTCMicrophoneSource::UpdateAGCSettingsIfNeeded(bool aEnable, AgcModes aMode)
{
#if defined(WEBRTC_IOS) || defined(ATA) || defined(WEBRTC_ANDROID)
  if (aMode == kAgcAdaptiveAnalog) {
    MOZ_LOG(GetMediaManagerLog(),
            LogLevel::Error,
            ("Invalid AGC mode kAgcAdaptiveAnalog on mobile"));
    MOZ_ASSERT_UNREACHABLE("Bad pref set in all.js or in about:config"
                           " for the auto gain, on mobile.");
    aMode = kAgcDefault;
  }
#endif
  GainControl::Mode mode = kDefaultAgcMode;

  switch (aMode) {
    case AgcModes::kAgcDefault:
      mode = kDefaultAgcMode;
      break;
    case AgcModes::kAgcUnchanged:
      mode = mAudioProcessing->gain_control()->mode();
      break;
    case AgcModes::kAgcFixedDigital:
      mode = GainControl::Mode::kFixedDigital;
      break;
    case AgcModes::kAgcAdaptiveAnalog:
      mode = GainControl::Mode::kAdaptiveAnalog;
      break;
    case AgcModes::kAgcAdaptiveDigital:
      mode = GainControl::Mode::kAdaptiveDigital;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Bad pref set in all.js or in about:config"
                             " for the auto gain.");
      // This is a good fallback, it works regardless of the platform.
      mode = GainControl::Mode::kAdaptiveDigital;
      break;
  }

  HANDLE_APM_ERROR(mAudioProcessing->gain_control()->set_mode(mode));
  HANDLE_APM_ERROR(mAudioProcessing->gain_control()->Enable(aEnable));
}

void
MediaEngineWebRTCMicrophoneSource::UpdateNSSettingsIfNeeded(bool aEnable, NsModes aMode)
{
  NoiseSuppression::Level nsLevel;

  switch (aMode) {
    case NsModes::kNsDefault:
      nsLevel = kDefaultNsMode;
      break;
    case NsModes::kNsUnchanged:
      nsLevel = mAudioProcessing->noise_suppression()->level();
      break;
    case NsModes::kNsConference:
      nsLevel = NoiseSuppression::kHigh;
      break;
    case NsModes::kNsLowSuppression:
      nsLevel = NoiseSuppression::kLow;
      break;
    case NsModes::kNsModerateSuppression:
      nsLevel = NoiseSuppression::kModerate;
      break;
    case NsModes::kNsHighSuppression:
      nsLevel = NoiseSuppression::kHigh;
      break;
    case NsModes::kNsVeryHighSuppression:
      nsLevel = NoiseSuppression::kVeryHigh;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Bad pref set in all.js or in about:config"
                             " for the noise suppression.");
      // Pick something sensible as a faillback in release.
      nsLevel = NoiseSuppression::kModerate;
  }
  HANDLE_APM_ERROR(mAudioProcessing->noise_suppression()->set_level(nsLevel));
  HANDLE_APM_ERROR(mAudioProcessing->noise_suppression()->Enable(aEnable));
}

#undef HANDLE_APM_ERROR

nsresult
MediaEngineWebRTCMicrophoneSource::UpdateSingleSource(
    const AllocationHandle* aHandle,
    const NormalizedConstraints& aNetConstraints,
    const NormalizedConstraints& aNewConstraint, /* Ignored */
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
      if (sChannelsOpen != 0) {
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
        // If the channel count changed, tell the MSG to open a new driver with
        // the correct channel count.
        auto& source = mSources.LastElement();
        mAudioInput->SetUserChannelCount(prefs.mChannels);
        // Get validated number of channel
        uint32_t channelCount = 0;
        mAudioInput->GetChannelCount(channelCount);
        MOZ_ASSERT(channelCount > 0 && mLastPrefs.mChannels > 0);
        if (mLastPrefs.mChannels != prefs.mChannels &&
            !source->OpenNewAudioCallbackDriver(mListener)) {
          MOZ_LOG(GetMediaManagerLog(), LogLevel::Error, ("Could not open a new AudioCallbackDriver for input"));
          return NS_ERROR_FAILURE;
        }
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
    UpdateAGCSettingsIfNeeded(prefs.mAgcOn, static_cast<AgcModes>(prefs.mAgc));
    UpdateNSSettingsIfNeeded(prefs.mNoiseOn, static_cast<NsModes>(prefs.mNoise));
    UpdateAECSettingsIfNeeded(prefs.mAecOn, static_cast<EcModes>(prefs.mAec));

    webrtc::Config config;
    config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(mExtendedFilter));
    config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(mDelayAgnostic));
    mAudioProcessing->SetExtraOptions(config);
  }
  SetLastPrefs(prefs);
  return NS_OK;
}

#undef HANDLE_APM_ERROR

void
MediaEngineWebRTCMicrophoneSource::SetLastPrefs(const MediaEnginePrefs& aPrefs)
{
  mLastPrefs = aPrefs;

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;

  NS_DispatchToMainThread(media::NewRunnableFrom([that, aPrefs]() mutable {
    that->mSettings->mEchoCancellation.Value() = aPrefs.mAecOn;
    that->mSettings->mAutoGainControl.Value() = aPrefs.mAgcOn;
    that->mSettings->mNoiseSuppression.Value() = aPrefs.mNoiseOn;
    that->mSettings->mChannelCount.Value() = aPrefs.mChannels;

    class Message : public ControlMessage {
    public:
      Message(MediaEngineWebRTCMicrophoneSource* aSource,
              bool aPassThrough)
        : ControlMessage(nullptr)
        , mMicrophoneSource(aSource)
        , mPassThrough(aPassThrough)
        {}

      void Run() override
      {
        mMicrophoneSource->SetPassThrough(mPassThrough);
      }

    protected:
      RefPtr<MediaEngineWebRTCMicrophoneSource> mMicrophoneSource;
      bool mPassThrough;
    };

    bool passThrough = !(aPrefs.mAecOn || aPrefs.mAgcOn || aPrefs.mNoiseOn);
    if (!that->mSources.IsEmpty()) {
      that->mSources[0]->GraphImpl()->AppendMessage(MakeUnique<Message>(that, passThrough));
    }

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
  aStream->AddAudioTrack(aID, aStream->GraphRate(), 0, segment, SourceMediaStream::ADDTRACK_QUEUED);

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

  mAudioOutputObserver->Clear();

  mAudioInput->StartRecording(aStream, mListener);

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

    mState = kStopped;
  }
  if (mListener) {
    // breaks a cycle, since the WebRTCAudioDataListener has a RefPtr to us
    mListener->Shutdown();
    mListener = nullptr;
  }

  mAudioInput->StopRecording(aSource);

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
  if (!PassThrough()) {
    mAudioOutputObserver->InsertFarEnd(aBuffer, aFrames, false,
                                       aRate, aChannels);
  }
}

// Only called if we're not in passthrough mode
void
MediaEngineWebRTCMicrophoneSource::PacketizeAndProcess(MediaStreamGraph* aGraph,
                                                       const AudioDataValue* aBuffer,
                                                       size_t aFrames,
                                                       TrackRate aRate,
                                                       uint32_t aChannels)
{
  MOZ_ASSERT(!PassThrough(), "This should be bypassed when in PassThrough mode.");
  size_t offset = 0;

  if (!mPacketizer ||
      mPacketizer->PacketSize() != aRate/100u ||
      mPacketizer->Channels() != aChannels) {
    // It's ok to drop the audio still in the packetizer here.
    mPacketizer =
      new AudioPacketizer<AudioDataValue, float>(aRate/100, aChannels);
  }

  // On initial capture, throw away all far-end data except the most recent sample
  // since it's already irrelevant and we want to keep avoid confusing the AEC far-end
  // input code with "old" audio.
  if (!mStarted) {
    mStarted  = true;
    while (mAudioOutputObserver->Size() > 1) {
      free(mAudioOutputObserver->Pop()); // only call if size() > 0
    }
  }

  // Feed the far-end audio data (speakers) to the feedback input of the AEC.
  while (mAudioOutputObserver->Size() > 0) {
    // Bug 1414837: This will call `free()`, and we should remove it.
    // Pop gives ownership.
    nsAutoPtr<FarEndAudioChunk> buffer(mAudioOutputObserver->Pop()); // only call if size() > 0
    if (!buffer) {
      continue;
    }
    AudioDataValue* packetDataPointer = buffer->mData;
    AutoTArray<AudioDataValue*, MAX_CHANNELS> deinterleavedPacketDataChannelPointers;
    AudioDataValue* interleavedFarend = nullptr;
    uint32_t channelCountFarend = 0;
    uint32_t framesPerPacketFarend = 0;

    // Downmix from aChannels to MAX_CHANNELS if needed
    if (mAudioOutputObserver->PlayoutChannels() > MAX_CHANNELS) {
      AudioConverter converter(AudioConfig(aChannels, 0, AudioConfig::FORMAT_DEFAULT),
                               AudioConfig(MAX_CHANNELS, 0, AudioConfig::FORMAT_DEFAULT));
      framesPerPacketFarend =
        buffer->mSamples;
      framesPerPacketFarend =
        converter.Process(mInputDownmixBuffer,
                          packetDataPointer,
                          framesPerPacketFarend);
      interleavedFarend = mInputDownmixBuffer.Data();
      channelCountFarend = MAX_CHANNELS;
      deinterleavedPacketDataChannelPointers.SetLength(MAX_CHANNELS);
    } else {
      uint32_t outputChannels = mAudioOutputObserver->PlayoutChannels();
      interleavedFarend = packetDataPointer;
      channelCountFarend = outputChannels;
      framesPerPacketFarend = buffer->mSamples;
      deinterleavedPacketDataChannelPointers.SetLength(outputChannels);
    }

    MOZ_ASSERT(interleavedFarend &&
               (channelCountFarend == 1 || channelCountFarend == 2) &&
               framesPerPacketFarend);

    offset = 0;
    for (size_t i = 0; i < deinterleavedPacketDataChannelPointers.Length(); ++i) {
      deinterleavedPacketDataChannelPointers[i] = packetDataPointer + offset;
      offset += framesPerPacketFarend;
    }

    // deinterleave back into the FarEndAudioChunk buffer to save an alloc.
    // There is enough room because either there is the same number of
    // channels/frames or we've just downmixed.
    Deinterleave(interleavedFarend,
                 framesPerPacketFarend,
                 channelCountFarend,
                 deinterleavedPacketDataChannelPointers.Elements());

    // Having the same config for input and output means we potentially save
    // some CPU. We won't need the output here, the API forces us to set a
    // valid pointer with enough space.
    StreamConfig inputConfig(mAudioOutputObserver->PlayoutFrequency(),
                             channelCountFarend,
                             false /* we don't use typing detection*/);
    StreamConfig outputConfig = inputConfig;

    // Prepare a channel pointers array, with enough storage for the
    // frames.
    //
    // If this is a platform that uses s16 for audio input and output,
    // convert to floats, the APM API we use only accepts floats.

    float* inputData = nullptr;
#ifdef MOZ_SAMPLE_TYPE_S16
    // Convert to floats, use mInputBuffer for this.
    size_t sampleCount = framesPerPacketFarend * channelCountFarend;
    if (mInputBuffer.Length() < sampleCount) {
      mInputBuffer.SetLength(sampleCount);
    }
    ConvertAudioSamples(buffer->mData, mInputBuffer.Data(), sampleCount);
    inputData = mInputBuffer.Data();
#else // MOZ_SAMPLE_TYPE_F32
    inputData = buffer->mData;
#endif

    AutoTArray<float*, MAX_CHANNELS> channelsPointers;
    channelsPointers.SetLength(channelCountFarend);
    offset = 0;
    for (size_t i = 0; i < channelsPointers.Length(); ++i) {
      channelsPointers[i]  = inputData + offset;
      offset += framesPerPacketFarend;
    }

    // Passing the same pointers here saves a copy inside this function.
    int err =
      mAudioProcessing->ProcessReverseStream(channelsPointers.Elements(),
                                             inputConfig,
                                             outputConfig,
                                             channelsPointers.Elements());

    if (err) {
      MOZ_LOG(GetMediaManagerLog(), LogLevel::Error,
          ("error in audio ProcessReverseStream(): %d", err));
      return;
    }
  }

  // Packetize our input data into 10ms chunks, deinterleave into planar channel
  // buffers, process, and append to the right MediaStreamTrack.
  mPacketizer->Input(aBuffer, static_cast<uint32_t>(aFrames));

  while (mPacketizer->PacketsAvailable()) {
    uint32_t samplesPerPacket = mPacketizer->PacketSize() *
      mPacketizer->Channels();
    if (mInputBuffer.Length() < samplesPerPacket) {
      mInputBuffer.SetLength(samplesPerPacket);
      mDeinterleavedBuffer.SetLength(samplesPerPacket);
    }
    float* packet = mInputBuffer.Data();
    mPacketizer->Output(packet);

    // Deinterleave the input data
    // Prepare an array pointing to deinterleaved channels.
    AutoTArray<float*, 8> deinterleavedPacketizedInputDataChannelPointers;
    deinterleavedPacketizedInputDataChannelPointers.SetLength(aChannels);
    offset = 0;
    for (size_t i = 0; i < deinterleavedPacketizedInputDataChannelPointers.Length(); ++i) {
      deinterleavedPacketizedInputDataChannelPointers[i] = mDeinterleavedBuffer.Data() + offset;
      offset += aFrames;
    }

    // Deinterleave to mInputBuffer, pointed to by inputBufferChannelPointers.
    Deinterleave(packet, mPacketizer->PacketSize(), aChannels,
        deinterleavedPacketizedInputDataChannelPointers.Elements());

    StreamConfig inputConfig(aRate,
                             aChannels,
                             false /* we don't use typing detection*/);
    StreamConfig outputConfig = inputConfig;

    // Bug 1404965: Get the right delay here, it saves some work down the line.
    mAudioProcessing->set_stream_delay_ms(0);

    // Bug 1414837: find a way to not allocate here.
    RefPtr<SharedBuffer> buffer =
      SharedBuffer::Create(mPacketizer->PacketSize() * aChannels * sizeof(float));
    AudioSegment segment;

    // Prepare channel pointers to the SharedBuffer created above.
    AutoTArray<float*, 8> processedOutputChannelPointers;
    AutoTArray<const float*, 8> processedOutputChannelPointersConst;
    processedOutputChannelPointers.SetLength(aChannels);
    processedOutputChannelPointersConst.SetLength(aChannels);

    offset = 0;
    for (size_t i = 0; i < processedOutputChannelPointers.Length(); ++i) {
      processedOutputChannelPointers[i] = static_cast<float*>(buffer->Data()) + offset;
      processedOutputChannelPointersConst[i] = static_cast<float*>(buffer->Data()) + offset;
      offset += aFrames;
    }

    mAudioProcessing->ProcessStream(deinterleavedPacketizedInputDataChannelPointers.Elements(),
                                    inputConfig,
                                    outputConfig,
                                    processedOutputChannelPointers.Elements());
    MonitorAutoLock lock(mMonitor);
    if (mState != kStarted)
      return;

    for (size_t i = 0; i < mSources.Length(); ++i) {
      if (!mSources[i]) { // why ?!
        continue;
      }

      // We already have planar audio data of the right format. Insert into the
      // MSG.
      MOZ_ASSERT(processedOutputChannelPointers.Length() == aChannels);
      segment.AppendFrames(buffer.forget(),
                           processedOutputChannelPointersConst,
                           mPacketizer->PacketSize(),
                           mPrincipalHandles[i]);
      mSources[i]->AppendToTrack(mTrackID, &segment);
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
  for (size_t i = 0; i < len; ++i) {
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

    AudioSegment segment;
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
    segment.AppendFrames(buffer.forget(), channels, aFrames,
                         mPrincipalHandles[i]);
    segment.GetStartTime(insertTime);

    mSources[i]->AppendToTrack(mTrackID, &segment);
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
  bool enabled = mAudioProcessing->_processing()->is_enabled();     \
                                                                    \
  if (enabled) {                                                    \
    int rv = mAudioProcessing->_processing()->Enable(!enabled);     \
    if (rv) {                                                       \
      NS_WARNING("Could not reset the status of the "               \
      #_processing " on device change.");                           \
      return;                                                       \
    }                                                               \
    rv = mAudioProcessing->_processing()->Enable(enabled);          \
    if (rv) {                                                       \
      NS_WARNING("Could not reset the status of the "               \
      #_processing " on device change.");                           \
      return;                                                       \
    }                                                               \
                                                                    \
  }                                                                 \
}  while(0)

void
MediaEngineWebRTCMicrophoneSource::DeviceChanged() {
  // Reset some processing
  ResetProcessingIfNeeded(gain_control);
  ResetProcessingIfNeeded(echo_cancellation);
  ResetProcessingIfNeeded(noise_suppression);
}

// mState records if a channel is allocated (slightly redundantly to mChannel)
void
MediaEngineWebRTCMicrophoneSource::FreeChannel()
{
  if (mState != kReleased) {
    mState = kReleased;

    MOZ_ASSERT(sChannelsOpen > 0);
    --sChannelsOpen;
  }
}

bool
MediaEngineWebRTCMicrophoneSource::AllocChannel()
{
  mSampleFrequency = MediaEngine::USE_GRAPH_RATE;
  LOG(("%s: sampling rate %u", __FUNCTION__, mSampleFrequency));

  mState = kAllocated;
  sChannelsOpen++;
  return true;
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
