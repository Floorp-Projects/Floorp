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

#include "GonkGPSGeolocationProvider.h"

#include <cmath>
#include <pthread.h>
#include <hardware/gps.h>

#include "base/task.h"
#include "GeolocationUtil.h"
#include "mozstumbler/MozStumbler.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsGeoPosition.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINetworkInterface.h"
#include "nsIObserverService.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "prtime.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"

#ifdef AGPS_TYPE_INVALID
#define AGPS_HAVE_DUAL_APN
#endif

#define FLUSH_AIDE_DATA 0

#undef LOG
#undef ERR
#undef DBG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO,  "GonkGPSGeolocationProvider", ## args)
#define ERR(args...)  __android_log_print(ANDROID_LOG_ERROR, "GonkGPSGeolocationProvider", ## args)
#define DBG(args...)  __android_log_print(ANDROID_LOG_DEBUG, "GonkGPSGeolocationProvider" , ## args)

using namespace mozilla;
using namespace mozilla::dom;

static const int kDefaultPeriod = 1000; // ms
static bool gDebug_isLoggingEnabled = false;
static bool gDebug_isGPSLocationIgnored = false;
static const char* kMozSettingsChangedTopic = "mozsettings-changed";
// Both of these settings can be toggled in the Gaia Developer settings screen.
static const char* kSettingDebugEnabled = "geolocation.debugging.enabled";
static const char* kSettingDebugGpsIgnored = "geolocation.debugging.gps-locations-ignored";

// While most methods of GonkGPSGeolocationProvider should only be
// called from main thread, we deliberately put the Init and ShutdownGPS
// methods off main thread to avoid blocking.
NS_IMPL_ISUPPORTS(GonkGPSGeolocationProvider,
                  nsIGeolocationProvider,
                  nsIObserver,
                  nsISettingsServiceCallback)

/* static */ GonkGPSGeolocationProvider* GonkGPSGeolocationProvider::sSingleton = nullptr;
GpsCallbacks GonkGPSGeolocationProvider::mCallbacks;


void
GonkGPSGeolocationProvider::LocationCallback(GpsLocation* location)
{
  if (gDebug_isGPSLocationIgnored) {
    return;
  }

  class UpdateLocationEvent : public Runnable {
  public:
    UpdateLocationEvent(nsGeoPosition* aPosition)
      : mPosition(aPosition)
    {}
    NS_IMETHOD Run() override {
      RefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();
      nsCOMPtr<nsIGeolocationUpdate> callback = provider->mLocationCallback;
      provider->mLastGPSPosition = mPosition;
      if (callback) {
        callback->Update(mPosition);
      }
      return NS_OK;
    }
  private:
    RefPtr<nsGeoPosition> mPosition;
  };

  MOZ_ASSERT(location);

  const float kImpossibleAccuracy_m = 0.001;
  if (location->accuracy < kImpossibleAccuracy_m) {
    return;
  }

  RefPtr<nsGeoPosition> somewhere = new nsGeoPosition(location->latitude,
                                                        location->longitude,
                                                        location->altitude,
                                                        location->accuracy,
                                                        location->accuracy,
                                                        location->bearing,
                                                        location->speed,
                                                        PR_Now() / PR_USEC_PER_MSEC);
  // Note above: Can't use location->timestamp as the time from the satellite is a
  // minimum of 16 secs old (see http://leapsecond.com/java/gpsclock.htm).
  // All code from this point on expects the gps location to be timestamped with the
  // current time, most notably: the geolocation service which respects maximumAge
  // set in the DOM JS.

  if (gDebug_isLoggingEnabled) {
    DBG("geo: GPS got a fix (%f, %f). accuracy: %f",
        location->latitude,
        location->longitude,
        location->accuracy);
  }

  RefPtr<UpdateLocationEvent> event = new UpdateLocationEvent(somewhere);
  NS_DispatchToMainThread(event);

}

class NotifyObserversGPSTask final : public Runnable
{
public:
  explicit NotifyObserversGPSTask(const char16_t* aData)
    : mData(aData)
  {}
  NS_IMETHOD Run() override {
    RefPtr<nsIGeolocationProvider> provider =
    GonkGPSGeolocationProvider::GetSingleton();
    nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
    obsService->NotifyObservers(provider, "geolocation-device-events", mData);
    return NS_OK;
  }
private:
  const char16_t* mData;
};

void
GonkGPSGeolocationProvider::StatusCallback(GpsStatus* status)
{
  const char* msgStream=0;
  switch (status->status) {
    case GPS_STATUS_NONE:
      msgStream = "geo: GPS_STATUS_NONE\n";
      break;
    case GPS_STATUS_SESSION_BEGIN:
      msgStream = "geo: GPS_STATUS_SESSION_BEGIN\n";
      break;
    case GPS_STATUS_SESSION_END:
      msgStream = "geo: GPS_STATUS_SESSION_END\n";
      break;
    case GPS_STATUS_ENGINE_ON:
      msgStream = "geo: GPS_STATUS_ENGINE_ON\n";
      NS_DispatchToMainThread(new NotifyObserversGPSTask(u"GPSStarting"));
      break;
    case GPS_STATUS_ENGINE_OFF:
      msgStream = "geo: GPS_STATUS_ENGINE_OFF\n";
      NS_DispatchToMainThread(new NotifyObserversGPSTask(u"GPSShutdown"));
      break;
    default:
      msgStream = "geo: Unknown GPS status\n";
      break;
  }
  if (gDebug_isLoggingEnabled){
    DBG("%s", msgStream);
  }
}

void
GonkGPSGeolocationProvider::SvStatusCallback(GpsSvStatus* sv_info)
{
  if (gDebug_isLoggingEnabled) {
    static int numSvs = 0;
    static uint32_t numEphemeris = 0;
    static uint32_t numAlmanac = 0;
    static uint32_t numUsedInFix = 0;

    unsigned int i = 1;
    uint32_t svAlmanacCount = 0;
    for (i = 1; i > 0; i <<= 1) {
      if (i & sv_info->almanac_mask) {
        svAlmanacCount++;
      }
    }

    uint32_t svEphemerisCount = 0;
    for (i = 1; i > 0; i <<= 1) {
      if (i & sv_info->ephemeris_mask) {
        svEphemerisCount++;
      }
    }

    uint32_t svUsedCount = 0;
    for (i = 1; i > 0; i <<= 1) {
      if (i & sv_info->used_in_fix_mask) {
        svUsedCount++;
      }
    }

    // Log the message only if the the status changed.
    if (sv_info->num_svs != numSvs ||
        svAlmanacCount != numAlmanac ||
        svEphemerisCount != numEphemeris ||
        svUsedCount != numUsedInFix) {

      LOG(
        "geo: Number of SVs have (visibility, almanac, ephemeris): (%d, %d, %d)."
        "  %d of these SVs were used in fix.\n",
        sv_info->num_svs, svAlmanacCount, svEphemerisCount, svUsedCount);

      numSvs = sv_info->num_svs;
      numAlmanac = svAlmanacCount;
      numEphemeris = svEphemerisCount;
      numUsedInFix = svUsedCount;
    }
  }
}

void
GonkGPSGeolocationProvider::NmeaCallback(GpsUtcTime timestamp, const char* nmea, int length)
{
  if (gDebug_isLoggingEnabled) {
    DBG("NMEA: timestamp:\t%lld, length: %d, %s", timestamp, length, nmea);
  }
}

void
GonkGPSGeolocationProvider::SetCapabilitiesCallback(uint32_t capabilities)
{
  class UpdateCapabilitiesEvent : public Runnable {
  public:
    UpdateCapabilitiesEvent(uint32_t aCapabilities)
      : mCapabilities(aCapabilities)
    {}
    NS_IMETHOD Run() override {
      RefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();

      provider->mSupportsScheduling = mCapabilities & GPS_CAPABILITY_SCHEDULING;
      provider->mSupportsSingleShot = mCapabilities & GPS_CAPABILITY_SINGLE_SHOT;
#ifdef GPS_CAPABILITY_ON_DEMAND_TIME
      provider->mSupportsTimeInjection = mCapabilities & GPS_CAPABILITY_ON_DEMAND_TIME;
#endif
      return NS_OK;
    }
  private:
    uint32_t mCapabilities;
  };

  NS_DispatchToMainThread(new UpdateCapabilitiesEvent(capabilities));
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

GonkGPSGeolocationProvider::GonkGPSGeolocationProvider()
  : mStarted(false)
  , mSupportsScheduling(false)
  , mObservingSettingsChange(false)
  , mSupportsSingleShot(false)
  , mSupportsTimeInjection(false)
  , mGpsInterface(nullptr)
{
}

GonkGPSGeolocationProvider::~GonkGPSGeolocationProvider()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mStarted, "Must call Shutdown before destruction");

  sSingleton = nullptr;
}

already_AddRefed<GonkGPSGeolocationProvider>
GonkGPSGeolocationProvider::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton)
    sSingleton = new GonkGPSGeolocationProvider();

  RefPtr<GonkGPSGeolocationProvider> provider = sSingleton;
  return provider.forget();
}

const GpsInterface*
GonkGPSGeolocationProvider::GetGPSInterface()
{
  hw_module_t* module;

  if (hw_get_module(GPS_HARDWARE_MODULE_ID, (hw_module_t const**)&module))
    return nullptr;

  hw_device_t* device;
  if (module->methods->open(module, GPS_HARDWARE_MODULE_ID, &device))
    return nullptr;

  gps_device_t* gps_device = (gps_device_t *)device;
  const GpsInterface* result = gps_device->get_gps_interface(gps_device);

  if (result->size != sizeof(GpsInterface)) {
    return nullptr;
  }
  return result;
}

void
GonkGPSGeolocationProvider::RequestSettingValue(const char* aKey)
{
  MOZ_ASSERT(aKey);
  nsCOMPtr<nsISettingsService> ss = do_GetService("@mozilla.org/settingsService;1");
  if (!ss) {
    MOZ_ASSERT(ss);
    return;
  }

  nsCOMPtr<nsISettingsServiceLock> lock;
  nsresult rv = ss->CreateLock(nullptr, getter_AddRefs(lock));
  if (NS_FAILED(rv)) {
    ERR("error while createLock setting '%s': %d\n", aKey, uint32_t(rv));
    return;
  }

  rv = lock->Get(aKey, this);
  if (NS_FAILED(rv)) {
    ERR("error while get setting '%s': %d\n", aKey, uint32_t(rv));
    return;
  }
}

void
GonkGPSGeolocationProvider::InjectLocation(double latitude,
                                           double longitude,
                                           float accuracy)
{
  if (gDebug_isLoggingEnabled) {
    DBG("injecting location (%f, %f) accuracy: %f", latitude, longitude, accuracy);
  }

  MOZ_ASSERT(NS_IsMainThread());
  if (!mGpsInterface) {
    return;
  }

  mGpsInterface->inject_location(latitude, longitude, accuracy);
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

  if (!mCallbacks.size) {
    mCallbacks.size = sizeof(GpsCallbacks);
    mCallbacks.location_cb = LocationCallback;
    mCallbacks.status_cb = StatusCallback;
    mCallbacks.sv_status_cb = SvStatusCallback;
    mCallbacks.nmea_cb = NmeaCallback;
    mCallbacks.set_capabilities_cb = SetCapabilitiesCallback;
    mCallbacks.acquire_wakelock_cb = AcquireWakelockCallback;
    mCallbacks.release_wakelock_cb = ReleaseWakelockCallback;
    mCallbacks.create_thread_cb = CreateThreadCallback;

#ifdef GPS_CAPABILITY_ON_DEMAND_TIME
    mCallbacks.request_utc_time_cb = RequestUtcTimeCallback;
#endif

  }

  if (mGpsInterface->init(&mCallbacks) != 0) {
    return;
  }

  NS_DispatchToMainThread(NewRunnableMethod(this, &GonkGPSGeolocationProvider::StartGPS));
}

void
GonkGPSGeolocationProvider::StartGPS()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGpsInterface);

  int32_t update = Preferences::GetInt("geo.default.update", kDefaultPeriod);

  int positionMode = GPS_POSITION_MODE_STANDALONE;

  if (!mSupportsScheduling) {
    update = kDefaultPeriod;
  }

  mGpsInterface->set_position_mode(positionMode,
                                   GPS_POSITION_RECURRENCE_PERIODIC,
                                   update, 0, 0);
#if FLUSH_AIDE_DATA
  // Delete cached data
  mGpsInterface->delete_aiding_data(GPS_DELETE_ALL);
#endif

  mGpsInterface->start();
}


NS_IMPL_ISUPPORTS(GonkGPSGeolocationProvider::NetworkLocationUpdate,
                  nsIGeolocationUpdate)

NS_IMETHODIMP
GonkGPSGeolocationProvider::NetworkLocationUpdate::Update(nsIDOMGeoPosition *position)
{
  RefPtr<GonkGPSGeolocationProvider> provider =
    GonkGPSGeolocationProvider::GetSingleton();

  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  position->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return NS_ERROR_FAILURE;
  }

  double lat, lon, acc;
  coords->GetLatitude(&lat);
  coords->GetLongitude(&lon);
  coords->GetAccuracy(&acc);

  double delta = -1.0;

  static double sLastMLSPosLat = 0;
  static double sLastMLSPosLon = 0;

  if (0 != sLastMLSPosLon || 0 != sLastMLSPosLat) {
    delta = CalculateDeltaInMeter(lat, lon, sLastMLSPosLat, sLastMLSPosLon);
  }

  sLastMLSPosLat = lat;
  sLastMLSPosLon = lon;

  // if the MLS coord change is smaller than this arbitrarily small value
  // assume the MLS coord is unchanged, and stick with the GPS location
  const double kMinMLSCoordChangeInMeters = 10;

  DOMTimeStamp time_ms = 0;
  if (provider->mLastGPSPosition) {
    provider->mLastGPSPosition->GetTimestamp(&time_ms);
  }
  const int64_t diff_ms = (PR_Now() / PR_USEC_PER_MSEC) - time_ms;

  // We want to distinguish between the GPS being inactive completely
  // and temporarily inactive. In the former case, we would use a low
  // accuracy network location; in the latter, we only want a network
  // location that appears to updating with movement.

  const bool isGPSFullyInactive = diff_ms > 1000 * 60 * 2; // two mins
  const bool isGPSTempInactive = diff_ms > 1000 * 10; // 10 secs

  if (provider->mLocationCallback) {
    if (isGPSFullyInactive ||
       (isGPSTempInactive && delta > kMinMLSCoordChangeInMeters))
    {
      if (gDebug_isLoggingEnabled) {
        DBG("Using MLS, GPS age:%fs, MLS Delta:%fm\n", diff_ms / 1000.0, delta);
      }
      provider->mLocationCallback->Update(position);
    } else if (provider->mLastGPSPosition) {
      if (gDebug_isLoggingEnabled) {
        DBG("Using old GPS age:%fs\n", diff_ms / 1000.0);
      }

      // This is a fallback case so that the GPS provider responds with its last
      // location rather than waiting for a more recent GPS or network location.
      // The service decides if the location is too old, not the provider.
      provider->mLocationCallback->Update(provider->mLastGPSPosition);
    }
  }
  provider->InjectLocation(lat, lon, acc);
  return NS_OK;
}
NS_IMETHODIMP
GonkGPSGeolocationProvider::NetworkLocationUpdate::NotifyError(uint16_t error)
{
  return NS_OK;
}
NS_IMETHODIMP
GonkGPSGeolocationProvider::Startup()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mStarted) {
    return NS_OK;
  }

  RequestSettingValue(kSettingDebugEnabled);
  RequestSettingValue(kSettingDebugGpsIgnored);

  // Setup an observer to watch changes to the setting.
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    MOZ_ASSERT(!mObservingSettingsChange);
    nsresult rv = observerService->AddObserver(this, kMozSettingsChangedTopic, false);
    if (NS_FAILED(rv)) {
      NS_WARNING("geo: Gonk GPS AddObserver failed");
    } else {
      mObservingSettingsChange = true;
    }
  }

  if (!mInitThread) {
    nsresult rv = NS_NewNamedThread("Gonk GPS", getter_AddRefs(mInitThread));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mInitThread->Dispatch(NewRunnableMethod(this, &GonkGPSGeolocationProvider::Init),
                        NS_DISPATCH_NORMAL);

  mNetworkLocationProvider = do_CreateInstance("@mozilla.org/geolocation/mls-provider;1");
  if (mNetworkLocationProvider) {
    nsresult rv = mNetworkLocationProvider->Startup();
    if (NS_SUCCEEDED(rv)) {
      RefPtr<NetworkLocationUpdate> update = new NetworkLocationUpdate();
      mNetworkLocationProvider->Watch(update);
    }
  }

  mStarted = true;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  mLocationCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mStarted) {
    return NS_OK;
  }

  mStarted = false;
  if (mNetworkLocationProvider) {
    mNetworkLocationProvider->Shutdown();
    mNetworkLocationProvider = nullptr;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    nsresult rv;
    rv = obs->RemoveObserver(this, kMozSettingsChangedTopic);
    if (NS_FAILED(rv)) {
      NS_WARNING("geo: Gonk GPS mozsettings RemoveObserver failed");
    } else {
      mObservingSettingsChange = false;
    }
  }

  mInitThread->Dispatch(NewRunnableMethod(this, &GonkGPSGeolocationProvider::ShutdownGPS),
                        NS_DISPATCH_NORMAL);

  return NS_OK;
}

void
GonkGPSGeolocationProvider::ShutdownGPS()
{
  MOZ_ASSERT(!mStarted, "Should only be called after Shutdown");

  if (mGpsInterface) {
    mGpsInterface->stop();
    mGpsInterface->cleanup();
  }
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::SetHighAccuracy(bool)
{
  return NS_OK;
}

namespace {
int
ConvertToGpsNetworkType(int aNetworkInterfaceType)
{
  switch (aNetworkInterfaceType) {
    case nsINetworkInfo::NETWORK_TYPE_WIFI:
      return AGPS_RIL_NETWORK_TYPE_WIFI;
    case nsINetworkInfo::NETWORK_TYPE_MOBILE:
      return AGPS_RIL_NETWORK_TYPE_MOBILE;
    case nsINetworkInfo::NETWORK_TYPE_MOBILE_MMS:
      return AGPS_RIL_NETWORK_TYPE_MOBILE_MMS;
    case nsINetworkInfo::NETWORK_TYPE_MOBILE_SUPL:
      return AGPS_RIL_NETWORK_TYPE_MOBILE_SUPL;
    case nsINetworkInfo::NETWORK_TYPE_MOBILE_DUN:
      return AGPS_RIL_NETWORK_TTYPE_MOBILE_DUN;
    default:
      NS_WARNING(nsPrintfCString("Unknown network type mapping %d",
                                 aNetworkInterfaceType).get());
      return -1;
  }
}
} // namespace

NS_IMETHODIMP
GonkGPSGeolocationProvider::Observe(nsISupports* aSubject,
                                    const char* aTopic,
                                    const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, kMozSettingsChangedTopic)) {
    // Read changed setting value
    RootedDictionary<SettingChangeNotification> setting(RootingCx());
    if (!WrappedJSToDictionary(aSubject, setting)) {
      return NS_OK;
    }

    if (setting.mKey.EqualsASCII(kSettingDebugGpsIgnored)) {
      LOG("received mozsettings-changed: ignoring\n");
      gDebug_isGPSLocationIgnored =
        setting.mValue.isBoolean() ? setting.mValue.toBoolean() : false;
      if (gDebug_isLoggingEnabled) {
        DBG("GPS ignored %d\n", gDebug_isGPSLocationIgnored);
      }
      return NS_OK;
    } else if (setting.mKey.EqualsASCII(kSettingDebugEnabled)) {
      LOG("received mozsettings-changed: logging\n");
      gDebug_isLoggingEnabled =
        setting.mValue.isBoolean() ? setting.mValue.toBoolean() : false;
      return NS_OK;
    }
  }

  return NS_OK;
}

/** nsISettingsServiceCallback **/

NS_IMETHODIMP
GonkGPSGeolocationProvider::Handle(const nsAString& aName,
                                   JS::Handle<JS::Value> aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::HandleError(const nsAString& aErrorMessage)
{
  return NS_OK;
}
