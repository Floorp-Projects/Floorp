/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_time_TimeManager_h
#define mozilla_dom_time_TimeManager_h

#include "mozilla/Attributes.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Date;

namespace time {

class TimeManager MOZ_FINAL : public nsISupports
                            , public nsWrapperCache
{
public:
  static bool PrefEnabled(JSContext* aCx, JSObject* aGlobal)
  {
#ifdef MOZ_TIME_MANAGER
    return true;
#else
    return false;
#endif
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TimeManager)

  explicit TimeManager(nsPIDOMWindow* aWindow)
    : mWindow(aWindow)
  {
    SetIsDOMBinding();
  }

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }
  JSObject* WrapObject(JSContext* aCx);

  void Set(Date& aDate);
  void Set(double aTime);

private:
  ~TimeManager() {}

  nsCOMPtr<nsPIDOMWindow> mWindow;
};

} // namespace time
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_time_TimeManager_h
