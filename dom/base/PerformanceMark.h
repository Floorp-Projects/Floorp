/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_performancemark_h___
#define mozilla_dom_performancemark_h___

#include "mozilla/dom/PerformanceEntry.h"

namespace mozilla {
namespace dom {

// http://www.w3.org/TR/user-timing/#performancemark
class PerformanceMark final : public PerformanceEntry
{
public:
  PerformanceMark(nsPerformance* aPerformance,
                  const nsAString& aName);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual DOMHighResTimeStamp StartTime() const override
  {
    return mStartTime;
  }

protected:
  virtual ~PerformanceMark();
  DOMHighResTimeStamp mStartTime;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_performancemark_h___ */
