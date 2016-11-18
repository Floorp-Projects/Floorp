/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_performance_PerformanceService_h
#define dom_performance_PerformanceService_h

#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsDOMNavigationTiming.h"

namespace mozilla {
namespace dom {

// This class is thread-safe.

// We use this singleton for having the correct value of performance.timeOrigin.
// This value must be calculated on top of the pair:
// - mCreationTimeStamp (monotonic clock)
// - mCreationEpochTime (unix epoch time)
// These 2 values must be taken "at the same time" in order to be used
// correctly.

class PerformanceService
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PerformanceService)

  static PerformanceService*
  GetOrCreate();

  DOMHighResTimeStamp
  TimeOrigin(const TimeStamp& aCreationTimeStamp) const;

private:
  PerformanceService();
  ~PerformanceService() = default;

  TimeStamp mCreationTimeStamp;
  PRTime mCreationEpochTime;
};

} // dom namespace
} // mozilla namespace

#endif // dom_performance_PerformanceService_h
