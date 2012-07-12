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
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  mVoEBase->Init();

  mVoERender = webrtc::VoEExternalMedia::GetInterface(mVoiceEngine);
  if (!mVoERender) {
    return NS_ERROR_FAILURE;
  }

  mChannel = mVoEBase->CreateChannel();
  if (mChannel < 0) {
    return NS_ERROR_FAILURE;
  }

  // Check for availability.
  webrtc::VoEHardware* ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
  if (ptrVoEHw->SetRecordingDevice(mCapIndex)) {
    return NS_ERROR_FAILURE;
  }

  bool avail = false;
  ptrVoEHw->GetRecordingDeviceStatus(avail);
  if (!avail) {
    return NS_ERROR_FAILURE;
  }

  // Set "codec" to PCM, 32kHz on 1 channel
  webrtc::VoECodec* ptrVoECodec;
  webrtc::CodecInst codec;
  ptrVoECodec = webrtc::VoECodec::GetInterface(mVoiceEngine);
  if (!ptrVoECodec) {
    return NS_ERROR_FAILURE;
  }

  strcpy(codec.plname, ENCODING);
  codec.channels = CHANNELS;
  codec.rate = SAMPLE_RATE;
  codec.plfreq = SAMPLE_FREQUENCY;
  codec.pacsize = SAMPLE_LENGTH;
  codec.pltype = 0; // Default payload type

  if (ptrVoECodec->SetSendCodec(mChannel, codec)) {
    return NS_ERROR_FAILURE;
  }

  // Audio doesn't play through unless we set a receiver and destination, so
  // we setup a dummy local destination, and do a loopback.
  mVoEBase->SetLocalReceiver(mChannel, DEFAULT_PORT);
  mVoEBase->SetSendDestination(mChannel, DEFAULT_PORT, "127.0.0.1");

  mState = kAllocated;
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioSource::Deallocate()
{
  if (mState != kStopped && mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  mVoEBase->Terminate();
  mVoERender->Release();

  mState = kReleased;
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  if (!mInitDone || mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }
  if (!aStream) {
    return NS_ERROR_FAILURE;
  }

  mSource = aStream;

  AudioSegment* segment = new AudioSegment();
  segment->Init(CHANNELS);
  mSource->AddTrack(aID, SAMPLE_FREQUENCY, 0, segment);
  mSource->AdvanceKnownTracksTime(STREAM_TIME_MAX);
  mTrackID = aID;

  if (mVoEBase->StartReceive(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  if (mVoEBase->StartSend(mChannel)) {
    return NS_ERROR_FAILURE;
  }

  // Attach external media processor, so this::Process will be called.
  mVoERender->RegisterExternalMediaProcessing(mChannel, webrtc::kRecordingPerChannel, *this);

  mState = kStarted;
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioSource::Stop()
{
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }
  if (!mVoEBase) {
    return NS_ERROR_FAILURE;
  }

  mVoERender->DeRegisterExternalMediaProcessing(mChannel, webrtc::kRecordingPerChannel);

  if (mVoEBase->StopSend(mChannel)) {
    return NS_ERROR_FAILURE;
  }
  if (mVoEBase->StopReceive(mChannel)) {
    return NS_ERROR_FAILURE;
  }

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineWebRTCAudioSource::Snapshot(PRUint32 aDuration, nsIDOMFile** aFile)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}


void
MediaEngineWebRTCAudioSource::Shutdown()
{
  if (!mInitDone) {
    return;
  }

  if (mState == kStarted) {
    Stop();
  }

  if (mState == kAllocated) {
    Deallocate();
  }

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

  nsRefPtr<SharedBuffer> buffer = SharedBuffer::Create(length * sizeof(sample));

  sample* dest = static_cast<sample*>(buffer->Data());
  for (int i = 0; i < length; i++) {
    dest[i] = audio10ms[i];
  }

  AudioSegment segment;
  segment.Init(CHANNELS);
  segment.AppendFrames(
    buffer.forget(), length, 0, length, nsAudioStream::FORMAT_S16_LE
  );
  mSource->AppendToTrack(mTrackID, &segment);

  return;
}

}
