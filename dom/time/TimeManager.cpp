/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "nsIDOMClassInfo.h"
#include "nsITimeService.h"
#include "TimeManager.h"

#ifdef MOZ_TIME_MANAGER
DOMCI_DATA(MozTimeManager, mozilla::dom::time::TimeManager)
#endif

namespace mozilla {
namespace dom {
namespace time {

NS_INTERFACE_MAP_BEGIN(TimeManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozTimeManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
#ifdef MOZ_TIME_MANAGER
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozTimeManager)
#endif
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(TimeManager)
NS_IMPL_RELEASE(TimeManager)

nsresult
TimeManager::Set(const JS::Value& date, JSContext* ctx) {
  double dateMSec;

  if (date.isObject()) {
    JS::Rooted<JSObject*> dateObj(ctx, date.toObjectOrNull());

    if (JS_ObjectIsDate(ctx, dateObj) && js_DateIsValid(dateObj)) {
      dateMSec = js_DateGetMsecSinceEpoch(dateObj);
    }
    else {
      NS_WARN_IF_FALSE(JS_ObjectIsDate(ctx, dateObj), "This is not a Date object");
      NS_WARN_IF_FALSE(js_DateIsValid(dateObj), "Date is not valid");
      return NS_ERROR_INVALID_ARG;
    }
  } else if (date.isNumber()) {
    dateMSec = date.toNumber();
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsITimeService> timeService = do_GetService(TIMESERVICE_CONTRACTID);
  if (timeService) {
    return timeService->Set(dateMSec);
  }
  return NS_OK;
}

} // namespace time
} // namespace dom
} // namespace mozilla
