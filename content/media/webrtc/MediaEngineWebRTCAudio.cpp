/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

#define CHANNELS 1
#define ENCODING "L16"
#define DEFAULT_PORT 5555

#define SAMPLE_RATE 256000
#define SAMPLE_FREQUENCY 16000
#define SAMPLE_LENGTH ((SAMPLE_FREQUENCY*10)/1000)

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#else
#define LOG(msg)
#endif

/**
 * Webrtc audio source.
 */
NS_IMPL_THREADSAFE_ISUPPORTS0(MediaEngineWebRTCAudioSource)

void
MediaEngineWebRTCAudioSource::GetName(nsAString& aName)
{
  if (mInitDone) {
    aName.Assign(mDeviceName);
  }

  return;
}

void
MediaEngineWebRTCAudioSource::GetUUID(nsAString& aUUID)
{
  if (mInitDone) {
    aUUID.Assign(mDeviceUUID);
  }

  return;
}

nsresult
MediaEngineWebRTCAudioSource::Allocate()
{
  if (mState == kReleased && mInitDone) {
    webrtc::VoEHardware* ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
    int res = ptrVoEHw->SetRecordingDevice(mCapIndex);
    ptrVoEHw->Release();
    if (res) {
      return NS_ERROR_FAILURE;
    }
    mState = kAllocated;
    LOG(("Audio device %d allocated", mCapIndex));
  } else if (mSources.IsEmpty()) {
    LOG(("Audio device %d reallocated", mCapIndex));
  } else {
    LOG(("Audio device %d allocated shared", mCapIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioSource::Deallocate()
{
  if (mSources.IsEmpty()) {
    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }

    mState = kReleased;
    LOG(("Audio device %d deallocated", mCapIndex));
  } else {
    LOG(("Audio device %d deallocated but still in use", mCapIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  if (!mInitDone || !aStream) {
    return NS_ERROR_FAILURE;
  }

  {
    ReentrantMonitorAutoEnter enter(mMonitor);
    mSources.AppendElement(aStream);
  }

  AudioSegment* segment = new AudioSegment();
  segment->Init(CHANNELS);
  aStream->AddTrack(aID, SAMPLE_FREQUENCY, 0, segment);
  aStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);
  LOG(("Initial audio"));
  mTrackID = aID;

  if (mState == kStarted) {
    return NS_OK;
  }
  mState = kStarted;

  if (mVoEBase->StartReceive(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  if (mVoEBase->StartSend(mChannel)) {
    return NS_ERROR_FAILURE;
  }

  // Attach external media processor, so this::Process will be called.
  mVoERender->RegisterExternalMediaProcessing(mChannel, webrtc::kRecordingPerChannel, *this);

  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioSource::Stop(SourceMediaStream *aSource, TrackID aID)
{
  {
    ReentrantMonitorAutoEnter enter(mMonitor);

    if (!mSources.RemoveElement(aSource)) {
      // Already stopped - this is allowed
      return NS_OK;
    }
    if (!mSources.IsEmpty()) {
      return NS_OK;
    }
    if (mState != kStarted) {
      return NS_ERROR_FAILURE;
    }
    if (!mVoEBase) {
      return NS_ERROR_FAILURE;
    }

    mState = kStopped;
    aSource->EndTrack(aID);
  }

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
MediaEngineWebRTCAudioSource::NotifyPull(MediaStreamGraph* aGraph,
                                         SourceMediaStream *aSource,
                                         TrackID aID,
                                         StreamTime aDesiredTime,
                                         TrackTicks &aLastEndTime)
{
  // Ignore - we push audio data
#ifdef DEBUG
  TrackTicks target = TimeToTicksRoundUp(SAMPLE_FREQUENCY, aDesiredTime);
  TrackTicks delta = target - aLastEndTime;
  LOG(("Audio: NotifyPull: aDesiredTime %ld, target %ld, delta %ld",(int64_t) aDesiredTime, (int64_t) target, (int64_t) delta));
  aLastEndTime = target;
#endif
}

nsresult
MediaEngineWebRTCAudioSource::Snapshot(uint32_t aDuration, nsIDOMFile** aFile)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

void
MediaEngineWebRTCAudioSource::Init()
{
  mVoEBase = webrtc::VoEBase::GetInterface(mVoiceEngine);

  mVoEBase->Init();

  mVoERender = webrtc::VoEExternalMedia::GetInterface(mVoiceEngine);
  if (!mVoERender) {
    return;
  }
  mVoENetwork = webrtc::VoENetwork::GetInterface(mVoiceEngine);
  if (!mVoENetwork) {
    return;
  }

  mChannel = mVoEBase->CreateChannel();
  if (mChannel < 0) {
    return;
  }
  mNullTransport = new NullTransport();
  if (mVoENetwork->RegisterExternalTransport(mChannel, *mNullTransport)) {
    return;
  }

  // Check for availability.
  webrtc::VoEHardware* ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
  if (ptrVoEHw->SetRecordingDevice(mCapIndex)) {
    ptrVoEHw->Release();
    return;
  }

  bool avail = false;
  ptrVoEHw->GetRecordingDeviceStatus(avail);
  ptrVoEHw->Release();
  if (!avail) {
    return;
  }

  // Set "codec" to PCM, 32kHz on 1 channel
  webrtc::VoECodec* ptrVoECodec;
  webrtc::CodecInst codec;
  ptrVoECodec = webrtc::VoECodec::GetInterface(mVoiceEngine);
  if (!ptrVoECodec) {
    return;
  }

  strcpy(codec.plname, ENCODING);
  codec.channels = CHANNELS;
  codec.rate = SAMPLE_RATE;
  codec.plfreq = SAMPLE_FREQUENCY;
  codec.pacsize = SAMPLE_LENGTH;
  codec.pltype = 0; // Default payload type

  if (ptrVoECodec->SetSendCodec(mChannel, codec)) {
    return;
  }

  mInitDone = true;
}

void
MediaEngineWebRTCAudioSource::Shutdown()
{
  if (!mInitDone) {
    // duplicate these here in case we failed during Init()
    if (mChannel != -1) {
      mVoENetwork->DeRegisterExternalTransport(mChannel);
    }

    if (mNullTransport) {
      delete mNullTransport;
    }

    return;
  }

  if (mState == kStarted) {
    while (!mSources.IsEmpty()) {
      Stop(mSources[0], kAudioTrack); // XXX change to support multiple tracks
    }
    MOZ_ASSERT(mState == kStopped);
  }

  if (mState == kAllocated || mState == kStopped) {
    Deallocate();
  }

  mVoEBase->Terminate();
  if (mChannel != -1) {
    mVoENetwork->DeRegisterExternalTransport(mChannel);
  }

  if (mNullTransport) {
    delete mNullTransport;
  }

  mVoERender->Release();
  mVoEBase->Release();

  mState = kReleased;
  mInitDone = false;
}

typedef WebRtc_Word16 sample;

void
MediaEngineWebRTCAudioSource::Process(const int channel,
  const webrtc::ProcessingTypes type, sample* audio10ms,
  const int length, const int samplingFreq, const bool isStereo)
{
  ReentrantMonitorAutoEnter enter(mMonitor);
  if (mState != kStarted)
    return;

  uint32_t len = mSources.Length();
  for (uint32_t i = 0; i < len; i++) {
    nsRefPtr<SharedBuffer> buffer = SharedBuffer::Create(length * sizeof(sample));

    sample* dest = static_cast<sample*>(buffer->Data());
    memcpy(dest, audio10ms, length * sizeof(sample));

    AudioSegment segment;
    segment.Init(CHANNELS);
    nsAutoTArray<const sample*,1> channels;
    channels.AppendElement(dest);
    segment.AppendFrames(buffer.forget(), channels, length);

    SourceMediaStream *source = mSources[i];
    if (source) {
      source->AppendToTrack(mTrackID, &segment);
    }
  }

  return;
}

}
