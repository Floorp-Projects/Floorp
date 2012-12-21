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
#include "nsIDOMNavigatorNetwork.h"
#include "nsIObserver.h"
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
#include "nsINavigatorAudioChannelManager.h"
#endif
#ifdef MOZ_B2G_RIL
#include "nsINavigatorMobileConnection.h"
#include "nsINavigatorCellBroadcast.h"
#endif
#include "nsAutoPtr.h"
#include "nsIDOMNavigatorTime.h"
#include "nsWeakReference.h"
#include "DeviceStorage.h"

class nsPluginArray;
class nsMimeTypeArray;
class nsGeolocation;
class nsDesktopNotificationCenter;
class nsPIDOMWindow;
class nsIDOMMozConnection;

#ifdef MOZ_MEDIA_NAVIGATOR
#include "nsIDOMNavigatorUserMedia.h"
#endif

#ifdef MOZ_B2G_RIL
#include "nsIDOMNavigatorTelephony.h"
class nsIDOMTelephony;
class nsIDOMMozVoicemail;
#endif

#ifdef MOZ_B2G_BT
#include "nsIDOMNavigatorBluetooth.h"
#endif

#include "nsIDOMNavigatorSystemMessages.h"

#include "nsIDOMNavigatorCamera.h"
#include "DOMCameraManager.h"

//*****************************************************************************
// Navigator: Script "navigator" object
//*****************************************************************************

namespace mozilla {
namespace dom {

namespace battery {
class BatteryManager;
} // namespace battery

namespace sms {
class SmsManager;
} // namespace sms

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
                , public nsIObserver
#ifdef MOZ_MEDIA_NAVIGATOR
                , public nsINavigatorUserMedia
                , public nsIDOMNavigatorUserMedia
#endif
#ifdef MOZ_B2G_RIL
                , public nsIDOMNavigatorTelephony
#endif
                , public nsIDOMMozNavigatorNetwork
#ifdef MOZ_B2G_RIL
                , public nsIMozNavigatorMobileConnection
                , public nsIMozNavigatorCellBroadcast
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
{
public:
  Navigator(nsPIDOMWindow *aInnerWindow);
  virtual ~Navigator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMNAVIGATOR
  NS_DECL_NSIDOMCLIENTINFORMATION
  NS_DECL_NSIDOMNAVIGATORDEVICESTORAGE
  NS_DECL_NSIDOMNAVIGATORGEOLOCATION
  NS_DECL_NSIDOMNAVIGATORDESKTOPNOTIFICATION
  NS_DECL_NSINAVIGATORBATTERY
  NS_DECL_NSIDOMMOZNAVIGATORSMS
  NS_DECL_NSIOBSERVER
#ifdef MOZ_MEDIA_NAVIGATOR
  NS_DECL_NSINAVIGATORUSERMEDIA
  NS_DECL_NSIDOMNAVIGATORUSERMEDIA
#endif
#ifdef MOZ_B2G_RIL
  NS_DECL_NSIDOMNAVIGATORTELEPHONY
#endif
  NS_DECL_NSIDOMMOZNAVIGATORNETWORK
#ifdef MOZ_B2G_RIL
  NS_DECL_NSIMOZNAVIGATORMOBILECONNECTION
  NS_DECL_NSIMOZNAVIGATORCELLBROADCAST
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
  nsPIDOMWindow *GetWindow();

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

#ifdef MOZ_SYS_MSG
  // Helper to initialize mMessagesManager.
  nsresult EnsureMessagesManager();
#endif
  NS_DECL_NSIDOMNAVIGATORCAMERA

private:
  bool CheckPermission(const char* type);

  nsRefPtr<nsMimeTypeArray> mMimeTypes;
  nsRefPtr<nsPluginArray> mPlugins;
  nsRefPtr<nsGeolocation> mGeolocation;
  nsRefPtr<nsDesktopNotificationCenter> mNotification;
  nsRefPtr<battery::BatteryManager> mBatteryManager;
  nsRefPtr<power::PowerManager> mPowerManager;
  nsRefPtr<sms::SmsManager> mSmsManager;
#ifdef MOZ_B2G_RIL
  nsCOMPtr<nsIDOMTelephony> mTelephony;
  nsCOMPtr<nsIDOMMozVoicemail> mVoicemail;
#endif
  nsRefPtr<network::Connection> mConnection;
#ifdef MOZ_B2G_RIL
  nsRefPtr<network::MobileConnection> mMobileConnection;
  nsCOMPtr<nsIDOMMozCellBroadcast> mCellBroadcast;
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
  nsWeakPtr mWindow;
};

} // namespace dom
} // namespace mozilla

nsresult NS_GetNavigatorUserAgent(nsAString& aUserAgent);
nsresult NS_GetNavigatorPlatform(nsAString& aPlatform);
nsresult NS_GetNavigatorAppVersion(nsAString& aAppVersion);
nsresult NS_GetNavigatorAppName(nsAString& aAppName);

#endif // mozilla_dom_Navigator_h
