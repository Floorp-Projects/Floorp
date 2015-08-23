/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_performancecompositetiming_h___
#define mozilla_dom_performancecompositetiming_h___

#include "mozilla/dom/PerformanceEntry.h"

namespace mozilla {
namespace dom {

// http://www.w3.org/TR/frame-timing/#performancecompositetiming
class PerformanceCompositeTiming final : public PerformanceEntry
{
public:
  PerformanceCompositeTiming(nsISupports* aParent,
                             const nsAString& aName,
                             const DOMHighResTimeStamp& aStartTime,
                             uint32_t aSourceFrameNumber);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual DOMHighResTimeStamp StartTime() const override
  {
    return mStartTime;
  }

  uint32_t SourceFrameNumber() const
  {
    return mSourceFrameNumber;
  }

protected:
  virtual ~PerformanceCompositeTiming();
  DOMHighResTimeStamp mStartTime;
  uint32_t mSourceFrameNumber;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_performancecompositetiming_h___ */
