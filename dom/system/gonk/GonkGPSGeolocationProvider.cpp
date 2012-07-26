/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pthread.h>
#include <hardware/gps.h>

#include "GonkGPSGeolocationProvider.h"
#include "SystemWorkerManager.h"
#include "mozilla/Preferences.h"
#include "nsGeoPosition.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINetworkManager.h"
#include "nsIRadioInterfaceLayer.h"
#include "nsThreadUtils.h"

#ifdef AGPS_TYPE_INVALID
#define AGPS_HAVE_DUAL_APN
#endif

#define DEBUG_GPS 0

using namespace mozilla;

static const int kDefaultPeriod = 1000; // ms

NS_IMPL_ISUPPORTS2(GonkGPSGeolocationProvider, nsIGeolocationProvider, nsIRILDataCallback)

GonkGPSGeolocationProvider* GonkGPSGeolocationProvider::sSingleton;
GpsCallbacks GonkGPSGeolocationProvider::mCallbacks = {
  sizeof(GpsCallbacks),
  LocationCallback,
  StatusCallback,
  SvStatusCallback,
  NmeaCallback,
  SetCapabilitiesCallback,
  AcquireWakelockCallback,
  ReleaseWakelockCallback,
  CreateThreadCallback,
#ifdef GPS_CAPABILITY_ON_DEMAND_TIME
  RequestUtcTimeCallback,
#endif
};

AGpsCallbacks
GonkGPSGeolocationProvider::mAGPSCallbacks = {
  AGPSStatusCallback,
  CreateThreadCallback,
};

AGpsRilCallbacks
GonkGPSGeolocationProvider::mAGPSRILCallbacks = {
  AGPSRILSetIDCallback,
  AGPSRILRefLocCallback,
  CreateThreadCallback,
};

void
GonkGPSGeolocationProvider::LocationCallback(GpsLocation* location)
{
  class UpdateLocationEvent : public nsRunnable {
  public:
    UpdateLocationEvent(nsGeoPosition* aPosition)
      : mPosition(aPosition)
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();
      nsCOMPtr<nsIGeolocationUpdate> callback = provider->mLocationCallback;
      if (callback) {
        callback->Update(mPosition);
      }
      return NS_OK;
    }
  private:
    nsRefPtr<nsGeoPosition> mPosition;
  };

  MOZ_ASSERT(location);

  nsRefPtr<nsGeoPosition> somewhere = new nsGeoPosition(location->latitude,
                                                        location->longitude,
                                                        location->altitude,
                                                        location->accuracy,
                                                        location->accuracy,
                                                        location->bearing,
                                                        location->speed,
                                                        location->timestamp);
  NS_DispatchToMainThread(new UpdateLocationEvent(somewhere));
}

void
GonkGPSGeolocationProvider::StatusCallback(GpsStatus* status)
{
}

void
GonkGPSGeolocationProvider::SvStatusCallback(GpsSvStatus* sv_info)
{
}

void
GonkGPSGeolocationProvider::NmeaCallback(GpsUtcTime timestamp, const char* nmea, int length)
{
#if DEBUG_GPS
  printf_stderr("*** nmea info\n");
  printf_stderr("timestamp:\t%lld\n", timestamp);
  printf_stderr("nmea:     \t%s\n", nmea);
  printf_stderr("length:   \t%d\n", length);
#endif
}

void
GonkGPSGeolocationProvider::SetCapabilitiesCallback(uint32_t capabilities)
{
  // Called by GPS engine in init(), hence we don't have to
  // protect the memebers

  nsRefPtr<GonkGPSGeolocationProvider> provider =
    GonkGPSGeolocationProvider::GetSingleton();

  provider->mSupportsScheduling = capabilities & GPS_CAPABILITY_SCHEDULING;
  provider->mSupportsMSB = capabilities & GPS_CAPABILITY_MSB;
  provider->mSupportsMSA = capabilities & GPS_CAPABILITY_MSA;
  provider->mSupportsSingleShot = capabilities & GPS_CAPABILITY_SINGLE_SHOT;
#ifdef GPS_CAPABILITY_ON_DEMAND_TIME
  provider->mSupportsTimeInjection = capabilities & GPS_CAPABILITY_ON_DEMAND_TIME;
#endif
}

void
GonkGPSGeolocationProvider::AcquireWakelockCallback()
{
}

void
GonkGPSGeolocationProvider::ReleaseWakelockCallback()
{
}

typedef void *(*pthread_func)(void *);

/** Callback for creating a thread that can call into the JS codes.
 */
pthread_t
GonkGPSGeolocationProvider::CreateThreadCallback(const char* name, void (*start)(void *), void* arg)
{
  pthread_t thread;
  pthread_attr_t attr;

  pthread_attr_init(&attr);

  /* Unfortunately pthread_create and the callback disagreed on what
   * start function should return.
   */
  pthread_create(&thread, &attr, reinterpret_cast<pthread_func>(start), arg);

  return thread;
}

void
GonkGPSGeolocationProvider::RequestUtcTimeCallback()
{
}

void
GonkGPSGeolocationProvider::AGPSStatusCallback(AGpsStatus* status)
{
  MOZ_ASSERT(status);

  nsRefPtr<GonkGPSGeolocationProvider> provider =
    GonkGPSGeolocationProvider::GetSingleton();

  nsCOMPtr<nsIRunnable> event;
  switch (status->status) {
    case GPS_REQUEST_AGPS_DATA_CONN:
      event = NS_NewRunnableMethod(provider, &GonkGPSGeolocationProvider::RequestDataConnection);
      break;
    case GPS_RELEASE_AGPS_DATA_CONN:
      event = NS_NewRunnableMethod(provider, &GonkGPSGeolocationProvider::ReleaseDataConnection);
      break;
  }
  if (event) {
    NS_DispatchToMainThread(event);
  }
}

void
GonkGPSGeolocationProvider::AGPSRILSetIDCallback(uint32_t flags)
{
  class RequestSetIDEvent : public nsRunnable {
  public:
    RequestSetIDEvent(uint32_t flags)
      : mFlags(flags)
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();
      provider->RequestSetID(mFlags);
      return NS_OK;
    }
  private:
    uint32_t mFlags;
  };

  NS_DispatchToMainThread(new RequestSetIDEvent(flags));
}

void
GonkGPSGeolocationProvider::AGPSRILRefLocCallback(uint32_t flags)
{
  nsRefPtr<GonkGPSGeolocationProvider> provider =
    GonkGPSGeolocationProvider::GetSingleton();

  if (flags & AGPS_RIL_REQUEST_REFLOC_CELLID) {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(provider, &GonkGPSGeolocationProvider::SetReferenceLocation);
    NS_DispatchToMainThread(event);
  }
}

GonkGPSGeolocationProvider::GonkGPSGeolocationProvider()
  : mStarted(false)
  , mSupportsScheduling(false)
  , mSupportsMSB(false) 
  , mSupportsMSA(false) 
  , mSupportsSingleShot(false)
  , mSupportsTimeInjection(false)
  , mGpsInterface(nsnull)
{
}

GonkGPSGeolocationProvider::~GonkGPSGeolocationProvider()
{
  ShutdownNow();
  sSingleton = nsnull;
}

already_AddRefed<GonkGPSGeolocationProvider>
GonkGPSGeolocationProvider::GetSingleton()
{
  if (!sSingleton)
    sSingleton = new GonkGPSGeolocationProvider();

  NS_ADDREF(sSingleton);
  return sSingleton;
}

const GpsInterface*
GonkGPSGeolocationProvider::GetGPSInterface()
{
  hw_module_t* module;

  if (hw_get_module(GPS_HARDWARE_MODULE_ID, (hw_module_t const**)&module))
    return nsnull;

  hw_device_t* device;
  if (module->methods->open(module, GPS_HARDWARE_MODULE_ID, &device))
    return nsnull;

  gps_device_t* gps_device = (gps_device_t *)device;
  const GpsInterface* result = gps_device->get_gps_interface(gps_device);

  if (result->size != sizeof(GpsInterface)) {
    return nsnull;
  }
  return result;
}

void
GonkGPSGeolocationProvider::RequestDataConnection()
{
  if (!mRIL) {
    return;
  }

  // TODO: Bug 772747 - We should ask NetworkManager or RIL to open
  // SUPL type connection for us.
  const nsAdoptingString& apnName = Preferences::GetString("geo.gps.apn.name");
  const nsAdoptingString& apnUser = Preferences::GetString("geo.gps.apn.user");
  const nsAdoptingString& apnPass = Preferences::GetString("geo.gps.apn.password");
  if (apnName && apnUser && apnPass) {
    mCid.Truncate();
    mRIL->SetupDataCall(1 /* DATACALL_RADIOTECHNOLOGY_GSM */,
                        apnName, apnUser, apnPass,
                        3 /* DATACALL_AUTH_PAP_OR_CHAP */,
                        NS_LITERAL_STRING("IP") /* pdptype */);
  }
}

void
GonkGPSGeolocationProvider::ReleaseDataConnection()
{
  if (!mRIL) {
    return;
  }

  if (mCid.IsEmpty()) {
    // We didn't request data call or the data call failed, bail out.
    return;
  }
  mRIL->DeactivateDataCall(mCid, NS_LITERAL_STRING("Close SUPL session"));
}

void
GonkGPSGeolocationProvider::RequestSetID(uint32_t flags)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRIL) {
    return;
  }

  AGpsSetIDType type = AGPS_SETID_TYPE_NONE;

  nsCOMPtr<nsIRilContext> rilCtx;
  mRIL->GetRilContext(getter_AddRefs(rilCtx));

  if (rilCtx) {
    nsCOMPtr<nsIICCRecords> icc;
    rilCtx->GetIcc(getter_AddRefs(icc));
    if (icc) {
      nsAutoString id;
      if (flags & AGPS_RIL_REQUEST_SETID_IMSI) {
        type = AGPS_SETID_TYPE_IMSI;
        icc->GetImsi(id);
      }
      if (flags & AGPS_RIL_REQUEST_SETID_MSISDN) {
        type = AGPS_SETID_TYPE_MSISDN;
        icc->GetMsisdn(id);
      }
      NS_ConvertUTF16toUTF8 idBytes(id);
      mAGpsRilInterface->set_set_id(type, idBytes.get());
    }
  }
}

void
GonkGPSGeolocationProvider::SetReferenceLocation()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRIL) {
    return;
  }

  nsCOMPtr<nsIRilContext> rilCtx;
  mRIL->GetRilContext(getter_AddRefs(rilCtx));

  AGpsRefLocation location;

  // TODO: Bug 772750 - get mobile connection technology from rilcontext
  location.type = AGPS_REF_LOCATION_TYPE_UMTS_CELLID;

  if (rilCtx) {
    nsCOMPtr<nsIICCRecords> icc;
    rilCtx->GetIcc(getter_AddRefs(icc));
    if (icc) {
      icc->GetMcc(&location.u.cellID.mcc);
      icc->GetMnc(&location.u.cellID.mnc);
    }
    nsCOMPtr<nsICellLocation> cell;
    rilCtx->GetCell(getter_AddRefs(cell));
    if (cell) {
      cell->GetLac(&location.u.cellID.lac);
      cell->GetCid(&location.u.cellID.cid);
    }
    if (mAGpsRilInterface) {
      mAGpsRilInterface->set_ref_location(&location, sizeof(location));
    }
  }
}

void
GonkGPSGeolocationProvider::Init()
{
  // Must not be main thread. Some GPS driver's first init takes very long.
  MOZ_ASSERT(!NS_IsMainThread());

  mGpsInterface = GetGPSInterface();
  if (!mGpsInterface) {
    return;
  }

  if (mGpsInterface->init(&mCallbacks) != 0) {
    return;
  }

  mAGpsInterface =
    static_cast<const AGpsInterface*>(mGpsInterface->get_extension(AGPS_INTERFACE));
  if (mAGpsInterface) {
    mAGpsInterface->init(&mAGPSCallbacks);
  }

  mAGpsRilInterface =
    static_cast<const AGpsRilInterface*>(mGpsInterface->get_extension(AGPS_RIL_INTERFACE));
  if (mAGpsRilInterface) {
    mAGpsRilInterface->init(&mAGPSRILCallbacks);
  }

  NS_DispatchToMainThread(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::StartGPS));
}

void
GonkGPSGeolocationProvider::StartGPS()
{
  MOZ_ASSERT(mGpsInterface);

  PRInt32 update = Preferences::GetInt("geo.default.update", kDefaultPeriod);

  if (mSupportsMSA || mSupportsMSB) {
    SetupAGPS();
  }

  int positionMode = GPS_POSITION_MODE_STANDALONE;
  bool singleShot = false;

  // XXX: If we know this is a single shot request, use MSA can be faster.
  if (singleShot && mSupportsMSA) {
    positionMode = GPS_POSITION_MODE_MS_ASSISTED;
  } else if (mSupportsMSB) {
    positionMode = GPS_POSITION_MODE_MS_BASED;
  }
  if (!mSupportsScheduling) {
    update = kDefaultPeriod;
  }

  mGpsInterface->set_position_mode(positionMode,
                                   GPS_POSITION_RECURRENCE_PERIODIC,
                                   update, 0, 0);
#if DEBUG_GPS
  // Delete cached data
  mGpsInterface->delete_aiding_data(GPS_DELETE_ALL);
#endif

  mGpsInterface->start();
}

void
GonkGPSGeolocationProvider::SetupAGPS()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mAGpsRilInterface);

  const nsAdoptingCString& suplServer = Preferences::GetCString("geo.gps.supl_server");
  PRInt32 suplPort = Preferences::GetInt("geo.gps.supl_port", -1);
  if (!suplServer.IsEmpty() && suplPort > 0) {
    mAGpsInterface->set_server(AGPS_TYPE_SUPL, suplServer.get(), suplPort);
  } else {
    NS_WARNING("Cannot get SUPL server settings");
    return;
  }

  // Setup network state listener
  nsIInterfaceRequestor* ireq = dom::gonk::SystemWorkerManager::GetInterfaceRequestor();
  if (ireq) {
    mRIL = do_GetInterface(ireq);
    if (mRIL) {
      mRIL->RegisterDataCallCallback(this);
    }
  }

  return;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Startup()
{
  if (mStarted) {
    return NS_OK;
  }

  if (!mInitThread) {
    nsresult rv = NS_NewThread(getter_AddRefs(mInitThread));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mInitThread->Dispatch(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::Init),
                        NS_DISPATCH_NORMAL);

  mStarted = true;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
  mLocationCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Shutdown()
{
  MOZ_ASSERT(mInitThread);

  if (!mStarted) {
    return NS_OK;
  }

  mInitThread->Dispatch(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::ShutdownNow),
                        NS_DISPATCH_NORMAL);

  return NS_OK;
}

void
GonkGPSGeolocationProvider::ShutdownNow()
{
  if (!mStarted) {
    return;
  }
  mStarted = false;

  if (mRIL) {
    mRIL->UnregisterDataCallCallback(this);
  }

  if (mGpsInterface) {
    mGpsInterface->stop();
    mGpsInterface->cleanup();
  }

  mInitThread = nsnull;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::SetHighAccuracy(bool)
{
  return NS_OK;
}

/** nsIRILDataCallback interface **/

NS_IMETHODIMP
GonkGPSGeolocationProvider::DataCallStateChanged(nsIRILDataCallInfo* aDataCall)
{
  MOZ_ASSERT(aDataCall);
  MOZ_ASSERT(mAGpsInterface);
  nsCOMPtr<nsIRILDataCallInfo> datacall = aDataCall;

  PRUint32 callState;
  nsresult rv = datacall->GetState(&callState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString apn;
  rv = datacall->GetApn(apn);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = datacall->GetCid(mCid);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF16toUTF8 currentApn(apn);
  const nsAdoptingCString& agpsApn = Preferences::GetCString("geo.gps.apn.name");

  // TODO: Bug 772748 - handle data call failed case.
  if (currentApn == agpsApn) {
    switch (callState) {
      case nsINetworkInterface::NETWORK_STATE_CONNECTED:
#ifdef AGPS_HAVE_DUAL_APN
        mAGpsInterface->data_conn_open(AGPS_TYPE_ANY,
                                       agpsApn.get(),
                                       AGPS_APN_BEARER_IPV4);
#else
        mAGpsInterface->data_conn_open(agpsApn.get());
#endif
        break;
      case nsINetworkInterface::NETWORK_STATE_DISCONNECTED:
#ifdef AGPS_HAVE_DUAL_APN
        mAGpsInterface->data_conn_closed(AGPS_TYPE_ANY);
#else
        mAGpsInterface->data_conn_closed();
#endif
        break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::ReceiveDataCallList(nsIRILDataCallInfo** aDataCalls,
                                                PRUint32 aLength)
{
  return NS_OK;
}
