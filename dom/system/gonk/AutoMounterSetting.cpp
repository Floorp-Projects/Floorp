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
#include "nsIJSContextStack.h"
#include "nsISettingsService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "xpcpublic.h"
#include "mozilla/Attributes.h"

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AutoMounterSetting" , ## args)
#define ERR(args...)  __android_log_print(ANDROID_LOG_ERROR, "AutoMounterSetting" , ## args)

#define UMS_MODE                "ums.mode"
#define MOZSETTINGS_CHANGED     "mozsettings-changed"

namespace mozilla {
namespace system {

class SettingsServiceCallback MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  SettingsServiceCallback() {}

  NS_IMETHOD Handle(const nsAString &aName, const JS::Value &aResult, JSContext *aContext) {
    if (JSVAL_IS_INT(aResult)) {
      int32_t mode = JSVAL_TO_INT(aResult);
      SetAutoMounterMode(mode);
    }
    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString &aName, JSContext *aContext) {
    ERR("SettingsCallback::HandleError: %s\n", NS_LossyConvertUTF16toASCII(aName).get());
    return NS_OK;
  }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(SettingsServiceCallback, nsISettingsServiceCallback)

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

  // Get the initial value of the setting.
  nsCOMPtr<nsISettingsService> settingsService =
    do_GetService("@mozilla.org/settingsService;1");
  if (!settingsService) {
    ERR("Failed to get settingsLock service!");
    return;
  }
  nsCOMPtr<nsISettingsServiceLock> lock;
  settingsService->GetLock(getter_AddRefs(lock));
  nsCOMPtr<nsISettingsServiceCallback> callback = new SettingsServiceCallback();
  lock->Get(UMS_MODE, callback);
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

NS_IMETHODIMP
AutoMounterSetting::Observe(nsISupports *aSubject,
                            const char *aTopic,
                            const PRUnichar *aData)
{
  if (strcmp(aTopic, MOZSETTINGS_CHANGED) != 0) {
    return NS_OK;
  }

  // Note that this function gets called for any and all settings changes,
  // so we need to carefully check if we have the one we're interested in.
  //
  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"ums.autoMount","value":true}

  nsCOMPtr<nsIThreadJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (!stack) {
    ERR("Failed to get JSContextStack");
    return NS_OK;
  }
  JSContext *cx = stack->GetSafeJSContext();
  if (!cx) {
    ERR("Failed to GetSafeJSContext");
    return NS_OK;
  }
  nsDependentString dataStr(aData);
  JS::Value val;
  if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
      !val.isObject()) {
    return NS_OK;
  }
  JSObject &obj(val.toObject());
  JS::Value key;
  if (!JS_GetProperty(cx, &obj, "key", &key) ||
      !key.isString()) {
    return NS_OK;
  }
  JSBool match;
  if (!JS_StringEqualsAscii(cx, key.toString(), UMS_MODE, &match) ||
      (match != JS_TRUE)) {
    return NS_OK;
  }
  JS::Value value;
  if (!JS_GetProperty(cx, &obj, "value", &value) ||
      !value.isInt32()) {
    return NS_OK;
  }
  int32_t mode = value.toInt32();
  SetAutoMounterMode(mode);

  return NS_OK;
}

}   // namespace system
}   // namespace mozilla
