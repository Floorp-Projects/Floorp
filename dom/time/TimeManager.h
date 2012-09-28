/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_time_TimeManager_h
#define mozilla_dom_time_TimeManager_h

#include "nsIDOMTimeManager.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace time {

class TimeManager MOZ_FINAL : public nsIDOMMozTimeManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZTIMEMANAGER
};

} // namespace time
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_time_TimeManager_h
