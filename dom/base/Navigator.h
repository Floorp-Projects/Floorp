/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigator_h
#define mozilla_dom_Navigator_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Fetch.h"
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
#include "mozilla/dom/MediaKeySystemAccessManager.h"

class nsPluginArray;
class nsMimeTypeArray;
class nsPIDOMWindowInner;
class nsIDOMNavigatorSystemMessages;
class nsIPrincipal;
class nsIURI;

namespace mozilla {
namespace dom {
class BodyExtractorBase;
class Geolocation;
class systemMessageCallback;
class MediaDevices;
struct MediaStreamConstraints;
class WakeLock;
class ArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams;
class ServiceWorkerContainer;
class DOMRequest;
struct FlyWebPublishOptions;
struct FlyWebFilter;
class WebAuthentication;
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

class Promise;

class DesktopNotificationCenter;
class MozIdleObserver;
class Gamepad;
class GamepadServiceTest;
class NavigatorUserMediaSuccessCallback;
class NavigatorUserMediaErrorCallback;
class MozGetUserMediaDevicesSuccessCallback;

namespace network {
class Connection;
} // namespace network

class PowerManager;
class Presentation;
class LegacyMozTCPSocket;
class VRDisplay;
class VRServiceTest;
class StorageManager;

namespace time {
class TimeManager;
} // namespace time

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

  // The XPCOM GetProduct is OK
  // The XPCOM GetLanguage is OK
  void GetAppName(nsAString& aAppName, CallerType aCallerType) const;
  void GetAppVersion(nsAString& aAppName, CallerType aCallerType,
                     ErrorResult& aRv) const;
  void GetPlatform(nsAString& aPlatform, CallerType aCallerType,
                   ErrorResult& aRv) const;
  void GetUserAgent(nsAString& aUserAgent, CallerType aCallerType,
                    ErrorResult& aRv) const;
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
                               bool aIsCallerChrome,
                               nsAString& aUserAgent);

  // Clears the user agent cache by calling:
  // NavigatorBinding::ClearCachedUserAgentValue(this);
  void ClearUserAgentCache();

  bool Vibrate(uint32_t aDuration);
  bool Vibrate(const nsTArray<uint32_t>& aDuration);
  void SetVibrationPermission(bool aPermitted, bool aPersistent);
  uint32_t MaxTouchPoints();
  void GetAppCodeName(nsString& aAppCodeName, ErrorResult& aRv)
  {
    aRv = GetAppCodeName(aAppCodeName);
  }
  void GetOscpu(nsAString& aOscpu, CallerType aCallerType,
                ErrorResult& aRv) const;
  // The XPCOM GetVendor is OK
  // The XPCOM GetVendorSub is OK
  // The XPCOM GetProductSub is OK
  bool CookieEnabled();
  void GetBuildID(nsAString& aBuildID, CallerType aCallerType,
                  ErrorResult& aRv) const;
  PowerManager* GetMozPower(ErrorResult& aRv);
  bool JavaEnabled(CallerType aCallerType, ErrorResult& aRv);
  uint64_t HardwareConcurrency();
  bool CpuHasSSE2();
  bool TaintEnabled()
  {
    return false;
  }
  void AddIdleObserver(MozIdleObserver& aObserver, ErrorResult& aRv);
  void RemoveIdleObserver(MozIdleObserver& aObserver, ErrorResult& aRv);
  already_AddRefed<WakeLock> RequestWakeLock(const nsAString &aTopic,
                                             ErrorResult& aRv);

  DesktopNotificationCenter* GetMozNotification(ErrorResult& aRv);
  already_AddRefed<LegacyMozTCPSocket> MozTCPSocket();
  network::Connection* GetConnection(ErrorResult& aRv);
  MediaDevices* GetMediaDevices(ErrorResult& aRv);

  void GetGamepads(nsTArray<RefPtr<Gamepad> >& aGamepads, ErrorResult& aRv);
  GamepadServiceTest* RequestGamepadServiceTest();
  already_AddRefed<Promise> GetVRDisplays(ErrorResult& aRv);
  void GetActiveVRDisplays(nsTArray<RefPtr<VRDisplay>>& aDisplays) const;
  VRServiceTest* RequestVRServiceTest();
#ifdef MOZ_TIME_MANAGER
  time::TimeManager* GetMozTime(ErrorResult& aRv);
#endif // MOZ_TIME_MANAGER

  Presentation* GetPresentation(ErrorResult& aRv);

  bool SendBeacon(const nsAString& aUrl,
                  const Nullable<fetch::BodyInit>& aData,
                  ErrorResult& aRv);

  void MozGetUserMedia(const MediaStreamConstraints& aConstraints,
                       NavigatorUserMediaSuccessCallback& aOnSuccess,
                       NavigatorUserMediaErrorCallback& aOnError,
                       CallerType aCallerType,
                       ErrorResult& aRv);
  void MozGetUserMediaDevices(const MediaStreamConstraints& aConstraints,
                              MozGetUserMediaDevicesSuccessCallback& aOnSuccess,
                              NavigatorUserMediaErrorCallback& aOnError,
                              uint64_t aInnerWindowID,
                              const nsAString& aCallID,
                              ErrorResult& aRv);

  already_AddRefed<ServiceWorkerContainer> ServiceWorker();

  mozilla::dom::WebAuthentication* Authentication();

  void GetLanguages(nsTArray<nsString>& aLanguages);

  bool MozE10sEnabled();

  StorageManager* Storage();

  static void GetAcceptLanguages(nsTArray<nsString>& aLanguages);

  // WebIDL helper methods
  static bool HasWakeLockSupport(JSContext* /* unused*/, JSObject* /*unused */);
  static bool HasWifiManagerSupport(JSContext* /* unused */,
                                  JSObject* aGlobal);
  static bool HasUserMediaSupport(JSContext* /* unused */,
                                  JSObject* /* unused */);

  static bool IsE10sEnabled(JSContext* aCx, JSObject* aGlobal);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return GetWindow();
  }

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

  // GetWindowFromGlobal returns the inner window for this global, if
  // any, else null.
  static already_AddRefed<nsPIDOMWindowInner> GetWindowFromGlobal(JSObject* aGlobal);

  already_AddRefed<Promise>
  RequestMediaKeySystemAccess(const nsAString& aKeySystem,
                              const Sequence<MediaKeySystemConfiguration>& aConfig,
                              ErrorResult& aRv);
private:
  RefPtr<MediaKeySystemAccessManager> mMediaKeySystemAccessManager;

public:
  void NotifyVRDisplaysUpdated();
  void NotifyActiveVRDisplaysChanged();

private:
  virtual ~Navigator();

  bool CheckPermission(const char* type);
  static bool CheckPermission(nsPIDOMWindowInner* aWindow, const char* aType);

  // This enum helps SendBeaconInternal to apply different behaviors to body
  // types.
  enum BeaconType {
    eBeaconTypeBlob,
    eBeaconTypeArrayBuffer,
    eBeaconTypeOther
  };

  bool SendBeaconInternal(const nsAString& aUrl,
                          BodyExtractorBase* aBody,
                          BeaconType aType,
                          ErrorResult& aRv);

  RefPtr<nsMimeTypeArray> mMimeTypes;
  RefPtr<nsPluginArray> mPlugins;
  RefPtr<Permissions> mPermissions;
  RefPtr<Geolocation> mGeolocation;
  RefPtr<DesktopNotificationCenter> mNotification;
  RefPtr<battery::BatteryManager> mBatteryManager;
  RefPtr<Promise> mBatteryPromise;
  RefPtr<PowerManager> mPowerManager;
  RefPtr<network::Connection> mConnection;
  RefPtr<WebAuthentication> mAuthentication;
  RefPtr<MediaDevices> mMediaDevices;
  RefPtr<time::TimeManager> mTimeManager;
  RefPtr<ServiceWorkerContainer> mServiceWorkerContainer;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<Presentation> mPresentation;
  RefPtr<GamepadServiceTest> mGamepadServiceTest;
  nsTArray<RefPtr<Promise> > mVRGetDisplaysPromises;
  RefPtr<VRServiceTest> mVRServiceTest;
  nsTArray<uint32_t> mRequestedVibrationPattern;
  RefPtr<StorageManager> mStorageManager;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Navigator_h
