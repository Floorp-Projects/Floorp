/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeManager.h"

#include "mozilla/dom/MozTimeManagerBinding.h"
#include "nsContentUtils.h"
#include "nsITimeService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace time {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TimeManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TimeManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TimeManager)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(TimeManager, mWindow)

JSObject*
TimeManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MozTimeManagerBinding::Wrap(aCx, aScope, this);
}

void
TimeManager::Set(Date& aDate)
{
  Set(aDate.TimeStamp());
}

void
TimeManager::Set(double aTime)
{
  nsCOMPtr<nsITimeService> timeService = do_GetService(TIMESERVICE_CONTRACTID);
  if (timeService) {
    timeService->Set(aTime);
  }
}

} // namespace time
} // namespace dom
} // namespace mozilla
