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
#include <binder/IServiceManager.h>

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
#include "mozilla/MozPromise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "base/message_loop.h"

#include "BluetoothCommon.h"
#include "BluetoothHfpManagerBase.h"

#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"

using namespace mozilla::dom::gonk;
using namespace android;
using namespace mozilla;
using namespace mozilla::dom::bluetooth;

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AudioManager" , ## args)

#define HEADPHONES_STATUS_HEADSET     MOZ_UTF16("headset")
#define HEADPHONES_STATUS_HEADPHONE   MOZ_UTF16("headphone")
#define HEADPHONES_STATUS_OFF         MOZ_UTF16("off")
#define HEADPHONES_STATUS_UNKNOWN     MOZ_UTF16("unknown")
#define HEADPHONES_STATUS_CHANGED     "headphones-status-changed"
#define MOZ_SETTINGS_CHANGE_ID        "mozsettings-changed"
#define AUDIO_CHANNEL_PROCESS_CHANGED "audio-channel-process-changed"
#define AUDIO_POLICY_SERVICE_NAME     "media.audio_policy"
#define SETTINGS_SERVICE              "@mozilla.org/settingsService;1"

// Refer AudioService.java from Android
static const uint32_t sMaxStreamVolumeTbl[AUDIO_STREAM_CNT] = {
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
#if ANDROID_VERSION < 19
  15,  // FM
#endif
};

// Use a half value of each volume category as the default volume.
static const uint32_t sDefaultVolumeCategoriesTbl[VOLUME_TOTAL_NUMBER] = {
  8, // VOLUME_MEDIA
  8, // VOLUME_NOTIFICATION
  8, // VOLUME_ALARM
  3, // VOLUME_TELEPHONY
  8, // VOLUME_BLUETOOTH_SCO
};

// Mappings AudioOutputProfiles to strings.
static const nsAttrValue::EnumTable kAudioOutputProfilesTable[] = {
  { "primary",   DEVICE_PRIMARY },
  { "headset",   DEVICE_HEADSET },
  { "bluetooth", DEVICE_BLUETOOTH },
  { nullptr }
};

static const int kBtSampleRate = 8000;

typedef MozPromise<bool, const char*, true> VolumeInitPromise;

namespace mozilla {
namespace dom {
namespace gonk {

static const VolumeData gVolumeData[VOLUME_TOTAL_NUMBER] = {
  {"audio.volume.content",      VOLUME_MEDIA},
  {"audio.volume.notification", VOLUME_NOTIFICATION},
  {"audio.volume.alarm",        VOLUME_ALARM},
  {"audio.volume.telephony",    VOLUME_TELEPHONY},
  {"audio.volume.bt_sco",       VOLUME_BLUETOOTH_SCO}
};

class RunnableCallTask : public Task
{
public:
  explicit RunnableCallTask(nsIRunnable* aRunnable)
    : mRunnable(aRunnable) {}

  void Run() override
  {
    mRunnable->Run();
  }
protected:
  nsCOMPtr<nsIRunnable> mRunnable;
};

class AudioProfileData final
{
public:
  explicit AudioProfileData(AudioOutputProfiles aProfile)
    : mProfile(aProfile)
    , mActive(false)
  {
    for (uint32_t idx = 0; idx < VOLUME_TOTAL_NUMBER; ++idx) {
      mVolumeTable.AppendElement(0);
    }
  };

  AudioOutputProfiles GetProfile() const
  {
    return mProfile;
  }

  void SetActive(bool aActive)
  {
    mActive = aActive;
  }

  bool GetActive() const
  {
    return mActive;
  }

  nsTArray<uint32_t> mVolumeTable;
private:
  const AudioOutputProfiles mProfile;
  bool mActive;
};

void
AudioManager::HandleAudioFlingerDied()
{
  uint32_t attempt;
  for (attempt = 0; attempt < 50; attempt++) {
    if (defaultServiceManager()->checkService(String16(AUDIO_POLICY_SERVICE_NAME)) != 0) {
      break;
    }

    LOG("AudioPolicyService is dead! attempt=%d", attempt);
    usleep(1000 * 200);
  }

  MOZ_RELEASE_ASSERT(attempt < 50);

  for (uint32_t loop = 0; loop < AUDIO_STREAM_CNT; ++loop) {
    AudioSystem::initStreamVolume(static_cast<audio_stream_type_t>(loop),
                                  0,
                                  sMaxStreamVolumeTbl[loop]);
    uint32_t index;
    GetStreamVolumeIndex(loop, &index);
    SetStreamVolumeIndex(loop, index);
  }

  if (mHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
    UpdateHeadsetConnectionState(hal::SWITCH_STATE_HEADSET);
  } else if (mHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
    UpdateHeadsetConnectionState(hal::SWITCH_STATE_HEADPHONE);
  } else {
    UpdateHeadsetConnectionState(hal::SWITCH_STATE_OFF);
  }

  int32_t phoneState = nsIAudioManager::PHONE_STATE_INVALID;
  GetPhoneState(&phoneState);
#if ANDROID_VERSION < 17
  AudioSystem::setPhoneState(phoneState);
#else
  AudioSystem::setPhoneState(static_cast<audio_mode_t>(phoneState));
#endif

  AudioSystem::get_audio_flinger();
}

class VolumeInitCallback final : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  VolumeInitCallback()
    : mInitCounter(0)
  {
    mPromise = mPromiseHolder.Ensure(__func__);
  }

  nsRefPtr<VolumeInitPromise> GetPromise() const
  {
    return mPromise;
  }

  NS_IMETHOD Handle(const nsAString& aName, JS::Handle<JS::Value> aResult)
  {
    nsRefPtr<AudioManager> audioManager = AudioManager::GetInstance();
    MOZ_ASSERT(audioManager);
    for (uint32_t idx = 0; idx < VOLUME_TOTAL_NUMBER; ++idx) {
      NS_ConvertASCIItoUTF16 volumeType(gVolumeData[idx].mChannelName);
      if (StringBeginsWith(aName, volumeType)) {
        AudioOutputProfiles profile = GetProfileFromSettingName(aName);
        MOZ_ASSERT(profile != DEVICE_ERROR);

        uint32_t category = gVolumeData[idx].mCategory;
        uint32_t volIndex = aResult.isInt32() ?
                  aResult.toInt32() : sDefaultVolumeCategoriesTbl[category];
        nsresult rv = audioManager->ValidateVolumeIndex(category, volIndex);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          mPromiseHolder.Reject("Error : invalid volume index.", __func__);
          return rv;
        }

        audioManager->InitProfileVolume(profile, category, volIndex);
        if (++mInitCounter == DEVICE_TOTAL_NUMBER * VOLUME_TOTAL_NUMBER) {
          mPromiseHolder.Resolve(true, __func__);
        }
        return NS_OK;
      }
    }
    mPromiseHolder.Reject("Error : unexpected audio init event.", __func__);
    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    mPromiseHolder.Reject(NS_ConvertUTF16toUTF8(aName).get(), __func__);
    return NS_OK;
  }

protected:
  ~VolumeInitCallback() {}

  AudioOutputProfiles GetProfileFromSettingName(const nsAString& aName) const
  {
    for (uint32_t idx = 0; kAudioOutputProfilesTable[idx].tag; ++idx) {
      NS_ConvertASCIItoUTF16 profile(kAudioOutputProfilesTable[idx].tag);
      if (StringEndsWith(aName, profile)) {
        return static_cast<AudioOutputProfiles>(kAudioOutputProfilesTable[idx].value);
      }
    }
    return DEVICE_ERROR;
  }

  nsRefPtr<VolumeInitPromise> mPromise;
  MozPromiseHolder<VolumeInitPromise> mPromiseHolder;
  uint32_t mInitCounter;
};

NS_IMPL_ISUPPORTS(VolumeInitCallback, nsISettingsServiceCallback)

static void
BinderDeadCallback(status_t aErr)
{
  if (aErr != DEAD_OBJECT) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableFunction([]() {
      MOZ_ASSERT(NS_IsMainThread());
      nsRefPtr<AudioManager> audioManager = AudioManager::GetInstance();
      NS_ENSURE_TRUE(audioManager.get(), );
      audioManager->HandleAudioFlingerDied();
    });

  NS_DispatchToMainThread(runnable);
}

static bool
IsDeviceOn(audio_devices_t device)
{
#if ANDROID_VERSION >= 15
  return AudioSystem::getDeviceConnectionState(device, "") ==
           AUDIO_POLICY_DEVICE_STATE_AVAILABLE;
#else
  return false;
#endif
}

NS_IMPL_ISUPPORTS(AudioManager, nsIAudioManager, nsIObserver)

void
AudioManager::UpdateHeadsetConnectionState(hal::SwitchState aState)
{
#if ANDROID_VERSION >= 15
  if (aState == hal::SWITCH_STATE_HEADSET) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADSET,
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE, "");
    mHeadsetState |= AUDIO_DEVICE_OUT_WIRED_HEADSET;
  } else if (aState == hal::SWITCH_STATE_HEADPHONE) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE, "");
    mHeadsetState |= AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
  } else if (aState == hal::SWITCH_STATE_OFF) {
    if (mHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
      AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADSET,
                                            AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "");
    }
    if (mHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
      AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
                                            AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "");
    }
    mHeadsetState = 0;
  }

#else
  NS_NOTREACHED("Doesn't support audio routing on GB version");
#endif
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
      SwitchProfileData(DEVICE_BLUETOOTH, true);
    } else {
      int32_t force;
      GetForceForUse(nsIAudioManager::USE_COMMUNICATION, &force);
      if (force == nsIAudioManager::FORCE_BT_SCO) {
        SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_NONE);
      }
      SwitchProfileData(DEVICE_BLUETOOTH, false);
    }
  } else if (!strcmp(aTopic, BLUETOOTH_A2DP_STATUS_CHANGED_ID)) {
    if (audioState == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE && mA2dpSwitchDone) {
      nsRefPtr<AudioManager> self = this;
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction([self, audioState, aAddress]() {
          if (self->mA2dpSwitchDone) {
            return;
          }
          AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
                                                audioState,
                                                aAddress.get());
          String8 cmd("bluetooth_enabled=false");
          AudioSystem::setParameters(0, cmd);
          cmd.setTo("A2dpSuspended=true");
          AudioSystem::setParameters(0, cmd);
          self->mA2dpSwitchDone = true;
        });
      MessageLoop::current()->PostDelayedTask(
        FROM_HERE, new RunnableCallTask(runnable), 1000);

      mA2dpSwitchDone = false;
      SwitchProfileData(DEVICE_BLUETOOTH, false);
    } else {
      AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
                                            audioState, aAddress.get());
      String8 cmd("bluetooth_enabled=true");
      AudioSystem::setParameters(0, cmd);
      cmd.setTo("A2dpSuspended=false");
      AudioSystem::setParameters(0, cmd);
      mA2dpSwitchDone = true;
      SwitchProfileData(DEVICE_BLUETOOTH, true);
#if ANDROID_VERSION >= 17
      if (AudioSystem::getForceUse(AUDIO_POLICY_FORCE_FOR_MEDIA) == AUDIO_POLICY_FORCE_NO_BT_A2DP) {
        SetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, AUDIO_POLICY_FORCE_NONE);
      }
#endif
    }
    mBluetoothA2dpEnabled = audioState == AUDIO_POLICY_DEVICE_STATE_AVAILABLE;
  } else if (!strcmp(aTopic, BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET,
                                          audioState, aAddress.get());
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
                                          audioState, aAddress.get());
  } else if (!strcmp(aTopic, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID)) {
      String8 cmd;
      BluetoothHfpManagerBase* hfp =
          static_cast<BluetoothHfpManagerBase*>(aSubject);
      if (hfp->IsNrecEnabled()) {
          cmd.setTo("bt_headset_name=<unknown>;bt_headset_nrec=on");
          AudioSystem::setParameters(0, cmd);
      } else {
          cmd.setTo("bt_headset_name=<unknown>;bt_headset_nrec=off");
          AudioSystem::setParameters(0, cmd);
      }
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
  // member will call the NotifyStoppedPlaying() method causing that this function will
  // be called again and therefore the audio manager sets the
  // PHONE_STATE_IN_COMMUNICATION audio state.

  if ((mPhoneState == PHONE_STATE_IN_CALL) ||
      (mPhoneState == PHONE_STATE_RINGTONE)) {
    return;
  }

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  MOZ_ASSERT(service);

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
      (strcmp(aTopic, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID) == 0) ||
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

  // To process the volume control on each volume categories according to
  // change of settings
  else if (!strcmp(aTopic, MOZ_SETTINGS_CHANGE_ID)) {
    RootedDictionary<dom::SettingChangeNotification> setting(nsContentUtils::RootingCxForThread());
    if (!WrappedJSToDictionary(aSubject, setting)) {
      return NS_OK;
    }
    if (!StringBeginsWith(setting.mKey, NS_LITERAL_STRING("audio.volume."))) {
      return NS_OK;
    }
    if (!setting.mValue.isNumber()) {
      return NS_OK;
    }

    uint32_t volIndex = setting.mValue.toNumber();
    for (uint32_t idx = 0; idx < VOLUME_TOTAL_NUMBER; ++idx) {
      if (setting.mKey.EqualsASCII(gVolumeData[idx].mChannelName)) {
        SetVolumeByCategory(gVolumeData[idx].mCategory, volIndex);
        return NS_OK;
      }
    }
  }

  NS_WARNING("Unexpected topic in AudioManager");
  return NS_ERROR_FAILURE;
}

static void
NotifyHeadphonesStatus(hal::SwitchState aState)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    if (aState == hal::SWITCH_STATE_HEADSET) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADSET);
    } else if (aState == hal::SWITCH_STATE_HEADPHONE) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADPHONE);
    } else if (aState == hal::SWITCH_STATE_OFF) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_OFF);
    } else {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_UNKNOWN);
    }
  }
}

class HeadphoneSwitchObserver : public hal::SwitchObserver
{
public:
  void Notify(const hal::SwitchEvent& aEvent) {
    nsRefPtr<AudioManager> audioManager = AudioManager::GetInstance();
    MOZ_ASSERT(audioManager);
    audioManager->HandleHeadphoneSwitchEvent(aEvent);
  }
};

void
AudioManager::HandleHeadphoneSwitchEvent(const hal::SwitchEvent& aEvent)
{
  NotifyHeadphonesStatus(aEvent.status());
  // When user pulled out the headset, a delay of routing here can avoid the leakage of audio from speaker.
  if (aEvent.status() == hal::SWITCH_STATE_OFF && mSwitchDone) {

    nsRefPtr<AudioManager> self = this;
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction([self]() {
        if (self->mSwitchDone) {
          return;
        }
        self->UpdateHeadsetConnectionState(hal::SWITCH_STATE_OFF);
        self->mSwitchDone = true;
    });
    MessageLoop::current()->PostDelayedTask(FROM_HERE, new RunnableCallTask(runnable), 1000);

    SwitchProfileData(DEVICE_HEADSET, false);
    mSwitchDone = false;
  } else if (aEvent.status() != hal::SWITCH_STATE_OFF) {
    UpdateHeadsetConnectionState(aEvent.status());
    SwitchProfileData(DEVICE_HEADSET, true);
    mSwitchDone = true;
  }
  // Handle the coexistence of a2dp / headset device, latest one wins.
#if ANDROID_VERSION >= 17
  int32_t forceUse = 0;
  GetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, &forceUse);
  if (aEvent.status() != hal::SWITCH_STATE_OFF && mBluetoothA2dpEnabled) {
    SetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, AUDIO_POLICY_FORCE_NO_BT_A2DP);
  } else if (forceUse == AUDIO_POLICY_FORCE_NO_BT_A2DP) {
    SetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, AUDIO_POLICY_FORCE_NONE);
  }
#endif
}

AudioManager::AudioManager()
  : mPhoneState(PHONE_STATE_CURRENT)
  , mHeadsetState(0)
  , mSwitchDone(true)
#if defined(MOZ_B2G_BT) || ANDROID_VERSION >= 17
  , mBluetoothA2dpEnabled(false)
#endif
#ifdef MOZ_B2G_BT
  , mA2dpSwitchDone(true)
#endif
  , mObserver(new HeadphoneSwitchObserver())
#ifdef MOZ_B2G_RIL
  , mMuteCallToRIL(false)
#endif
{
  hal::RegisterSwitchObserver(hal::SWITCH_HEADPHONES, mObserver);

  UpdateHeadsetConnectionState(hal::GetCurrentSwitchState(hal::SWITCH_HEADPHONES));
  NotifyHeadphonesStatus(hal::GetCurrentSwitchState(hal::SWITCH_HEADPHONES));

  for (uint32_t loop = 0; loop < AUDIO_STREAM_CNT; ++loop) {
    AudioSystem::initStreamVolume(static_cast<audio_stream_type_t>(loop), 0,
                                  sMaxStreamVolumeTbl[loop]);
    mCurrentStreamVolumeTbl[loop] = sMaxStreamVolumeTbl[loop];
  }
  // Force publicnotification to output at maximal volume
  SetStreamVolumeIndex(AUDIO_STREAM_ENFORCED_AUDIBLE,
                       sMaxStreamVolumeTbl[AUDIO_STREAM_ENFORCED_AUDIBLE]);
  CreateAudioProfilesData();

  // Get the initial volume index from settings DB during boot up.
  InitVolumeFromDatabase();

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
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth hfp NREC status changed observer!");
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
  hal::UnregisterSwitchObserver(hal::SWITCH_HEADPHONES, mObserver);

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
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth hfp NREC status changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this, MOZ_SETTINGS_CHANGE_ID))) {
    NS_WARNING("Failed to remove mozsettings-changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this,  AUDIO_CHANNEL_PROCESS_CHANGED))) {
    NS_WARNING("Failed to remove audio-channel-process-changed!");
  }

  // Store the present volume setting to setting database.
  SendVolumeChangeNotification(FindAudioProfileData(mPresentProfile));
}

static StaticRefPtr<AudioManager> sAudioManager;

already_AddRefed<AudioManager>
AudioManager::GetInstance()
{
  // Avoid createing AudioManager from content process.
  if (!XRE_IsParentProcess()) {
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
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetForceForUse(int32_t aUsage, int32_t aForce)
{
#if ANDROID_VERSION >= 15
  status_t status = AudioSystem::setForceUse(
                      (audio_policy_force_use_t)aUsage,
                      (audio_policy_forced_cfg_t)aForce);
  return status ? NS_ERROR_FAILURE : NS_OK;
#else
  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
#endif
}

NS_IMETHODIMP
AudioManager::GetForceForUse(int32_t aUsage, int32_t* aForce) {
#if ANDROID_VERSION >= 15
   *aForce = AudioSystem::getForceUse((audio_policy_force_use_t)aUsage);
   return NS_OK;
#else
  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
#endif
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
  AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_FM,
    aFmRadioAudioEnabled ? AUDIO_POLICY_DEVICE_STATE_AVAILABLE :
    AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "");
  UpdateHeadsetConnectionState(hal::GetCurrentSwitchState(hal::SWITCH_HEADPHONES));
  // AUDIO_STREAM_FM is not used on recent gonk.
  // AUDIO_STREAM_MUSIC is used for FM radio volume control.
#if ANDROID_VERSION < 19
  // sync volume with music after powering on fm radio
  if (aFmRadioAudioEnabled) {
    uint32_t volIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC];
    SetStreamVolumeIndex(AUDIO_STREAM_FM, volIndex);
    mCurrentStreamVolumeTbl[AUDIO_STREAM_FM] = volIndex;
  }
#endif
  return NS_OK;
}

nsresult
AudioManager::ValidateVolumeIndex(uint32_t aCategory, uint32_t aIndex) const
{
  uint32_t maxIndex = GetMaxVolumeByCategory(aCategory);
  if (aIndex > maxIndex) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
AudioManager::SetVolumeByCategory(uint32_t aCategory, uint32_t aIndex)
{
  nsresult status;
  switch (static_cast<AudioVolumeCategories>(aCategory)) {
    case VOLUME_MEDIA:
      // AUDIO_STREAM_FM is not used on recent gonk.
      // AUDIO_STREAM_MUSIC is used for FM radio volume control.
#if ANDROID_VERSION < 19
      // sync FMRadio's volume with content channel.
      if (IsDeviceOn(AUDIO_DEVICE_OUT_FM)) {
        status = SetStreamVolumeIndex(AUDIO_STREAM_FM, aIndex);
        if (NS_WARN_IF(NS_FAILED(status))) {
          return status;
        }
      }
#endif
      status = SetStreamVolumeIndex(AUDIO_STREAM_MUSIC, aIndex);
      break;
    case VOLUME_NOTIFICATION:
      status = SetStreamVolumeIndex(AUDIO_STREAM_NOTIFICATION, aIndex);
      if (NS_WARN_IF(NS_FAILED(status))) {
        return status;
      }
      status = SetStreamVolumeIndex(AUDIO_STREAM_RING, aIndex);
      if (NS_WARN_IF(NS_FAILED(status))) {
        return status;
      }
      status = SetStreamVolumeIndex(AUDIO_STREAM_SYSTEM, aIndex);
      break;
    case VOLUME_ALARM:
      status = SetStreamVolumeIndex(AUDIO_STREAM_ALARM, aIndex);
      break;
    case VOLUME_TELEPHONY:
      status = SetStreamVolumeIndex(AUDIO_STREAM_VOICE_CALL, aIndex);
    case VOLUME_BLUETOOTH_SCO:
      status = SetStreamVolumeIndex(AUDIO_STREAM_BLUETOOTH_SCO, aIndex);
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return status;
}

uint32_t
AudioManager::GetVolumeByCategory(uint32_t aCategory) const
{
  switch (static_cast<AudioVolumeCategories>(aCategory)) {
    case VOLUME_MEDIA:
      return mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC];
    case VOLUME_NOTIFICATION:
      MOZ_ASSERT(mCurrentStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 mCurrentStreamVolumeTbl[AUDIO_STREAM_RING]);
      MOZ_ASSERT(mCurrentStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 mCurrentStreamVolumeTbl[AUDIO_STREAM_SYSTEM]);
      return mCurrentStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION];
    case VOLUME_ALARM:
      return mCurrentStreamVolumeTbl[AUDIO_STREAM_ALARM];
    case VOLUME_TELEPHONY:
      return mCurrentStreamVolumeTbl[AUDIO_STREAM_VOICE_CALL];
    case VOLUME_BLUETOOTH_SCO:
      return mCurrentStreamVolumeTbl[AUDIO_STREAM_BLUETOOTH_SCO];
    default:
      NS_WARNING("Can't get volume from error volume category.");
      return 0;
  }
}

uint32_t
AudioManager::GetMaxVolumeByCategory(uint32_t aCategory) const
{
  switch (static_cast<AudioVolumeCategories>(aCategory)) {
    case VOLUME_MEDIA:
      return sMaxStreamVolumeTbl[AUDIO_STREAM_MUSIC];
    case VOLUME_NOTIFICATION:
      MOZ_ASSERT(sMaxStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 sMaxStreamVolumeTbl[AUDIO_STREAM_RING]);
      MOZ_ASSERT(sMaxStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 sMaxStreamVolumeTbl[AUDIO_STREAM_SYSTEM]);
      return sMaxStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION];
    case VOLUME_ALARM:
      return sMaxStreamVolumeTbl[AUDIO_STREAM_ALARM];
    case VOLUME_TELEPHONY:
      return sMaxStreamVolumeTbl[AUDIO_STREAM_VOICE_CALL];
    case VOLUME_BLUETOOTH_SCO:
      return sMaxStreamVolumeTbl[AUDIO_STREAM_BLUETOOTH_SCO];
    default:
      NS_WARNING("Can't get max volume from error volume category.");
      return 0;
  }
}

NS_IMETHODIMP
AudioManager::SetAudioChannelVolume(uint32_t aChannel, uint32_t aIndex)
{
  nsresult status;
  AudioVolumeCategories category = (mPresentProfile == DEVICE_BLUETOOTH) ?
                                    VOLUME_BLUETOOTH_SCO : VOLUME_TELEPHONY;
  switch (static_cast<AudioChannel>(aChannel)) {
    case AudioChannel::Normal:
    case AudioChannel::Content:
      status = SetVolumeByCategory(VOLUME_MEDIA, aIndex);
      break;
    case AudioChannel::Notification:
    case AudioChannel::Ringer:
    case AudioChannel::Publicnotification:
    case AudioChannel::System:
      status = SetVolumeByCategory(VOLUME_NOTIFICATION, aIndex);
      break;
    case AudioChannel::Alarm:
      status = SetVolumeByCategory(VOLUME_ALARM, aIndex);
      break;
    case AudioChannel::Telephony:
      status = SetVolumeByCategory(category, aIndex);
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return status;
}

NS_IMETHODIMP
AudioManager::GetAudioChannelVolume(uint32_t aChannel, uint32_t* aIndex)
{
  if (!aIndex) {
    return NS_ERROR_NULL_POINTER;
  }
  AudioVolumeCategories category = (mPresentProfile == DEVICE_BLUETOOTH) ?
                                    VOLUME_BLUETOOTH_SCO : VOLUME_TELEPHONY;
  switch (static_cast<AudioChannel>(aChannel)) {
    case AudioChannel::Normal:
    case AudioChannel::Content:
      *aIndex = GetVolumeByCategory(VOLUME_MEDIA);
      break;
    case AudioChannel::Notification:
    case AudioChannel::Ringer:
    case AudioChannel::Publicnotification:
    case AudioChannel::System:
      *aIndex = GetVolumeByCategory(VOLUME_NOTIFICATION);
      break;
    case AudioChannel::Alarm:
      *aIndex = GetVolumeByCategory(VOLUME_ALARM);
      break;
    case AudioChannel::Telephony:
      *aIndex = GetVolumeByCategory(category);
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMaxAudioChannelVolume(uint32_t aChannel, uint32_t* aMaxIndex)
{
  if (!aMaxIndex) {
    return NS_ERROR_NULL_POINTER;
  }
  AudioVolumeCategories category = (mPresentProfile == DEVICE_BLUETOOTH) ?
                                    VOLUME_BLUETOOTH_SCO : VOLUME_TELEPHONY;
  switch (static_cast<AudioChannel>(aChannel)) {
    case AudioChannel::Normal:
    case AudioChannel::Content:
      *aMaxIndex = GetMaxVolumeByCategory(VOLUME_MEDIA);
      break;
    case AudioChannel::Notification:
    case AudioChannel::Ringer:
    case AudioChannel::Publicnotification:
    case AudioChannel::System:
      *aMaxIndex = GetMaxVolumeByCategory(VOLUME_NOTIFICATION);
      break;
    case AudioChannel::Alarm:
      *aMaxIndex = GetMaxVolumeByCategory(VOLUME_ALARM);
      break;
    case AudioChannel::Telephony:
      *aMaxIndex = GetMaxVolumeByCategory(category);
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

nsresult
AudioManager::SetStreamVolumeIndex(int32_t aStream, uint32_t aIndex) {
  if (aIndex > sMaxStreamVolumeTbl[aStream]) {
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
#if ANDROID_VERSION < 19
  if (aStream == AUDIO_STREAM_FM) {
    status = AudioSystem::setStreamVolumeIndex(
               static_cast<audio_stream_type_t>(aStream),
               aIndex,
               AUDIO_DEVICE_OUT_FM);
    return status ? NS_ERROR_FAILURE : NS_OK;
  }
#endif
  if (mPresentProfile == DEVICE_PRIMARY) {
    status = AudioSystem::setStreamVolumeIndex(
            static_cast<audio_stream_type_t>(aStream),
            aIndex,
            AUDIO_DEVICE_OUT_SPEAKER);
    status += AudioSystem::setStreamVolumeIndex(
            static_cast<audio_stream_type_t>(aStream),
            aIndex,
            AUDIO_DEVICE_OUT_EARPIECE);
  } else if (mPresentProfile == DEVICE_HEADSET) {
    status = AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_WIRED_HEADSET);
    status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_WIRED_HEADPHONE);
  } else if (mPresentProfile == DEVICE_BLUETOOTH) {
    status = AudioSystem::setStreamVolumeIndex(
             static_cast<audio_stream_type_t>(aStream),
             aIndex,
             AUDIO_DEVICE_OUT_BLUETOOTH_A2DP);
    status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET);
  } else {
    NS_WARNING("Can't set stream volume on error profile!");
  }
  return status ? NS_ERROR_FAILURE : NS_OK;
#endif
}

nsresult
AudioManager::GetStreamVolumeIndex(int32_t aStream, uint32_t *aIndex) {
  if (!aIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aStream <= AUDIO_STREAM_DEFAULT || aStream >= AUDIO_STREAM_MAX) {
    return NS_ERROR_INVALID_ARG;
  }

  *aIndex = mCurrentStreamVolumeTbl[aStream];

  return NS_OK;
}

AudioProfileData*
AudioManager::FindAudioProfileData(AudioOutputProfiles aProfile)
{
  uint32_t profilesNum = mAudioProfiles.Length();
  MOZ_ASSERT(profilesNum == DEVICE_TOTAL_NUMBER, "Error profile numbers!");
  for (uint32_t idx = 0; idx < profilesNum; ++idx) {
    if (mAudioProfiles[idx]->GetProfile() == aProfile) {
      return mAudioProfiles[idx];
    }
  }
  NS_WARNING("Can't find audio profile data");
  return nullptr;
}

nsAutoCString
AudioManager::AppendProfileToVolumeSetting(const char* aName, AudioOutputProfiles aProfile)
{
  nsAutoCString topic;
  topic.Assign(aName);
  for (uint32_t idx = 0; kAudioOutputProfilesTable[idx].tag; ++idx) {
    if (kAudioOutputProfilesTable[idx].value == aProfile) {
      topic.Append(".");
      topic.Append(kAudioOutputProfilesTable[idx].tag);
      break;
    }
  }
  return topic;
}

void
AudioManager::InitVolumeFromDatabase()
{
  nsresult rv;
  nsCOMPtr<nsISettingsService> service = do_GetService(SETTINGS_SERVICE, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsISettingsServiceLock> lock;
  rv = service->CreateLock(nullptr, getter_AddRefs(lock));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsRefPtr<VolumeInitCallback> callback = new VolumeInitCallback();
  MOZ_ASSERT(callback);
  callback->GetPromise()->Then(AbstractThread::MainThread(), __func__, this,
                               &AudioManager::InitProfileVolumeSucceeded,
                               &AudioManager::InitProfileVolumeFailed);

  for (uint32_t idx = 0; idx < VOLUME_TOTAL_NUMBER; ++idx) {
    for (uint32_t pIdx = 0; pIdx < DEVICE_TOTAL_NUMBER; ++pIdx) {
      lock->Get(AppendProfileToVolumeSetting(gVolumeData[idx].mChannelName,
                  static_cast<AudioOutputProfiles>(pIdx)).get(), callback);
    }
  }
}

void
AudioManager::InitProfileVolumeSucceeded()
{
  SendVolumeChangeNotification(FindAudioProfileData(mPresentProfile));
}

void
AudioManager::InitProfileVolumeFailed(const char* aError)
{
  NS_WARNING(aError);
}

void
AudioManager::SendVolumeChangeNotification(AudioProfileData* aProfileData)
{
  MOZ_ASSERT(aProfileData);
  nsresult rv;
  nsCOMPtr<nsISettingsService> service = do_GetService(SETTINGS_SERVICE, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsISettingsServiceLock> lock;
  rv = service->CreateLock(nullptr, getter_AddRefs(lock));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Send events to update the Gaia volume
  mozilla::AutoSafeJSContext cx;
  JS::Rooted<JS::Value> value(cx);
  for (uint32_t idx = 0; idx < VOLUME_TOTAL_NUMBER; ++idx) {
    value.setInt32(aProfileData->mVolumeTable[gVolumeData[idx].mCategory]);
    // For reducing the code dependency, Gaia doesn't need to know the current
    // profile, it only need to care about different volume categories.
    // However, we need to send the setting volume to the permanent database,
    // so that we can store the volume setting even if the phone reboots.
    lock->Set(gVolumeData[idx].mChannelName, value, nullptr, nullptr);
    lock->Set(AppendProfileToVolumeSetting(gVolumeData[idx].mChannelName,
               mPresentProfile).get(), value, nullptr, nullptr);
  }
}

void
AudioManager::CreateAudioProfilesData()
{
  MOZ_ASSERT(mAudioProfiles.IsEmpty(), "mAudioProfiles should be empty!");
  for (uint32_t idx = 0; idx < DEVICE_TOTAL_NUMBER; ++idx) {
    AudioProfileData* profile = new AudioProfileData(static_cast<AudioOutputProfiles>(idx));
    mAudioProfiles.AppendElement(profile);
  }
  UpdateProfileState(DEVICE_PRIMARY, true);
}

void
AudioManager::InitProfileVolume(AudioOutputProfiles aProfile,
                                 uint32_t aCategory,
                                 uint32_t aIndex)
{
  AudioProfileData* profileData = FindAudioProfileData(aProfile);
  MOZ_ASSERT(profileData);
  profileData->mVolumeTable[aCategory] = aIndex;
  SetVolumeByCategory(aCategory, aIndex);
}

void
AudioManager::SwitchProfileData(AudioOutputProfiles aProfile,
                                bool aActive)
{
  MOZ_ASSERT(DEVICE_PRIMARY <= aProfile &&
             aProfile < DEVICE_TOTAL_NUMBER, "Error profile type!");

  // Save the present profile volume data.
  AudioOutputProfiles oldProfile = mPresentProfile;
  AudioProfileData* profileData = FindAudioProfileData(oldProfile);
  MOZ_ASSERT(profileData);
  UpdateVolumeToProfile(profileData);
  UpdateProfileState(aProfile, aActive);

  AudioOutputProfiles newProfile = mPresentProfile;
  if (oldProfile == newProfile) {
    return;
  }

  // Update new profile volume data and send the changing event.
  profileData = FindAudioProfileData(newProfile);
  MOZ_ASSERT(profileData);
  UpdateVolumeFromProfile(profileData);
  SendVolumeChangeNotification(profileData);
}

void
AudioManager::UpdateProfileState(AudioOutputProfiles aProfile, bool aActive)
{
  MOZ_ASSERT(DEVICE_PRIMARY <= aProfile && aProfile < DEVICE_TOTAL_NUMBER,
             "Error profile type!");
  if (aProfile == DEVICE_PRIMARY && !aActive) {
    NS_WARNING("Can't turn off the primary profile!");
    return;
  }

  mAudioProfiles[aProfile]->SetActive(aActive);
  if (aActive) {
    mPresentProfile = aProfile;
    return;
  }

  // The primary profile has the lowest priority. We will check whether there
  // are other profiles. The bluetooth and headset have the same priotity.
  uint32_t profilesNum = mAudioProfiles.Length();
  MOZ_ASSERT(profilesNum == DEVICE_TOTAL_NUMBER, "Error profile numbers!");
  for (int32_t idx = profilesNum - 1; idx >= 0; --idx) {
    if (mAudioProfiles[idx]->GetActive()) {
      mPresentProfile = static_cast<AudioOutputProfiles>(idx);
      break;
    }
  }
}

void
AudioManager::UpdateVolumeToProfile(AudioProfileData* aProfileData)
{
  MOZ_ASSERT(aProfileData);
  for (uint32_t idx = 0; idx < VOLUME_TOTAL_NUMBER; ++idx) {
    uint32_t volume = GetVolumeByCategory(gVolumeData[idx].mCategory);
    aProfileData->mVolumeTable[gVolumeData[idx].mCategory] = volume;
  }
}

void
AudioManager::UpdateVolumeFromProfile(AudioProfileData* aProfileData)
{
  MOZ_ASSERT(aProfileData);
  for (uint32_t idx = 0; idx < VOLUME_TOTAL_NUMBER; ++idx) {
    SetVolumeByCategory(gVolumeData[idx].mCategory,
                        aProfileData->mVolumeTable[gVolumeData[idx].mCategory]);
  }
}

} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */
