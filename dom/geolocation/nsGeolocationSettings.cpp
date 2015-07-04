/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Telemetry.h"

#include "nsISettingsService.h"

#include "nsGeolocation.h"
#include "nsGeolocationSettings.h"
#include "nsDOMClassInfoID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsContentPermissionHelper.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"

#include "nsJSUtils.h"
#include "prdtoa.h"

#define GEO_ALA_TYPE_VALUE_PRECISE "precise"
#define GEO_ALA_TYPE_VALUE_APPROX  "blur"
#define GEO_ALA_TYPE_VALUE_FIXED   "user-defined"
#define GEO_ALA_TYPE_VALUE_NONE    "no-location"

using mozilla::unused;
using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsGeolocationSettings, nsIObserver)

StaticRefPtr<nsGeolocationSettings> nsGeolocationSettings::sSettings;

already_AddRefed<nsGeolocationSettings>
nsGeolocationSettings::GetGeolocationSettings()
{
  // this singleton is only needed in the parent process...
  if (XRE_IsContentProcess()) {
    return nullptr;
  }

  nsRefPtr<nsGeolocationSettings> result;
  if (nsGeolocationSettings::sSettings) {
    result = nsGeolocationSettings::sSettings;
    return result.forget();
  }

  result = new nsGeolocationSettings();
  if (NS_FAILED(result->Init())) {
    return nullptr;
  }
  ClearOnShutdown(&nsGeolocationSettings::sSettings);
  nsGeolocationSettings::sSettings = result;
  return result.forget();
}

nsresult nsGeolocationSettings::Init()
{
  // query for the current settings...
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  // hook up observers
  obs->AddObserver(this, "quit-application", false);
  obs->AddObserver(this, "mozsettings-changed", false);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationSettings::Observe(nsISupports* aSubject,
                               const char* aTopic,
                               const char16_t* aData)
{
  // remove observers
  if (!strcmp("quit-application", aTopic)) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "quit-application");
      obs->RemoveObserver(this, "mozsettings-changed");
    }
    return NS_OK;
  }

  if (!strcmp("mozsettings-changed", aTopic)) {
    HandleMozsettingsChanged(aSubject);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


GeolocationSetting
nsGeolocationSettings::LookupGeolocationSetting(int32_t aWatchID)
{
  nsCString *origin;
  if (!mCurrentWatches.Get(aWatchID, &origin) || !origin) {
    return mGlobalSetting;
  }

  // if there is no per-app setting for the given origin, this will
  // set gb == nullptr
  GeolocationSetting const * const gb =
    mPerOriginSettings.Get(NS_ConvertUTF8toUTF16(*origin));

  // return a copy of the per-app or global settings
  return gb ? *gb : mGlobalSetting;
}


void
nsGeolocationSettings::HandleGeolocationSettingsChange(const nsAString& aKey,
                                                       const JS::Value& aVal)
{
  if (aKey.EqualsASCII(GEO_ALA_ENABLED)) {
    HandleGeolocationAlaEnabledChange(aVal);
  } else if (aKey.EqualsASCII(GEO_ALA_TYPE)) {
    mGlobalSetting.HandleTypeChange(aVal);
#ifdef MOZ_APPROX_LOCATION
  } else if (aKey.EqualsASCII(GEO_ALA_APPROX_DISTANCE)) {
    mGlobalSetting.HandleApproxDistanceChange(aVal);
#endif
  } else if (aKey.EqualsASCII(GEO_ALA_FIXED_COORDS)) {
    mGlobalSetting.HandleFixedCoordsChange(aVal);
  } else if (aKey.EqualsASCII(GEO_ALA_APP_SETTINGS)) {
    HandleGeolocationPerOriginSettingsChange(aVal);
  } else if (aKey.EqualsASCII(GEO_ALA_ALWAYS_PRECISE)) {
    HandleGeolocationAlwaysPreciseChange(aVal);
  }
}

void
nsGeolocationSettings::HandleMozsettingsChanged(nsISupports* aSubject)
{
  MOZ_ASSERT(NS_IsMainThread());

  RootedDictionary<SettingChangeNotification> setting(nsContentUtils::RootingCx());
  if (!WrappedJSToDictionary(aSubject, setting)) {
    return;
  }

  GPSLOG("mozsettings changed: %s", NS_ConvertUTF16toUTF8(setting.mKey).get());

  // and handle the geolocation settings change...
  HandleGeolocationSettingsChange(setting.mKey, setting.mValue);
}


void
nsGeolocationSettings::HandleGeolocationSettingsError(const nsAString& aName)
{
  if (aName.EqualsASCII(GEO_ALA_ENABLED)) {
    GPSLOG("Unable to get value for '" GEO_ALA_ENABLED "'");
  } else if (aName.EqualsASCII(GEO_ALA_TYPE)) {
    GPSLOG("Unable to get value for '" GEO_ALA_TYPE "'");
#ifdef MOZ_APPROX_LOCATION
  } else if (aName.EqualsASCII(GEO_ALA_APPROX_DISTANCE)) {
    GPSLOG("Unable to get value for '" GEO_ALA_APPROX_DISTANCE "'");
#endif
  } else if (aName.EqualsASCII(GEO_ALA_FIXED_COORDS)) {
    GPSLOG("Unable to get value for '" GEO_ALA_FIXED_COORDS "'");
  } else if (aName.EqualsASCII(GEO_ALA_APP_SETTINGS)) {
    GPSLOG("Unable to get value for '" GEO_ALA_APP_SETTINGS "'");
  } else if (aName.EqualsASCII(GEO_ALA_ALWAYS_PRECISE)) {
    GPSLOG("Unable to get value for '" GEO_ALA_ALWAYS_PRECISE "'");
  }
}

void
nsGeolocationSettings::PutWatchOrigin(int32_t aWatchID,
                                      const nsCString& aOrigin)
{
  if (aWatchID < 0) {
    return;
  }

  GPSLOG("mapping watch ID %d to origin %s", aWatchID, aOrigin.get());
  mCurrentWatches.Put(static_cast<uint32_t>(aWatchID), new nsCString(aOrigin));
}

void
nsGeolocationSettings::RemoveWatchOrigin(int32_t aWatchID)
{
  GPSLOG("unmapping watch ID %d", aWatchID);
  mCurrentWatches.Remove(static_cast<uint32_t>(aWatchID));
}

void
nsGeolocationSettings::GetWatchOrigin(int32_t aWatchID, nsCString& aOrigin)
{
  nsCString *str;
  if (!mCurrentWatches.Get(aWatchID, &str) || !str) {
    return;
  }
  aOrigin = *str;
  GPSLOG("returning origin %s for watch ID %d", aOrigin.get(), aWatchID);
}

void
nsGeolocationSettings::HandleGeolocationAlaEnabledChange(const JS::Value& aVal)
{
  if (!aVal.isBoolean()) {
    return;
  }

  mAlaEnabled = aVal.toBoolean();
}

void
nsGeolocationSettings::HandleGeolocationPerOriginSettingsChange(const JS::Value& aVal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aVal.isObject()) {
    return;
  }

  // clear the hash table
  mPerOriginSettings.Clear();

  // root the object and get the global
  JS::Rooted<JSObject*> obj(nsContentUtils::RootingCx(), &aVal.toObject());
  MOZ_ASSERT(obj);
  nsIGlobalObject* global = xpc::NativeGlobal(obj);
  NS_ENSURE_TRUE_VOID(global && global->GetGlobalJSObject());

  // because the spec requires calling getters when enumerating the key of a
  // dictionary
  AutoEntryScript aes(global, "geolocation.app_settings enumeration");
  aes.TakeOwnershipOfErrorReporting();
  JSContext *cx = aes.cx();
  JS::AutoIdArray ids(cx, JS_Enumerate(cx, obj));

  // if we get no ids then the exception list is empty and we can return here.
  if (!ids) {
      return;
  }

  // go through all of the objects in the exceptions dictionary
  for (size_t i = 0; i < ids.length(); i++) {
    JS::RootedId id(cx);
    id = ids[i];

    // if it is an app that is always precise, skip it
    nsAutoJSString origin;
    if (!origin.init(cx, id)) {
      continue;
    }
    if (mAlwaysPreciseApps.Contains(origin)) {
      continue;
    }

    // get the app setting object
    JS::RootedValue propertyValue(cx);
    if (!JS_GetPropertyById(cx, obj, id, &propertyValue) || !propertyValue.isObject()) {
      continue;
    }
    JS::RootedObject settingObj(cx, &propertyValue.toObject());

    GeolocationSetting *settings = new GeolocationSetting(origin);
    GPSLOG("adding exception for %s", NS_ConvertUTF16toUTF8(origin).get());

    // get the geolocation type
    JS::RootedValue fm(cx);
    if (JS_GetProperty(cx, settingObj, "type", &fm)) {
      settings->HandleTypeChange(fm);
    }

#ifdef MOZ_APPROX_LOCATION
    // get the approximate distance if there is one
    JS::RootedValue distance(cx);
    if (JS_GetProperty(cx, settingObj, "distance", &distance)) {
      settings->HandleApproxDistanceChange(distance);
    }
#endif

    // get and parse the coords, if any
    JS::RootedValue coords(cx);
    if (JS_GetProperty(cx, settingObj, "coords", &coords)) {
      settings->HandleFixedCoordsChange(coords);
    }

    // add the per-app setting object to the hashtable
    mPerOriginSettings.Put(origin, settings);
  }
}

void
nsGeolocationSettings::HandleGeolocationAlwaysPreciseChange(const JS::Value& aVal)
{
  if (!aVal.isObject()) {
    return;
  }

  // clear the list of apps that are always precise
  mAlwaysPreciseApps.Clear();

  // root the object and get the global
  JS::Rooted<JSObject*> obj(nsContentUtils::RootingCx(), &aVal.toObject());
  MOZ_ASSERT(obj);
  nsIGlobalObject* global = xpc::NativeGlobal(obj);
  NS_ENSURE_TRUE_VOID(global && global->GetGlobalJSObject());

  // the spec requires calling getters when accessing array by index
  AutoEntryScript aes(global, "geolocation.always_precise indexing");
  aes.TakeOwnershipOfErrorReporting();
  JSContext *cx = aes.cx();

  if (!JS_IsArrayObject(cx, obj)) {
    return;
  }

  uint32_t length;
  if (!JS_GetArrayLength(cx, obj, &length)) {
    return;
  }

  // process the list of apps...
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue value(cx);

    if (!JS_GetElement(cx, obj, i, &value) || !value.isString()) {
      continue;
    }

    nsAutoJSString origin;
    if (!origin.init(cx, value)) {
      continue;
    }

    GPSLOG("adding always precise for %s", NS_ConvertUTF16toUTF8(origin).get());

    // add the origin to the list of apps that will always receive
    // precise location information
    mAlwaysPreciseApps.AppendElement(origin);
  }
}


void
GeolocationSetting::HandleTypeChange(const JS::Value& aVal)
{
  nsAutoJSString str;
  if (!str.init(aVal)) {
    return;
  }

  GeolocationFuzzMethod fm = GEO_ALA_TYPE_DEFAULT;
  if (str.EqualsASCII(GEO_ALA_TYPE_VALUE_PRECISE)) {
    fm = GEO_ALA_TYPE_PRECISE;
  } else if (str.EqualsASCII(GEO_ALA_TYPE_VALUE_APPROX)) {
#ifdef MOZ_APPROX_LOCATION
    fm = GEO_ALA_TYPE_APPROX;
#else
    // we are loading a profile from a build with MOZ_APPROX_LOCATION
    // enabled, then we need to force the type to a sane value
    fm = GEO_ALA_TYPE_NONE;
#endif
  } else if (str.EqualsASCII(GEO_ALA_TYPE_VALUE_FIXED)) {
    fm = GEO_ALA_TYPE_FIXED;
  } else if (str.EqualsASCII(GEO_ALA_TYPE_VALUE_NONE)) {
    fm = GEO_ALA_TYPE_NONE;
  }

  if ((fm >= GEO_ALA_TYPE_FIRST) && (fm <= GEO_ALA_TYPE_LAST)) {
    GPSLOG("changing type for %s to %s (%d)",
           (mOrigin.IsEmpty() ? "global" : NS_ConvertUTF16toUTF8(mOrigin).get()),
           NS_ConvertUTF16toUTF8(str).get(),
           static_cast<int>(fm));
    mFuzzMethod = fm;
  }

  // based on the new type, we need to clean up the other settings
  switch (mFuzzMethod) {
    case GEO_ALA_TYPE_PRECISE:
    case GEO_ALA_TYPE_NONE:
#ifdef MOZ_APPROX_LOCATION
      mDistance = 0;
#endif
      mLatitude = 0.0;
      mLongitude = 0.0;
      break;
#ifdef MOZ_APPROX_LOCATION
    case GEO_ALA_TYPE_APPROX:
      mLatitude = 0.0;
      mLongitude = 0.0;
      break;
#endif
    case GEO_ALA_TYPE_FIXED:
#ifdef MOZ_APPROX_LOCATION
      mDistance = 0;
#endif
      break;
  }
}

#ifdef MOZ_APPROX_LOCATION
void
GeolocationSetting::HandleApproxDistanceChange(const JS::Value& aVal)
{
  if (!aVal.isInt32()) {
    return;
  }

  GPSLOG("changing approx distance for %s to %d",
       (mOrigin.IsEmpty() ? "global" : NS_ConvertUTF16toUTF8(mOrigin).get()),
       aVal.toInt32());

  // set the approximate distance
  mDistance = aVal.toInt32();
}
#endif


void
GeolocationSetting::HandleFixedCoordsChange(const JS::Value& aVal)
{
  nsAutoJSString str;
  if (!str.init(aVal)) {
      return;
  }

  // parse the string and store the global lat/lon
  // the string is in the form of @1.2345,6.7890
  // check for leading '@' and a comma in the middle
  int32_t const comma = str.Find(",");
  if ( (str.CharAt(0) != '@') || (comma == -1) ) {
    return;
  }

  // pull the lat and lon out of the string and convert to doubles
  nsresult rv;
  nsString slat(Substring(str, 1, comma - 1));
  nsString slon(Substring(str, comma + 1));
  double lat = slat.ToDouble(&rv);
  NS_ENSURE_SUCCESS(rv,);
  double lon = slon.ToDouble(&rv);
  NS_ENSURE_SUCCESS(rv,);

  // store the values
  mLatitude = lat;
  mLongitude = lon;

  GPSLOG("changing coords for %s to %s (%f, %f)",
         (mOrigin.IsEmpty() ? "global" : NS_ConvertUTF16toUTF8(mOrigin).get()),
         NS_ConvertUTF16toUTF8(str).get(),
         mLatitude, mLongitude);
}

