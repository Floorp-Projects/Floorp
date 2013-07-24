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
#include "nsContentUtils.h"
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

  NS_IMETHOD Handle(const nsAString& aName, const JS::Value& aResult)
  {
    if (JSVAL_IS_INT(aResult)) {
      int32_t mode = JSVAL_TO_INT(aResult);
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

NS_IMPL_ISUPPORTS1(SettingsServiceCallback, nsISettingsServiceCallback)

class CheckVolumeSettingsCallback MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CheckVolumeSettingsCallback(const nsACString& aVolumeName)
  : mVolumeName(aVolumeName) {}

  NS_IMETHOD Handle(const nsAString& aName, const JS::Value& aResult)
  {
    if (JSVAL_IS_BOOLEAN(aResult)) {
      bool isSharingEnabled = JSVAL_TO_BOOLEAN(aResult);
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

NS_IMPL_ISUPPORTS1(CheckVolumeSettingsCallback, nsISettingsServiceCallback)

AutoMounterSetting::AutoMounterSetting()
{
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
  settingsService->CreateLock(getter_AddRefs(lock));
  nsCOMPtr<nsISettingsServiceCallback> callback = new SettingsServiceCallback();
  lock->Set(UMS_MODE, INT_TO_JSVAL(AUTOMOUNTER_DISABLE), callback, nullptr);
}

AutoMounterSetting::~AutoMounterSetting()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, MOZSETTINGS_CHANGED);
  }
}

NS_IMPL_ISUPPORTS1(AutoMounterSetting, nsIObserver)

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
    settingsService->CreateLock(getter_AddRefs(lock));
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

NS_IMETHODIMP
AutoMounterSetting::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar* aData)
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
  JSObject& obj(val.toObject());
  JS::Value key;
  if (!JS_GetProperty(cx, &obj, "key", &key) ||
      !key.isString()) {
    return NS_OK;
  }

  JSString *jsKey = JS_ValueToString(cx, key);
  nsDependentJSString keyStr;
  if (!keyStr.init(cx, jsKey)) {
    return NS_OK;
  }

  JS::Value value;
  if (!JS_GetProperty(cx, &obj, "value", &value)) {
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
