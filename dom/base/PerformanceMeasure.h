/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_performancemeasure_h___
#define mozilla_dom_performancemeasure_h___

#include "mozilla/dom/PerformanceEntry.h"

namespace mozilla {
namespace dom {

// http://www.w3.org/TR/user-timing/#performancemeasure
class PerformanceMeasure MOZ_FINAL : public PerformanceEntry
{
public:
  PerformanceMeasure(nsPerformance* aPerformance,
                     const nsAString& aName,
                     DOMHighResTimeStamp aStartTime,
                     DOMHighResTimeStamp aEndTime);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual DOMHighResTimeStamp StartTime() const MOZ_OVERRIDE
  {
    return mStartTime;
  }

  virtual DOMHighResTimeStamp Duration() const MOZ_OVERRIDE
  {
    return mDuration;
  }

protected:
  virtual ~PerformanceMeasure();
  DOMHighResTimeStamp mStartTime;
  DOMHighResTimeStamp mDuration;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_performancemeasure_h___ */
