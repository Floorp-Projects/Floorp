/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

#include <stdio.h>
#include <algorithm>

#include "AllocationHandle.h"
#include "AudioConverter.h"
#include "MediaManager.h"
#include "MediaStreamGraphImpl.h"
#include "MediaTrackConstraints.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorNames.h"
#include "mtransport/runnable_utils.h"
#include "nsAutoPtr.h"
#include "Tracing.h"

// scoped_ptr.h uses FF
#ifdef FF
#undef FF
#endif
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

LogModule* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOG_FRAMES(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

LogModule* AudioLogModule() {
  static mozilla::LazyLogModule log("AudioLatency");
  return static_cast<LogModule*>(log);
}

void
WebRTCAudioDataListener::NotifyOutputData(MediaStreamGraphImpl* aGraph,
                                          AudioDataValue* aBuffer,
                                          size_t aFrames,
                                          TrackRate aRate,
                                          uint32_t aChannels)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  if (mAudioSource) {
    mAudioSource->NotifyOutputData(aGraph, aBuffer, aFrames, aRate, aChannels);
  }
}

void
WebRTCAudioDataListener::NotifyInputData(MediaStreamGraphImpl* aGraph,
                                         const AudioDataValue* aBuffer,
                                         size_t aFrames,
                                         TrackRate aRate,
                                         uint32_t aChannels)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  if (mAudioSource) {
    mAudioSource->NotifyInputData(aGraph, aBuffer, aFrames, aRate, aChannels);
  }
}

void
WebRTCAudioDataListener::DeviceChanged(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  if (mAudioSource) {
    mAudioSource->DeviceChanged(aGraph);
  }
}

uint32_t
WebRTCAudioDataListener::RequestedInputChannelCount(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  if (mAudioSource) {
    return mAudioSource->RequestedInputChannelCount(aGraph);
  }
  return 0;
}

void
WebRTCAudioDataListener::Disconnect(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  if (mAudioSource) {
    mAudioSource->Disconnect(aGraph);
    mAudioSource = nullptr;
  }
}

/**
 * WebRTC Microphone MediaEngineSource.
 */

MediaEngineWebRTCMicrophoneSource::Allocation::Allocation(
    const RefPtr<AllocationHandle>& aHandle)
  : mHandle(aHandle)
{}

MediaEngineWebRTCMicrophoneSource::Allocation::~Allocation() = default;

MediaEngineWebRTCMicrophoneSource::MediaEngineWebRTCMicrophoneSource(
    RefPtr<AudioDeviceInfo> aInfo,
    const nsString& aDeviceName,
    const nsCString& aDeviceUUID,
    uint32_t aMaxChannelCount,
    bool aDelayAgnostic,
    bool aExtendedFilter)
  : mDeviceInfo(std::move(aInfo))
  , mAudioProcessing(AudioProcessing::Create())
  , mMutex("WebRTCMic::Mutex")
  , mDelayAgnostic(aDelayAgnostic)
  , mExtendedFilter(aExtendedFilter)
  , mStarted(false)
  , mDeviceName(aDeviceName)
  , mDeviceUUID(aDeviceUUID)
  , mSettings(
      new nsMainThreadPtrHolder<media::Refcountable<dom::MediaTrackSettings>>(
        "MediaEngineWebRTCMicrophoneSource::mSettings",
        new media::Refcountable<dom::MediaTrackSettings>(),
        // Non-strict means it won't assert main thread for us.
        // It would be great if it did but we're already on the media thread.
        /* aStrict = */ false))
  , mRequestedInputChannelCount(aMaxChannelCount)
  , mTotalFrames(0)
  , mLastLogFrames(0)
  , mSkipProcessing(false)
  , mInputDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100)
{
#ifndef ANDROID
  MOZ_ASSERT(mDeviceInfo->DeviceID());
#endif

  // We'll init lazily as needed
  mSettings->mEchoCancellation.Construct(0);
  mSettings->mAutoGainControl.Construct(0);
  mSettings->mNoiseSuppression.Construct(0);
  mSettings->mChannelCount.Construct(0);

  mState = kReleased;
}

nsString
MediaEngineWebRTCMicrophoneSource::GetName() const
{
  return mDeviceName;
}

nsCString
MediaEngineWebRTCMicrophoneSource::GetUUID() const
{
  return mDeviceUUID;
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
    distance = MediaConstraintsHelper::GetMinimumFitnessDistance(*cs, aDeviceId);
    break; // distance is read from first entry only
  }
  return distance;
}

nsresult
MediaEngineWebRTCMicrophoneSource::ReevaluateAllocation(
    const RefPtr<AllocationHandle>& aHandle,
    const NormalizedConstraints* aConstraintsUpdate,
    const MediaEnginePrefs& aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();

  // aHandle and/or aConstraintsUpdate may be nullptr (see below)

  AutoTArray<const NormalizedConstraints*, 10> allConstraints;
  for (const Allocation& registered : mAllocations) {
    if (aConstraintsUpdate && registered.mHandle == aHandle) {
      continue; // Don't count old constraints
    }
    allConstraints.AppendElement(&registered.mHandle->mConstraints);
  }
  if (aConstraintsUpdate) {
    allConstraints.AppendElement(aConstraintsUpdate);
  } else if (aHandle) {
    // In the case of AddShareOfSingleSource, the handle isn't registered yet.
    allConstraints.AppendElement(&aHandle->mConstraints);
  }

  NormalizedConstraints netConstraints(allConstraints);
  if (netConstraints.mBadConstraint) {
    *aOutBadConstraint = netConstraints.mBadConstraint;
    return NS_ERROR_FAILURE;
  }

  nsresult rv = UpdateSingleSource(aHandle,
                                   netConstraints,
                                   aPrefs,
                                   aDeviceId,
                                   aOutBadConstraint);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aHandle && aConstraintsUpdate) {
    aHandle->mConstraints = *aConstraintsUpdate;
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                                               const dom::MediaTrackConstraints& aConstraints,
                                               const MediaEnginePrefs& aPrefs,
                                               const nsString& aDeviceId,
                                               const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aHandle);

  LOG(("Mic source %p allocation %p Reconfigure()", this, aHandle.get()));

  NormalizedConstraints constraints(aConstraints);
  nsresult rv = ReevaluateAllocation(aHandle, &constraints, aPrefs, aDeviceId,
                                     aOutBadConstraint);
  if (NS_FAILED(rv)) {
    if (aOutBadConstraint) {
      return NS_ERROR_INVALID_ARG;
    }

    nsAutoCString name;
    GetErrorName(rv, name);
    LOG(("Mic source %p Reconfigure() failed unexpectedly. rv=%s",
         this, name.Data()));
    Stop(aHandle);
    return NS_ERROR_UNEXPECTED;
  }

  size_t i = mAllocations.IndexOf(aHandle, 0, AllocationHandleComparator());
  MOZ_DIAGNOSTIC_ASSERT(i != mAllocations.NoIndex);
  ApplySettings(mNetPrefs, mAllocations[i].mStream->GraphImpl());

  return NS_OK;
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
  AssertIsOnOwningThread();

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
  AssertIsOnOwningThread();

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
  AssertIsOnOwningThread();

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
    const RefPtr<const AllocationHandle>& aHandle,
    const NormalizedConstraints& aNetConstraints,
    const MediaEnginePrefs& aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();

  FlattenedConstraints c(aNetConstraints);

  MediaEnginePrefs prefs = aPrefs;
  prefs.mAecOn = c.mEchoCancellation.Get(prefs.mAecOn);
  prefs.mAgcOn = c.mAutoGainControl.Get(prefs.mAgcOn);
  prefs.mNoiseOn = c.mNoiseSuppression.Get(prefs.mNoiseOn);

  // Determine an actual channel count to use for this source. Three factors at
  // play here: the device capabilities, the constraints passed in by content,
  // and a pref that can force things (for testing)
  int32_t maxChannels = mDeviceInfo->MaxChannels();

  // First, check channelCount violation wrt constraints. This fails in case of
  // error.
  if (c.mChannelCount.mMin > maxChannels) {
    *aOutBadConstraint = "channelCount";
    return NS_ERROR_FAILURE;
  }
  // A pref can force the channel count to use. If the pref has a value of zero
  // or lower, it has no effect.
  if (prefs.mChannels <= 0) {
    prefs.mChannels = maxChannels;
  }

  // Get the number of channels asked for by content, and clamp it between the
  // pref and the maximum number of channels that the device supports.
  prefs.mChannels = c.mChannelCount.Get(std::min(prefs.mChannels,
                                        maxChannels));
  prefs.mChannels = std::max(1, std::min(prefs.mChannels, maxChannels));

  LOG(("Audio config: aec: %d, agc: %d, noise: %d, channels: %d",
      prefs.mAecOn ? prefs.mAec : -1,
      prefs.mAgcOn ? prefs.mAgc : -1,
      prefs.mNoiseOn ? prefs.mNoise : -1,
      prefs.mChannels));

  switch (mState) {
    case kReleased:
      MOZ_ASSERT(aHandle);
      {
        MutexAutoLock lock(mMutex);
        mState = kAllocated;
      }
      LOG(("Audio device %s allocated", NS_ConvertUTF16toUTF8(mDeviceInfo->Name()).get()));
      break;

    case kStarted:
    case kStopped:
      if (prefs == mNetPrefs) {
        LOG(("UpdateSingleSource: new prefs for %s are the same as the current prefs, returning.",
             NS_ConvertUTF16toUTF8(mDeviceName).get()));
        return NS_OK;
      }
      break;

    default:
      LOG(("Audio device %s in ignored state %d", NS_ConvertUTF16toUTF8(mDeviceInfo->Name()).get(), MediaEngineSourceState(mState)));
      break;
  }

  if (mState != kReleased) {
    UpdateAGCSettingsIfNeeded(prefs.mAgcOn, static_cast<AgcModes>(prefs.mAgc));
    UpdateNSSettingsIfNeeded(prefs.mNoiseOn, static_cast<NsModes>(prefs.mNoise));
    UpdateAECSettingsIfNeeded(prefs.mAecOn, static_cast<EcModes>(prefs.mAec));

    webrtc::Config config;
    config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(mExtendedFilter));
    config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(mDelayAgnostic));
    mAudioProcessing->SetExtraOptions(config);
  }
  mNetPrefs = prefs;
  return NS_OK;
}

#undef HANDLE_APM_ERROR

bool
MediaEngineWebRTCMicrophoneSource::PassThrough(MediaStreamGraphImpl* aGraph) const
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  return mSkipProcessing;
}
void
MediaEngineWebRTCMicrophoneSource::SetPassThrough(bool aPassThrough)
{
  {
    MutexAutoLock lock(mMutex);
    if (mAllocations.IsEmpty()) {
      // This can be the case, for now, because we're mixing mutable shared state
      // and linearization via message queue. This is temporary.
      return;
    }

    // mStream is always valid because it's set right before ::Start is called.
    // SetPassThrough cannot be called before that, because it's running on the
    // graph thread, and this cannot happen before the source has been started.
    MOZ_ASSERT(mAllocations.Length() == 1 &&
               mAllocations[0].mStream &&
               mAllocations[0].mStream->GraphImpl()->CurrentDriver()->OnThread(),
               "Wrong calling pattern, don't call this before ::SetTrack.");
  }
  mSkipProcessing = aPassThrough;
}

uint32_t
MediaEngineWebRTCMicrophoneSource::GetRequestedInputChannelCount(MediaStreamGraphImpl* aGraphImpl)
{
  MOZ_ASSERT(aGraphImpl->CurrentDriver()->OnThread(),
             "Wrong calling pattern, don't call this before ::SetTrack.");

  if (mState == kReleased) {
    // This source has been released, and is waiting for collection. Simply
    // return 0, this source won't contribute to the channel count decision.
    // Again, this is temporary.
    return 0;
  }

  return mRequestedInputChannelCount;
}

void
MediaEngineWebRTCMicrophoneSource::SetRequestedInputChannelCount(
  uint32_t aRequestedInputChannelCount)
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mAllocations.Length() <= 1);

  if (mAllocations.IsEmpty()) {
      return;
  }
  MOZ_ASSERT(mAllocations.Length() == 1 &&
             mAllocations[0].mStream &&
             mAllocations[0].mStream->GraphImpl()->CurrentDriver()->OnThread(),
             "Wrong calling pattern, don't call this before ::SetTrack.");
  mRequestedInputChannelCount = aRequestedInputChannelCount;
  mAllocations[0].mStream->GraphImpl()->ReevaluateInputDevice();
}

void
MediaEngineWebRTCMicrophoneSource::ApplySettings(const MediaEnginePrefs& aPrefs,
                                                 RefPtr<MediaStreamGraphImpl> aGraph)
{
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(aGraph);
#ifdef DEBUG
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mAllocations.Length() <= 1);
  }
#endif

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  NS_DispatchToMainThread(media::NewRunnableFrom([that, graph = std::move(aGraph), aPrefs]() mutable {
    that->mSettings->mEchoCancellation.Value() = aPrefs.mAecOn;
    that->mSettings->mAutoGainControl.Value() = aPrefs.mAgcOn;
    that->mSettings->mNoiseSuppression.Value() = aPrefs.mNoiseOn;
    that->mSettings->mChannelCount.Value() = aPrefs.mChannels;

    class Message : public ControlMessage {
    public:
      Message(MediaEngineWebRTCMicrophoneSource* aSource,
              bool aPassThrough,
              uint32_t aRequestedInputChannelCount)
        : ControlMessage(nullptr)
        , mMicrophoneSource(aSource)
        , mPassThrough(aPassThrough)
        , mRequestedInputChannelCount(aRequestedInputChannelCount)
        {}

      void Run() override
      {
        mMicrophoneSource->SetPassThrough(mPassThrough);
        mMicrophoneSource->SetRequestedInputChannelCount(mRequestedInputChannelCount);
      }

    protected:
      RefPtr<MediaEngineWebRTCMicrophoneSource> mMicrophoneSource;
      bool mPassThrough;
      uint32_t mRequestedInputChannelCount;
    };

    bool passThrough = !(aPrefs.mAecOn || aPrefs.mAgcOn || aPrefs.mNoiseOn);
    if (graph) {
      graph->AppendMessage(MakeUnique<Message>(that,
                                               passThrough,
                                               aPrefs.mChannels));
    }

    return NS_OK;
  }));
}

nsresult
MediaEngineWebRTCMicrophoneSource::Allocate(const dom::MediaTrackConstraints &aConstraints,
                                            const MediaEnginePrefs& aPrefs,
                                            const nsString& aDeviceId,
                                            const ipc::PrincipalInfo& aPrincipalInfo,
                                            AllocationHandle** aOutHandle,
                                            const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aOutHandle);
  auto handle = MakeRefPtr<AllocationHandle>(aConstraints, aPrincipalInfo,
                                             aDeviceId);

#ifdef DEBUG
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mAllocations.Length() <= 1);
  }
#endif
  LOG(("Mic source %p allocation %p Allocate()", this, handle.get()));

  nsresult rv = ReevaluateAllocation(handle, nullptr, aPrefs, aDeviceId,
                                     aOutBadConstraint);
  if (NS_FAILED(rv)) {
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mAllocations.IsEmpty(), "Only allocate once.");
    mAllocations.AppendElement(Allocation(handle));
  }

  handle.forget(aOutHandle);
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Deallocate(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kStopped);

  size_t i = mAllocations.IndexOf(aHandle, 0, AllocationHandleComparator());
  MOZ_DIAGNOSTIC_ASSERT(i != mAllocations.NoIndex);
  MOZ_DIAGNOSTIC_ASSERT(!mAllocations[i].mEnabled,
                        "Source should be stopped for the track before removing");

  if (mAllocations[i].mStream && IsTrackIDExplicit(mAllocations[i].mTrackID)) {
    mAllocations[i].mStream->EndTrack(mAllocations[i].mTrackID);
  }

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mAllocations.Length() == 1, "Only allocate once.");
    mAllocations.RemoveElementAt(i);
  }

  if (mAllocations.IsEmpty()) {
    // If empty, no callbacks to deliver data should be occuring
    MOZ_ASSERT(mState != kReleased, "Source not allocated");
    MOZ_ASSERT(mState != kStarted, "Source not stopped");

    MutexAutoLock lock(mMutex);
    mState = kReleased;
    LOG(("Audio device %s deallocated", NS_ConvertUTF16toUTF8(mDeviceName).get()));
  } else {
    LOG(("Audio device %s deallocated but still in use", NS_ConvertUTF16toUTF8(mDeviceName).get()));
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                                            const RefPtr<SourceMediaStream>& aStream,
                                            TrackID aTrackID,
                                            const PrincipalHandle& aPrincipal)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(IsTrackIDExplicit(aTrackID));

  if (mAllocations.Length() == 1 &&
      mAllocations[0].mStream &&
      mAllocations[0].mStream->Graph() != aStream->Graph()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(mAllocations.Length() == 1, "Only allocate once.");

  size_t i = mAllocations.IndexOf(aHandle, 0, AllocationHandleComparator());
  MOZ_DIAGNOSTIC_ASSERT(i != mAllocations.NoIndex);
  MOZ_ASSERT(!mAllocations[i].mStream);
  MOZ_ASSERT(mAllocations[i].mTrackID == TRACK_NONE);
  MOZ_ASSERT(mAllocations[i].mPrincipal == PRINCIPAL_HANDLE_NONE);
  {
    MutexAutoLock lock(mMutex);
    mAllocations[i].mStream = aStream;
    mAllocations[i].mTrackID = aTrackID;
    mAllocations[i].mPrincipal = aPrincipal;
  }

  AudioSegment* segment = new AudioSegment();

  aStream->AddAudioTrack(aTrackID,
                         aStream->GraphRate(),
                         0,
                         segment,
                         SourceMediaStream::ADDTRACK_QUEUED);

  // XXX Make this based on the pref.
  aStream->RegisterForAudioMixing();

  LOG(("Stream %p registered for microphone capture", aStream.get()));
  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Start(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();
  if (mState == kStarted) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);

  size_t i = mAllocations.IndexOf(aHandle, 0, AllocationHandleComparator());
  MOZ_DIAGNOSTIC_ASSERT(i != mAllocations.NoIndex,
                        "Can't start track that hasn't been added");
  Allocation& allocation = mAllocations[i];

  CubebUtils::AudioDeviceID deviceID = mDeviceInfo->DeviceID();
  if (allocation.mStream->GraphImpl()->InputDeviceID() &&
      allocation.mStream->GraphImpl()->InputDeviceID() != deviceID) {
    // For now, we only allow opening a single audio input device per document,
    // because we can only have one MSG per document.
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!allocation.mEnabled, "Source already started");
  {
    // This spans setting both the enabled state and mState.
    MutexAutoLock lock(mMutex);
    allocation.mEnabled = true;

#ifdef DEBUG
    // Ensure that callback-tracking state is reset when callbacks start coming.
    allocation.mLastCallbackAppendTime = 0;
#endif
    allocation.mLiveFramesAppended = false;
    allocation.mLiveSilenceAppended = false;

    if (!mListener) {
      mListener = new WebRTCAudioDataListener(this);
    }

    // Make sure logger starts before capture
    AsyncLatencyLogger::Get(true);

    allocation.mStream->OpenAudioInput(deviceID, mListener);

    MOZ_ASSERT(mState != kReleased);
    mState = kStarted;
  }

  ApplySettings(mNetPrefs, allocation.mStream->GraphImpl());

  return NS_OK;
}

nsresult
MediaEngineWebRTCMicrophoneSource::Stop(const RefPtr<const AllocationHandle>& aHandle)
{
  MOZ_ASSERT(mAllocations.Length() <= 1);
  AssertIsOnOwningThread();

  LOG(("Mic source %p allocation %p Stop()", this, aHandle.get()));

  size_t i = mAllocations.IndexOf(aHandle, 0, AllocationHandleComparator());
  MOZ_DIAGNOSTIC_ASSERT(i != mAllocations.NoIndex,
                        "Cannot stop track that we don't know about");
  Allocation& allocation = mAllocations[i];
  MOZ_ASSERT(allocation.mStream, "SetTrack must have been called before ::Stop");

  if (!allocation.mEnabled) {
    // Already stopped - this is allowed
    return NS_OK;
  }

  {
    // This spans setting both the enabled state and mState.
    MutexAutoLock lock(mMutex);
    allocation.mEnabled = false;

    CubebUtils::AudioDeviceID deviceID = mDeviceInfo->DeviceID();
    Maybe<CubebUtils::AudioDeviceID> id = Some(deviceID);
    allocation.mStream->CloseAudioInput(id, mListener);
    mListener = nullptr;

    if (HasEnabledTrack()) {
      // Another track is keeping us from stopping
      return NS_OK;
    }

    MOZ_ASSERT(mState == kStarted, "Should be started when stopping");
    mState = kStopped;
  }

  return NS_OK;
}

void
MediaEngineWebRTCMicrophoneSource::GetSettings(dom::MediaTrackSettings& aOutSettings) const
{
  MOZ_ASSERT(NS_IsMainThread());
  aOutSettings = *mSettings;
}

void
MediaEngineWebRTCMicrophoneSource::Pull(const RefPtr<const AllocationHandle>& aHandle,
                                        const RefPtr<SourceMediaStream>& aStream,
                                        TrackID aTrackID,
                                        StreamTime aDesiredTime,
                                        const PrincipalHandle& aPrincipalHandle)
{
  TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p track %i",
                               aStream.get(), aTrackID);
  StreamTime delta;

  {
    MutexAutoLock lock(mMutex);
    size_t i = mAllocations.IndexOf(aHandle, 0, AllocationHandleComparator());
    if (i == mAllocations.NoIndex) {
      // This handle must have been deallocated. That's fine, and its track
      // will already be ended. No need to do anything.
      return;
    }

    // We don't want to GetEndOfAppendedData() above at the declaration if the
    // allocation was removed and the track non-existant. An assert will fail.
    delta = aDesiredTime - aStream->GetEndOfAppendedData(aTrackID);

    if (delta < 0) {
      LOG_FRAMES(("Not appending silence for allocation %p; %" PRId64 " frames already buffered",
                  mAllocations[i].mHandle.get(), -delta));
      return;
    }

    if (!mAllocations[i].mLiveFramesAppended ||
        !mAllocations[i].mLiveSilenceAppended) {
      // These are the iterations after starting or resuming audio capture.
      // Make sure there's at least one extra block buffered until audio
      // callbacks come in. We also allow appending silence one time after
      // audio callbacks have started, to cover the case where audio callbacks
      // start appending data immediately and there is no extra data buffered.
      delta += WEBAUDIO_BLOCK_SIZE;

      // If we're supposed to be packetizing but there's no packetizer yet,
      // there must not have been any live frames appended yet.
      // If there were live frames appended and we haven't appended the
      // right amount of silence, we'll have to append silence once more,
      // failing the other assert below.
      MOZ_ASSERT_IF(!PassThrough(aStream->GraphImpl()) && !mPacketizerInput,
                    !mAllocations[i].mLiveFramesAppended);

      if (!PassThrough(aStream->GraphImpl()) && mPacketizerInput) {
        // Processing is active and is processed in chunks of 10ms through the
        // input packetizer. We allow for 10ms of silence on the track to
        // accomodate the buffering worst-case.
        delta += mPacketizerInput->PacketSize();
      }
    }

    LOG_FRAMES(("Pulling %" PRId64 " frames of silence for allocation %p",
                delta, mAllocations[i].mHandle.get()));

    // This assertion fails when we append silence here in the same iteration
    // as there were real audio samples already appended by the audio callback.
    // Note that this is exempted until live samples and a subsequent chunk of
    // silence have been appended to the track. This will cover cases like:
    // - After Start(), there is silence (maybe multiple times) appended before
    //   the first audio callback.
    // - After Start(), there is real data (maybe multiple times) appended
    //   before the first graph iteration.
    // And other combinations of order of audio sample sources.
    MOZ_ASSERT_IF(
      mAllocations[i].mEnabled &&
      mAllocations[i].mLiveFramesAppended &&
      mAllocations[i].mLiveSilenceAppended,
      aStream->GraphImpl()->IterationEnd() >
      mAllocations[i].mLastCallbackAppendTime);

    if (mAllocations[i].mLiveFramesAppended) {
      mAllocations[i].mLiveSilenceAppended = true;
    }
  }

  AudioSegment audio;
  audio.AppendNullData(delta);
  aStream->AppendToTrack(aTrackID, &audio);
}

void
MediaEngineWebRTCMicrophoneSource::NotifyOutputData(MediaStreamGraphImpl* aGraph,
                                                    AudioDataValue* aBuffer,
                                                    size_t aFrames,
                                                    TrackRate aRate,
                                                    uint32_t aChannels)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());

  if (!mPacketizerOutput ||
      mPacketizerOutput->PacketSize() != aRate/100u ||
      mPacketizerOutput->Channels() != aChannels) {
    // It's ok to drop the audio still in the packetizer here: if this changes,
    // we changed devices or something.
    mPacketizerOutput =
      new AudioPacketizer<AudioDataValue, float>(aRate/100, aChannels);
  }

  mPacketizerOutput->Input(aBuffer, aFrames);

  while (mPacketizerOutput->PacketsAvailable()) {
    uint32_t samplesPerPacket = mPacketizerOutput->PacketSize() *
                                mPacketizerOutput->Channels();
    if (mOutputBuffer.Length() < samplesPerPacket) {
      mOutputBuffer.SetLength(samplesPerPacket);
    }
    if (mDeinterleavedBuffer.Length() < samplesPerPacket) {
      mDeinterleavedBuffer.SetLength(samplesPerPacket);
    }
    float* packet = mOutputBuffer.Data();
    mPacketizerOutput->Output(packet);

    AutoTArray<float*, MAX_CHANNELS> deinterleavedPacketDataChannelPointers;
    float* interleavedFarend = nullptr;
    uint32_t channelCountFarend = 0;
    uint32_t framesPerPacketFarend = 0;

    // Downmix from aChannels to MAX_CHANNELS if needed. We always have floats
    // here, the packetized performed the conversion.
    if (aChannels > MAX_CHANNELS) {
      AudioConverter converter(AudioConfig(aChannels, 0, AudioConfig::FORMAT_FLT),
                               AudioConfig(MAX_CHANNELS, 0, AudioConfig::FORMAT_FLT));
      framesPerPacketFarend = mPacketizerOutput->PacketSize();
      framesPerPacketFarend =
        converter.Process(mInputDownmixBuffer,
                          packet,
                          framesPerPacketFarend);
      interleavedFarend = mInputDownmixBuffer.Data();
      channelCountFarend = MAX_CHANNELS;
      deinterleavedPacketDataChannelPointers.SetLength(MAX_CHANNELS);
    } else {
      interleavedFarend = packet;
      channelCountFarend = aChannels;
      framesPerPacketFarend = mPacketizerOutput->PacketSize();
      deinterleavedPacketDataChannelPointers.SetLength(aChannels);
    }

    MOZ_ASSERT(interleavedFarend &&
               (channelCountFarend == 1 || channelCountFarend == 2) &&
               framesPerPacketFarend);

    if (mInputBuffer.Length() < framesPerPacketFarend * channelCountFarend) {
      mInputBuffer.SetLength(framesPerPacketFarend * channelCountFarend);
    }

    size_t offset = 0;
    for (size_t i = 0; i < deinterleavedPacketDataChannelPointers.Length(); ++i) {
      deinterleavedPacketDataChannelPointers[i] = mInputBuffer.Data() + offset;
      offset += framesPerPacketFarend;
    }

    // Deinterleave, prepare a channel pointers array, with enough storage for
    // the frames.
    DeinterleaveAndConvertBuffer(interleavedFarend,
                                 framesPerPacketFarend,
                                 channelCountFarend,
                                 deinterleavedPacketDataChannelPointers.Elements());

    // Having the same config for input and output means we potentially save
    // some CPU.
    StreamConfig inputConfig(aRate, channelCountFarend, false);
    StreamConfig outputConfig = inputConfig;

    // Passing the same pointers here saves a copy inside this function.
    DebugOnly<int> err =
      mAudioProcessing->ProcessReverseStream(deinterleavedPacketDataChannelPointers.Elements(),
                                             inputConfig,
                                             outputConfig,
                                             deinterleavedPacketDataChannelPointers.Elements());

    MOZ_ASSERT(!err, "Could not process the reverse stream.");
  }
}

// Only called if we're not in passthrough mode
void
MediaEngineWebRTCMicrophoneSource::PacketizeAndProcess(MediaStreamGraphImpl* aGraph,
                                                       const AudioDataValue* aBuffer,
                                                       size_t aFrames,
                                                       TrackRate aRate,
                                                       uint32_t aChannels)
{
  MOZ_ASSERT(!PassThrough(aGraph), "This should be bypassed when in PassThrough mode.");
  size_t offset = 0;

  if (!mPacketizerInput ||
      mPacketizerInput->PacketSize() != aRate/100u ||
      mPacketizerInput->Channels() != aChannels) {
    // It's ok to drop the audio still in the packetizer here.
    mPacketizerInput =
      new AudioPacketizer<AudioDataValue, float>(aRate/100, aChannels);
  }

  // On initial capture, throw away all far-end data except the most recent
  // sample since it's already irrelevant and we want to avoid confusing the AEC
  // far-end input code with "old" audio.
  if (!mStarted) {
    mStarted  = true;
  }

  // Packetize our input data into 10ms chunks, deinterleave into planar channel
  // buffers, process, and append to the right MediaStreamTrack.
  mPacketizerInput->Input(aBuffer, static_cast<uint32_t>(aFrames));

  while (mPacketizerInput->PacketsAvailable()) {
    uint32_t samplesPerPacket = mPacketizerInput->PacketSize() *
      mPacketizerInput->Channels();
    if (mInputBuffer.Length() < samplesPerPacket) {
      mInputBuffer.SetLength(samplesPerPacket);
    }
    if (mDeinterleavedBuffer.Length() < samplesPerPacket) {
      mDeinterleavedBuffer.SetLength(samplesPerPacket);
    }
    float* packet = mInputBuffer.Data();
    mPacketizerInput->Output(packet);

    // Deinterleave the input data
    // Prepare an array pointing to deinterleaved channels.
    AutoTArray<float*, 8> deinterleavedPacketizedInputDataChannelPointers;
    deinterleavedPacketizedInputDataChannelPointers.SetLength(aChannels);
    offset = 0;
    for (size_t i = 0; i < deinterleavedPacketizedInputDataChannelPointers.Length(); ++i) {
      deinterleavedPacketizedInputDataChannelPointers[i] = mDeinterleavedBuffer.Data() + offset;
      offset += mPacketizerInput->PacketSize();
    }

    // Deinterleave to mInputBuffer, pointed to by inputBufferChannelPointers.
    Deinterleave(packet, mPacketizerInput->PacketSize(), aChannels,
        deinterleavedPacketizedInputDataChannelPointers.Elements());

    StreamConfig inputConfig(aRate,
                             aChannels,
                             false /* we don't use typing detection*/);
    StreamConfig outputConfig = inputConfig;

    // Bug 1404965: Get the right delay here, it saves some work down the line.
    mAudioProcessing->set_stream_delay_ms(0);

    // Bug 1414837: find a way to not allocate here.
    RefPtr<SharedBuffer> buffer =
      SharedBuffer::Create(mPacketizerInput->PacketSize() * aChannels * sizeof(float));

    // Prepare channel pointers to the SharedBuffer created above.
    AutoTArray<float*, 8> processedOutputChannelPointers;
    AutoTArray<const float*, 8> processedOutputChannelPointersConst;
    processedOutputChannelPointers.SetLength(aChannels);
    processedOutputChannelPointersConst.SetLength(aChannels);

    offset = 0;
    for (size_t i = 0; i < processedOutputChannelPointers.Length(); ++i) {
      processedOutputChannelPointers[i] = static_cast<float*>(buffer->Data()) + offset;
      processedOutputChannelPointersConst[i] = static_cast<float*>(buffer->Data()) + offset;
      offset += mPacketizerInput->PacketSize();
    }

    mAudioProcessing->ProcessStream(deinterleavedPacketizedInputDataChannelPointers.Elements(),
                                    inputConfig,
                                    outputConfig,
                                    processedOutputChannelPointers.Elements());
    MutexAutoLock lock(mMutex);
    if (mState != kStarted) {
      return;
    }

    AudioSegment segment;
    for (Allocation& allocation : mAllocations) {
      if (!allocation.mStream) {
        continue;
      }

      if (!allocation.mStream->GraphImpl()) {
        // The DOMMediaStream that owns allocation.mStream has been cleaned up
        // and MediaStream::DestroyImpl() has run in the MSG. This is fine and
        // can happen before the MediaManager thread gets to stop capture for
        // this allocation.
        continue;
      }

      if (!allocation.mEnabled) {
        continue;
      }

      LOG_FRAMES(("Appending %" PRIu32 " frames of packetized audio for allocation %p",
                  mPacketizerInput->PacketSize(), allocation.mHandle.get()));

#ifdef DEBUG
      allocation.mLastCallbackAppendTime =
        allocation.mStream->GraphImpl()->IterationEnd();
#endif
      allocation.mLiveFramesAppended = true;

      // We already have planar audio data of the right format. Insert into the
      // MSG.
      MOZ_ASSERT(processedOutputChannelPointers.Length() == aChannels);
      RefPtr<SharedBuffer> other = buffer;
      segment.AppendFrames(other.forget(),
                           processedOutputChannelPointersConst,
                           mPacketizerInput->PacketSize(),
                           allocation.mPrincipal);
      allocation.mStream->AppendToTrack(allocation.mTrackID, &segment);
    }
  }
}

template<typename T>
void
MediaEngineWebRTCMicrophoneSource::InsertInGraph(const T* aBuffer,
                                                 size_t aFrames,
                                                 uint32_t aChannels)
{
  MutexAutoLock lock(mMutex);

  if (mState != kStarted) {
    return;
  }

  if (MOZ_LOG_TEST(AudioLogModule(), LogLevel::Debug)) {
    mTotalFrames += aFrames;
    if (mAllocations[0].mStream &&
        mTotalFrames > mLastLogFrames +
                       mAllocations[0].mStream->GraphRate()) { // ~ 1 second
      MOZ_LOG(AudioLogModule(), LogLevel::Debug,
              ("%p: Inserting %zu samples into graph, total frames = %" PRIu64,
               (void*)this, aFrames, mTotalFrames));
      mLastLogFrames = mTotalFrames;
    }
  }

  for (Allocation& allocation : mAllocations) {
    if (!allocation.mStream) {
      continue;
    }

    if (!allocation.mStream->GraphImpl()) {
      // The DOMMediaStream that owns allocation.mStream has been cleaned up
      // and MediaStream::DestroyImpl() has run in the MSG. This is fine and
      // can happen before the MediaManager thread gets to stop capture for
      // this allocation.
      continue;
    }

    if (!allocation.mEnabled) {
      continue;
    }

#ifdef DEBUG
    allocation.mLastCallbackAppendTime =
      allocation.mStream->GraphImpl()->IterationEnd();
#endif
    allocation.mLiveFramesAppended = true;

    TimeStamp insertTime;
    // Make sure we include the stream and the track.
    // The 0:1 is a flag to note when we've done the final insert for a given input block.
    LogTime(AsyncLatencyLogger::AudioTrackInsertion,
            LATENCY_STREAM_ID(allocation.mStream.get(), allocation.mTrackID),
            (&allocation != &mAllocations.LastElement()) ? 0 : 1, insertTime);

    // Bug 971528 - Support stereo capture in gUM
    MOZ_ASSERT(aChannels >= 1 && aChannels <= 8, "Support up to 8 channels");

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

    LOG_FRAMES(("Appending %zu frames of raw audio for allocation %p",
                aFrames, allocation.mHandle.get()));

    MOZ_ASSERT(aChannels == channels.Length());
    segment.AppendFrames(buffer.forget(), channels, aFrames,
                          allocation.mPrincipal);
    segment.GetStartTime(insertTime);

    allocation.mStream->AppendToTrack(allocation.mTrackID, &segment);
  }
}

// Called back on GraphDriver thread!
// Note this can be called back after ::Shutdown()
void
MediaEngineWebRTCMicrophoneSource::NotifyInputData(MediaStreamGraphImpl* aGraph,
                                                   const AudioDataValue* aBuffer,
                                                   size_t aFrames,
                                                   TrackRate aRate,
                                                   uint32_t aChannels)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  TRACE_AUDIO_CALLBACK();

  {
    MutexAutoLock lock(mMutex);
    if (mAllocations.IsEmpty()) {
      // This can happen because mAllocations is not yet using message passing, and
      // is access both on the media manager thread and the MSG thread. This is to
      // be fixed soon.
      // When deallocating, the listener is removed via message passing, while the
      // allocation is removed immediately, so there can be a few iterations where
      // we need to return early here.
      return;
    }
  }
  // If some processing is necessary, packetize and insert in the WebRTC.org
  // code. Otherwise, directly insert the mic data in the MSG, bypassing all processing.
  if (PassThrough(aGraph)) {
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
MediaEngineWebRTCMicrophoneSource::DeviceChanged(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  // Reset some processing
  ResetProcessingIfNeeded(gain_control);
  ResetProcessingIfNeeded(echo_cancellation);
  ResetProcessingIfNeeded(noise_suppression);
}

void
MediaEngineWebRTCMicrophoneSource::Disconnect(MediaStreamGraphImpl* aGraph)
{
  // This method is just for asserts.
  MOZ_ASSERT(aGraph->CurrentDriver()->OnThread());
  MOZ_ASSERT(!mListener);
}

void
MediaEngineWebRTCMicrophoneSource::Shutdown()
{
  AssertIsOnOwningThread();

  if (mState == kStarted) {
    for (const Allocation& allocation : mAllocations) {
      if (allocation.mEnabled) {
        Stop(allocation.mHandle);
      }
    }
    MOZ_ASSERT(mState == kStopped);
  }

  while (!mAllocations.IsEmpty()) {
    MOZ_ASSERT(mState == kAllocated || mState == kStopped);
    Deallocate(mAllocations[0].mHandle);
  }
  MOZ_ASSERT(mState == kReleased);
}

nsString
MediaEngineWebRTCAudioCaptureSource::GetName() const
{
  return NS_LITERAL_STRING(u"AudioCapture");
}

nsCString
MediaEngineWebRTCAudioCaptureSource::GetUUID() const
{
  nsID uuid;
  char uuidBuffer[NSID_LENGTH];
  nsCString asciiString;
  ErrorResult rv;

  rv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (rv.Failed()) {
    return NS_LITERAL_CSTRING("");
  }

  uuid.ToProvidedString(uuidBuffer);
  asciiString.AssignASCII(uuidBuffer);

  // Remove {} and the null terminator
  return nsCString(Substring(asciiString, 1, NSID_LENGTH - 3));
}

bool
MediaEngineWebRTCMicrophoneSource::HasEnabledTrack() const
{
  AssertIsOnOwningThread();
  for (const Allocation& allocation : mAllocations) {
    if (allocation.mEnabled) {
      return true;
    }
  }
  return false;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                                              const RefPtr<SourceMediaStream>& aStream,
                                              TrackID aTrackID,
                                              const PrincipalHandle& aPrincipalHandle)
{
  AssertIsOnOwningThread();
  // Nothing to do here. aStream is a placeholder dummy and not exposed.
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Start(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Stop(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioCaptureSource::Reconfigure(
    const RefPtr<AllocationHandle>& aHandle,
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
