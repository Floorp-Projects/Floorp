/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelCommon.h"
#include "AudioChannelService.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_1(AudioChannelAgent, mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioChannelAgent)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgent)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioChannelAgent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AudioChannelAgent)

AudioChannelAgent::AudioChannelAgent()
  : mAudioChannelType(AUDIO_AGENT_CHANNEL_ERROR)
  , mIsRegToService(false)
  , mVisible(true)
{
}

AudioChannelAgent::~AudioChannelAgent()
{
  if (mIsRegToService) {
    StopPlaying();
  }
}

/* readonly attribute long audioChannelType; */
NS_IMETHODIMP AudioChannelAgent::GetAudioChannelType(int32_t *aAudioChannelType)
{
  *aAudioChannelType = mAudioChannelType;
  return NS_OK;
}

/* boolean init (in long channelType, in nsIAudioChannelAgentCallback callback); */
NS_IMETHODIMP AudioChannelAgent::Init(int32_t channelType, nsIAudioChannelAgentCallback *callback)
{
  return InitInternal(channelType, callback, /* useWeakRef = */ false);
}

/* boolean initWithWeakCallback (in long channelType,
 *                               in nsIAudioChannelAgentCallback callback); */
NS_IMETHODIMP
AudioChannelAgent::InitWithWeakCallback(int32_t channelType,
                                        nsIAudioChannelAgentCallback *callback)
{
  return InitInternal(channelType, callback, /* useWeakRef = */ true);
}

nsresult
AudioChannelAgent::InitInternal(int32_t aChannelType,
                                nsIAudioChannelAgentCallback *aCallback,
                                bool aUseWeakRef)
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
      aChannelType > AUDIO_AGENT_CHANNEL_PUBLICNOTIFICATION ||
      aChannelType < AUDIO_AGENT_CHANNEL_NORMAL) {
    return NS_ERROR_FAILURE;
  }

  mAudioChannelType = aChannelType;

  if (aUseWeakRef) {
    mWeakCallback = do_GetWeakReference(aCallback);
  } else {
    mCallback = aCallback;
  }

  return NS_OK;
}

/* boolean startPlaying (); */
NS_IMETHODIMP AudioChannelAgent::StartPlaying(bool *_retval)
{
  AudioChannelService *service = AudioChannelService::GetAudioChannelService();
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      service == nullptr || mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  service->RegisterAudioChannelAgent(this,
    static_cast<AudioChannelType>(mAudioChannelType));
  *_retval = !service->GetMuted(this, !mVisible);
  mIsRegToService = true;
  return NS_OK;
}

/* void stopPlaying (); */
NS_IMETHODIMP AudioChannelAgent::StopPlaying(void)
{
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      !mIsRegToService) {
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

  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();

  mVisible = visible;
  if (mIsRegToService && oldVisibility != mVisible && callback) {
    AudioChannelService *service = AudioChannelService::GetAudioChannelService();
    callback->CanPlayChanged(!service->GetMuted(this, !mVisible));
  }
  return NS_OK;
}

void AudioChannelAgent::NotifyAudioChannelStateChanged()
{
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (callback) {
    AudioChannelService *service = AudioChannelService::GetAudioChannelService();
    callback->CanPlayChanged(!service->GetMuted(this, !mVisible));
  }
}

already_AddRefed<nsIAudioChannelAgentCallback>
AudioChannelAgent::GetCallback()
{
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = mCallback;
  if (!callback) {
    callback = do_QueryReferent(mWeakCallback);
  }
  return callback.forget();
}
