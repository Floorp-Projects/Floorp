/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigator_h
#define mozilla_dom_Navigator_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/ErrorResult.h"
#include "nsIDOMNavigator.h"
#include "nsIMozNavigatorNetwork.h"
#include "nsWrapperCache.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWeakPtr.h"
#ifdef MOZ_EME
#include "mozilla/dom/MediaKeySystemAccessManager.h"
#endif

class nsPluginArray;
class nsMimeTypeArray;
class nsPIDOMWindowInner;
class nsIDOMNavigatorSystemMessages;
class nsDOMCameraManager;
class nsDOMDeviceStorage;
class nsIPrincipal;
class nsIURI;

namespace mozilla {
namespace dom {
class Geolocation;
class systemMessageCallback;
class MediaDevices;
struct MediaStreamConstraints;
class WakeLock;
class ArrayBufferViewOrBlobOrStringOrFormData;
struct MobileIdOptions;
class ServiceWorkerContainer;
class DOMRequest;
struct FlyWebPublishOptions;
struct FlyWebFilter;
} // namespace dom
} // namespace mozilla

//*****************************************************************************
// Navigator: Script "navigator" object
//*****************************************************************************

namespace mozilla {
namespace dom {

class Permissions;

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
class NavigatorUserMediaSuccessCallback;
class NavigatorUserMediaErrorCallback;
class MozGetUserMediaDevicesSuccessCallback;

namespace network {
class Connection;
} // namespace network

#ifdef MOZ_B2G_BT
namespace bluetooth {
class BluetoothManager;
} // namespace bluetooth
#endif // MOZ_B2G_BT

#ifdef MOZ_B2G_RIL
class MobileConnectionArray;
#endif

class PowerManager;
class CellBroadcast;
class IccManager;
class Telephony;
class Voicemail;
class TVManager;
class InputPortManager;
class DeviceStorageAreaListener;
class Presentation;
class LegacyMozTCPSocket;

namespace time {
class TimeManager;
} // namespace time

namespace system {
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
class AudioChannelManager;
#endif
} // namespace system

class Navigator final : public nsIDOMNavigator
                      , public nsIMozNavigatorNetwork
                      , public nsWrapperCache
{
public:
  explicit Navigator(nsPIDOMWindowInner* aInnerWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Navigator,
                                                         nsIDOMNavigator)
  NS_DECL_NSIDOMNAVIGATOR
  NS_DECL_NSIMOZNAVIGATORNETWORK

  static void Init();

  void Invalidate();
  nsPIDOMWindowInner *GetWindow() const
  {
    return mWindow;
  }

  void RefreshMIMEArray();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  /**
   * For use during document.write where our inner window changes.
   */
  void SetWindow(nsPIDOMWindowInner *aInnerWindow);

  /**
   * Called when the inner window navigates to a new page.
   */
  void OnNavigation();

  // Helper to initialize mMessagesManager.
  nsresult EnsureMessagesManager();

  // The XPCOM GetProduct is OK
  // The XPCOM GetLanguage is OK
  void GetUserAgent(nsString& aUserAgent, ErrorResult& /* unused */)
  {
    GetUserAgent(aUserAgent);
  }
  bool OnLine();
  void RegisterProtocolHandler(const nsAString& aScheme, const nsAString& aURL,
                               const nsAString& aTitle, ErrorResult& aRv);
  void RegisterContentHandler(const nsAString& aMIMEType, const nsAString& aURL,
                              const nsAString& aTitle, ErrorResult& aRv);
  nsMimeTypeArray* GetMimeTypes(ErrorResult& aRv);
  nsPluginArray* GetPlugins(ErrorResult& aRv);
  Permissions* GetPermissions(ErrorResult& aRv);
  // The XPCOM GetDoNotTrack is ok
  Geolocation* GetGeolocation(ErrorResult& aRv);
  Promise* GetBattery(ErrorResult& aRv);

  already_AddRefed<Promise> PublishServer(const nsAString& aName,
                                          const FlyWebPublishOptions& aOptions,
                                          ErrorResult& aRv);
  static void AppName(nsAString& aAppName, bool aUsePrefOverriddenValue);

  static nsresult GetPlatform(nsAString& aPlatform,
                              bool aUsePrefOverriddenValue);

  static nsresult GetAppVersion(nsAString& aAppVersion,
                                bool aUsePrefOverriddenValue);

  static nsresult GetUserAgent(nsPIDOMWindowInner* aWindow,
                               nsIURI* aURI,
                               bool aIsCallerChrome,
                               nsAString& aUserAgent);

  // Clears the user agent cache by calling:
  // NavigatorBinding::ClearCachedUserAgentValue(this);
  void ClearUserAgentCache();

  // Feature Detection API
  already_AddRefed<Promise> GetFeature(const nsAString& aName,
                                       ErrorResult& aRv);

  already_AddRefed<Promise> HasFeature(const nsAString &aName,
                                       ErrorResult& aRv);

  bool Vibrate(uint32_t aDuration);
  bool Vibrate(const nsTArray<uint32_t>& aDuration);
  void SetVibrationPermission(bool aPermitted, bool aPersistent);
  uint32_t MaxTouchPoints();
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
  uint64_t HardwareConcurrency();
  bool TaintEnabled()
  {
    return false;
  }
  void AddIdleObserver(MozIdleObserver& aObserver, ErrorResult& aRv);
  void RemoveIdleObserver(MozIdleObserver& aObserver, ErrorResult& aRv);
  already_AddRefed<WakeLock> RequestWakeLock(const nsAString &aTopic,
                                             ErrorResult& aRv);
  DeviceStorageAreaListener* GetDeviceStorageAreaListener(ErrorResult& aRv);

  already_AddRefed<nsDOMDeviceStorage> GetDeviceStorage(const nsAString& aType,
                                                        ErrorResult& aRv);

  void GetDeviceStorages(const nsAString& aType,
                         nsTArray<RefPtr<nsDOMDeviceStorage> >& aStores,
                         ErrorResult& aRv);

  already_AddRefed<nsDOMDeviceStorage>
  GetDeviceStorageByNameAndType(const nsAString& aName, const nsAString& aType,
                                ErrorResult& aRv);

  DesktopNotificationCenter* GetMozNotification(ErrorResult& aRv);
  CellBroadcast* GetMozCellBroadcast(ErrorResult& aRv);
  IccManager* GetMozIccManager(ErrorResult& aRv);
  MobileMessageManager* GetMozMobileMessage();
  Telephony* GetMozTelephony(ErrorResult& aRv);
  Voicemail* GetMozVoicemail(ErrorResult& aRv);
  TVManager* GetTv();
  InputPortManager* GetInputPortManager(ErrorResult& aRv);
  already_AddRefed<LegacyMozTCPSocket> MozTCPSocket();
  network::Connection* GetConnection(ErrorResult& aRv);
  nsDOMCameraManager* GetMozCameras(ErrorResult& aRv);
  MediaDevices* GetMediaDevices(ErrorResult& aRv);
  void MozSetMessageHandler(const nsAString& aType,
                            systemMessageCallback* aCallback,
                            ErrorResult& aRv);
  bool MozHasPendingMessage(const nsAString& aType, ErrorResult& aRv);
  void MozSetMessageHandlerPromise(Promise& aPromise, ErrorResult& aRv);

#ifdef MOZ_B2G
  already_AddRefed<Promise> GetMobileIdAssertion(const MobileIdOptions& options,
                                                 ErrorResult& aRv);
#endif
#ifdef MOZ_B2G_RIL
  MobileConnectionArray* GetMozMobileConnections(ErrorResult& aRv);
#endif // MOZ_B2G_RIL
#ifdef MOZ_GAMEPAD
  void GetGamepads(nsTArray<RefPtr<Gamepad> >& aGamepads, ErrorResult& aRv);
#endif // MOZ_GAMEPAD
  already_AddRefed<Promise> GetVRDevices(ErrorResult& aRv);
  void NotifyVRDevicesUpdated();
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

  Presentation* GetPresentation(ErrorResult& aRv);

  bool SendBeacon(const nsAString& aUrl,
                  const Nullable<ArrayBufferViewOrBlobOrStringOrFormData>& aData,
                  ErrorResult& aRv);

  void MozGetUserMedia(const MediaStreamConstraints& aConstraints,
                       NavigatorUserMediaSuccessCallback& aOnSuccess,
                       NavigatorUserMediaErrorCallback& aOnError,
                       ErrorResult& aRv);
  void MozGetUserMediaDevices(const MediaStreamConstraints& aConstraints,
                              MozGetUserMediaDevicesSuccessCallback& aOnSuccess,
                              NavigatorUserMediaErrorCallback& aOnError,
                              uint64_t aInnerWindowID,
                              const nsAString& aCallID,
                              ErrorResult& aRv);

  already_AddRefed<ServiceWorkerContainer> ServiceWorker();

  void GetLanguages(nsTArray<nsString>& aLanguages);

  bool MozE10sEnabled();

#ifdef MOZ_PAY
  already_AddRefed<DOMRequest> MozPay(JSContext* aCx,
                                      JS::Handle<JS::Value> aJwts,
                                      ErrorResult& aRv);
#endif // MOZ_PAY

  static void GetAcceptLanguages(nsTArray<nsString>& aLanguages);

  // WebIDL helper methods
  static bool HasWakeLockSupport(JSContext* /* unused*/, JSObject* /*unused */);
  static bool HasCameraSupport(JSContext* /* unused */,
                               JSObject* aGlobal);
  static bool HasWifiManagerSupport(JSContext* /* unused */,
                                  JSObject* aGlobal);
#ifdef MOZ_NFC
  static bool HasNFCSupport(JSContext* /* unused */, JSObject* aGlobal);
#endif // MOZ_NFC
  static bool HasUserMediaSupport(JSContext* /* unused */,
                                  JSObject* /* unused */);

#ifdef MOZ_B2G
  static bool HasMobileIdSupport(JSContext* aCx, JSObject* aGlobal);
#endif

  static bool HasPresentationSupport(JSContext* aCx, JSObject* aGlobal);

  static bool IsE10sEnabled(JSContext* aCx, JSObject* aGlobal);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return GetWindow();
  }

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

  // GetWindowFromGlobal returns the inner window for this global, if
  // any, else null.
  static already_AddRefed<nsPIDOMWindowInner> GetWindowFromGlobal(JSObject* aGlobal);

#ifdef MOZ_EME
  already_AddRefed<Promise>
  RequestMediaKeySystemAccess(const nsAString& aKeySystem,
                              const Sequence<MediaKeySystemConfiguration>& aConfig,
                              ErrorResult& aRv);
private:
  RefPtr<MediaKeySystemAccessManager> mMediaKeySystemAccessManager;
#endif

private:
  virtual ~Navigator();

  bool CheckPermission(const char* type);
  static bool CheckPermission(nsPIDOMWindowInner* aWindow, const char* aType);

  already_AddRefed<nsDOMDeviceStorage> FindDeviceStorage(const nsAString& aName,
                                                         const nsAString& aType);

  RefPtr<nsMimeTypeArray> mMimeTypes;
  RefPtr<nsPluginArray> mPlugins;
  RefPtr<Permissions> mPermissions;
  RefPtr<Geolocation> mGeolocation;
  RefPtr<DesktopNotificationCenter> mNotification;
  RefPtr<battery::BatteryManager> mBatteryManager;
  RefPtr<Promise> mBatteryPromise;
#ifdef MOZ_B2G_FM
  RefPtr<FMRadio> mFMRadio;
#endif
  RefPtr<PowerManager> mPowerManager;
  RefPtr<CellBroadcast> mCellBroadcast;
  RefPtr<IccManager> mIccManager;
  RefPtr<MobileMessageManager> mMobileMessageManager;
  RefPtr<Telephony> mTelephony;
  RefPtr<Voicemail> mVoicemail;
  RefPtr<TVManager> mTVManager;
  RefPtr<InputPortManager> mInputPortManager;
  RefPtr<network::Connection> mConnection;
#ifdef MOZ_B2G_RIL
  RefPtr<MobileConnectionArray> mMobileConnections;
#endif
#ifdef MOZ_B2G_BT
  RefPtr<bluetooth::BluetoothManager> mBluetooth;
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  RefPtr<system::AudioChannelManager> mAudioChannelManager;
#endif
  RefPtr<nsDOMCameraManager> mCameraManager;
  RefPtr<MediaDevices> mMediaDevices;
  nsCOMPtr<nsIDOMNavigatorSystemMessages> mMessagesManager;
  nsTArray<nsWeakPtr> mDeviceStorageStores;
  RefPtr<time::TimeManager> mTimeManager;
  RefPtr<ServiceWorkerContainer> mServiceWorkerContainer;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<DeviceStorageAreaListener> mDeviceStorageAreaListener;
  RefPtr<Presentation> mPresentation;

  nsTArray<RefPtr<Promise> > mVRGetDevicesPromises;
  nsTArray<uint32_t> mRequestedVibrationPattern;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Navigator_h
