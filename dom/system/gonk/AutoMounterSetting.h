/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_automountersetting_h__
#define mozilla_system_automountersetting_h__

#include "nsIObserver.h"

namespace mozilla {
namespace system {

class ResultListener;

class AutoMounterSetting : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  AutoMounterSetting();
  virtual ~AutoMounterSetting();
};

}   // namespace system
}   // namespace mozilla

#endif  // mozilla_system_automountersetting_h__

