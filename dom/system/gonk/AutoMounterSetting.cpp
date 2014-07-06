/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoMounter.h"
#include "AutoMounterSetting.h"

#include "base/message_loop.h"
#include "jsapi.h"
#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIObserverService.h"
#include "nsCxPusher.h"
#include "nsISettingsService.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"
#include "mozilla/Attributes.h"

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AutoMounterSetting" , ## args)
#define ERR(args...)  __android_log_print(ANDROID_LOG_ERROR, "AutoMounterSetting" , ## args)

#define UMS_MODE                  "ums.mode"
#define UMS_STATUS                "ums.status"
#define UMS_VOLUME_ENABLED_PREFIX "ums.volume."
#define UMS_VOLUME_ENABLED_SUFFIX ".enabled"
#define MOZSETTINGS_CHANGED       "mozsettings-changed"

namespace mozilla {
namespace system {

class SettingsServiceCallback MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  SettingsServiceCallback() {}

  NS_IMETHOD Handle(const nsAString& aName, JS::Handle<JS::Value> aResult)
  {
    if (aResult.isInt32()) {
      int32_t mode = aResult.toInt32();
      SetAutoMounterMode(mode);
    }
    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    ERR("SettingsCallback::HandleError: %s\n", NS_LossyConvertUTF16toASCII(aName).get());
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(SettingsServiceCallback, nsISettingsServiceCallback)

class CheckVolumeSettingsCallback MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CheckVolumeSettingsCallback(const nsACString& aVolumeName)
  : mVolumeName(aVolumeName) {}

  NS_IMETHOD Handle(const nsAString& aName, JS::Handle<JS::Value> aResult)
  {
    if (aResult.isBoolean()) {
      bool isSharingEnabled = aResult.toBoolean();
      SetAutoMounterSharingMode(mVolumeName, isSharingEnabled);
    }
    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    ERR("CheckVolumeSettingsCallback::HandleError: %s\n", NS_LossyConvertUTF16toASCII(aName).get());
    return NS_OK;
  }
private:
  nsCString mVolumeName;
};

NS_IMPL_ISUPPORTS(CheckVolumeSettingsCallback, nsISettingsServiceCallback)

AutoMounterSetting::AutoMounterSetting()
  : mStatus(AUTOMOUNTER_STATUS_DISABLED)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Setup an observer to watch changes to the setting
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
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

  // Force ums.mode to be 0 initially. We do this because settings are persisted.
  // We don't want UMS to be enabled until such time as the phone is unlocked,
  // and gaia/apps/system/js/storage.js takes care of detecting when the phone
  // becomes unlocked and changes ums.mode appropriately.
  nsCOMPtr<nsISettingsService> settingsService =
    do_GetService("@mozilla.org/settingsService;1");
  if (!settingsService) {
    ERR("Failed to get settingsLock service!");
    return;
  }
  nsCOMPtr<nsISettingsServiceLock> lock;
  settingsService->CreateLock(nullptr, getter_AddRefs(lock));
  nsCOMPtr<nsISettingsServiceCallback> callback = new SettingsServiceCallback();
  mozilla::AutoSafeJSContext cx;
  JS::Rooted<JS::Value> value(cx);
  value.setInt32(AUTOMOUNTER_DISABLE);
  lock->Set(UMS_MODE, value, callback, nullptr);
  value.setInt32(mStatus);
  lock->Set(UMS_STATUS, value, nullptr, nullptr);
}

AutoMounterSetting::~AutoMounterSetting()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, MOZSETTINGS_CHANGED);
  }
}

NS_IMPL_ISUPPORTS(AutoMounterSetting, nsIObserver)

const char *
AutoMounterSetting::StatusStr(int32_t aStatus)
{
  switch (aStatus) {
    case AUTOMOUNTER_STATUS_DISABLED:   return "Disabled";
    case AUTOMOUNTER_STATUS_ENABLED:    return "Enabled";
    case AUTOMOUNTER_STATUS_FILES_OPEN: return "FilesOpen";
  }
  return "??? Unknown ???";
}

class CheckVolumeSettingsRunnable : public nsRunnable
{
public:
  CheckVolumeSettingsRunnable(const nsACString& aVolumeName)
    : mVolumeName(aVolumeName) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsISettingsService> settingsService =
      do_GetService("@mozilla.org/settingsService;1");
    NS_ENSURE_TRUE(settingsService, NS_ERROR_FAILURE);
    nsCOMPtr<nsISettingsServiceLock> lock;
    settingsService->CreateLock(nullptr, getter_AddRefs(lock));
    nsCOMPtr<nsISettingsServiceCallback> callback =
      new CheckVolumeSettingsCallback(mVolumeName);
    nsPrintfCString setting(UMS_VOLUME_ENABLED_PREFIX "%s" UMS_VOLUME_ENABLED_SUFFIX,
                            mVolumeName.get());
    lock->Get(setting.get(), callback);
    return NS_OK;
  }

private:
  nsCString mVolumeName;
};

//static
void
AutoMounterSetting::CheckVolumeSettings(const nsACString& aVolumeName)
{
  NS_DispatchToMainThread(new CheckVolumeSettingsRunnable(aVolumeName));
}

class SetStatusRunnable : public nsRunnable
{
public:
  SetStatusRunnable(int32_t aStatus) : mStatus(aStatus) {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsISettingsService> settingsService =
      do_GetService("@mozilla.org/settingsService;1");
    NS_ENSURE_TRUE(settingsService, NS_ERROR_FAILURE);
    nsCOMPtr<nsISettingsServiceLock> lock;
    settingsService->CreateLock(nullptr, getter_AddRefs(lock));
    // lock may be null if this gets called during shutdown.
    if (lock) {
      mozilla::AutoSafeJSContext cx;
      JS::Rooted<JS::Value> value(cx, JS::Int32Value(mStatus));
      lock->Set(UMS_STATUS, value, nullptr, nullptr);
    }
    return NS_OK;
  }

private:
  int32_t mStatus;
};

//static
void
AutoMounterSetting::SetStatus(int32_t aStatus)
{
  if (aStatus != mStatus) {
    LOG("Changing status from '%s' to '%s'",
        StatusStr(mStatus), StatusStr(aStatus));
    mStatus = aStatus;
    NS_DispatchToMainThread(new SetStatusRunnable(aStatus));
  }
}

NS_IMETHODIMP
AutoMounterSetting::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const char16_t* aData)
{
  if (strcmp(aTopic, MOZSETTINGS_CHANGED) != 0) {
    return NS_OK;
  }

  // Note that this function gets called for any and all settings changes,
  // so we need to carefully check if we have the one we're interested in.
  //
  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"ums.autoMount","value":true}

  mozilla::AutoSafeJSContext cx;
  nsDependentString dataStr(aData);
  JS::Rooted<JS::Value> val(cx);
  if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
      !val.isObject()) {
    return NS_OK;
  }
  JS::Rooted<JSObject*> obj(cx, &val.toObject());
  JS::Rooted<JS::Value> key(cx);
  if (!JS_GetProperty(cx, obj, "key", &key) ||
      !key.isString()) {
    return NS_OK;
  }

  JSString *jsKey = JS::ToString(cx, key);
  nsAutoJSString keyStr;
  if (!keyStr.init(cx, jsKey)) {
    return NS_OK;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!JS_GetProperty(cx, obj, "value", &value)) {
    return NS_OK;
  }

  // Check for ums.mode changes
  if (keyStr.EqualsLiteral(UMS_MODE)) {
    if (!value.isInt32()) {
      return NS_OK;
    }
    int32_t mode = value.toInt32();
    SetAutoMounterMode(mode);
    return NS_OK;
  }

  // Check for ums.volume.NAME.enabled
  if (StringBeginsWith(keyStr, NS_LITERAL_STRING(UMS_VOLUME_ENABLED_PREFIX)) &&
      StringEndsWith(keyStr, NS_LITERAL_STRING(UMS_VOLUME_ENABLED_SUFFIX))) {
    if (!value.isBoolean()) {
      return NS_OK;
    }
    const size_t prefixLen = sizeof(UMS_VOLUME_ENABLED_PREFIX) - 1;
    const size_t suffixLen = sizeof(UMS_VOLUME_ENABLED_SUFFIX) - 1;
    nsDependentSubstring volumeName =
      Substring(keyStr, prefixLen, keyStr.Length() - prefixLen - suffixLen);
    bool isSharingEnabled = value.toBoolean();
    SetAutoMounterSharingMode(NS_LossyConvertUTF16toASCII(volumeName), isSharingEnabled);
    return NS_OK;
  }

  return NS_OK;
}

}   // namespace system
}   // namespace mozilla
