/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigator_h
#define mozilla_dom_Navigator_h

#include "mozilla/MemoryReporting.h"
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
class systemMessageCallback;
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

void NS_GetNavigatorAppName(nsAString& aAppName);

namespace mozilla {
namespace dom {

namespace battery {
class BatteryManager;
} // namespace battery

class DesktopNotificationCenter;
class SmsManager;
class MobileMessageManager;
class MozIdleObserver;
#ifdef MOZ_GAMEPAD
class Gamepad;
#endif // MOZ_GAMEPAD
#ifdef MOZ_MEDIA_NAVIGATOR
class MozDOMGetUserMediaSuccessCallback;
class MozDOMGetUserMediaErrorCallback;
class MozGetUserMediaDevicesSuccessCallback;
#endif // MOZ_MEDIA_NAVIGATOR

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
  nsPIDOMWindow *GetWindow() const
  {
    return mWindow;
  }

  void RefreshMIMEArray();

  static bool HasDesktopNotificationSupport();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

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

  // WebIDL API
  void GetAppName(nsString& aAppName)
  {
    NS_GetNavigatorAppName(aAppName);
  }
  void GetAppVersion(nsString& aAppVersion, ErrorResult& aRv)
  {
    aRv = GetAppVersion(aAppVersion);
  }
  void GetPlatform(nsString& aPlatform, ErrorResult& aRv)
  {
    aRv = GetPlatform(aPlatform);
  }
  void GetUserAgent(nsString& aUserAgent, ErrorResult& aRv)
  {
    aRv = GetUserAgent(aUserAgent);
  }
  // The XPCOM GetProduct is OK
  // The XPCOM GetLanguage is OK
  bool OnLine();
  void RegisterProtocolHandler(const nsAString& aScheme, const nsAString& aURL,
                               const nsAString& aTitle, ErrorResult& rv)
  {
    rv = RegisterProtocolHandler(aScheme, aURL, aTitle);
  }
  void RegisterContentHandler(const nsAString& aMIMEType, const nsAString& aURL,
                              const nsAString& aTitle, ErrorResult& rv)
  {
    rv = RegisterContentHandler(aMIMEType, aURL, aTitle);
  }
  nsMimeTypeArray* GetMimeTypes(ErrorResult& aRv);
  nsPluginArray* GetPlugins(ErrorResult& aRv);
  // The XPCOM GetDoNotTrack is ok
  Geolocation* GetGeolocation(ErrorResult& aRv);
  battery::BatteryManager* GetBattery(ErrorResult& aRv);
  void Vibrate(uint32_t aDuration, ErrorResult& aRv);
  void Vibrate(const nsTArray<uint32_t>& aDuration, ErrorResult& aRv);
  void GetAppCodeName(nsString& aAppCodeName, ErrorResult& aRv)
  {
    aRv = GetAppCodeName(aAppCodeName);
  }
  void GetOscpu(nsString& aOscpu, ErrorResult& aRv)
  {
    aRv = GetOscpu(aOscpu);
  }
  // The XPCOM GetVendor is OK
  // The XPCOM GetVendorSub is OK
  // The XPCOM GetProductSub is OK
  bool CookieEnabled();
  void GetBuildID(nsString& aBuildID, ErrorResult& aRv)
  {
    aRv = GetBuildID(aBuildID);
  }
  nsIDOMMozPowerManager* GetMozPower(ErrorResult& aRv);
  bool JavaEnabled(ErrorResult& aRv);
  bool TaintEnabled()
  {
    return false;
  }
  void AddIdleObserver(MozIdleObserver& aObserver, ErrorResult& aRv);
  void RemoveIdleObserver(MozIdleObserver& aObserver, ErrorResult& aRv);
  already_AddRefed<nsIDOMMozWakeLock> RequestWakeLock(const nsAString &aTopic,
                                                      ErrorResult& aRv);
  nsDOMDeviceStorage* GetDeviceStorage(const nsAString& aType,
                                       ErrorResult& aRv);
  void GetDeviceStorages(const nsAString& aType,
                         nsTArray<nsRefPtr<nsDOMDeviceStorage> >& aStores,
                         ErrorResult& aRv);
  DesktopNotificationCenter* GetMozNotification(ErrorResult& aRv);
  bool MozIsLocallyAvailable(const nsAString& aURI, bool aWhenOffline,
                             ErrorResult& aRv)
  {
    bool available = false;
    aRv = MozIsLocallyAvailable(aURI, aWhenOffline, &available);
    return available;
  }
  nsIDOMMozSmsManager* GetMozSms();
  nsIDOMMozMobileMessageManager* GetMozMobileMessage();
  nsIDOMMozConnection* GetMozConnection();
  nsDOMCameraManager* GetMozCameras(ErrorResult& aRv);
  void MozSetMessageHandler(const nsAString& aType,
                            systemMessageCallback* aCallback,
                            ErrorResult& aRv);
  bool MozHasPendingMessage(const nsAString& aType, ErrorResult& aRv);
#ifdef MOZ_B2G_RIL
  nsIDOMTelephony* GetMozTelephony(ErrorResult& aRv);
  nsIDOMMozMobileConnection* GetMozMobileConnection(ErrorResult& aRv);
  nsIDOMMozCellBroadcast* GetMozCellBroadcast(ErrorResult& aRv);
  nsIDOMMozVoicemail* GetMozVoicemail(ErrorResult& aRv);
  nsIDOMMozIccManager* GetMozIccManager(ErrorResult& aRv);
#endif // MOZ_B2G_RIL
#ifdef MOZ_GAMEPAD
  void GetGamepads(nsTArray<nsRefPtr<Gamepad> >& aGamepads, ErrorResult& aRv);
#endif // MOZ_GAMEPAD
#ifdef MOZ_B2G_BT
  nsIDOMBluetoothManager* GetMozBluetooth(ErrorResult& aRv);
#endif // MOZ_B2G_BT
#ifdef MOZ_TIME_MANAGER
  time::TimeManager* GetMozTime(ErrorResult& aRv);
#endif // MOZ_TIME_MANAGER
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  system::AudioChannelManager* GetMozAudioChannelManager(ErrorResult& aRv);
#endif // MOZ_AUDIO_CHANNEL_MANAGER
#ifdef MOZ_MEDIA_NAVIGATOR
  void MozGetUserMedia(nsIMediaStreamOptions* aParams,
                       MozDOMGetUserMediaSuccessCallback* aOnSuccess,
                       MozDOMGetUserMediaErrorCallback* aOnError,
                       ErrorResult& aRv);
  void MozGetUserMedia(nsIMediaStreamOptions* aParams,
                       nsIDOMGetUserMediaSuccessCallback* aOnSuccess,
                       nsIDOMGetUserMediaErrorCallback* aOnError,
                       ErrorResult& aRv);
  void MozGetUserMediaDevices(MozGetUserMediaDevicesSuccessCallback* aOnSuccess,
                              MozDOMGetUserMediaErrorCallback* aOnError,
                              ErrorResult& aRv);
  void MozGetUserMediaDevices(nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
                              nsIDOMGetUserMediaErrorCallback* aOnError,
                              ErrorResult& aRv);
#endif // MOZ_MEDIA_NAVIGATOR
  bool DoNewResolve(JSContext* aCx, JS::Handle<JSObject*> aObject,
                    JS::Handle<jsid> aId, unsigned aFlags,
                    JS::MutableHandle<JSObject*> aObjp);

  // WebIDL helper methods
  static bool HasBatterySupport(JSContext* /* unused*/, JSObject* /*unused */);
  static bool HasPowerSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasIdleSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasWakeLockSupport(JSContext* /* unused*/, JSObject* /*unused */);
  static bool HasDesktopNotificationSupport(JSContext* /* unused*/,
                                            JSObject* /*unused */)
  {
    return HasDesktopNotificationSupport();
  }
  static bool HasSmsSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasMobileMessageSupport(JSContext* /* unused */,
                                      JSObject* aGlobal);
  static bool HasCameraSupport(JSContext* /* unused */,
                               JSObject* aGlobal);
#ifdef MOZ_B2G_RIL
  static bool HasTelephonySupport(JSContext* /* unused */,
                                  JSObject* aGlobal);
  static bool HasMobileConnectionSupport(JSContext* /* unused */,
                                         JSObject* aGlobal);
  static bool HasCellBroadcastSupport(JSContext* /* unused */,
                                      JSObject* aGlobal);
  static bool HasVoicemailSupport(JSContext* /* unused */,
                                  JSObject* aGlobal);
  static bool HasIccManagerSupport(JSContext* /* unused */,
                                   JSObject* aGlobal);
#endif // MOZ_B2G_RIL
#ifdef MOZ_B2G_BT
  static bool HasBluetoothSupport(JSContext* /* unused */, JSObject* aGlobal);
#endif // MOZ_B2G_BT
#ifdef MOZ_TIME_MANAGER
  static bool HasTimeSupport(JSContext* /* unused */, JSObject* aGlobal);
#endif // MOZ_TIME_MANAGER
#ifdef MOZ_MEDIA_NAVIGATOR
  static bool HasUserMediaSupport(JSContext* /* unused */,
                                  JSObject* /* unused */);
#endif // MOZ_MEDIA_NAVIGATOR

  nsPIDOMWindow* GetParentObject() const
  {
    return GetWindow();
  }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

private:
  bool CheckPermission(const char* type);
  static bool CheckPermission(nsPIDOMWindow* aWindow, const char* aType);
  static bool HasMobileMessageSupport(nsPIDOMWindow* aWindow);
  // GetWindowFromGlobal returns the inner window for this global, if
  // any, else null.
  static already_AddRefed<nsPIDOMWindow> GetWindowFromGlobal(JSObject* aGlobal);

  // Methods to common up the XPCOM and WebIDL implementations of
  // Add/RemoveIdleObserver.
  void AddIdleObserver(nsIIdleObserver& aIdleObserver);
  void RemoveIdleObserver(nsIIdleObserver& aIdleObserver);

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

#endif // mozilla_dom_Navigator_h
