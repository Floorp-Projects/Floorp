/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CPOWTIMER_H
#define CPOWTIMER_H

#include "prinrval.h"

class JSObject;

/**
 * A stopwatch measuring the duration of a CPOW call.
 *
 * As the process is consuming neither user time nor system time
 * during a CPOW call, we measure such durations using wallclock time.
 *
 * This stopwatch is active iff JSRuntime::stopwatch.isActive is set.
 * Upon destruction, update JSRuntime::stopwatch.data.totalCPOWTime.
 */
class MOZ_STACK_CLASS CPOWTimer final {
  public:
    explicit inline CPOWTimer(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
        : startInterval(PR_IntervalNow())
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~CPOWTimer();

  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    /**
     * The instant at which the stopwatch was started.
     */
    PRIntervalTime startInterval;
};

#endif
