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
#include <cutils/properties.h>

#include "AudioChannelService.h"
#include "AudioManager.h"

#include "nsIObserverService.h"
#ifdef MOZ_B2G_RIL
#include "nsIRadioInterfaceLayer.h"
#endif
#include "nsISettingsService.h"
#include "nsPrintfCString.h"

#include "mozilla/Hal.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ScriptSettings.h"
#include "base/message_loop.h"

#include "BluetoothCommon.h"
#include "BluetoothHfpManagerBase.h"

#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"

using namespace mozilla::dom::gonk;
using namespace android;
using namespace mozilla::hal;
using namespace mozilla;
using namespace mozilla::dom::bluetooth;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AudioManager" , ## args)

#define HEADPHONES_STATUS_HEADSET     MOZ_UTF16("headset")
#define HEADPHONES_STATUS_HEADPHONE   MOZ_UTF16("headphone")
#define HEADPHONES_STATUS_OFF         MOZ_UTF16("off")
#define HEADPHONES_STATUS_UNKNOWN     MOZ_UTF16("unknown")
#define HEADPHONES_STATUS_CHANGED     "headphones-status-changed"
#define MOZ_SETTINGS_CHANGE_ID        "mozsettings-changed"
#define AUDIO_CHANNEL_PROCESS_CHANGED "audio-channel-process-changed"

static void BinderDeadCallback(status_t aErr);
static void InternalSetAudioRoutes(SwitchState aState);
// Refer AudioService.java from Android
static int sMaxStreamVolumeTbl[AUDIO_STREAM_CNT] = {
  5,   // voice call
  15,  // system
  15,  // ring
  15,  // music
  15,  // alarm
  15,  // notification
  15,  // BT SCO
  15,  // enforced audible
  15,  // DTMF
  15,  // TTS
  15,  // FM
};
// A bitwise variable for recording what kind of headset is attached.
static int sHeadsetState;
static const int kBtSampleRate = 8000;
static bool sSwitchDone = true;
static bool sA2dpSwitchDone = true;

namespace mozilla {
namespace dom {
namespace gonk {
class RecoverTask : public nsRunnable
{
public:
  RecoverTask() {}
  NS_IMETHODIMP Run() {
    nsCOMPtr<nsIAudioManager> amService = do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(amService, NS_OK);
    AudioManager *am = static_cast<AudioManager *>(amService.get());
    for (int loop = 0; loop < AUDIO_STREAM_CNT; loop++) {
      AudioSystem::initStreamVolume(static_cast<audio_stream_type_t>(loop), 0,
                                   sMaxStreamVolumeTbl[loop]);
      int32_t index;
      am->GetStreamVolumeIndex(loop, &index);
      am->SetStreamVolumeIndex(loop, index);
    }

    if (sHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADSET)
      InternalSetAudioRoutes(SWITCH_STATE_HEADSET);
    else if (sHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)
      InternalSetAudioRoutes(SWITCH_STATE_HEADPHONE);
    else
      InternalSetAudioRoutes(SWITCH_STATE_OFF);

    int32_t phoneState = nsIAudioManager::PHONE_STATE_INVALID;
    am->GetPhoneState(&phoneState);
#if ANDROID_VERSION < 17
    AudioSystem::setPhoneState(phoneState);
#else
    AudioSystem::setPhoneState(static_cast<audio_mode_t>(phoneState));
#endif

    AudioSystem::get_audio_flinger();
    return NS_OK;
  }
};

class AudioChannelVolInitCallback MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  AudioChannelVolInitCallback() {}

  NS_IMETHOD Handle(const nsAString& aName, JS::Handle<JS::Value> aResult)
  {
    nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(aResult.isInt32(), NS_OK);

    int32_t volIndex = aResult.toInt32();
    if (aName.EqualsLiteral("audio.volume.content")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Content,
                                          volIndex);
    } else if (aName.EqualsLiteral("audio.volume.notification")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Notification,
                                          volIndex);
    } else if (aName.EqualsLiteral("audio.volume.alarm")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Alarm,
                                          volIndex);
    } else if (aName.EqualsLiteral("audio.volume.telephony")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Telephony,
                                          volIndex);
    } else if (aName.EqualsLiteral("audio.volume.bt_sco")) {
      static_cast<AudioManager *>(audioManager.get())->SetStreamVolumeIndex(
        AUDIO_STREAM_BLUETOOTH_SCO, volIndex);
    } else {
      MOZ_ASSERT_UNREACHABLE("unexpected audio channel for initializing "
                             "volume control");
    }

    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    LOG("AudioChannelVolInitCallback::HandleError: %s\n",
      NS_ConvertUTF16toUTF8(aName).get());
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(AudioChannelVolInitCallback, nsISettingsServiceCallback)
} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */

static void
BinderDeadCallback(status_t aErr)
{
  if (aErr == DEAD_OBJECT) {
    NS_DispatchToMainThread(new RecoverTask());
  }
}

static bool
IsDeviceOn(audio_devices_t device)
{
  if (static_cast<
      audio_policy_dev_state_t (*) (audio_devices_t, const char *)
      >(AudioSystem::getDeviceConnectionState))
    return AudioSystem::getDeviceConnectionState(device, "") ==
           AUDIO_POLICY_DEVICE_STATE_AVAILABLE;

  return false;
}

static void ProcessDelayedAudioRoute(SwitchState aState)
{
  if (sSwitchDone)
    return;
  InternalSetAudioRoutes(aState);
  sSwitchDone = true;
}

static void ProcessDelayedA2dpRoute(audio_policy_dev_state_t aState, const char *aAddress)
{
  if (sA2dpSwitchDone)
    return;
  AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
                                        aState, aAddress);
  String8 cmd("bluetooth_enabled=false");
  AudioSystem::setParameters(0, cmd);
  cmd.setTo("A2dpSuspended=true");
  AudioSystem::setParameters(0, cmd);
  sA2dpSwitchDone = true;
}

NS_IMPL_ISUPPORTS(AudioManager, nsIAudioManager, nsIObserver)

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
}

static void
InternalSetAudioRoutes(SwitchState aState)
{
  if (static_cast<
    status_t (*)(audio_devices_t, audio_policy_dev_state_t, const char*)
    >(AudioSystem::setDeviceConnectionState)) {
    InternalSetAudioRoutesICS(aState);
  } else {
    NS_NOTREACHED("Doesn't support audio routing on GB version");
  }
}

void
AudioManager::HandleBluetoothStatusChanged(nsISupports* aSubject,
                                           const char* aTopic,
                                           const nsCString aAddress)
{
#ifdef MOZ_B2G_BT
  bool status;
  if (!strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    BluetoothHfpManagerBase* hfp =
      static_cast<BluetoothHfpManagerBase*>(aSubject);
    status = hfp->IsScoConnected();
  } else {
    BluetoothProfileManagerBase* profile =
      static_cast<BluetoothProfileManagerBase*>(aSubject);
    status = profile->IsConnected();
  }

  audio_policy_dev_state_t audioState = status ?
    AUDIO_POLICY_DEVICE_STATE_AVAILABLE :
    AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE;

  if (!strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    if (audioState == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) {
      String8 cmd;
      cmd.appendFormat("bt_samplerate=%d", kBtSampleRate);
      AudioSystem::setParameters(0, cmd);
      SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_BT_SCO);
    } else {
      int32_t force;
      GetForceForUse(nsIAudioManager::USE_COMMUNICATION, &force);
      if (force == nsIAudioManager::FORCE_BT_SCO)
        SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_NONE);
    }
  } else if (!strcmp(aTopic, BLUETOOTH_A2DP_STATUS_CHANGED_ID)) {
    if (audioState == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE && sA2dpSwitchDone) {
      MessageLoop::current()->PostDelayedTask(
        FROM_HERE, NewRunnableFunction(&ProcessDelayedA2dpRoute, audioState, aAddress.get()), 1000);
      sA2dpSwitchDone = false;
    } else {
      AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
                                            audioState, aAddress.get());
      String8 cmd("bluetooth_enabled=true");
      AudioSystem::setParameters(0, cmd);
      cmd.setTo("A2dpSuspended=false");
      AudioSystem::setParameters(0, cmd);
      sA2dpSwitchDone = true;
    }
  } else if (!strcmp(aTopic, BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET,
                                          audioState, aAddress.get());
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
                                          audioState, aAddress.get());
  }
#endif
}

void
AudioManager::HandleAudioChannelProcessChanged()
{
  // Note: If the user answers a VoIP call (e.g. WebRTC calls) during the
  // telephony call (GSM/CDMA calls) the audio manager won't set the
  // PHONE_STATE_IN_COMMUNICATION audio state. Once the telephony call finishes
  // the RIL plumbing sets the PHONE_STATE_NORMAL audio state. This seems to be
  // an issue for the VoIP call but it is not. Once the RIL plumbing sets the
  // the PHONE_STATE_NORMAL audio state the AudioManager::mPhoneAudioAgent
  // member will call the StopPlaying() method causing that this function will
  // be called again and therefore the audio manager sets the
  // PHONE_STATE_IN_COMMUNICATION audio state.

  if ((mPhoneState == PHONE_STATE_IN_CALL) ||
      (mPhoneState == PHONE_STATE_RINGTONE)) {
    return;
  }

  AudioChannelService *service = AudioChannelService::GetAudioChannelService();
  NS_ENSURE_TRUE_VOID(service);

  bool telephonyChannelIsActive = service->TelephonyChannelIsActive();
  telephonyChannelIsActive ? SetPhoneState(PHONE_STATE_IN_COMMUNICATION) :
                             SetPhoneState(PHONE_STATE_NORMAL);
}

nsresult
AudioManager::Observe(nsISupports* aSubject,
                      const char* aTopic,
                      const char16_t* aData)
{
  if ((strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID) == 0) ||
      (strcmp(aTopic, BLUETOOTH_HFP_STATUS_CHANGED_ID) == 0) ||
      (strcmp(aTopic, BLUETOOTH_A2DP_STATUS_CHANGED_ID) == 0)) {
    nsCString address = NS_ConvertUTF16toUTF8(nsDependentString(aData));
    if (address.IsEmpty()) {
      NS_WARNING(nsPrintfCString("Invalid address of %s", aTopic).get());
      return NS_ERROR_FAILURE;
    }

    HandleBluetoothStatusChanged(aSubject, aTopic, address);
    return NS_OK;
  }

  else if (!strcmp(aTopic, AUDIO_CHANNEL_PROCESS_CHANGED)) {
    HandleAudioChannelProcessChanged();
    return NS_OK;
  }

  // To process the volume control on each audio channel according to
  // change of settings
  else if (!strcmp(aTopic, MOZ_SETTINGS_CHANGE_ID)) {
    AutoSafeJSContext cx;
    nsDependentString dataStr(aData);
    JS::Rooted<JS::Value> val(cx);
    if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
        !val.isObject()) {
      return NS_OK;
    }

    JS::Rooted<JSObject*> obj(cx, &val.toObject());
    JS::Rooted<JS::Value> key(cx);
    if (!JS_GetProperty(cx, obj, "key", &key) ||
        !key.isString()) {
      return NS_OK;
    }

    JS::Rooted<JSString*> jsKey(cx, JS::ToString(cx, key));
    if (!jsKey) {
      return NS_OK;
    }
    nsAutoJSString keyStr;
    if (!keyStr.init(cx, jsKey) || !keyStr.EqualsLiteral("audio.volume.bt_sco")) {
      return NS_OK;
    }

    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, obj, "value", &value) || !value.isInt32()) {
      return NS_OK;
    }

    int32_t index = value.toInt32();
    SetStreamVolumeIndex(AUDIO_STREAM_BLUETOOTH_SCO, index);

    return NS_OK;
  }

  NS_WARNING("Unexpected topic in AudioManager");
  return NS_ERROR_FAILURE;
}

static void
NotifyHeadphonesStatus(SwitchState aState)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    if (aState == SWITCH_STATE_HEADSET) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADSET);
    } else if (aState == SWITCH_STATE_HEADPHONE) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADPHONE);
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
    NotifyHeadphonesStatus(aEvent.status());
    // When user pulled out the headset, a delay of routing here can avoid the leakage of audio from speaker.
    if (aEvent.status() == SWITCH_STATE_OFF && sSwitchDone) {
      MessageLoop::current()->PostDelayedTask(
        FROM_HERE, NewRunnableFunction(&ProcessDelayedAudioRoute, SWITCH_STATE_OFF), 1000);
      sSwitchDone = false;
    } else if (aEvent.status() != SWITCH_STATE_OFF) {
      InternalSetAudioRoutes(aEvent.status());
      sSwitchDone = true;
    }
  }
};

AudioManager::AudioManager()
  : mPhoneState(PHONE_STATE_CURRENT)
  , mObserver(new HeadphoneSwitchObserver())
#ifdef MOZ_B2G_RIL
  , mMuteCallToRIL(false)
#endif
{
  RegisterSwitchObserver(SWITCH_HEADPHONES, mObserver);

  InternalSetAudioRoutes(GetCurrentSwitchState(SWITCH_HEADPHONES));
  NotifyHeadphonesStatus(GetCurrentSwitchState(SWITCH_HEADPHONES));

  for (int loop = 0; loop < AUDIO_STREAM_CNT; loop++) {
    AudioSystem::initStreamVolume(static_cast<audio_stream_type_t>(loop), 0,
                                  sMaxStreamVolumeTbl[loop]);
    mCurrentStreamVolumeTbl[loop] = sMaxStreamVolumeTbl[loop];
  }
  // Force publicnotification to output at maximal volume
  SetStreamVolumeIndex(AUDIO_STREAM_ENFORCED_AUDIBLE,
                       sMaxStreamVolumeTbl[AUDIO_STREAM_ENFORCED_AUDIBLE]);

  // Get the initial volume index from settings DB during boot up.
  nsCOMPtr<nsISettingsService> settingsService =
    do_GetService("@mozilla.org/settingsService;1");
  NS_ENSURE_TRUE_VOID(settingsService);
  nsCOMPtr<nsISettingsServiceLock> lock;
  nsresult rv = settingsService->CreateLock(nullptr, getter_AddRefs(lock));
  NS_ENSURE_SUCCESS_VOID(rv);
  nsCOMPtr<nsISettingsServiceCallback> callback = new AudioChannelVolInitCallback();
  NS_ENSURE_TRUE_VOID(callback);
  lock->Get("audio.volume.content", callback);
  lock->Get("audio.volume.notification", callback);
  lock->Get("audio.volume.alarm", callback);
  lock->Get("audio.volume.telephony", callback);
  lock->Get("audio.volume.bt_sco", callback);

  // Gecko only control stream volume not master so set to default value
  // directly.
  AudioSystem::setMasterVolume(1.0);
  AudioSystem::setErrorCallback(BinderDeadCallback);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_SCO_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth sco status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_A2DP_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth a2dp status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_HFP_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth hfp status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, MOZ_SETTINGS_CHANGE_ID, false))) {
    NS_WARNING("Failed to add mozsettings-changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, AUDIO_CHANNEL_PROCESS_CHANGED, false))) {
    NS_WARNING("Failed to add audio-channel-process-changed observer!");
  }

#ifdef MOZ_B2G_RIL
  char value[PROPERTY_VALUE_MAX];
  property_get("ro.moz.mute.call.to_ril", value, "false");
  if (!strcmp(value, "true")) {
    mMuteCallToRIL = true;
  }
#endif
}

AudioManager::~AudioManager() {
  UnregisterSwitchObserver(SWITCH_HEADPHONES, mObserver);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_SCO_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth sco status changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_A2DP_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth a2dp status changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_HFP_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth hfp status changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this, MOZ_SETTINGS_CHANGE_ID))) {
    NS_WARNING("Failed to remove mozsettings-changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this,  AUDIO_CHANNEL_PROCESS_CHANGED))) {
    NS_WARNING("Failed to remove audio-channel-process-changed!");
  }
}

static StaticRefPtr<AudioManager> sAudioManager;

already_AddRefed<AudioManager>
AudioManager::GetInstance()
{
  // Avoid createing AudioManager from content process.
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    MOZ_CRASH("Non-chrome processes should not get here.");
  }

  // Avoid createing multiple AudioManager instance inside main process.
  if (!sAudioManager) {
    sAudioManager = new AudioManager();
    ClearOnShutdown(&sAudioManager);
  }

  nsRefPtr<AudioManager> audioMgr = sAudioManager.get();
  return audioMgr.forget();
}

NS_IMETHODIMP
AudioManager::GetMicrophoneMuted(bool* aMicrophoneMuted)
{
#ifdef MOZ_B2G_RIL
  if (mMuteCallToRIL) {
    // Simply return cached mIsMicMuted if mute call go via RIL.
    *aMicrophoneMuted = mIsMicMuted;
    return NS_OK;
  }
#endif

  if (AudioSystem::isMicrophoneMuted(aMicrophoneMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMicrophoneMuted(bool aMicrophoneMuted)
{
  if (!AudioSystem::muteMicrophone(aMicrophoneMuted)) {
#ifdef MOZ_B2G_RIL
    if (mMuteCallToRIL) {
      // Extra mute request to RIL for specific platform.
      nsCOMPtr<nsIRadioInterfaceLayer> ril = do_GetService("@mozilla.org/ril;1");
      NS_ENSURE_TRUE(ril, NS_ERROR_FAILURE);
      ril->SetMicrophoneMuted(aMicrophoneMuted);
      mIsMicMuted = aMicrophoneMuted;
    }
#endif
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
  if (mPhoneState == aState) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    nsString state;
    state.AppendInt(aState);
    obs->NotifyObservers(nullptr, "phone-state-changed", state.get());
  }

#if ANDROID_VERSION < 17
  if (AudioSystem::setPhoneState(aState)) {
#else
  if (AudioSystem::setPhoneState(static_cast<audio_mode_t>(aState))) {
#endif
    return NS_ERROR_FAILURE;
  }

  mPhoneState = aState;

  if (mPhoneAudioAgent) {
    mPhoneAudioAgent->StopPlaying();
    mPhoneAudioAgent = nullptr;
  }

  if (aState == PHONE_STATE_IN_CALL || aState == PHONE_STATE_RINGTONE) {
    mPhoneAudioAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
    MOZ_ASSERT(mPhoneAudioAgent);
    if (aState == PHONE_STATE_IN_CALL) {
      // Telephony doesn't be paused by any other channels.
      mPhoneAudioAgent->Init(nullptr, (int32_t)AudioChannel::Telephony, nullptr);
    } else {
      mPhoneAudioAgent->Init(nullptr, (int32_t)AudioChannel::Ringer, nullptr);
    }

    // Telephony can always play.
    int32_t canPlay;
    mPhoneAudioAgent->StartPlaying(&canPlay);
  }

  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetForceForUse(int32_t aUsage, int32_t aForce)
{
  if (static_cast<
             status_t (*)(audio_policy_force_use_t, audio_policy_forced_cfg_t)
             >(AudioSystem::setForceUse)) {
    // Dynamically resolved the ICS signature.
    status_t status = AudioSystem::setForceUse(
                        (audio_policy_force_use_t)aUsage,
                        (audio_policy_forced_cfg_t)aForce);
    return status ? NS_ERROR_FAILURE : NS_OK;
  }

  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
AudioManager::GetForceForUse(int32_t aUsage, int32_t* aForce) {
  if (static_cast<
      audio_policy_forced_cfg_t (*)(audio_policy_force_use_t)
      >(AudioSystem::getForceUse)) {
    // Dynamically resolved the ICS signature.
    *aForce = AudioSystem::getForceUse((audio_policy_force_use_t)aUsage);
    return NS_OK;
  }

  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
AudioManager::GetFmRadioAudioEnabled(bool *aFmRadioAudioEnabled)
{
  *aFmRadioAudioEnabled = IsDeviceOn(AUDIO_DEVICE_OUT_FM);
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
    // sync volume with music after powering on fm radio
    if (aFmRadioAudioEnabled) {
      int32_t volIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC];
      SetStreamVolumeIndex(AUDIO_STREAM_FM, volIndex);
      mCurrentStreamVolumeTbl[AUDIO_STREAM_FM] = volIndex;
    }
    return NS_OK;
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP
AudioManager::SetAudioChannelVolume(int32_t aChannel, int32_t aIndex) {
  nsresult status;

  switch (static_cast<AudioChannel>(aChannel)) {
    case AudioChannel::Content:
      // sync FMRadio's volume with content channel.
      if (IsDeviceOn(AUDIO_DEVICE_OUT_FM)) {
        status = SetStreamVolumeIndex(AUDIO_STREAM_FM, aIndex);
        NS_ENSURE_SUCCESS(status, status);
      }
      status = SetStreamVolumeIndex(AUDIO_STREAM_MUSIC, aIndex);
      NS_ENSURE_SUCCESS(status, status);
      status = SetStreamVolumeIndex(AUDIO_STREAM_SYSTEM, aIndex);
      break;
    case AudioChannel::Notification:
      status = SetStreamVolumeIndex(AUDIO_STREAM_NOTIFICATION, aIndex);
      NS_ENSURE_SUCCESS(status, status);
      status = SetStreamVolumeIndex(AUDIO_STREAM_RING, aIndex);
      break;
    case AudioChannel::Alarm:
      status = SetStreamVolumeIndex(AUDIO_STREAM_ALARM, aIndex);
      break;
    case AudioChannel::Telephony:
      status = SetStreamVolumeIndex(AUDIO_STREAM_VOICE_CALL, aIndex);
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  return status;
}

NS_IMETHODIMP
AudioManager::GetAudioChannelVolume(int32_t aChannel, int32_t* aIndex) {
  if (!aIndex) {
    return NS_ERROR_NULL_POINTER;
  }

  switch (static_cast<AudioChannel>(aChannel)) {
    case AudioChannel::Content:
      MOZ_ASSERT(mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC] ==
                 mCurrentStreamVolumeTbl[AUDIO_STREAM_SYSTEM]);
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC];
      break;
    case AudioChannel::Notification:
      MOZ_ASSERT(mCurrentStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 mCurrentStreamVolumeTbl[AUDIO_STREAM_RING]);
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION];
      break;
    case AudioChannel::Alarm:
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_ALARM];
      break;
    case AudioChannel::Telephony:
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_VOICE_CALL];
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMaxAudioChannelVolume(int32_t aChannel, int32_t* aMaxIndex) {
  if (!aMaxIndex) {
    return NS_ERROR_NULL_POINTER;
  }

  int32_t stream;
  switch (static_cast<AudioChannel>(aChannel)) {
    case AudioChannel::Content:
      MOZ_ASSERT(sMaxStreamVolumeTbl[AUDIO_STREAM_MUSIC] ==
                 sMaxStreamVolumeTbl[AUDIO_STREAM_SYSTEM]);
      stream = AUDIO_STREAM_MUSIC;
      break;
    case AudioChannel::Notification:
      MOZ_ASSERT(sMaxStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 sMaxStreamVolumeTbl[AUDIO_STREAM_RING]);
      stream = AUDIO_STREAM_NOTIFICATION;
      break;
    case AudioChannel::Alarm:
      stream = AUDIO_STREAM_ALARM;
      break;
    case AudioChannel::Telephony:
      stream = AUDIO_STREAM_VOICE_CALL;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  *aMaxIndex = sMaxStreamVolumeTbl[stream];
   return NS_OK;
}

nsresult
AudioManager::SetStreamVolumeIndex(int32_t aStream, int32_t aIndex) {
  if (aIndex < 0 || aIndex > sMaxStreamVolumeTbl[aStream]) {
    return NS_ERROR_INVALID_ARG;
  }

  mCurrentStreamVolumeTbl[aStream] = aIndex;
  status_t status;
#if ANDROID_VERSION < 17
   status = AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex);
   return status ? NS_ERROR_FAILURE : NS_OK;
#else
  int device = 0;

  if (aStream == AUDIO_STREAM_BLUETOOTH_SCO) {
    device = AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
  } else if (aStream == AUDIO_STREAM_FM) {
    device = AUDIO_DEVICE_OUT_FM;
  }

  if (device != 0) {
    status = AudioSystem::setStreamVolumeIndex(
               static_cast<audio_stream_type_t>(aStream),
               aIndex,
               device);
    return status ? NS_ERROR_FAILURE : NS_OK;
  }

  status = AudioSystem::setStreamVolumeIndex(
             static_cast<audio_stream_type_t>(aStream),
             aIndex,
             AUDIO_DEVICE_OUT_BLUETOOTH_A2DP);
  status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_SPEAKER);
  status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_WIRED_HEADSET);
  status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_WIRED_HEADPHONE);
  status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_EARPIECE);
  return status ? NS_ERROR_FAILURE : NS_OK;
#endif
}

nsresult
AudioManager::GetStreamVolumeIndex(int32_t aStream, int32_t *aIndex) {
  if (!aIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aStream <= AUDIO_STREAM_DEFAULT || aStream >= AUDIO_STREAM_MAX) {
    return NS_ERROR_INVALID_ARG;
  }

  *aIndex = mCurrentStreamVolumeTbl[aStream];

  return NS_OK;
}
