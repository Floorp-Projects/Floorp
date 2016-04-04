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

#ifndef mozilla_dom_system_b2g_audiomanager_h__
#define mozilla_dom_system_b2g_audiomanager_h__

#include "mozilla/HalTypes.h"
#include "mozilla/Observer.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsIAudioManager.h"
#include "nsIObserver.h"
#include "android_audio/AudioSystem.h"

// {b2b51423-502d-4d77-89b3-7786b562b084}
#define NS_AUDIOMANAGER_CID {0x94f6fd70, 0x7615, 0x4af9, \
      {0x89, 0x10, 0xf9, 0x3c, 0x55, 0xe6, 0x62, 0xec}}
#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"

class nsISettingsServiceLock;

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
} // namespace hal

namespace dom {
namespace gonk {

class VolumeInitCallback;

class AudioManager final : public nsIAudioManager
                         , public nsIObserver
{
public:
  static already_AddRefed<AudioManager> GetInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIOMANAGER
  NS_DECL_NSIOBSERVER

  // Validate whether the volume index is within the range
  nsresult ValidateVolumeIndex(int32_t aStream, uint32_t aIndex) const;

  // Called when android AudioFlinger in mediaserver is died
  void HandleAudioFlingerDied();

  void HandleHeadphoneSwitchEvent(const hal::SwitchEvent& aEvent);

  class VolumeStreamState {
  public:
    explicit VolumeStreamState(AudioManager& aManager, int32_t aStreamType);
    int32_t GetStreamType()
    {
      return mStreamType;
    }
    bool IsDevicesChanged(bool aFromCache = true);
    void ClearDevicesChanged();
    uint32_t GetLastDevices()
    {
      return mLastDevices;
    }
    bool IsVolumeIndexesChanged();
    void ClearVolumeIndexesChanged();
    void InitStreamVolume();
    uint32_t GetMaxIndex();
    uint32_t GetDefaultIndex();
    uint32_t GetVolumeIndex();
    uint32_t GetVolumeIndex(uint32_t aDevice);
    void ClearCurrentVolumeUpdated();
    // Set volume index to all active devices.
    // Active devices are chosen by android AudioPolicyManager.
    nsresult SetVolumeIndexToActiveDevices(uint32_t aIndex);
    // Set volume index to all alias streams for device. Alias streams have same volume.
    nsresult SetVolumeIndexToAliasStreams(uint32_t aIndex, uint32_t aDevice);
    nsresult SetVolumeIndexToConsistentDeviceIfNeeded(uint32_t aIndex, uint32_t aDevice);
    nsresult SetVolumeIndex(uint32_t aIndex, uint32_t aDevice, bool aUpdateCache = true);
    // Restore volume index to all devices. Called when AudioFlinger is restarted.
    void RestoreVolumeIndexToAllDevices();
  private:
    AudioManager& mManager;
    const int32_t mStreamType;
    uint32_t mLastDevices;
    bool mIsDevicesChanged;
    bool mIsVolumeIndexesChanged;
    nsDataHashtable<nsUint32HashKey, uint32_t> mVolumeIndexes;
  };

protected:
  int32_t mPhoneState;

  bool mIsVolumeInited;

  // A bitwise variable for volume update of audio output devices,
  // clear it after store the value into database.
  uint32_t mAudioOutDevicesUpdated;

  // Connected devices that are controlled by setDeviceConnectionState()
  nsDataHashtable<nsUint32HashKey, nsCString> mConnectedDevices;

  nsDataHashtable<nsUint32HashKey, uint32_t> mAudioDeviceTableIdMaps;

  bool mSwitchDone;

#if defined(MOZ_B2G_BT) || ANDROID_VERSION >= 17
  bool mBluetoothA2dpEnabled;
#endif
#ifdef MOZ_B2G_BT
  bool mA2dpSwitchDone;
#endif
  nsTArray<UniquePtr<VolumeStreamState> > mStreamStates;
  uint32_t mLastChannelVolume[AUDIO_STREAM_CNT];

  bool IsFmOutConnected();

  nsresult SetStreamVolumeForDevice(int32_t aStream,
                                    uint32_t aIndex,
                                    uint32_t aDevice);
  nsresult SetStreamVolumeIndex(int32_t aStream, uint32_t aIndex);
  nsresult GetStreamVolumeIndex(int32_t aStream, uint32_t* aIndex);

  void UpdateCachedActiveDevicesForStreams();
  uint32_t GetDevicesForStream(int32_t aStream, bool aFromCache = true);
  uint32_t GetDeviceForStream(int32_t aStream);
  // Choose one device as representative of active devices.
  static uint32_t SelectDeviceFromDevices(uint32_t aOutDevices);

private:
  nsAutoPtr<mozilla::hal::SwitchObserver> mObserver;
#ifdef MOZ_B2G_RIL
  bool                                    mMuteCallToRIL;
  // mIsMicMuted is only used for toggling mute call to RIL.
  bool                                    mIsMicMuted;
#endif

  void HandleBluetoothStatusChanged(nsISupports* aSubject,
                                    const char* aTopic,
                                    const nsCString aAddress);
  void HandleAudioChannelProcessChanged();

  // Append the audio output device to the volume setting string.
  nsAutoCString AppendDeviceToVolumeSetting(const char* aName,
                                            uint32_t aDevice);

  // We store the volume setting in the database, these are related functions.
  void InitVolumeFromDatabase();
  void MaybeUpdateVolumeSettingToDatabase(bool aForce = false);

  // Promise functions.
  void InitDeviceVolumeSucceeded();
  void InitDeviceVolumeFailed(const char* aError);

  void AudioOutDeviceUpdated(uint32_t aDevice);

  void UpdateHeadsetConnectionState(hal::SwitchState aState);
  void UpdateDeviceConnectionState(bool aIsConnected, uint32_t aDevice, const nsCString& aDeviceName);
  void SetAllDeviceConnectionStates();

  AudioManager();
  ~AudioManager();

  friend class VolumeInitCallback;
  friend class VolumeStreamState;
  friend class GonkAudioPortCallback;
};

} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */

#endif // mozilla_dom_system_b2g_audiomanager_h__
