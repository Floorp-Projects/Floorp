/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GpsdLocationProvider.h"
#include <errno.h>
#include <gps.h>
#include "MLSFallback.h"
#include "mozilla/Atomics.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/dom/GeolocationPositionErrorBinding.h"
#include "GeolocationPosition.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

//
// MLSGeolocationUpdate
//

/**
 * |MLSGeolocationUpdate| provides a fallback if gpsd is not supported.
 */
class GpsdLocationProvider::MLSGeolocationUpdate final
    : public nsIGeolocationUpdate {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONUPDATE

  explicit MLSGeolocationUpdate(nsIGeolocationUpdate* aCallback);

 protected:
  ~MLSGeolocationUpdate() = default;

 private:
  nsCOMPtr<nsIGeolocationUpdate> mCallback;
};

GpsdLocationProvider::MLSGeolocationUpdate::MLSGeolocationUpdate(
    nsIGeolocationUpdate* aCallback)
    : mCallback(aCallback) {
  MOZ_ASSERT(mCallback);
}

// nsISupports
//

NS_IMPL_ISUPPORTS(GpsdLocationProvider::MLSGeolocationUpdate,
                  nsIGeolocationUpdate);

// nsIGeolocationUpdate
//

NS_IMETHODIMP
GpsdLocationProvider::MLSGeolocationUpdate::Update(
    nsIDOMGeoPosition* aPosition) {
  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  aPosition->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return NS_ERROR_FAILURE;
  }

  return mCallback->Update(aPosition);
}

NS_IMETHODIMP
GpsdLocationProvider::MLSGeolocationUpdate::NotifyError(uint16_t aError) {
  return mCallback->NotifyError(aError);
}

//
// UpdateRunnable
//

class GpsdLocationProvider::UpdateRunnable final : public Runnable {
 public:
  UpdateRunnable(
      const nsMainThreadPtrHandle<GpsdLocationProvider>& aLocationProvider,
      nsIDOMGeoPosition* aPosition)
      : Runnable("GpsdU"),
        mLocationProvider(aLocationProvider),
        mPosition(aPosition) {
    MOZ_ASSERT(mLocationProvider);
    MOZ_ASSERT(mPosition);
  }

  // nsIRunnable
  //

  NS_IMETHOD Run() override {
    mLocationProvider->Update(mPosition);
    return NS_OK;
  }

 private:
  nsMainThreadPtrHandle<GpsdLocationProvider> mLocationProvider;
  RefPtr<nsIDOMGeoPosition> mPosition;
};

//
// NotifyErrorRunnable
//

class GpsdLocationProvider::NotifyErrorRunnable final : public Runnable {
 public:
  NotifyErrorRunnable(
      const nsMainThreadPtrHandle<GpsdLocationProvider>& aLocationProvider,
      int aError)
      : Runnable("GpsdNE"),
        mLocationProvider(aLocationProvider),
        mError(aError) {
    MOZ_ASSERT(mLocationProvider);
  }

  // nsIRunnable
  //

  NS_IMETHOD Run() override {
    mLocationProvider->NotifyError(mError);
    return NS_OK;
  }

 private:
  nsMainThreadPtrHandle<GpsdLocationProvider> mLocationProvider;
  int mError;
};

//
// PollRunnable
//

/**
 * |PollRunnable| does the main work of processing GPS data received
 * from gpsd. libgps blocks while polling, so this runnable has to be
 * executed on it's own thread. To cancel the poll runnable, invoke
 * |StopRunning| and |PollRunnable| will stop within a reasonable time
 * frame.
 */
class GpsdLocationProvider::PollRunnable final : public Runnable {
 public:
  PollRunnable(
      const nsMainThreadPtrHandle<GpsdLocationProvider>& aLocationProvider)
      : Runnable("GpsdP"),
        mLocationProvider(aLocationProvider),
        mRunning(true) {
    MOZ_ASSERT(mLocationProvider);
  }

  static bool IsSupported() {
    return GPSD_API_MAJOR_VERSION >= 5 && GPSD_API_MAJOR_VERSION <= 10;
  }

  bool IsRunning() const { return mRunning; }

  void StopRunning() { mRunning = false; }

  // nsIRunnable
  //

  NS_IMETHOD Run() override {
    int err;

    switch (GPSD_API_MAJOR_VERSION) {
      case 5 ... 10:
        err = PollLoop5();
        break;
      default:
        err = GeolocationPositionError_Binding::POSITION_UNAVAILABLE;
        break;
    }

    if (err) {
      NS_DispatchToMainThread(
          MakeAndAddRef<NotifyErrorRunnable>(mLocationProvider, err));
    }

    mLocationProvider = nullptr;

    return NS_OK;
  }

 protected:
  int PollLoop5() {
#if GPSD_API_MAJOR_VERSION >= 5 && GPSD_API_MAJOR_VERSION <= 10
    static const int GPSD_WAIT_TIMEOUT_US =
        1000000; /* us to wait for GPS data */

    struct gps_data_t gpsData;

    auto res = gps_open(nullptr, nullptr, &gpsData);

    if (res < 0) {
      return ErrnoToError(errno);
    }

    gps_stream(&gpsData, WATCH_ENABLE | WATCH_JSON, NULL);

    int err = 0;

    // nsGeoPositionCoords will convert NaNs to null for optional properties of
    // the JavaScript Coordinates object.
    double lat = 0;
    double lon = 0;
    double alt = UnspecifiedNaN<double>();
    double hError = 0;
    double vError = UnspecifiedNaN<double>();
    double heading = UnspecifiedNaN<double>();
    double speed = UnspecifiedNaN<double>();

    while (IsRunning()) {
      errno = 0;
      auto hasGpsData = gps_waiting(&gpsData, GPSD_WAIT_TIMEOUT_US);
      int status;

      if (errno) {
        err = ErrnoToError(errno);
        break;
      }
      if (!hasGpsData) {
        continue; /* woke up from timeout */
      }

#  if GPSD_API_MAJOR_VERSION >= 7
      res = gps_read(&gpsData, nullptr, 0);
#  else

      res = gps_read(&gpsData);
#  endif

      if (res < 0) {
        err = ErrnoToError(errno);
        break;
      } else if (!res) {
        continue; /* no data available */
      }

#  if GPSD_API_MAJOR_VERSION >= 10
      status = gpsData.fix.status;
#  else
      status = gpsData.status;
#  endif

      if (status == STATUS_NO_FIX) {
        continue;
      }

      switch (gpsData.fix.mode) {
        case MODE_3D:
          double galt;

#  if GPSD_API_MAJOR_VERSION >= 9
          galt = gpsData.fix.altMSL;
#  else
          galt = gpsData.fix.altitude;
#  endif
          if (!IsNaN(galt)) {
            alt = galt;
          }
          [[fallthrough]];
        case MODE_2D:
          if (!IsNaN(gpsData.fix.latitude)) {
            lat = gpsData.fix.latitude;
          }
          if (!IsNaN(gpsData.fix.longitude)) {
            lon = gpsData.fix.longitude;
          }
          if (!IsNaN(gpsData.fix.epx) && !IsNaN(gpsData.fix.epy)) {
            hError = std::max(gpsData.fix.epx, gpsData.fix.epy);
          } else if (!IsNaN(gpsData.fix.epx)) {
            hError = gpsData.fix.epx;
          } else if (!IsNaN(gpsData.fix.epy)) {
            hError = gpsData.fix.epy;
          }
          if (!IsNaN(gpsData.fix.epv)) {
            vError = gpsData.fix.epv;
          }
          if (!IsNaN(gpsData.fix.track)) {
            heading = gpsData.fix.track;
          }
          if (!IsNaN(gpsData.fix.speed)) {
            speed = gpsData.fix.speed;
          }
          break;
        default:
          continue;  // There's no useful data in this fix; continue.
      }

      NS_DispatchToMainThread(MakeAndAddRef<UpdateRunnable>(
          mLocationProvider,
          new nsGeoPosition(lat, lon, alt, hError, vError, heading, speed,
                            PR_Now() / PR_USEC_PER_MSEC)));
    }

    gps_stream(&gpsData, WATCH_DISABLE, NULL);
    gps_close(&gpsData);

    return err;
#else
    return GeolocationPositionError_Binding::POSITION_UNAVAILABLE;
#endif  // GPSD_MAJOR_API_VERSION
  }

  static int ErrnoToError(int aErrno) {
    switch (aErrno) {
      case EACCES:
        [[fallthrough]];
      case EPERM:
        [[fallthrough]];
      case EROFS:
        return GeolocationPositionError_Binding::PERMISSION_DENIED;
      case ETIME:
        [[fallthrough]];
      case ETIMEDOUT:
        return GeolocationPositionError_Binding::TIMEOUT;
      default:
        return GeolocationPositionError_Binding::POSITION_UNAVAILABLE;
    }
  }

 private:
  nsMainThreadPtrHandle<GpsdLocationProvider> mLocationProvider;
  Atomic<bool> mRunning;
};

//
// GpsdLocationProvider
//

const uint32_t GpsdLocationProvider::GPSD_POLL_THREAD_TIMEOUT_MS = 5000;

GpsdLocationProvider::GpsdLocationProvider() {}

GpsdLocationProvider::~GpsdLocationProvider() {}

void GpsdLocationProvider::Update(nsIDOMGeoPosition* aPosition) {
  if (!mCallback || !mPollRunnable) {
    return;  // not initialized or already shut down
  }

  if (mMLSProvider) {
    /* We got a location from gpsd, so let's cancel our MLS fallback. */
    mMLSProvider->Shutdown();
    mMLSProvider = nullptr;
  }

  mCallback->Update(aPosition);
}

void GpsdLocationProvider::NotifyError(int aError) {
  if (!mCallback) {
    return;  // not initialized or already shut down
  }

  if (!mMLSProvider) {
    /* With gpsd failed, we restart MLS. It will be canceled once we
     * get another location from gpsd.
     */
    mMLSProvider = MakeAndAddRef<MLSFallback>();
    mMLSProvider->Startup(new MLSGeolocationUpdate(mCallback));
  }

  mCallback->NotifyError(aError);
}

// nsISupports
//

NS_IMPL_ISUPPORTS(GpsdLocationProvider, nsIGeolocationProvider)

// nsIGeolocationProvider
//

NS_IMETHODIMP
GpsdLocationProvider::Startup() {
  if (!PollRunnable::IsSupported()) {
    return NS_OK;  // We'll fall back to MLS.
  }

  if (mPollRunnable) {
    return NS_OK;  // already running
  }

  RefPtr<PollRunnable> pollRunnable =
      MakeAndAddRef<PollRunnable>(nsMainThreadPtrHandle<GpsdLocationProvider>(
          new nsMainThreadPtrHolder<GpsdLocationProvider>("GpsdLP", this)));

  // Use existing poll thread...
  RefPtr<LazyIdleThread> pollThread = mPollThread;

  // ... or create a new one.
  if (!pollThread) {
    pollThread = MakeAndAddRef<LazyIdleThread>(GPSD_POLL_THREAD_TIMEOUT_MS,
                                               "Gpsd poll thread"_ns,
                                               LazyIdleThread::ManualShutdown);
  }

  auto rv = pollThread->Dispatch(pollRunnable, NS_DISPATCH_NORMAL);

  if (NS_FAILED(rv)) {
    return rv;
  }

  mPollRunnable = pollRunnable.forget();
  mPollThread = pollThread.forget();

  return NS_OK;
}

NS_IMETHODIMP
GpsdLocationProvider::Watch(nsIGeolocationUpdate* aCallback) {
  mCallback = aCallback;

  /* The MLS fallback will kick in after a few seconds if gpsd
   * doesn't provide location information within time. Once we
   * see the first message from gpsd, the fallback will be
   * disabled in |Update|.
   */
  mMLSProvider = MakeAndAddRef<MLSFallback>();
  mMLSProvider->Startup(new MLSGeolocationUpdate(aCallback));

  return NS_OK;
}

NS_IMETHODIMP
GpsdLocationProvider::Shutdown() {
  if (mMLSProvider) {
    mMLSProvider->Shutdown();
    mMLSProvider = nullptr;
  }

  if (!mPollRunnable) {
    return NS_OK;  // not running
  }

  mPollRunnable->StopRunning();
  mPollRunnable = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
GpsdLocationProvider::SetHighAccuracy(bool aHigh) { return NS_OK; }

}  // namespace dom
}  // namespace mozilla
