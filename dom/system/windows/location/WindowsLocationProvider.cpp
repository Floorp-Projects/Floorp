/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsLocationProvider.h"
#include "WindowsLocationParent.h"
#include "mozilla/dom/WindowsUtilsParent.h"
#include "GeolocationPosition.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "prtime.h"
#include "MLSFallback.h"
#include "mozilla/Attributes.h"
#include "mozilla/Logging.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/GeolocationPositionErrorBinding.h"

namespace mozilla::dom {

LazyLogModule gWindowsLocationProviderLog("WindowsLocationProvider");
#define LOG(...) \
  MOZ_LOG(gWindowsLocationProviderLog, LogLevel::Debug, (__VA_ARGS__))

class MLSUpdate : public nsIGeolocationUpdate {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONUPDATE
  explicit MLSUpdate(nsIGeolocationUpdate* aCallback) : mCallback(aCallback) {}

 private:
  nsCOMPtr<nsIGeolocationUpdate> mCallback;
  virtual ~MLSUpdate() {}
};

NS_IMPL_ISUPPORTS(MLSUpdate, nsIGeolocationUpdate);

NS_IMETHODIMP
MLSUpdate::Update(nsIDOMGeoPosition* aPosition) {
  if (!mCallback) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  aPosition->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return NS_ERROR_FAILURE;
  }
  Telemetry::Accumulate(Telemetry::GEOLOCATION_WIN8_SOURCE_IS_MLS, true);
  return mCallback->Update(aPosition);
}
NS_IMETHODIMP
MLSUpdate::NotifyError(uint16_t aError) {
  if (!mCallback) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIGeolocationUpdate> callback(mCallback);
  return callback->NotifyError(aError);
}

NS_IMPL_ISUPPORTS(WindowsLocationProvider, nsIGeolocationProvider)

WindowsLocationProvider::WindowsLocationProvider() {
  LOG("WindowsLocationProvider::WindowsLocationProvider(%p)", this);
  MOZ_ASSERT(XRE_IsParentProcess());
  MaybeCreateLocationActor();
}

WindowsLocationProvider::~WindowsLocationProvider() {
  LOG("WindowsLocationProvider::~WindowsLocationProvider(%p,%p,%p)", this,
      mActor.get(), mActorPromise.get());
  Send__delete__();
  ReleaseUtilityProcess();
  CancelMLSProvider();
}

void WindowsLocationProvider::MaybeCreateLocationActor() {
  LOG("WindowsLocationProvider::MaybeCreateLocationActor(%p)", this);
  if (mActor || mActorPromise) {
    return;
  }

  auto utilityProc = mozilla::ipc::UtilityProcessManager::GetSingleton();
  MOZ_ASSERT(utilityProc);

  // Create a PWindowsLocation actor in the Windows utility process.
  // This will attempt to launch the process if it doesn't already exist.
  RefPtr<WindowsLocationProvider> self = this;
  auto wuPromise = utilityProc->GetWindowsUtilsPromise();
  mActorPromise = wuPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self](RefPtr<WindowsUtilsParent> wup) {
        self->mActorPromise = nullptr;
        auto actor = MakeRefPtr<WindowsLocationParent>(self);
        if (!wup->SendPWindowsLocationConstructor(actor)) {
          LOG("WindowsLocationProvider(%p) SendPWindowsLocationConstructor "
              "failed",
              self.get());
          actor->DetachFromLocationProvider();
          self->mActor = nullptr;
          return WindowsLocationPromise::CreateAndReject(false, __func__);
        }
        LOG("WindowsLocationProvider connected to actor (%p,%p,%p)", self.get(),
            self->mActor.get(), self->mActorPromise.get());
        self->mActor = actor;
        return WindowsLocationPromise::CreateAndResolve(self->mActor, __func__);
      },

      [self](nsresult aError) {
        LOG("WindowsLocationProvider failed to connect to actor (%p,%p,%p)",
            self.get(), self->mActor.get(), self->mActorPromise.get());
        self->mActorPromise = nullptr;
        return WindowsLocationPromise::CreateAndReject(false, __func__);
      });

  if (mActor) {
    // Utility process already existed and mActorPromise was resolved
    // immediately.
    mActorPromise = nullptr;
  }
}

void WindowsLocationProvider::ReleaseUtilityProcess() {
  LOG("WindowsLocationProvider::ReleaseUtilityProcess(%p)", this);
  auto utilityProc = mozilla::ipc::UtilityProcessManager::GetIfExists();
  if (utilityProc) {
    utilityProc->ReleaseWindowsUtils();
  }
}

template <typename Fn>
bool WindowsLocationProvider::WhenActorIsReady(Fn&& fn) {
  if (mActor) {
    return fn(mActor);
  }

  if (mActorPromise) {
    mActorPromise->Then(
        GetCurrentSerialEventTarget(), __func__,
        [fn](const RefPtr<WindowsLocationParent>& actor) {
          Unused << fn(actor.get());
          return actor;
        },
        [](bool) { return false; });
    return true;
  }

  // The remote process failed to start.
  return false;
}

bool WindowsLocationProvider::SendStartup() {
  LOG("WindowsLocationProvider::SendStartup(%p)", this);
  MaybeCreateLocationActor();
  return WhenActorIsReady(
      [](WindowsLocationParent* actor) { return actor->SendStartup(); });
}

bool WindowsLocationProvider::SendRegisterForReport(
    nsIGeolocationUpdate* aCallback) {
  LOG("WindowsLocationProvider::SendRegisterForReport(%p)", this);
  RefPtr<WindowsLocationProvider> self = this;
  RefPtr<nsIGeolocationUpdate> cb = aCallback;
  return WhenActorIsReady([self, cb](WindowsLocationParent* actor) {
    MOZ_ASSERT(!self->mCallback);
    if (actor->SendRegisterForReport()) {
      self->mCallback = cb;
      return true;
    }
    return false;
  });
}

bool WindowsLocationProvider::SendUnregisterForReport() {
  LOG("WindowsLocationProvider::SendUnregisterForReport(%p)", this);
  RefPtr<WindowsLocationProvider> self = this;
  return WhenActorIsReady([self](WindowsLocationParent* actor) {
    self->mCallback = nullptr;
    if (actor->SendUnregisterForReport()) {
      return true;
    }
    return false;
  });
}

bool WindowsLocationProvider::SendSetHighAccuracy(bool aEnable) {
  LOG("WindowsLocationProvider::SendSetHighAccuracy(%p)", this);
  return WhenActorIsReady([aEnable](WindowsLocationParent* actor) {
    return actor->SendSetHighAccuracy(aEnable);
  });
}

bool WindowsLocationProvider::Send__delete__() {
  LOG("WindowsLocationProvider::Send__delete__(%p)", this);
  return WhenActorIsReady([self = RefPtr{this}](WindowsLocationParent*) {
    if (WindowsLocationParent::Send__delete__(self->mActor)) {
      if (self->mActor) {
        self->mActor->DetachFromLocationProvider();
        self->mActor = nullptr;
      }
      return true;
    }
    return false;
  });
}

void WindowsLocationProvider::RecvUpdate(
    RefPtr<nsIDOMGeoPosition> aGeoPosition) {
  LOG("WindowsLocationProvider::RecvUpdate(%p)", this);
  if (!mCallback) {
    return;
  }

  mCallback->Update(aGeoPosition.get());

  Telemetry::Accumulate(Telemetry::GEOLOCATION_WIN8_SOURCE_IS_MLS, false);
}

void WindowsLocationProvider::RecvFailed(uint16_t err) {
  LOG("WindowsLocationProvider::RecvFailed(%p)", this);
  // Cannot get current location at this time.  We use MLS instead.
  if (mMLSProvider || !mCallback) {
    return;
  }

  if (NS_SUCCEEDED(CreateAndWatchMLSProvider(mCallback))) {
    return;
  }

  // No ILocation and no MLS, so we have failed completely.
  // We keep strong references to objects that we need to guarantee
  // will live past the NotifyError callback.
  RefPtr<WindowsLocationProvider> self = this;
  nsCOMPtr<nsIGeolocationUpdate> callback = mCallback;
  callback->NotifyError(err);
}

void WindowsLocationProvider::ActorStopped() {
  // ActorDestroy has run.  Make sure UtilityProcessHost no longer tries to use
  // it.
  ReleaseUtilityProcess();

  if (mWatching) {
    // Treat as remote geolocation error, which will cause it to fallback
    // to MLS if it hasn't already.
    mWatching = false;
    RecvFailed(GeolocationPositionError_Binding::POSITION_UNAVAILABLE);
    return;
  }

  MOZ_ASSERT(!mActorPromise);
  if (mActor) {
    mActor->DetachFromLocationProvider();
    mActor = nullptr;
  }
}

NS_IMETHODIMP
WindowsLocationProvider::Startup() {
  LOG("WindowsLocationProvider::Startup(%p, %p, %p)", this, mActor.get(),
      mActorPromise.get());
  // If this fails, we will use the MLS fallback.
  SendStartup();
  return NS_OK;
}

NS_IMETHODIMP
WindowsLocationProvider::Watch(nsIGeolocationUpdate* aCallback) {
  LOG("WindowsLocationProvider::Watch(%p, %p, %p, %p, %d)", this, mActor.get(),
      mActorPromise.get(), aCallback, mWatching);
  if (mWatching) {
    return NS_OK;
  }

  if (SendRegisterForReport(aCallback)) {
    mWatching = true;
    return NS_OK;
  }

  // Couldn't send request.  We will use MLS instead.
  return CreateAndWatchMLSProvider(aCallback);
}

NS_IMETHODIMP
WindowsLocationProvider::Shutdown() {
  LOG("WindowsLocationProvider::Shutdown(%p, %p, %p)", this, mActor.get(),
      mActorPromise.get());

  if (mWatching) {
    SendUnregisterForReport();
    mWatching = false;
  }

  CancelMLSProvider();
  return NS_OK;
}

NS_IMETHODIMP
WindowsLocationProvider::SetHighAccuracy(bool enable) {
  LOG("WindowsLocationProvider::SetHighAccuracy(%p, %p, %p, %s)", this,
      mActor.get(), mActorPromise.get(), enable ? "true" : "false");
  if (mMLSProvider) {
    // Ignored when running MLS fallback.
    return NS_OK;
  }

  if (!SendSetHighAccuracy(enable)) {
    return NS_ERROR_FAILURE;
  }

  // Since we SendSetHighAccuracy asynchronously, we cannot say for sure
  // that it will succeed.  If it does fail then we will get a
  // RecvFailed IPC message, which will cause a fallback to MLS.
  return NS_OK;
}

nsresult WindowsLocationProvider::CreateAndWatchMLSProvider(
    nsIGeolocationUpdate* aCallback) {
  LOG("WindowsLocationProvider::CreateAndWatchMLSProvider"
      "(%p, %p, %p, %p, %p)",
      this, mMLSProvider.get(), mActor.get(), mActorPromise.get(), aCallback);

  if (mMLSProvider) {
    return NS_OK;
  }

  mMLSProvider = new MLSFallback(0);
  return mMLSProvider->Startup(new MLSUpdate(aCallback));
}

void WindowsLocationProvider::CancelMLSProvider() {
  LOG("WindowsLocationProvider::CancelMLSProvider"
      "(%p, %p, %p, %p, %p)",
      this, mMLSProvider.get(), mActor.get(), mActorPromise.get(),
      mCallback.get());

  if (!mMLSProvider) {
    return;
  }

  mMLSProvider->Shutdown();
  mMLSProvider = nullptr;
}

#undef LOG

}  // namespace mozilla::dom
