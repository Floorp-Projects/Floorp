/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigator_h
#define mozilla_dom_Navigator_h

#include "nsIDOMNavigator.h"
#include "nsIDOMNavigatorGeolocation.h"
#include "nsIDOMNavigatorDeviceStorage.h"
#include "nsIDOMNavigatorDesktopNotification.h"
#include "nsIDOMClientInformation.h"
#include "nsINavigatorBattery.h"
#include "nsIDOMNavigatorSms.h"
#include "nsIDOMNavigatorMobileMessage.h"
#include "nsIDOMNavigatorNetwork.h"
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
#include "nsINavigatorAudioChannelManager.h"
#endif
#ifdef MOZ_B2G_RIL
#include "nsINavigatorMobileConnection.h"
#include "nsINavigatorCellBroadcast.h"
#include "nsINavigatorVoicemail.h"
#include "nsINavigatorIccManager.h"
#endif
#include "nsAutoPtr.h"
#include "nsIDOMNavigatorTime.h"
#include "nsWeakReference.h"
#include "DeviceStorage.h"
#include "nsWrapperCache.h"

class nsPluginArray;
class nsMimeTypeArray;
class nsPIDOMWindow;
class nsIDOMMozConnection;

namespace mozilla {
namespace dom {
class Geolocation;
}
}

#ifdef MOZ_MEDIA_NAVIGATOR
#include "nsIDOMNavigatorUserMedia.h"
#endif

#ifdef MOZ_B2G_RIL
#include "nsIDOMNavigatorTelephony.h"
class nsIDOMTelephony;
#endif

#ifdef MOZ_B2G_BT
#include "nsIDOMNavigatorBluetooth.h"
#endif

#include "nsIDOMNavigatorSystemMessages.h"

#include "nsIDOMNavigatorCamera.h"
#include "DOMCameraManager.h"

#ifdef MOZ_GAMEPAD
#include "nsINavigatorGamepads.h"
#endif

//*****************************************************************************
// Navigator: Script "navigator" object
//*****************************************************************************

namespace mozilla {
namespace dom {

namespace battery {
class BatteryManager;
} // namespace battery

class DesktopNotificationCenter;
class SmsManager;
class MobileMessageManager;

namespace icc {
#ifdef MOZ_B2G_RIL
class IccManager;
#endif
}

namespace network {
class Connection;
#ifdef MOZ_B2G_RIL
class MobileConnection;
#endif
} // namespace Connection;

namespace power {
class PowerManager;
} // namespace power

namespace time {
class TimeManager;
} // namespace time

namespace system {
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
class AudioChannelManager;
#endif
} // namespace system

class Navigator : public nsIDOMNavigator
                , public nsIDOMClientInformation
                , public nsIDOMNavigatorDeviceStorage
                , public nsIDOMNavigatorGeolocation
                , public nsIDOMNavigatorDesktopNotification
                , public nsINavigatorBattery
                , public nsIDOMMozNavigatorSms
                , public nsIDOMMozNavigatorMobileMessage
#ifdef MOZ_MEDIA_NAVIGATOR
                , public nsINavigatorUserMedia
                , public nsIDOMNavigatorUserMedia
#endif
#ifdef MOZ_B2G_RIL
                , public nsIDOMNavigatorTelephony
#endif
#ifdef MOZ_GAMEPAD
                , public nsINavigatorGamepads
#endif
                , public nsIDOMMozNavigatorNetwork
#ifdef MOZ_B2G_RIL
                , public nsIMozNavigatorMobileConnection
                , public nsIMozNavigatorCellBroadcast
                , public nsIMozNavigatorVoicemail
                , public nsIMozNavigatorIccManager
#endif
#ifdef MOZ_B2G_BT
                , public nsIDOMNavigatorBluetooth
#endif
                , public nsIDOMNavigatorCamera
                , public nsIDOMNavigatorSystemMessages
#ifdef MOZ_TIME_MANAGER
                , public nsIDOMMozNavigatorTime
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
                , public nsIMozNavigatorAudioChannelManager
#endif
                , public nsWrapperCache
{
public:
  Navigator(nsPIDOMWindow *aInnerWindow);
  virtual ~Navigator();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Navigator,
                                                         nsIDOMNavigator)
  NS_DECL_NSIDOMNAVIGATOR
  NS_DECL_NSIDOMCLIENTINFORMATION
  NS_DECL_NSIDOMNAVIGATORDEVICESTORAGE
  NS_DECL_NSIDOMNAVIGATORGEOLOCATION
  NS_DECL_NSIDOMNAVIGATORDESKTOPNOTIFICATION
  NS_DECL_NSINAVIGATORBATTERY
  NS_DECL_NSIDOMMOZNAVIGATORSMS
  NS_DECL_NSIDOMMOZNAVIGATORMOBILEMESSAGE
#ifdef MOZ_MEDIA_NAVIGATOR
  NS_DECL_NSINAVIGATORUSERMEDIA
  NS_DECL_NSIDOMNAVIGATORUSERMEDIA
#endif
#ifdef MOZ_B2G_RIL
  NS_DECL_NSIDOMNAVIGATORTELEPHONY
#endif
#ifdef MOZ_GAMEPAD
  NS_DECL_NSINAVIGATORGAMEPADS
#endif
  NS_DECL_NSIDOMMOZNAVIGATORNETWORK
#ifdef MOZ_B2G_RIL
  NS_DECL_NSIMOZNAVIGATORMOBILECONNECTION
  NS_DECL_NSIMOZNAVIGATORCELLBROADCAST
  NS_DECL_NSIMOZNAVIGATORVOICEMAIL
  NS_DECL_NSIMOZNAVIGATORICCMANAGER
#endif

#ifdef MOZ_B2G_BT
  NS_DECL_NSIDOMNAVIGATORBLUETOOTH
#endif
  NS_DECL_NSIDOMNAVIGATORSYSTEMMESSAGES
#ifdef MOZ_TIME_MANAGER
  NS_DECL_NSIDOMMOZNAVIGATORTIME
#endif

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  NS_DECL_NSIMOZNAVIGATORAUDIOCHANNELMANAGER
#endif
  static void Init();

  void Invalidate();
  nsPIDOMWindow *GetWindow()
  {
    return mWindow;
  }

  void RefreshMIMEArray();

  static bool HasDesktopNotificationSupport();

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  /**
   * For use during document.write where our inner window changes.
   */
  void SetWindow(nsPIDOMWindow *aInnerWindow);

  /**
   * Called when the inner window navigates to a new page.
   */
  void OnNavigation();

  // Helper to initialize mMessagesManager.
  nsresult EnsureMessagesManager();

  NS_DECL_NSIDOMNAVIGATORCAMERA

private:
  bool CheckPermission(const char* type);

  nsRefPtr<nsMimeTypeArray> mMimeTypes;
  nsRefPtr<nsPluginArray> mPlugins;
  nsRefPtr<Geolocation> mGeolocation;
  nsRefPtr<DesktopNotificationCenter> mNotification;
  nsRefPtr<battery::BatteryManager> mBatteryManager;
  nsRefPtr<power::PowerManager> mPowerManager;
  nsRefPtr<SmsManager> mSmsManager;
  nsRefPtr<MobileMessageManager> mMobileMessageManager;
#ifdef MOZ_B2G_RIL
  nsCOMPtr<nsIDOMTelephony> mTelephony;
  nsCOMPtr<nsIDOMMozVoicemail> mVoicemail;
#endif
  nsRefPtr<network::Connection> mConnection;
#ifdef MOZ_B2G_RIL
  nsRefPtr<network::MobileConnection> mMobileConnection;
  nsCOMPtr<nsIDOMMozCellBroadcast> mCellBroadcast;
  nsRefPtr<icc::IccManager> mIccManager;
#endif
#ifdef MOZ_B2G_BT
  nsCOMPtr<nsIDOMBluetoothManager> mBluetooth;
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  nsRefPtr<system::AudioChannelManager> mAudioChannelManager;
#endif
  nsRefPtr<nsDOMCameraManager> mCameraManager;
  nsCOMPtr<nsIDOMNavigatorSystemMessages> mMessagesManager;
  nsTArray<nsRefPtr<nsDOMDeviceStorage> > mDeviceStorageStores;
  nsRefPtr<time::TimeManager> mTimeManager;
  nsCOMPtr<nsPIDOMWindow> mWindow;
};

} // namespace dom
} // namespace mozilla

nsresult NS_GetNavigatorUserAgent(nsAString& aUserAgent);
nsresult NS_GetNavigatorPlatform(nsAString& aPlatform);
nsresult NS_GetNavigatorAppVersion(nsAString& aAppVersion);
nsresult NS_GetNavigatorAppName(nsAString& aAppName);

#endif // mozilla_dom_Navigator_h
