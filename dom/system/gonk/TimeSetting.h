/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_timesetting_h__
#define mozilla_system_timesetting_h__

#include "jspubtd.h"
#include "nsIObserver.h"

namespace mozilla {
namespace system {

class ResultListener;

class TimeSetting : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  TimeSetting();
  virtual ~TimeSetting();
  static nsresult SetTimezone(const JS::Value &aValue, JSContext *aContext);
};

} // namespace system
} // namespace mozilla

#endif // mozilla_system_timesetting_h__

