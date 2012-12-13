/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/ClearOnShutdown.h"
#include "DateCacheCleaner.h"

#include "nsIJSContextStack.h"
#include "mozilla/StaticPtr.h"

using namespace mozilla::hal;

namespace mozilla {
namespace dom {
namespace time {

class DateCacheCleaner : public SystemTimezoneChangeObserver
{
public:
  DateCacheCleaner()
  {
    RegisterSystemTimezoneChangeObserver(this);
  }

  ~DateCacheCleaner()
  {
    UnregisterSystemTimezoneChangeObserver(this);
  }
  void Notify(const SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo)
  {
    nsCOMPtr<nsIThreadJSContextStack> stack =
      do_GetService("@mozilla.org/js/xpc/ContextStack;1");
    if (!stack) {
      NS_WARNING("Failed to get JSContextStack");
    }
    JSContext *cx = stack->GetSafeJSContext();
    if (!cx) {
      NS_WARNING("Failed to GetSafeJSContext");
    }
    JSAutoRequest ar(cx);
    JS_ClearDateCaches(cx);
  }

};

StaticAutoPtr<DateCacheCleaner> sDateCacheCleaner;

void
InitializeDateCacheCleaner()
{
  if (!sDateCacheCleaner) {
    sDateCacheCleaner = new DateCacheCleaner();
    ClearOnShutdown(&sDateCacheCleaner);
  }
}

} // namespace time
} // namespace dom
} // namespace mozilla
