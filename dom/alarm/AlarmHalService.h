/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef mozilla_dom_alarm_AlarmHalService_h
#define mozilla_dom_alarm_AlarmHalService_h

#include "base/basictypes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "mozilla/Services.h"
#include "nsIAlarmHalService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "prtime.h"

namespace mozilla {
namespace dom {
namespace alarm {

class AlarmHalService : public nsIAlarmHalService, 
                        mozilla::hal::AlarmObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALARMHALSERVICE

  void Init();
  virtual ~AlarmHalService();

  static nsRefPtr<AlarmHalService> sSingleton;
  static already_AddRefed<nsIAlarmHalService> GetInstance();

  // Implementing hal::AlarmObserver
  void Notify(const mozilla::void_t& aVoid);

private:
  bool mAlarmEnabled;
  nsCOMPtr<nsIAlarmFiredCb> mAlarmFiredCb;

  // TODO The mTimezoneChangedCb would be called 
  // when a timezone-changed event is detected 
  // at run-time. To do so, we can register a 
  // timezone-changed observer, see bug 714358.
  // We need to adjust the alarm time respect to
  // the correct timezone where user is located.
  nsCOMPtr<nsITimezoneChangedCb> mTimezoneChangedCb;

  PRInt32 GetTimezoneOffset(bool aIgnoreDST);
};

} // namespace alarm
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_alarm_AlarmHalService_h
