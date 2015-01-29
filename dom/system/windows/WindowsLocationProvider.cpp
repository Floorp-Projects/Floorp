/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsLocationProvider.h"
#include "nsGeoPosition.h"
#include "nsIDOMGeoPositionError.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

class LocationEvent MOZ_FINAL : public ILocationEvents
{
public:
  LocationEvent(nsIGeolocationUpdate* aCallback)
    : mCallback(aCallback), mCount(0) {
  }

  // IUnknown interface
  STDMETHODIMP_(ULONG) AddRef() MOZ_OVERRIDE;
  STDMETHODIMP_(ULONG) Release() MOZ_OVERRIDE;
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv) MOZ_OVERRIDE;

  // ILocationEvents interface
  STDMETHODIMP OnStatusChanged(REFIID aReportType,
                               LOCATION_REPORT_STATUS aStatus) MOZ_OVERRIDE;
  STDMETHODIMP OnLocationChanged(REFIID aReportType,
                                 ILocationReport *aReport) MOZ_OVERRIDE;

private:
  nsCOMPtr<nsIGeolocationUpdate> mCallback;
  ULONG mCount;
};

STDMETHODIMP_(ULONG)
LocationEvent::AddRef()
{
  return InterlockedIncrement(&mCount);
}

STDMETHODIMP_(ULONG)
LocationEvent::Release()
{
  ULONG count = InterlockedDecrement(&mCount);
  if (!count) {
    delete this;
    return 0;
  }
  return count;
}

STDMETHODIMP
LocationEvent::QueryInterface(REFIID iid, void** ppv)
{
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
                               LOCATION_REPORT_STATUS aStatus)
{
  if (aReportType != IID_ILatLongReport) {
    return S_OK;
  }

  uint16_t err;
  switch (aStatus) {
  case REPORT_ACCESS_DENIED:
    err = nsIDOMGeoPositionError::PERMISSION_DENIED;
    break;
  case REPORT_ERROR:
    err = nsIDOMGeoPositionError::POSITION_UNAVAILABLE;
    break;
  default:
    return S_OK;
  }

  mCallback->NotifyError(err);
  return S_OK;
}

STDMETHODIMP
LocationEvent::OnLocationChanged(REFIID aReportType,
                                 ILocationReport *aReport)
{
  if (aReportType != IID_ILatLongReport) {
    return S_OK;
  }

  nsRefPtr<ILatLongReport> latLongReport;
  if (FAILED(aReport->QueryInterface(IID_ILatLongReport,
                                     getter_AddRefs(latLongReport)))) {
    return E_FAIL;
  }

  DOUBLE latitude = 0.0;
  latLongReport->GetLatitude(&latitude);

  DOUBLE longitude = 0.0;
  latLongReport->GetLongitude(&longitude);

  DOUBLE alt = 0.0;
  latLongReport->GetAltitude(&alt);

  DOUBLE herror = 0.0;
  latLongReport->GetErrorRadius(&herror);

  DOUBLE verror = 0.0;
  latLongReport->GetAltitudeError(&verror);

  nsRefPtr<nsGeoPosition> position =
    new nsGeoPosition(latitude, longitude, alt, herror, verror, 0.0, 0.0,
                      PR_Now());
  mCallback->Update(position);

  return S_OK;
}

NS_IMPL_ISUPPORTS(WindowsLocationProvider, nsIGeolocationProvider)

WindowsLocationProvider::WindowsLocationProvider()
{
}

NS_IMETHODIMP
WindowsLocationProvider::Startup()
{
  nsRefPtr<ILocation> location;
  if (FAILED(::CoCreateInstance(CLSID_Location, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ILocation,
                                getter_AddRefs(location)))) {
    return NS_ERROR_FAILURE;
  }

  IID reportTypes[] = { IID_ILatLongReport };
  if (FAILED(location->RequestPermissions(nullptr, reportTypes, 1, FALSE))) {
    return NS_ERROR_FAILURE;
  }

  mLocation = location;
  return NS_OK;
}

NS_IMETHODIMP
WindowsLocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
  nsRefPtr<LocationEvent> event = new LocationEvent(aCallback);
  if (FAILED(mLocation->RegisterForReport(event, IID_ILatLongReport, 0))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
WindowsLocationProvider::Shutdown()
{
  if (mLocation) {
    mLocation->UnregisterForReport(IID_ILatLongReport);
    mLocation = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
WindowsLocationProvider::SetHighAccuracy(bool enable)
{
  if (!mLocation) {
    return NS_ERROR_FAILURE;
  }

  LOCATION_DESIRED_ACCURACY desiredAccuracy;
  if (enable) {
    desiredAccuracy = LOCATION_DESIRED_ACCURACY_HIGH;
  } else {
    desiredAccuracy = LOCATION_DESIRED_ACCURACY_DEFAULT;
  }
  if (FAILED(mLocation->SetDesiredAccuracy(IID_ILatLongReport,
                                           desiredAccuracy))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
