/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_time_TimeManager_h
#define mozilla_dom_time_TimeManager_h

#include "mozilla/HalTypes.h"
#include "nsIDOMTimeManager.h"
#include "mozilla/Observer.h"
#include "mozilla/Attributes.h"

class nsPIDOMWindow;

namespace mozilla {

typedef Observer<hal::SystemTimeChange> SystemTimeObserver;

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
