/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeolocationSystem.h"
#include "mozilla/Components.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/ScopeExit.h"
#include "nsIGeolocationUIUtilsWin.h"
#include "nsIWifiListener.h"
#include "nsIWifiMonitor.h"

#include <windows.system.h>
#include <windows.security.authorization.appcapabilityaccess.h>
#include <wrl.h>

namespace mozilla::dom::geolocation {

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Security::Authorization::AppCapabilityAccess;
using namespace ABI::Windows::System;
using namespace Microsoft::WRL;
using Wrappers::HStringReference;

namespace {

const auto& kAppCapabilityGuid =
    RuntimeClass_Windows_Security_Authorization_AppCapabilityAccess_AppCapability;
const auto& kLauncherGuid = RuntimeClass_Windows_System_Launcher;
const auto& kUriGuid = RuntimeClass_Windows_Foundation_Uri;
const wchar_t kLocationSettingsPage[] = L"ms-settings:privacy-location";

template <typename TypeToCreate>
ComPtr<TypeToCreate> CreateFromActivationFactory(const wchar_t* aNamespace) {
  ComPtr<TypeToCreate> newObject;
  GetActivationFactory(HStringReference(aNamespace).Get(), &newObject);
  return newObject;
}

RefPtr<IAppCapability> GetWifiControlAppCapability() {
  ComPtr<IAppCapabilityStatics> appCapabilityStatics =
      CreateFromActivationFactory<IAppCapabilityStatics>(kAppCapabilityGuid);
  NS_ENSURE_TRUE(appCapabilityStatics, nullptr);

  RefPtr<IAppCapability> appCapability;
  HRESULT hr = appCapabilityStatics->Create(
      HStringReference(L"wifiControl").Get(), getter_AddRefs(appCapability));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  NS_ENSURE_TRUE(appCapability, nullptr);
  return appCapability;
}

Maybe<AppCapabilityAccessStatus> GetWifiControlAccess() {
  auto appCapability = GetWifiControlAppCapability();
  NS_ENSURE_TRUE(appCapability, Nothing());
  AppCapabilityAccessStatus status;
  HRESULT hr = appCapability->CheckAccess(&status);
  NS_ENSURE_TRUE(SUCCEEDED(hr), Nothing());
  return Some(status);
}

bool SystemWillPromptForPermissionHint() {
  auto wifiAccess = GetWifiControlAccess();
  if (wifiAccess !=
      mozilla::Some(AppCapabilityAccessStatus::
                        AppCapabilityAccessStatus_UserPromptRequired)) {
    return false;
  }

  // If wifi is not available (e.g. because there is no wifi device present)
  // then the API may report that Windows will request geolocation permission
  // but it can't without the wifi scanner.  Check for that case.
  nsCOMPtr<nsIWifiMonitor> wifiMonitor = components::WifiMonitor::Service();
  NS_ENSURE_TRUE(wifiMonitor, false);
  return wifiMonitor->GetHasWifiAdapter();
}

bool LocationIsPermittedHint() {
  auto wifiAccess = GetWifiControlAccess();
  // This API wasn't available on earlier versions of Windows, so a failure to
  // get the result means that we will assume that location access is permitted.
  return wifiAccess.isNothing() ||
         *wifiAccess ==
             AppCapabilityAccessStatus::AppCapabilityAccessStatus_Allowed;
}

class WindowsGeolocationPermissionRequest final
    : public SystemGeolocationPermissionRequest,
      public SupportsThreadSafeWeakPtr<WindowsGeolocationPermissionRequest> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(WindowsGeolocationPermissionRequest);
  // Define SystemGeolocationPermissionRequest's ref-counting by forwarding the
  // calls to SupportsThreadSafeWeakPtr<WindowsGeolocationPermissionRequest>
  NS_INLINE_DECL_REFCOUNTING_INHERITED(
      WindowsGeolocationPermissionRequest,
      SupportsThreadSafeWeakPtr<WindowsGeolocationPermissionRequest>);

  WindowsGeolocationPermissionRequest(BrowsingContext* aBrowsingContext,
                                      ParentRequestResolver&& aResolver)
      : mResolver(std::move(aResolver)), mBrowsingContext(aBrowsingContext) {}

  void Initialize() {
    MOZ_ASSERT(!mIsRunning);
    auto failedToWatch = MakeScopeExit([&]() {
      if (!mIsRunning) {
        mAppCapability = nullptr;
        mToken = EventRegistrationToken{};
        mResolver(GeolocationPermissionStatus::Error);
      }
    });

    mAppCapability = GetWifiControlAppCapability();
    if (!mAppCapability) {
      return;
    }

    using AccessChangedHandler =
        ITypedEventHandler<AppCapability*,
                           AppCapabilityAccessChangedEventArgs*>;

    // Note: This creates the callback that listens for location permission
    // changes as free threaded, which we need to do to overcome a (Microsoft)
    // issue with the callback proxy's exports with (at least) the pre-24H2
    // versions of Windows.
    ComPtr<AccessChangedHandler> acHandlerRef = Callback<Implements<
        RuntimeClassFlags<ClassicCom>, AccessChangedHandler, FtmBase>>(
        [weakSelf = ThreadSafeWeakPtr<WindowsGeolocationPermissionRequest>(
             this)](IAppCapability*, IAppCapabilityAccessChangedEventArgs*) {
          // Because of the free threaded access mentioned above, our
          // callback can run on a background thread, so dispatch it to
          // main.
          if (!NS_IsMainThread()) {
            NS_DispatchToMainThread(
                NS_NewRunnableFunction(__func__, [weakSelf]() {
                  RefPtr<WindowsGeolocationPermissionRequest> self(weakSelf);
                  if (self) {
                    self->StopIfLocationIsPermitted();
                  }
                }));
            return S_OK;
          }

          RefPtr<WindowsGeolocationPermissionRequest> self(weakSelf);
          if (self) {
            self->StopIfLocationIsPermitted();
          }
          return S_OK;
        });

    if (!acHandlerRef) {
      return;
    }

    HRESULT hr = mAppCapability->add_AccessChanged(acHandlerRef.Get(), &mToken);
    NS_ENSURE_TRUE_VOID(SUCCEEDED(hr));
    MOZ_ASSERT(mToken.value);

    mIsRunning = true;
    failedToWatch.release();

    // Avoid a race for accessChanged by checking it now and stopping if
    // permission is already granted.
    StopIfLocationIsPermitted();
  }

  void Stop() override {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mIsRunning) {
      return;
    }
    mIsRunning = false;

    if (LocationIsPermittedHint()) {
      mResolver(GeolocationPermissionStatus::Granted);
    } else {
      mResolver(GeolocationPermissionStatus::Canceled);
    }

    // Remove the modal with the cancel button.
    nsresult rv = DismissPrompt();
    NS_ENSURE_SUCCESS_VOID(rv);

    // Stop watching location settings.
    MOZ_ASSERT(mAppCapability);
    mAppCapability->remove_AccessChanged(mToken);
    mAppCapability = nullptr;
    mToken = EventRegistrationToken{};
  }

  bool IsStopped() { return !mIsRunning; }

 protected:
  // Ref-counting demands that the destructor be non-public but
  // SupportsThreadSafeWeakPtr needs to be able to call it, because we use
  // NS_INLINE_DECL_REFCOUNTING_INHERITED.
  friend SupportsThreadSafeWeakPtr<WindowsGeolocationPermissionRequest>;

  virtual ~WindowsGeolocationPermissionRequest() {
    Stop();
    MOZ_ASSERT(mToken.value == 0);
  }

  void StopIfLocationIsPermitted() {
    if (LocationIsPermittedHint()) {
      Stop();
    }
  }

  nsresult DismissPrompt() {
    nsresult rv;
    nsCOMPtr<nsIGeolocationUIUtilsWin> utils =
        do_GetService("@mozilla.org/geolocation/ui-utils-win;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    return utils->DismissPrompts(mBrowsingContext);
  }

  // This IAppCapability object must be held for the duration of the period
  // where we listen for location permission changes or else the callback
  // will not be called.
  RefPtr<IAppCapability> mAppCapability;
  ParentRequestResolver mResolver;
  RefPtr<BrowsingContext> mBrowsingContext;
  EventRegistrationToken mToken;
  bool mIsRunning = false;
};

// Opens Windows geolocation settings and cancels the geolocation request on
// error.
void OpenWindowsLocationSettings(
    SystemGeolocationPermissionRequest* aPermissionRequest) {
  auto cancelRequest = MakeScopeExit([&]() { aPermissionRequest->Stop(); });

  ComPtr<IUriRuntimeClassFactory> uriFactory =
      CreateFromActivationFactory<IUriRuntimeClassFactory>(kUriGuid);
  NS_ENSURE_TRUE_VOID(uriFactory);

  RefPtr<IUriRuntimeClass> uri;
  HRESULT hr = uriFactory->CreateUri(
      HStringReference(kLocationSettingsPage).Get(), getter_AddRefs(uri));
  NS_ENSURE_TRUE_VOID(SUCCEEDED(hr));

  ComPtr<ILauncherStatics> launcherStatics =
      CreateFromActivationFactory<ILauncherStatics>(kLauncherGuid);
  NS_ENSURE_TRUE_VOID(launcherStatics);

  RefPtr<IAsyncOperation<bool>> handler;
  hr = launcherStatics->LaunchUriAsync(uri, getter_AddRefs(handler));
  NS_ENSURE_TRUE_VOID(SUCCEEDED(hr));

  // The IAsyncOperation is similar to a promise so there is no race here,
  // despite us adding this callback after requesting launch instead of before.
  handler->put_Completed(
      Callback<IAsyncOperationCompletedHandler<bool>>(
          [permissionRequest = RefPtr{aPermissionRequest}](
              IAsyncOperation<bool>* asyncInfo, AsyncStatus status) {
            unsigned char verdict = 0;
            asyncInfo->GetResults(&verdict);
            if (!verdict) {
              permissionRequest->Stop();
            }
            return S_OK;
          })
          .Get());

  cancelRequest.release();
}

class LocationPermissionWifiScanListener final : public nsIWifiListener {
 public:
  NS_DECL_ISUPPORTS

  explicit LocationPermissionWifiScanListener(
      SystemGeolocationPermissionRequest* aRequest)
      : mRequest(aRequest) {}

  NS_IMETHOD OnChange(const nsTArray<RefPtr<nsIWifiAccessPoint>>&) override {
    // We will remove ourselves from the nsIWifiMonitor, which is our owner.
    // Hold a strong reference to ourselves until we complete the callback.
    RefPtr<LocationPermissionWifiScanListener> self = this;
    return PermissionWasDecided();
  }

  NS_IMETHOD OnError(nsresult) override {
    // We will remove ourselves from the nsIWifiMonitor, which is our owner.
    // Hold a strong reference to ourselves until we complete the callback.
    RefPtr<LocationPermissionWifiScanListener> self = this;
    return PermissionWasDecided();
  }

 private:
  virtual ~LocationPermissionWifiScanListener() = default;
  RefPtr<SystemGeolocationPermissionRequest> mRequest;

  // Any response to our wifi scan request means that the user has selected
  // either to grant or deny permission in the Windows dialog.  Either way, we
  // are done asking for permission, so Stop the permission request.
  nsresult PermissionWasDecided() {
    nsCOMPtr<nsIWifiMonitor> wifiMonitor = components::WifiMonitor::Service();
    NS_ENSURE_TRUE(wifiMonitor, NS_ERROR_FAILURE);
    wifiMonitor->StopWatching(this);
    mRequest->Stop();
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(LocationPermissionWifiScanListener, nsIWifiListener)

}  // namespace

//-----------------------------------------------------------------------------

SystemGeolocationPermissionBehavior GetGeolocationPermissionBehavior() {
  if (SystemWillPromptForPermissionHint()) {
    return SystemGeolocationPermissionBehavior::SystemWillPromptUser;
  }
  if (!LocationIsPermittedHint()) {
    return SystemGeolocationPermissionBehavior::GeckoWillPromptUser;
  }
  return SystemGeolocationPermissionBehavior::NoPrompt;
}

already_AddRefed<SystemGeolocationPermissionRequest>
RequestLocationPermissionFromUser(BrowsingContext* aBrowsingContext,
                                  ParentRequestResolver&& aResolver) {
  RefPtr<WindowsGeolocationPermissionRequest> permissionRequest =
      new WindowsGeolocationPermissionRequest(aBrowsingContext,
                                              std::move(aResolver));
  permissionRequest->Initialize();
  if (permissionRequest->IsStopped()) {
    return nullptr;
  }
  if (SystemWillPromptForPermissionHint()) {
    // To tell the system to prompt for permission, run one wifi scan (no need
    // to poll). We won't use the result -- either the user will grant
    // geolocation permission, meaning we will not need wifi scanning, or the
    // user will deny permission, in which case no scan can be done.  We just
    // want the prompt.
    nsCOMPtr<nsIWifiMonitor> wifiMonitor = components::WifiMonitor::Service();
    NS_ENSURE_TRUE(wifiMonitor, nullptr);
    auto listener =
        MakeRefPtr<LocationPermissionWifiScanListener>(permissionRequest);
    wifiMonitor->StartWatching(listener, false);
  } else {
    OpenWindowsLocationSettings(permissionRequest);
  }
  return permissionRequest.forget();
}

}  // namespace mozilla::dom::geolocation
