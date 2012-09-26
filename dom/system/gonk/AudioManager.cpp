/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h> 

#include "mozilla/Hal.h"
#include "AudioManager.h"
#include "gonk/AudioSystem.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

using namespace mozilla::dom::gonk;
using namespace android;
using namespace mozilla::hal;
using namespace mozilla;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AudioManager" , ## args) 

#define HEADPHONES_STATUS_CHANGED "headphones-status-changed"
#define HEADPHONES_STATUS_ON      NS_LITERAL_STRING("on").get()
#define HEADPHONES_STATUS_OFF     NS_LITERAL_STRING("off").get()
#define HEADPHONES_STATUS_UNKNOWN NS_LITERAL_STRING("unknown").get()

// A bitwise variable for recording what kind of headset is attached.
static int sHeadsetState;

static bool
IsFmRadioAudioOn()
{
  if (static_cast<
      audio_policy_dev_state_t (*) (audio_devices_t, const char *)
      >(AudioSystem::getDeviceConnectionState)) {
    return AudioSystem::getDeviceConnectionState(AUDIO_DEVICE_OUT_FM, "") == 
           AUDIO_POLICY_DEVICE_STATE_AVAILABLE ? true : false;
  } else {
    return false;
  }
}

NS_IMPL_ISUPPORTS1(AudioManager, nsIAudioManager)

static AudioSystem::audio_devices
GetRoutingMode(int aType) {
  if (aType == nsIAudioManager::FORCE_SPEAKER) {
    return AudioSystem::DEVICE_OUT_SPEAKER;
  } else if (aType == nsIAudioManager::FORCE_HEADPHONES) {
    return AudioSystem::DEVICE_OUT_WIRED_HEADSET;
  } else if (aType == nsIAudioManager::FORCE_BT_SCO) {
    return AudioSystem::DEVICE_OUT_BLUETOOTH_SCO;
  } else if (aType == nsIAudioManager::FORCE_BT_A2DP) {
    return AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP;
  } else {
    return AudioSystem::DEVICE_IN_DEFAULT;
  }
}

static void
InternalSetAudioRoutesICS(SwitchState aState)
{
  if (aState == SWITCH_STATE_HEADSET) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADSET,
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE, "");
    sHeadsetState |= AUDIO_DEVICE_OUT_WIRED_HEADSET;
  } else if (aState == SWITCH_STATE_HEADPHONE) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE, "");
    sHeadsetState |= AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
  } else if (aState == SWITCH_STATE_OFF) {
    AudioSystem::setDeviceConnectionState(static_cast<audio_devices_t>(sHeadsetState),
                                          AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "");
    sHeadsetState = 0;
  }

  // The audio volume is not consistent when we plug and unplug the headset.
  // Set the fm volume again here.
  if (IsFmRadioAudioOn()) {
    float masterVolume;
    AudioSystem::getMasterVolume(&masterVolume);
    AudioSystem::setFmVolume(masterVolume);
  }
}

static void
InternalSetAudioRoutesGB(SwitchState aState)
{
  audio_io_handle_t handle = 
    AudioSystem::getOutput((AudioSystem::stream_type)AudioSystem::SYSTEM);
  String8 cmd;

  if (aState == SWITCH_STATE_HEADSET || aState == SWITCH_STATE_HEADPHONE) {
    cmd.appendFormat("routing=%d", GetRoutingMode(nsIAudioManager::FORCE_HEADPHONES));
  } else if (aState == SWITCH_STATE_OFF) {
    cmd.appendFormat("routing=%d", GetRoutingMode(nsIAudioManager::FORCE_SPEAKER));
  }

  AudioSystem::setParameters(handle, cmd);
}

static void
InternalSetAudioRoutes(SwitchState aState)
{
  if (static_cast<
    status_t (*)(audio_devices_t, audio_policy_dev_state_t, const char*)
    >(AudioSystem::setDeviceConnectionState)) {
    InternalSetAudioRoutesICS(aState);
  } else if (static_cast<
    audio_io_handle_t (*)(AudioSystem::stream_type, uint32_t, uint32_t, uint32_t, AudioSystem::output_flags)
    >(AudioSystem::getOutput)) {
    InternalSetAudioRoutesGB(aState);
  }
}

static void
NotifyHeadphonesStatus(SwitchState aState)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    if (aState == SWITCH_STATE_ON) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_ON);
    } else if (aState == SWITCH_STATE_OFF) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_OFF);
    } else {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_UNKNOWN);
    }
  }
}

class HeadphoneSwitchObserver : public SwitchObserver
{
public:
  void Notify(const SwitchEvent& aEvent) {
    InternalSetAudioRoutes(aEvent.status());
    NotifyHeadphonesStatus(aEvent.status());
  }
};

AudioManager::AudioManager() : mPhoneState(PHONE_STATE_CURRENT),
                 mObserver(new HeadphoneSwitchObserver())
{
  RegisterSwitchObserver(SWITCH_HEADPHONES, mObserver);

  InternalSetAudioRoutes(GetCurrentSwitchState(SWITCH_HEADPHONES));
}

AudioManager::~AudioManager() {
  UnregisterSwitchObserver(SWITCH_HEADPHONES, mObserver);
}

NS_IMETHODIMP
AudioManager::GetMicrophoneMuted(bool* aMicrophoneMuted)
{
  if (AudioSystem::isMicrophoneMuted(aMicrophoneMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMicrophoneMuted(bool aMicrophoneMuted)
{
  if (AudioSystem::muteMicrophone(aMicrophoneMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMasterVolume(float* aMasterVolume)
{
  if (AudioSystem::getMasterVolume(aMasterVolume)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMasterVolume(float aMasterVolume)
{
  if (AudioSystem::setMasterVolume(aMasterVolume)) {
    return NS_ERROR_FAILURE;
  }
  // For now, just set the voice volume at the same level
  if (AudioSystem::setVoiceVolume(aMasterVolume)) {
    return NS_ERROR_FAILURE;
  }

  if (IsFmRadioAudioOn() && AudioSystem::setFmVolume(aMasterVolume)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMasterMuted(bool* aMasterMuted)
{
  if (AudioSystem::getMasterMute(aMasterMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMasterMuted(bool aMasterMuted)
{
  if (AudioSystem::setMasterMute(aMasterMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetPhoneState(int32_t* aState)
{
  *aState = mPhoneState;
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetPhoneState(int32_t aState)
{
  if (AudioSystem::setPhoneState(aState)) {
    return NS_ERROR_FAILURE;
  }

  mPhoneState = aState;
  return NS_OK;
}

//
// Kids, don't try this at home.  We want this to link and work on
// both GB and ICS.  Problem is, the symbol exported by audioflinger
// is different on the two gonks.
//
// So what we do here is weakly link to both of them, and then call
// whichever symbol resolves at dynamic link time (if any).
//
NS_IMETHODIMP
AudioManager::SetForceForUse(int32_t aUsage, int32_t aForce)
{
  status_t status = 0;
  if (static_cast<
      status_t (*)(AudioSystem::force_use, AudioSystem::forced_config)
      >(AudioSystem::setForceUse)) {
    // Dynamically resolved the GB signature.
    status = AudioSystem::setForceUse((AudioSystem::force_use)aUsage,
                                      (AudioSystem::forced_config)aForce);
  } else if (static_cast<
             status_t (*)(audio_policy_force_use_t, audio_policy_forced_cfg_t)
             >(AudioSystem::setForceUse)) {
    // Dynamically resolved the ICS signature.
    status = AudioSystem::setForceUse((audio_policy_force_use_t)aUsage,
                                      (audio_policy_forced_cfg_t)aForce);
  }

  return status ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
AudioManager::GetForceForUse(int32_t aUsage, int32_t* aForce) {
  if (static_cast<
      AudioSystem::forced_config (*)(AudioSystem::force_use)
      >(AudioSystem::getForceUse)) {
    // Dynamically resolved the GB signature.
    *aForce = AudioSystem::getForceUse((AudioSystem::force_use)aUsage);
  } else if (static_cast<
             audio_policy_forced_cfg_t (*)(audio_policy_force_use_t)
             >(AudioSystem::getForceUse)) {
    // Dynamically resolved the ICS signature.
    *aForce = AudioSystem::getForceUse((audio_policy_force_use_t)aUsage);
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetFmRadioAudioEnabled(bool *aFmRadioAudioEnabled)
{
  *aFmRadioAudioEnabled = IsFmRadioAudioOn();
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetFmRadioAudioEnabled(bool aFmRadioAudioEnabled)
{
  if (static_cast<
      status_t (*) (AudioSystem::audio_devices, AudioSystem::device_connection_state, const char *)
      >(AudioSystem::setDeviceConnectionState)) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_FM,
      aFmRadioAudioEnabled ? AUDIO_POLICY_DEVICE_STATE_AVAILABLE : 
      AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "");
    InternalSetAudioRoutes(GetCurrentSwitchState(SWITCH_HEADPHONES));
    return NS_OK;
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
}
