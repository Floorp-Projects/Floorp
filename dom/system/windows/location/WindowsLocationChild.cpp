/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "WindowsLocationChild.h"
#include "nsCOMPtr.h"
#include "WindowsLocationProvider.h"
#include "mozilla/dom/GeolocationPosition.h"
#include "mozilla/dom/GeolocationPositionErrorBinding.h"
#include "mozilla/Telemetry.h"
#include "nsIGeolocationProvider.h"

#include <locationapi.h>

namespace mozilla::dom {

extern LazyLogModule gWindowsLocationProviderLog;
#define LOG(...) \
  MOZ_LOG(gWindowsLocationProviderLog, LogLevel::Debug, (__VA_ARGS__))

class LocationEvent final : public ILocationEvents {
 public:
  explicit LocationEvent(WindowsLocationChild* aActor)
      : mActor(aActor), mRefCnt(0) {}

  // IUnknown interface
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;

  // ILocationEvents interface
  STDMETHODIMP OnStatusChanged(REFIID aReportType,
                               LOCATION_REPORT_STATUS aStatus) override;
  STDMETHODIMP OnLocationChanged(REFIID aReportType,
                                 ILocationReport* aReport) override;

 private:
  // Making this a WeakPtr breaks the following cycle of strong references:
  // WindowsLocationChild -> ILocation -> ILocationEvents (this)
  //   -> WindowsLocationChild.
  WeakPtr<WindowsLocationChild> mActor;

  ULONG mRefCnt;
};

STDMETHODIMP_(ULONG)
LocationEvent::AddRef() { return InterlockedIncrement(&mRefCnt); }

STDMETHODIMP_(ULONG)
LocationEvent::Release() {
  ULONG count = InterlockedDecrement(&mRefCnt);
  if (!count) {
    delete this;
    return 0;
  }
  return count;
}

STDMETHODIMP
LocationEvent::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) {
    return E_INVALIDARG;
  }

  if (iid == IID_IUnknown) {
    *ppv = static_cast<IUnknown*>(this);
  } else if (iid == IID_ILocationEvents) {
    *ppv = static_cast<ILocationEvents*>(this);
  } else {
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}

STDMETHODIMP
LocationEvent::OnStatusChanged(REFIID aReportType,
                               LOCATION_REPORT_STATUS aStatus) {
  LOG("LocationEvent::OnStatusChanged(%p, %p, %s, %04x)", this, mActor.get(),
      aReportType == IID_ILatLongReport ? "true" : "false",
      static_cast<uint32_t>(aStatus));

  if (!mActor || aReportType != IID_ILatLongReport) {
    return S_OK;
  }

  // When registering event, REPORT_INITIALIZING is fired at first.
  // Then, when the location is found, REPORT_RUNNING is fired.
  // We ignore those messages.
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

  mActor->SendFailed(err);
  return S_OK;
}

STDMETHODIMP
LocationEvent::OnLocationChanged(REFIID aReportType, ILocationReport* aReport) {
  LOG("LocationEvent::OnLocationChanged(%p, %p, %s)", this, mActor.get(),
      aReportType == IID_ILatLongReport ? "true" : "false");

  if (!mActor || aReportType != IID_ILatLongReport) {
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
  mActor->SendUpdate(position);

  return S_OK;
}

WindowsLocationChild::WindowsLocationChild() {
  LOG("WindowsLocationChild::WindowsLocationChild(%p)", this);
}

WindowsLocationChild::~WindowsLocationChild() {
  LOG("WindowsLocationChild::~WindowsLocationChild(%p)", this);
}

::mozilla::ipc::IPCResult WindowsLocationChild::RecvStartup() {
  LOG("WindowsLocationChild::RecvStartup(%p, %p)", this, mLocation.get());
  if (mLocation) {
    return IPC_OK();
  }

  RefPtr<ILocation> location;
  if (FAILED(::CoCreateInstance(CLSID_Location, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ILocation, getter_AddRefs(location)))) {
    LOG("WindowsLocationChild(%p) failed to create ILocation", this);
    // We will use MLS provider
    SendFailed(GeolocationPositionError_Binding::POSITION_UNAVAILABLE);
    return IPC_OK();
  }

  IID reportTypes[] = {IID_ILatLongReport};
  if (FAILED(location->RequestPermissions(nullptr, reportTypes, 1, FALSE))) {
    LOG("WindowsLocationChild(%p) failed to set ILocation permissions", this);
    // We will use MLS provider
    SendFailed(GeolocationPositionError_Binding::POSITION_UNAVAILABLE);
    return IPC_OK();
  }

  mLocation = location;
  return IPC_OK();
}

::mozilla::ipc::IPCResult WindowsLocationChild::RecvSetHighAccuracy(
    bool aEnable) {
  LOG("WindowsLocationChild::RecvSetHighAccuracy(%p, %p, %s)", this,
      mLocation.get(), aEnable ? "true" : "false");

  // We sometimes call SetHighAccuracy before Startup, so we save the
  // request and set it later, in RegisterForReport.
  mHighAccuracy = aEnable;

  return IPC_OK();
}

::mozilla::ipc::IPCResult WindowsLocationChild::RecvRegisterForReport() {
  LOG("WindowsLocationChild::RecvRegisterForReport(%p, %p)", this,
      mLocation.get());

  if (!mLocation) {
    SendFailed(GeolocationPositionError_Binding::POSITION_UNAVAILABLE);
    return IPC_OK();
  }

  LOCATION_DESIRED_ACCURACY desiredAccuracy;
  if (mHighAccuracy) {
    desiredAccuracy = LOCATION_DESIRED_ACCURACY_HIGH;
  } else {
    desiredAccuracy = LOCATION_DESIRED_ACCURACY_DEFAULT;
  }

  if (NS_WARN_IF(FAILED(mLocation->SetDesiredAccuracy(IID_ILatLongReport,
                                                      desiredAccuracy)))) {
    SendFailed(GeolocationPositionError_Binding::POSITION_UNAVAILABLE);
    return IPC_OK();
  }

  auto event = MakeRefPtr<LocationEvent>(this);
  if (NS_WARN_IF(
          FAILED(mLocation->RegisterForReport(event, IID_ILatLongReport, 0)))) {
    SendFailed(GeolocationPositionError_Binding::POSITION_UNAVAILABLE);
  }

  LOG("WindowsLocationChild::RecvRegisterForReport successfully registered");
  return IPC_OK();
}

::mozilla::ipc::IPCResult WindowsLocationChild::RecvUnregisterForReport() {
  LOG("WindowsLocationChild::RecvUnregisterForReport(%p, %p)", this,
      mLocation.get());

  if (!mLocation) {
    return IPC_OK();
  }

  // This will free the LocationEvent we created in RecvRegisterForReport.
  Unused << NS_WARN_IF(
      FAILED(mLocation->UnregisterForReport(IID_ILatLongReport)));

  // The ILocation object is not reusable.  Unregistering, restarting and
  // re-registering for reports does not work;  the callback is never
  // called in that case.  For that reason, we re-create the ILocation
  // object with a call to Startup after unregistering if we need it again.
  mLocation = nullptr;
  return IPC_OK();
}

void WindowsLocationChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG("WindowsLocationChild::ActorDestroy(%p, %p)", this, mLocation.get());
  mLocation = nullptr;
}

}  // namespace mozilla::dom
