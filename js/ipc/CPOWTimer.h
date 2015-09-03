/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CPOWTIMER_H
#define CPOWTIMER_H

#include "prinrval.h"
#include "jsapi.h"

/**
 * A stopwatch measuring the duration of a CPOW call.
 *
 * As the process is consuming neither user time nor system time
 * during a CPOW call, we measure such durations using wallclock time.
 *
 * This stopwatch is active iff JSRuntime::stopwatch.isActive is set.
 * Upon destruction, update JSRuntime::stopwatch.data.totalCPOWTime.
 */
class MOZ_RAII CPOWTimer final {
  public:
    explicit inline CPOWTimer(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~CPOWTimer();

  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    /**
     * The context in which this timer was created, or `nullptr` if
     * CPOW monitoring was off when the timer was created.
     */
    JSContext* cx_;

    /**
     * The instant at which the stopwatch was started. Undefined
     * if CPOW monitoring was off when the timer was created.
     */
    int64_t startInterval_;
};

#endif
