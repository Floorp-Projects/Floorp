/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateCacheCleaner.h"

#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "mozilla/StaticPtr.h"
#include "nsCxPusher.h"

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
    mozilla::AutoSafeJSContext cx;
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
