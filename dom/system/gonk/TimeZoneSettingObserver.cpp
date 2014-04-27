/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/message_loop.h"
#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "TimeZoneSettingObserver.h"
#include "xpcpublic.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsPrintfCString.h"

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Time Zone Setting" , ## args)
#define ERR(args...)  __android_log_print(ANDROID_LOG_ERROR, "Time Zone Setting" , ## args)

#define TIME_TIMEZONE       "time.timezone"
#define MOZSETTINGS_CHANGED "mozsettings-changed"

using namespace mozilla;

namespace {

class TimeZoneSettingObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  TimeZoneSettingObserver();
  virtual ~TimeZoneSettingObserver();
  static nsresult SetTimeZone(const JS::Value &aValue, JSContext *aContext);
};

class TimeZoneSettingCb MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  TimeZoneSettingCb() {}

  NS_IMETHOD Handle(const nsAString &aName, JS::Handle<JS::Value> aResult) {

    JSContext *cx = nsContentUtils::GetCurrentJSContext();
    NS_ENSURE_TRUE(cx, NS_OK);

    // If we don't have time.timezone value in the settings, we need
    // to initialize the settings based on the current system timezone
    // to make settings consistent with system. This usually happens
    // at the very first boot. After that, settings must have a value.
    if (aResult.isNull()) {
      // Get the current system time zone offset. Note that we need to
      // convert the value to a UTC representation in the format of
      // "UTC{+,-}hh:mm", so that the Gaia end can know how to interpret.
      // E.g., -480 is "UTC+08:00"; 630 is "UTC-10:30".
      int32_t timeZoneOffset = hal::GetTimezoneOffset();
      nsPrintfCString curTimeZone("UTC%+03d:%02d",
                                  -timeZoneOffset / 60,
                                  abs(timeZoneOffset) % 60);

      // Convert it to a JS string.
      NS_ConvertUTF8toUTF16 utf16Str(curTimeZone);

      JS::Rooted<JSString*> jsStr(cx, JS_NewUCStringCopyN(cx,
                                                          utf16Str.get(),
                                                          utf16Str.Length()));
      if (!jsStr) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // Set the settings based on the current system timezone.
      nsCOMPtr<nsISettingsServiceLock> lock;
      nsCOMPtr<nsISettingsService> settingsService =
        do_GetService("@mozilla.org/settingsService;1");
      if (!settingsService) {
        ERR("Failed to get settingsLock service!");
        return NS_OK;
      }
      settingsService->CreateLock(nullptr, getter_AddRefs(lock));
      JS::Rooted<JS::Value> value(cx, JS::StringValue(jsStr));
      lock->Set(TIME_TIMEZONE, value, nullptr, nullptr);
      return NS_OK;
    }

    // Set the system timezone based on the current settings.
    if (aResult.isString()) {
      return TimeZoneSettingObserver::SetTimeZone(aResult, cx);
    }

    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString &aName) {
    ERR("TimeZoneSettingCb::HandleError: %s\n", NS_LossyConvertUTF16toASCII(aName).get());
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(TimeZoneSettingCb, nsISettingsServiceCallback)

TimeZoneSettingObserver::TimeZoneSettingObserver()
{
  // Setup an observer to watch changes to the setting.
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    ERR("GetObserverService failed");
    return;
  }
  nsresult rv;
  rv = observerService->AddObserver(this, MOZSETTINGS_CHANGED, false);
  if (NS_FAILED(rv)) {
    ERR("AddObserver failed");
    return;
  }

  // Read the 'time.timezone' setting in order to start with a known
  // value at boot time. The handle() will be called after reading.
  nsCOMPtr<nsISettingsServiceLock> lock;
  nsCOMPtr<nsISettingsService> settingsService =
    do_GetService("@mozilla.org/settingsService;1");
  if (!settingsService) {
    ERR("Failed to get settingsLock service!");
    return;
  }
  settingsService->CreateLock(nullptr, getter_AddRefs(lock));
  nsCOMPtr<nsISettingsServiceCallback> callback = new TimeZoneSettingCb();
  lock->Get(TIME_TIMEZONE, callback);
}

nsresult TimeZoneSettingObserver::SetTimeZone(const JS::Value &aValue, JSContext *aContext)
{
  // Convert the JS value to a nsCString type.
  // The value should be a JS string like "America/Chicago" or "UTC-05:00".
  nsDependentJSString valueStr;
  if (!valueStr.init(aContext, aValue.toString())) {
    ERR("Failed to convert JS value to nsCString");
    return NS_ERROR_FAILURE;
  }
  NS_ConvertUTF16toUTF8 newTimezone(valueStr);

  // Hal expects opposite sign from general notations,
  // so we need to flip it.
  if (newTimezone.Find(NS_LITERAL_CSTRING("UTC+")) == 0) {
    if (!newTimezone.SetCharAt('-', 3)) {
      return NS_ERROR_FAILURE;
    }
  } else if (newTimezone.Find(NS_LITERAL_CSTRING("UTC-")) == 0) {
    if (!newTimezone.SetCharAt('+', 3)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Set the timezone only when the system timezone is not identical.
  nsCString curTimezone = hal::GetTimezone();
  if (!curTimezone.Equals(newTimezone)) {
    hal::SetTimezone(newTimezone);
  }

  return NS_OK;
}

TimeZoneSettingObserver::~TimeZoneSettingObserver()
{
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, MOZSETTINGS_CHANGED);
  }
}

NS_IMPL_ISUPPORTS(TimeZoneSettingObserver, nsIObserver)

NS_IMETHODIMP
TimeZoneSettingObserver::Observe(nsISupports *aSubject,
                     const char *aTopic,
                     const char16_t *aData)
{
  if (strcmp(aTopic, MOZSETTINGS_CHANGED) != 0) {
    return NS_OK;
  }

  // Note that this function gets called for any and all settings changes,
  // so we need to carefully check if we have the one we're interested in.
  //
  // The string that we're interested in will be a JSON string that looks like:
  // {"key":"time.timezone","value":"America/Chicago"} or
  // {"key":"time.timezone","value":"UTC-05:00"}

  AutoSafeJSContext cx;

  // Parse the JSON value.
  nsDependentString dataStr(aData);
  JS::Rooted<JS::Value> val(cx);
  if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
      !val.isObject()) {
    return NS_OK;
  }

  // Get the key, which should be the JS string "time.timezone".
  JS::Rooted<JSObject*> obj(cx, &val.toObject());
  JS::Rooted<JS::Value> key(cx);
  if (!JS_GetProperty(cx, obj, "key", &key) ||
      !key.isString()) {
    return NS_OK;
  }
  bool match;
  if (!JS_StringEqualsAscii(cx, key.toString(), TIME_TIMEZONE, &match) ||
      !match) {
    return NS_OK;
  }

  // Get the value, which should be a JS string like "America/Chicago".
  JS::Rooted<JS::Value> value(cx);
  if (!JS_GetProperty(cx, obj, "value", &value) ||
      !value.isString()) {
    return NS_OK;
  }

  // Set the system timezone.
  return SetTimeZone(value, cx);
}

} // anonymous namespace

static mozilla::StaticRefPtr<TimeZoneSettingObserver> sTimeZoneSettingObserver;
namespace mozilla {
namespace system {
void
InitializeTimeZoneSettingObserver()
{
  sTimeZoneSettingObserver = new TimeZoneSettingObserver();
  ClearOnShutdown(&sTimeZoneSettingObserver);
}

} // namespace system
} // namespace mozilla
