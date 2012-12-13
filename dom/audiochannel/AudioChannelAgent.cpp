/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelCommon.h"
#include "AudioChannelService.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS1(AudioChannelAgent, nsIAudioChannelAgent)

AudioChannelAgent::AudioChannelAgent()
  : mCallback(nullptr)
  , mAudioChannelType(AUDIO_AGENT_CHANNEL_ERROR)
  , mIsRegToService(false)
  , mVisible(true)
{
}

AudioChannelAgent::~AudioChannelAgent()
{
}

/* readonly attribute long audioChannelType; */
NS_IMETHODIMP AudioChannelAgent::GetAudioChannelType(int32_t *aAudioChannelType)
{
  *aAudioChannelType = mAudioChannelType;
  return NS_OK;
}

/* boolean init (in long channelType); */
NS_IMETHODIMP AudioChannelAgent::Init(int32_t channelType, nsIAudioChannelAgentCallback *callback)
{
  // We syncd the enum of channel type between nsIAudioChannelAgent.idl and
  // AudioChannelCommon.h the same.
  MOZ_STATIC_ASSERT(static_cast<AudioChannelType>(AUDIO_AGENT_CHANNEL_NORMAL) ==
                    AUDIO_CHANNEL_NORMAL &&
                    static_cast<AudioChannelType>(AUDIO_AGENT_CHANNEL_CONTENT) ==
                    AUDIO_CHANNEL_CONTENT &&
                    static_cast<AudioChannelType>(AUDIO_AGENT_CHANNEL_NOTIFICATION) ==
                    AUDIO_CHANNEL_NOTIFICATION &&
                    static_cast<AudioChannelType>(AUDIO_AGENT_CHANNEL_ALARM) ==
                    AUDIO_CHANNEL_ALARM &&
                    static_cast<AudioChannelType>(AUDIO_AGENT_CHANNEL_TELEPHONY) ==
                    AUDIO_CHANNEL_TELEPHONY &&
                    static_cast<AudioChannelType>(AUDIO_AGENT_CHANNEL_RINGER) ==
                    AUDIO_CHANNEL_RINGER &&
                    static_cast<AudioChannelType>(AUDIO_AGENT_CHANNEL_PUBLICNOTIFICATION) ==
                    AUDIO_CHANNEL_PUBLICNOTIFICATION,
                    "Enum of channel on nsIAudioChannelAgent.idl should be the same with AudioChannelCommon.h");

  if (mAudioChannelType != AUDIO_AGENT_CHANNEL_ERROR ||
      channelType > AUDIO_AGENT_CHANNEL_PUBLICNOTIFICATION ||
      channelType < AUDIO_AGENT_CHANNEL_NORMAL) {
    return NS_ERROR_FAILURE;
  }

  mAudioChannelType = channelType;
  mCallback = callback;
  return NS_OK;
}

/* boolean startPlaying (); */
NS_IMETHODIMP AudioChannelAgent::StartPlaying(bool *_retval)
{
  AudioChannelService *service = AudioChannelService::GetAudioChannelService();
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      service == nullptr) {
    return NS_ERROR_FAILURE;
  }

  service->RegisterAudioChannelAgent(this,
    static_cast<AudioChannelType>(mAudioChannelType));
  *_retval = !service->GetMuted(static_cast<AudioChannelType>(mAudioChannelType), !mVisible);
  mIsRegToService = true;
  return NS_OK;
}

/* void stopPlaying (); */
NS_IMETHODIMP AudioChannelAgent::StopPlaying(void)
{
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      mIsRegToService == false) {
    return NS_ERROR_FAILURE;
  }

  AudioChannelService *service = AudioChannelService::GetAudioChannelService();
  service->UnregisterAudioChannelAgent(this);
  mIsRegToService = false;
  return NS_OK;
}

/* void setVisibilityState (in boolean visible); */
NS_IMETHODIMP AudioChannelAgent::SetVisibilityState(bool visible)
{
  bool oldVisibility = mVisible;

  mVisible = visible;
  if (mIsRegToService && oldVisibility != mVisible && mCallback != nullptr) {
    AudioChannelService *service = AudioChannelService::GetAudioChannelService();
    mCallback->CanPlayChanged(!service->GetMuted(static_cast<AudioChannelType>(mAudioChannelType),
       !mVisible));
  }
  return NS_OK;
}

void AudioChannelAgent::NotifyAudioChannelStateChanged()
{
  if (mCallback != nullptr) {
    AudioChannelService *service = AudioChannelService::GetAudioChannelService();
    mCallback->CanPlayChanged(!service->GetMuted(static_cast<AudioChannelType>(mAudioChannelType),
      !mVisible));
  }
}

