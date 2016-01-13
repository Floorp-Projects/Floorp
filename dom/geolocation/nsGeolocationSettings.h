/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGeolocationSettings_h
#define nsGeolocationSettings_h

#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsJSUtils.h"
#include "nsTArray.h"

#if (defined(MOZ_GPS_DEBUG) && defined(ANDROID))
#include <android/log.h>
#define GPSLOG(fmt, ...) __android_log_print(ANDROID_LOG_WARN, "GPS", "%12s:%-5d " fmt,  __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define GPSLOG(...) {;}
#endif // MOZ_GPS_DEBUG && ANDROID

// The settings key.
#define GEO_ENABLED             "geolocation.enabled"
#define GEO_ALA_ENABLED         "ala.settings.enabled"
#define GEO_ALA_TYPE            "geolocation.type"
#define GEO_ALA_FIXED_COORDS    "geolocation.fixed_coords"
#define GEO_ALA_APP_SETTINGS    "geolocation.app_settings"
#define GEO_ALA_ALWAYS_PRECISE  "geolocation.always_precise"
#ifdef MOZ_APPROX_LOCATION
#define GEO_ALA_APPROX_DISTANCE "geolocation.approx_distance"
#endif

enum GeolocationFuzzMethod {
  GEO_ALA_TYPE_PRECISE, // default, GPS/AGPS location
  GEO_ALA_TYPE_FIXED,   // user supplied lat/long
  GEO_ALA_TYPE_NONE,    // no location given
#ifdef MOZ_APPROX_LOCATION
  GEO_ALA_TYPE_APPROX   // approximate, grid-based location
#endif
};

#define GEO_ALA_TYPE_DEFAULT    (GEO_ALA_TYPE_PRECISE)
#define GEO_ALA_TYPE_FIRST      (GEO_ALA_TYPE_PRECISE)
#ifdef MOZ_APPROX_LOCATION
#define GEO_ALA_TYPE_LAST       (GEO_ALA_TYPE_APPROX)
#else
#define GEO_ALA_TYPE_LAST       (GEO_ALA_TYPE_NONE)
#endif

/**
 * Simple class for holding the geolocation settings values.
 */

class GeolocationSetting final {
public:
  explicit GeolocationSetting(const nsString& aOrigin) :
    mFuzzMethod(GEO_ALA_TYPE_DEFAULT),
#ifdef MOZ_APPROX_LOCATION
    mDistance(0),
#endif
    mLatitude(0.0),
    mLongitude(0.0),
    mOrigin(aOrigin) {}

  GeolocationSetting(const GeolocationSetting& rhs) :
    mFuzzMethod(rhs.mFuzzMethod),
#ifdef MOZ_APPROX_LOCATION
    mDistance(rhs.mDistance),
#endif
    mLatitude(rhs.mLatitude),
    mLongitude(rhs.mLongitude),
    mOrigin(rhs.mOrigin) {}

  ~GeolocationSetting() {}

  GeolocationSetting& operator=(const GeolocationSetting& rhs) {
    mFuzzMethod = rhs.mFuzzMethod;
#ifdef MOZ_APPROX_LOCATION
    mDistance = rhs.mDistance;
#endif
    mLatitude = rhs.mLatitude;
    mLongitude = rhs.mLongitude;
    mOrigin = rhs.mOrigin;
    return *this;
  }

  void HandleTypeChange(const JS::Value& aVal);
  void HandleApproxDistanceChange(const JS::Value& aVal);
  void HandleFixedCoordsChange(const JS::Value& aVal);

  inline GeolocationFuzzMethod GetType() const { return mFuzzMethod; }
#ifdef MOZ_APPROX_LOCATION
  inline int32_t GetApproxDistance() const { return mDistance; }
#endif
  inline double GetFixedLatitude() const { return mLatitude; }
  inline double GetFixedLongitude() const { return mLongitude; }
  inline const nsString& GetOrigin() const { return mOrigin; }

private:
  GeolocationSetting() :
#ifdef MOZ_APPROX_LOCATION
    mDistance(0),
#endif
    mLatitude(0),
    mLongitude(0)
  {} // can't default construct

  GeolocationFuzzMethod mFuzzMethod;
#ifdef MOZ_APPROX_LOCATION
  int32_t         mDistance;
#endif
  double          mLatitude,
                  mLongitude;
  nsString        mOrigin;
};

/**
 * Singleton that holds the global and per-origin geolocation settings.
 */
class nsGeolocationSettings final : public nsIObserver
{
public:
  static already_AddRefed<nsGeolocationSettings> GetGeolocationSettings();
  static mozilla::StaticRefPtr<nsGeolocationSettings> sSettings;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsGeolocationSettings() : mAlaEnabled(false), mGlobalSetting(NullString()) {}
  nsresult Init();

  void HandleGeolocationSettingsChange(const nsAString& aKey, const JS::Value& aVal);
  void HandleGeolocationSettingsError(const nsAString& aName);

  void PutWatchOrigin(int32_t aWatchID, const nsCString& aOrigin);
  void RemoveWatchOrigin(int32_t aWatchID);
  void GetWatchOrigin(int32_t aWatchID, nsCString& aOrigin);
  inline bool IsAlaEnabled() const { return mAlaEnabled; }

  // given a watch ID, retrieve the geolocation settings.  the watch ID is
  // mapped to the origin of the listener/request which is then used to
  // retreive the geolocation settings for the origin.
  // if the origin is in the always-precise list, the settings will always be
  // 'precise'. if the origin has origin-specific settings, that will be returned
  // otherwise the global geolocation settings will be returned.
  // NOTE: this returns a copy of the settings to enforce read-only client access
  GeolocationSetting LookupGeolocationSetting(int32_t aWatchID);

private:
  ~nsGeolocationSettings() {}
  nsGeolocationSettings(const nsGeolocationSettings&) :
    mGlobalSetting(NullString()) {} // can't copy obj

  void HandleMozsettingsChanged(nsISupports* aSubject);
  void HandleGeolocationAlaEnabledChange(const JS::Value& aVal);
  void HandleGeolocationPerOriginSettingsChange(const JS::Value& aVal);
  void HandleGeolocationAlwaysPreciseChange(const JS::Value& aVal);

private:
  bool mAlaEnabled;
  GeolocationSetting mGlobalSetting;
  nsClassHashtable<nsStringHashKey, GeolocationSetting> mPerOriginSettings;
  nsTArray<nsString> mAlwaysPreciseApps;
  nsClassHashtable<nsUint32HashKey, nsCString> mCurrentWatches;
};

#endif /* nsGeolocationSettings_h */

