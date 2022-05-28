/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsLocationProvider.h"
#include "GeolocationPosition.h"
#include "nsComponentManagerUtils.h"
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

NS_IMPL_ISUPPORTS(WindowsLocationProvider::MLSUpdate, nsIGeolocationUpdate);

WindowsLocationProvider::MLSUpdate::MLSUpdate(nsIGeolocationUpdate* aCallback)
    : mCallback(aCallback) {}

NS_IMETHODIMP
WindowsLocationProvider::MLSUpdate::Update(nsIDOMGeoPosition* aPosition) {
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
WindowsLocationProvider::MLSUpdate::NotifyError(uint16_t aError) {
  if (!mCallback) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIGeolocationUpdate> callback(mCallback);
  return callback->NotifyError(aError);
}

class LocationEvent final : public ILocationEvents {
 public:
  LocationEvent(nsIGeolocationUpdate* aCallback,
                WindowsLocationProvider* aProvider)
      : mCallback(aCallback), mProvider(aProvider), mCount(0) {}

  // IUnknown interface
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;

  // ILocationEvents interface
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  STDMETHODIMP OnStatusChanged(REFIID aReportType,
                               LOCATION_REPORT_STATUS aStatus) override;
  STDMETHODIMP OnLocationChanged(REFIID aReportType,
                                 ILocationReport* aReport) override;

 private:
  nsCOMPtr<nsIGeolocationUpdate> mCallback;
  RefPtr<WindowsLocationProvider> mProvider;
  ULONG mCount;
};

STDMETHODIMP_(ULONG)
LocationEvent::AddRef() { return InterlockedIncrement(&mCount); }

STDMETHODIMP_(ULONG)
LocationEvent::Release() {
  ULONG count = InterlockedDecrement(&mCount);
  if (!count) {
    delete this;
    return 0;
  }
  return count;
}

STDMETHODIMP
LocationEvent::QueryInterface(REFIID iid, void** ppv) {
  if (iid == IID_IUnknown) {
    *ppv = static_cast<IUnknown*>(this);
  } else if (iid == IID_ILocationEvents) {
    *ppv = static_cast<ILocationEvents*>(this);
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP
LocationEvent::OnStatusChanged(REFIID aReportType,
                               LOCATION_REPORT_STATUS aStatus) {
  if (aReportType != IID_ILatLongReport) {
    return S_OK;
  }

  // When registering event, REPORT_INITIALIZING is fired at first.
  // Then, when the location is found, REPORT_RUNNING is fired.
  if (aStatus == REPORT_RUNNING) {
    // location is found by Windows Location provider, we use it.
    mProvider->CancelMLSProvider();
    return S_OK;
  }

  // Cannot get current location at this time.  We use MLS instead until
  // Location API returns RUNNING status.
  if (NS_SUCCEEDED(mProvider->CreateAndWatchMLSProvider(mCallback))) {
    return S_OK;
  }

  // Cannot watch location by MLS provider.  We must return error by
  // Location API.
  uint16_t err;
  switch (aStatus) {
    case REPORT_ACCESS_DENIED:
      err = GeolocationPositionError_Binding::PERMISSION_DENIED;
      break;
    case REPORT_NOT_SUPPORTED:
    case REPORT_ERROR:
      err = GeolocationPositionError_Binding::POSITION_UNAVAILABLE;
      break;
    default:
      return S_OK;
  }
  nsCOMPtr<nsIGeolocationUpdate> callback(mCallback);
  callback->NotifyError(err);
  return S_OK;
}

STDMETHODIMP
LocationEvent::OnLocationChanged(REFIID aReportType, ILocationReport* aReport) {
  if (aReportType != IID_ILatLongReport) {
    return S_OK;
  }

  RefPtr<ILatLongReport> latLongReport;
  if (FAILED(aReport->QueryInterface(IID_ILatLongReport,
                                     getter_AddRefs(latLongReport)))) {
    return E_FAIL;
  }

  DOUBLE latitude = 0.0;
  latLongReport->GetLatitude(&latitude);

  DOUBLE longitude = 0.0;
  latLongReport->GetLongitude(&longitude);

  DOUBLE alt = UnspecifiedNaN<double>();
  latLongReport->GetAltitude(&alt);

  DOUBLE herror = 0.0;
  latLongReport->GetErrorRadius(&herror);

  DOUBLE verror = UnspecifiedNaN<double>();
  latLongReport->GetAltitudeError(&verror);

  double heading = UnspecifiedNaN<double>();
  double speed = UnspecifiedNaN<double>();

  // nsGeoPositionCoords will convert NaNs to null for optional properties of
  // the JavaScript Coordinates object.
  RefPtr<nsGeoPosition> position =
      new nsGeoPosition(latitude, longitude, alt, herror, verror, heading,
                        speed, PR_Now() / PR_USEC_PER_MSEC);
  mCallback->Update(position);

  Telemetry::Accumulate(Telemetry::GEOLOCATION_WIN8_SOURCE_IS_MLS, false);

  return S_OK;
}

NS_IMPL_ISUPPORTS(WindowsLocationProvider, nsIGeolocationProvider)

WindowsLocationProvider::WindowsLocationProvider() {
  LOG("WindowsLocationProvider::WindowsLocationProvider(%p)\n", this);
}

WindowsLocationProvider::~WindowsLocationProvider() {
  LOG("WindowsLocationProvider::~WindowsLocationProvider(%p, %p)\n", this,
      mLocation.get());
}

NS_IMETHODIMP
WindowsLocationProvider::Startup() {
  LOG("WindowsLocationProvider::Startup(%p, %p)\n", this, mLocation.get());
  if (mLocation) {
    return NS_OK;
  }

  RefPtr<ILocation> location;
  if (FAILED(::CoCreateInstance(CLSID_Location, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ILocation, getter_AddRefs(location)))) {
    // We will use MLS provider
    return NS_OK;
  }

  IID reportTypes[] = {IID_ILatLongReport};
  if (FAILED(location->RequestPermissions(nullptr, reportTypes, 1, FALSE))) {
    // We will use MLS provider
    return NS_OK;
  }

  mLocation = location;
  return NS_OK;
}

NS_IMETHODIMP
WindowsLocationProvider::Watch(nsIGeolocationUpdate* aCallback) {
  LOG("WindowsLocationProvider::Watch(%p, %p, %d)\n", this, mLocation.get(),
      mWatching);
  if (mLocation) {
    if (mWatching) {
      return NS_OK;
    }
    RefPtr<LocationEvent> event = new LocationEvent(aCallback, this);
    if (SUCCEEDED(mLocation->RegisterForReport(event, IID_ILatLongReport, 0))) {
      mWatching = true;
      return NS_OK;
    }
  }

  // Cannot use Location API.  We will use MLS instead.
  LOG(" > MLS fallback\n");
  mLocation = nullptr;

  return CreateAndWatchMLSProvider(aCallback);
}

NS_IMETHODIMP
WindowsLocationProvider::Shutdown() {
  LOG("WindowsLocationProvider::Shutdown(%p, %p)\n", this, mLocation.get());
  if (mLocation) {
    if (mWatching) {
      mLocation->UnregisterForReport(IID_ILatLongReport);
    }
    mLocation = nullptr;
    mWatching = false;
  }

  CancelMLSProvider();
  return NS_OK;
}

NS_IMETHODIMP
WindowsLocationProvider::SetHighAccuracy(bool enable) {
  if (!mLocation) {
    // MLS provider doesn't support HighAccuracy
    return NS_OK;
  }

  LOCATION_DESIRED_ACCURACY desiredAccuracy;
  if (enable) {
    desiredAccuracy = LOCATION_DESIRED_ACCURACY_HIGH;
  } else {
    desiredAccuracy = LOCATION_DESIRED_ACCURACY_DEFAULT;
  }
  if (FAILED(
          mLocation->SetDesiredAccuracy(IID_ILatLongReport, desiredAccuracy))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult WindowsLocationProvider::CreateAndWatchMLSProvider(
    nsIGeolocationUpdate* aCallback) {
  if (mMLSProvider) {
    return NS_OK;
  }

  mMLSProvider = new MLSFallback(0);
  return mMLSProvider->Startup(new MLSUpdate(aCallback));
}

void WindowsLocationProvider::CancelMLSProvider() {
  if (!mMLSProvider) {
    return;
  }

  mMLSProvider->Shutdown();
  mMLSProvider = nullptr;
}

#undef LOG

}  // namespace mozilla::dom
