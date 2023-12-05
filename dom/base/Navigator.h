/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigator_h
#define mozilla_dom_Navigator_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/AddonManagerBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/Nullable.h"
#include "nsWrapperCache.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/dom/MediaKeySystemAccessManager.h"

class nsPluginArray;
class nsMimeTypeArray;
class nsPIDOMWindowInner;
class nsIDOMNavigatorSystemMessages;
class nsIPrincipal;
class nsIURI;

namespace mozilla {
class ErrorResult;

namespace dom {
class AddonManager;
class BodyExtractorBase;
class Geolocation;
class systemMessageCallback;
class MediaDevices;
struct MediaStreamConstraints;
class ArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams;
class ServiceWorkerContainer;
class DOMRequest;
class CredentialsContainer;
class Clipboard;
class LockManager;
class HTMLMediaElement;
class AudioContext;
class WakeLockJS;
}  // namespace dom
namespace webgpu {
class Instance;
}  // namespace webgpu
}  // namespace mozilla

//*****************************************************************************
// Navigator: Script "navigator" object
//*****************************************************************************

namespace mozilla::dom {

class Permissions;

namespace battery {
class BatteryManager;
}  // namespace battery

class Promise;

class Gamepad;
class GamepadServiceTest;
class NavigatorUserMediaSuccessCallback;
class NavigatorUserMediaErrorCallback;

struct MIDIOptions;

nsTArray<uint32_t> SanitizeVibratePattern(const nsTArray<uint32_t>& aPattern);

namespace network {
class Connection;
}  // namespace network

class LegacyMozTCPSocket;
class VRDisplay;
class VRServiceTest;
class XRSystem;
class StorageManager;
class MediaCapabilities;
class MediaSession;
class UserActivation;
struct ShareData;
class WindowGlobalChild;

class Navigator final : public nsISupports, public nsWrapperCache {
 public:
  explicit Navigator(nsPIDOMWindowInner* aInnerWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Navigator)

  void Invalidate();
  nsPIDOMWindowInner* GetWindow() const { return mWindow; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  /**
   * Called when the inner window navigates to a new page.
   */
  void OnNavigation();

  void GetProduct(nsAString& aProduct);
  void GetLanguage(nsAString& aLanguage);
  void GetAppName(nsAString& aAppName) const;
  void GetAppVersion(nsAString& aAppName, CallerType aCallerType,
                     ErrorResult& aRv) const;
  void GetPlatform(nsAString& aPlatform, CallerType aCallerType,
                   ErrorResult& aRv) const;
  void GetUserAgent(nsAString& aUserAgent, CallerType aCallerType,
                    ErrorResult& aRv) const;
  bool OnLine();
  void CheckProtocolHandlerAllowed(const nsAString& aScheme,
                                   nsIURI* aHandlerURI, nsIURI* aDocumentURI,
                                   ErrorResult& aRv);
  void RegisterProtocolHandler(const nsAString& aScheme, const nsAString& aURL,
                               ErrorResult& aRv);
  nsMimeTypeArray* GetMimeTypes(ErrorResult& aRv);
  nsPluginArray* GetPlugins(ErrorResult& aRv);
  bool PdfViewerEnabled();
  Permissions* GetPermissions(ErrorResult& aRv);
  void GetDoNotTrack(nsAString& aResult);
  bool GlobalPrivacyControl();
  Geolocation* GetGeolocation(ErrorResult& aRv);
  Promise* GetBattery(ErrorResult& aRv);
  dom::WakeLockJS* WakeLock();

  bool CanShare(const ShareData& aData);
  already_AddRefed<Promise> Share(const ShareData& aData, ErrorResult& aRv);

  static nsresult GetPlatform(nsAString& aPlatform, Document* aCallerDoc,
                              bool aUsePrefOverriddenValue);

  static nsresult GetAppVersion(nsAString& aAppVersion, Document* aCallerDoc,
                                bool aUsePrefOverriddenValue);

  static nsresult GetUserAgent(nsPIDOMWindowInner* aWindow,
                               Document* aCallerDoc,
                               Maybe<bool> aShouldResistFingerprinting,
                               nsAString& aUserAgent);

  // Clears the platform cache by calling:
  // Navigator_Binding::ClearCachedPlatformValue(this);
  void ClearPlatformCache();

  // Clears the user agent cache by calling:
  // Navigator_Binding::ClearCachedUserAgentValue(this);
  void ClearUserAgentCache();

  bool Vibrate(uint32_t aDuration);
  bool Vibrate(const nsTArray<uint32_t>& aDuration);
  void SetVibrationPermission(bool aPermitted, bool aPersistent);
  uint32_t MaxTouchPoints(CallerType aCallerType);
  void GetAppCodeName(nsAString& aAppCodeName, ErrorResult& aRv);
  void GetOscpu(nsAString& aOscpu, CallerType aCallerType,
                ErrorResult& aRv) const;
  void GetVendorSub(nsAString& aVendorSub);
  void GetVendor(nsAString& aVendor);
  void GetProductSub(nsAString& aProductSub);
  bool CookieEnabled();
  void GetBuildID(nsAString& aBuildID, CallerType aCallerType,
                  ErrorResult& aRv) const;
  bool JavaEnabled() { return false; }
  uint64_t HardwareConcurrency();
  bool TaintEnabled() { return false; }

  already_AddRefed<LegacyMozTCPSocket> MozTCPSocket();
  network::Connection* GetConnection(ErrorResult& aRv);
  MediaDevices* GetMediaDevices(ErrorResult& aRv);
  MediaDevices* GetExtantMediaDevices() const { return mMediaDevices; };

  void GetGamepads(nsTArray<RefPtr<Gamepad>>& aGamepads, ErrorResult& aRv);
  GamepadServiceTest* RequestGamepadServiceTest(ErrorResult& aRv);
  already_AddRefed<Promise> GetVRDisplays(ErrorResult& aRv);
  void FinishGetVRDisplays(bool isWebVRSupportedInwindow, Promise* p);
  void GetActiveVRDisplays(nsTArray<RefPtr<VRDisplay>>& aDisplays) const;
  void OnXRPermissionRequestAllow();
  void OnXRPermissionRequestCancel();
  VRServiceTest* RequestVRServiceTest(ErrorResult& aRv);
  bool IsWebVRContentDetected() const;
  bool IsWebVRContentPresenting() const;
  void RequestVRPresentation(VRDisplay& aDisplay);
  XRSystem* GetXr(ErrorResult& aRv);
  already_AddRefed<Promise> RequestMIDIAccess(const MIDIOptions& aOptions,
                                              ErrorResult& aRv);

  bool SendBeacon(const nsAString& aUrl, const Nullable<fetch::BodyInit>& aData,
                  ErrorResult& aRv);

  void MozGetUserMedia(const MediaStreamConstraints& aConstraints,
                       NavigatorUserMediaSuccessCallback& aOnSuccess,
                       NavigatorUserMediaErrorCallback& aOnError,
                       CallerType aCallerType, ErrorResult& aRv);

  already_AddRefed<ServiceWorkerContainer> ServiceWorker();
  // NOTE(krosylight): This currently exists solely for use counter purpose,
  // since Navigator::ServiceWorker is also called by native functions. Remove
  // this when we don't need the counter.
  already_AddRefed<ServiceWorkerContainer> ServiceWorkerJS();

  mozilla::dom::CredentialsContainer* Credentials();
  dom::Clipboard* Clipboard();
  webgpu::Instance* Gpu();
  dom::LockManager* Locks();

  static bool Webdriver();

  void GetLanguages(nsTArray<nsString>& aLanguages);

  StorageManager* Storage();

  static void GetAcceptLanguages(nsTArray<nsString>& aLanguages);

  dom::MediaCapabilities* MediaCapabilities();
  dom::MediaSession* MediaSession();

  AddonManager* GetMozAddonManager(ErrorResult& aRv);

  // WebIDL helper methods
  static bool HasUserMediaSupport(JSContext* /* unused */,
                                  JSObject* /* unused */);
  static bool HasShareSupport(JSContext* /* unused */, JSObject* /* unused */);

  static bool HasMidiSupport(JSContext* /* unused */, JSObject* /* unused */);

  nsPIDOMWindowInner* GetParentObject() const { return GetWindow(); }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // GetWindowFromGlobal returns the inner window for this global, if
  // any, else null.
  static already_AddRefed<nsPIDOMWindowInner> GetWindowFromGlobal(
      JSObject* aGlobal);

  already_AddRefed<Promise> RequestMediaKeySystemAccess(
      const nsAString& aKeySystem,
      const Sequence<MediaKeySystemConfiguration>& aConfig, ErrorResult& aRv);

  bool HasCreatedMediaSession() const;

  // Following methods are for the Autoplay Policy Detection API.
  // https://w3c.github.io/autoplay/#autoplay-detection-methods
  AutoplayPolicy GetAutoplayPolicy(AutoplayPolicyMediaType aType);
  AutoplayPolicy GetAutoplayPolicy(HTMLMediaElement& aElement);
  AutoplayPolicy GetAutoplayPolicy(AudioContext& aContext);

  already_AddRefed<dom::UserActivation> UserActivation();

 private:
  void ValidateShareData(const ShareData& aData, ErrorResult& aRv);
  RefPtr<MediaKeySystemAccessManager> mMediaKeySystemAccessManager;

 public:
  void NotifyVRDisplaysUpdated();
  void NotifyActiveVRDisplaysChanged();

  bool TestTrialGatedAttribute() const { return true; }

 private:
  virtual ~Navigator();

  // This enum helps SendBeaconInternal to apply different behaviors to body
  // types.
  enum BeaconType { eBeaconTypeBlob, eBeaconTypeArrayBuffer, eBeaconTypeOther };

  bool SendBeaconInternal(const nsAString& aUrl, BodyExtractorBase* aBody,
                          BeaconType aType, ErrorResult& aRv);

  nsIDocShell* GetDocShell() const {
    return mWindow ? mWindow->GetDocShell() : nullptr;
  }

  RefPtr<nsPluginArray> mPlugins;
  RefPtr<Permissions> mPermissions;
  RefPtr<Geolocation> mGeolocation;
  RefPtr<battery::BatteryManager> mBatteryManager;
  RefPtr<Promise> mBatteryPromise;
  RefPtr<network::Connection> mConnection;
  RefPtr<CredentialsContainer> mCredentials;
  RefPtr<dom::Clipboard> mClipboard;
  RefPtr<MediaDevices> mMediaDevices;
  RefPtr<ServiceWorkerContainer> mServiceWorkerContainer;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<GamepadServiceTest> mGamepadServiceTest;
  nsTArray<RefPtr<Promise>> mVRGetDisplaysPromises;
  RefPtr<VRServiceTest> mVRServiceTest;
  RefPtr<XRSystem> mXRSystem;
  nsTArray<uint32_t> mRequestedVibrationPattern;
  RefPtr<StorageManager> mStorageManager;
  RefPtr<dom::MediaCapabilities> mMediaCapabilities;
  RefPtr<dom::MediaSession> mMediaSession;
  RefPtr<AddonManager> mAddonManager;
  RefPtr<webgpu::Instance> mWebGpu;
  RefPtr<Promise> mSharePromise;  // Web Share API related
  RefPtr<dom::LockManager> mLocks;
  RefPtr<dom::UserActivation> mUserActivation;
  RefPtr<dom::WakeLockJS> mWakeLock;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Navigator_h
