/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jslock_h
#define jslock_h

#include "mozilla/TimeStamp.h"

#ifdef JS_POSIX_NSPR

#include "vm/PosixNSPR.h"

#else /* JS_POSIX_NSPR */

# include "prcvar.h"
# include "prinit.h"
# include "prlock.h"
# include "prthread.h"

#endif

inline PRIntervalTime
DurationToPRInterval(mozilla::TimeDuration duration)
{
    double millis = duration.ToMilliseconds();
    return millis < double(UINT32_MAX)
           ? PR_MillisecondsToInterval(millis)
           : PR_INTERVAL_NO_TIMEOUT;
}

#endif /* jslock_h */
