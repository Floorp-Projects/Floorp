/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigator_h
#define mozilla_dom_Navigator_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/ErrorResult.h"
#include "nsIDOMNavigator.h"
#include "nsIMozNavigatorNetwork.h"
#include "nsAutoPtr.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "nsTArray.h"

class nsPluginArray;
class nsMimeTypeArray;
class nsPIDOMWindow;
class nsIDOMMozConnection;
class nsIDOMMozMobileMessageManager;
class nsIDOMNavigatorSystemMessages;
class nsDOMCameraManager;
class nsDOMDeviceStorage;

namespace mozilla {
namespace dom {
class Geolocation;
class systemMessageCallback;
class MediaStreamConstraints;
class MediaStreamConstraintsInternal;
}
}

#ifdef MOZ_B2G_RIL
class nsIDOMMozMobileConnection;
class nsIDOMMozIccManager;
#endif // MOZ_B2G_RIL

//*****************************************************************************
// Navigator: Script "navigator" object
//*****************************************************************************

void NS_GetNavigatorAppName(nsAString& aAppName);

namespace mozilla {
namespace dom {

namespace battery {
class BatteryManager;
} // namespace battery

#ifdef MOZ_B2G_FM
class FMRadio;
#endif

class Promise;

class DesktopNotificationCenter;
class MobileMessageManager;
class MozIdleObserver;
#ifdef MOZ_GAMEPAD
class Gamepad;
#endif // MOZ_GAMEPAD
#ifdef MOZ_MEDIA_NAVIGATOR
class NavigatorUserMediaSuccessCallback;
class NavigatorUserMediaErrorCallback;
class MozGetUserMediaDevicesSuccessCallback;
#endif // MOZ_MEDIA_NAVIGATOR

namespace network {
class Connection;
#ifdef MOZ_B2G_RIL
class MobileConnection;
#endif
} // namespace Connection;

#ifdef MOZ_B2G_BT
namespace bluetooth {
class BluetoothManager;
} // namespace bluetooth
#endif // MOZ_B2G_BT

#ifdef MOZ_B2G_RIL
class CellBroadcast;
class IccManager;
class Telephony;
class Voicemail;
#endif

class PowerManager;

namespace time {
class TimeManager;
} // namespace time

namespace system {
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
class AudioChannelManager;
#endif
} // namespace system

class Navigator : public nsIDOMNavigator
                , public nsIMozNavigatorNetwork
                , public nsWrapperCache
{
public:
  Navigator(nsPIDOMWindow *aInnerWindow);
  virtual ~Navigator();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Navigator,
                                                         nsIDOMNavigator)
  NS_DECL_NSIDOMNAVIGATOR
  NS_DECL_NSIMOZNAVIGATORNETWORK

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
                               const nsAString& aTitle, ErrorResult& aRv);
  void RegisterContentHandler(const nsAString& aMIMEType, const nsAString& aURL,
                              const nsAString& aTitle, ErrorResult& aRv);
  nsMimeTypeArray* GetMimeTypes(ErrorResult& aRv);
  nsPluginArray* GetPlugins(ErrorResult& aRv);
  // The XPCOM GetDoNotTrack is ok
  Geolocation* GetGeolocation(ErrorResult& aRv);
  battery::BatteryManager* GetBattery(ErrorResult& aRv);
  already_AddRefed<Promise> GetDataStores(const nsAString &aName,
                                          ErrorResult& aRv);
  bool Vibrate(uint32_t aDuration);
  bool Vibrate(const nsTArray<uint32_t>& aDuration);
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
  PowerManager* GetMozPower(ErrorResult& aRv);
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
                             ErrorResult& aRv);
  nsIDOMMozMobileMessageManager* GetMozMobileMessage();
  nsIDOMMozConnection* GetMozConnection();
  nsDOMCameraManager* GetMozCameras(ErrorResult& aRv);
  void MozSetMessageHandler(const nsAString& aType,
                            systemMessageCallback* aCallback,
                            ErrorResult& aRv);
  bool MozHasPendingMessage(const nsAString& aType, ErrorResult& aRv);
#ifdef MOZ_B2G_RIL
  Telephony* GetMozTelephony(ErrorResult& aRv);
  nsIDOMMozMobileConnection* GetMozMobileConnection(ErrorResult& aRv);
  CellBroadcast* GetMozCellBroadcast(ErrorResult& aRv);
  Voicemail* GetMozVoicemail(ErrorResult& aRv);
  nsIDOMMozIccManager* GetMozIccManager(ErrorResult& aRv);
#endif // MOZ_B2G_RIL
#ifdef MOZ_GAMEPAD
  void GetGamepads(nsTArray<nsRefPtr<Gamepad> >& aGamepads, ErrorResult& aRv);
#endif // MOZ_GAMEPAD
#ifdef MOZ_B2G_FM
  FMRadio* GetMozFMRadio(ErrorResult& aRv);
#endif
#ifdef MOZ_B2G_BT
  bluetooth::BluetoothManager* GetMozBluetooth(ErrorResult& aRv);
#endif // MOZ_B2G_BT
#ifdef MOZ_TIME_MANAGER
  time::TimeManager* GetMozTime(ErrorResult& aRv);
#endif // MOZ_TIME_MANAGER
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  system::AudioChannelManager* GetMozAudioChannelManager(ErrorResult& aRv);
#endif // MOZ_AUDIO_CHANNEL_MANAGER
#ifdef MOZ_MEDIA_NAVIGATOR
  void MozGetUserMedia(JSContext* aCx,
                       const MediaStreamConstraints& aConstraints,
                       NavigatorUserMediaSuccessCallback& aOnSuccess,
                       NavigatorUserMediaErrorCallback& aOnError,
                       ErrorResult& aRv);
  void MozGetUserMediaDevices(const MediaStreamConstraintsInternal& aConstraints,
                              MozGetUserMediaDevicesSuccessCallback& aOnSuccess,
                              NavigatorUserMediaErrorCallback& aOnError,
                              ErrorResult& aRv);
#endif // MOZ_MEDIA_NAVIGATOR
  bool DoNewResolve(JSContext* aCx, JS::Handle<JSObject*> aObject,
                    JS::Handle<jsid> aId, JS::MutableHandle<JS::Value> aValue);
  void GetOwnPropertyNames(JSContext* aCx, nsTArray<nsString>& aNames,
                           ErrorResult& aRv);

  // WebIDL helper methods
  static bool HasBatterySupport(JSContext* /* unused*/, JSObject* /*unused */);
  static bool HasPowerSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasPhoneNumberSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasIdleSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasWakeLockSupport(JSContext* /* unused*/, JSObject* /*unused */);
  static bool HasDesktopNotificationSupport(JSContext* /* unused*/,
                                            JSObject* /*unused */)
  {
    return HasDesktopNotificationSupport();
  }
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
#ifdef MOZ_B2G_FM
  static bool HasFMRadioSupport(JSContext* /* unused */, JSObject* aGlobal);
#endif // MOZ_B2G_FM
#ifdef MOZ_TIME_MANAGER
  static bool HasTimeSupport(JSContext* /* unused */, JSObject* aGlobal);
#endif // MOZ_TIME_MANAGER
#ifdef MOZ_MEDIA_NAVIGATOR
  static bool HasUserMediaSupport(JSContext* /* unused */,
                                  JSObject* /* unused */);
#endif // MOZ_MEDIA_NAVIGATOR

  static bool HasPushNotificationsSupport(JSContext* /* unused */,
                                          JSObject* aGlobal);

  static bool HasInputMethodSupport(JSContext* /* unused */, JSObject* aGlobal);

  nsPIDOMWindow* GetParentObject() const
  {
    return GetWindow();
  }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

private:
  bool CheckPermission(const char* type);
  static bool CheckPermission(nsPIDOMWindow* aWindow, const char* aType);
  // GetWindowFromGlobal returns the inner window for this global, if
  // any, else null.
  static already_AddRefed<nsPIDOMWindow> GetWindowFromGlobal(JSObject* aGlobal);

  nsRefPtr<nsMimeTypeArray> mMimeTypes;
  nsRefPtr<nsPluginArray> mPlugins;
  nsRefPtr<Geolocation> mGeolocation;
  nsRefPtr<DesktopNotificationCenter> mNotification;
  nsRefPtr<battery::BatteryManager> mBatteryManager;
#ifdef MOZ_B2G_FM
  nsRefPtr<FMRadio> mFMRadio;
#endif
  nsRefPtr<PowerManager> mPowerManager;
  nsRefPtr<MobileMessageManager> mMobileMessageManager;
  nsRefPtr<network::Connection> mConnection;
#ifdef MOZ_B2G_RIL
  nsRefPtr<network::MobileConnection> mMobileConnection;
  nsRefPtr<CellBroadcast> mCellBroadcast;
  nsRefPtr<IccManager> mIccManager;
  nsRefPtr<Telephony> mTelephony;
  nsRefPtr<Voicemail> mVoicemail;
#endif
#ifdef MOZ_B2G_BT
  nsCOMPtr<bluetooth::BluetoothManager> mBluetooth;
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
