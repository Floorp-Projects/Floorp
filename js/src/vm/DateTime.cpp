/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTime.h"

#include <time.h>

#include "jsutil.h"

static bool
ComputeLocalTime(time_t local, struct tm *ptm)
{
#ifdef HAVE_LOCALTIME_R
    return localtime_r(&local, ptm);
#else
    struct tm *otm = localtime(&local);
    if (!otm)
        return false;
    *ptm = *otm;
    return true;
#endif
}

static int32_t
LocalUTCDifferenceSeconds()
{
    using js::SecondsPerDay;

#if defined(XP_WIN)
    // Windows doesn't follow POSIX: updates to the TZ environment variable are
    // not reflected immediately on that platform as they are on other systems
    // without this call.
    _tzset();
#endif

    /*
     * Get the difference between this time zone and GMT, by checking the local
     * time for days 0 and 180 of 1970, using a date for which daylight savings
     * time was not in effect.
     */
    int day = 0;
    struct tm tm;

    if (!ComputeLocalTime(0, &tm))
        return 0;
    if (tm.tm_isdst > 0) {
        day = 180;
        if (!ComputeLocalTime(SecondsPerDay * day, &tm))
            return 0;
    }

    int32_t time = (tm.tm_hour * 3600) + (tm.tm_min * 60) + tm.tm_sec;
    time = SecondsPerDay - time;

    if (tm.tm_yday == day)
        time -= SecondsPerDay;

    return time;
}

void
js::DateTimeInfo::updateTimeZoneAdjustment()
{
    double newTZA = -(LocalUTCDifferenceSeconds() * msPerSecond);
    if (newTZA == localTZA_)
        return;

    localTZA_ = newTZA;

    /*
     * The initial range values are carefully chosen to result in a cache miss
     * on first use given the range of possible values.  Be careful to keep
     * these values and the caching algorithm in sync!
     */
    offsetMilliseconds = 0;
    rangeStartSeconds = rangeEndSeconds = INT64_MIN;
    oldOffsetMilliseconds = 0;
    oldRangeStartSeconds = oldRangeEndSeconds = INT64_MIN;

    sanityCheck();
}

/*
 * Since getDSTOffsetMilliseconds guarantees that all times seen will be
 * positive, we can initialize the range at construction time with large
 * negative numbers to ensure the first computation is always a cache miss and
 * doesn't return a bogus offset.
 */
js::DateTimeInfo::DateTimeInfo()
{
    // Set to a totally impossible TZA so that the comparison above will fail
    // and all fields will be properly initialized.
    localTZA_ = MOZ_DOUBLE_NaN();
    updateTimeZoneAdjustment();
}

int64_t
js::DateTimeInfo::computeDSTOffsetMilliseconds(int64_t localTimeSeconds)
{
    MOZ_ASSERT(localTimeSeconds >= 0);
    MOZ_ASSERT(localTimeSeconds <= MaxUnixTimeT);

#if defined(XP_WIN)
    /* Windows does not follow POSIX. Updates to the
     * TZ environment variable are not reflected
     * immediately on that platform as they are
     * on UNIX systems without this call.
     */
    _tzset();
#endif

    struct tm tm;
    if (!ComputeLocalTime(static_cast<time_t>(localTimeSeconds), &tm))
        return 0;

    int32_t base = LocalUTCDifferenceSeconds();

    int32_t dayoff = int32_t((localTimeSeconds - base) % (SecondsPerHour * 24));
    int32_t tmoff = tm.tm_sec + (tm.tm_min * SecondsPerMinute) +
        (tm.tm_hour * SecondsPerHour);

    int32_t diff = tmoff - dayoff;

    if (diff < 0)
        diff += SecondsPerDay;

    return diff * msPerSecond;
}

int64_t
js::DateTimeInfo::getDSTOffsetMilliseconds(int64_t localTimeMilliseconds)
{
    sanityCheck();

    int64_t localTimeSeconds = localTimeMilliseconds / msPerSecond;

    if (localTimeSeconds > MaxUnixTimeT) {
        localTimeSeconds = MaxUnixTimeT;
    } else if (localTimeSeconds < 0) {
        /* Go ahead a day to make localtime work (does not work with 0). */
        localTimeSeconds = SecondsPerDay;
    }

    /*
     * NB: Be aware of the initial range values when making changes to this
     *     code: the first call to this method, with those initial range
     *     values, must result in a cache miss.
     */

    if (rangeStartSeconds <= localTimeSeconds &&
        localTimeSeconds <= rangeEndSeconds) {
        return offsetMilliseconds;
    }

    if (oldRangeStartSeconds <= localTimeSeconds &&
        localTimeSeconds <= oldRangeEndSeconds) {
        return oldOffsetMilliseconds;
    }

    oldOffsetMilliseconds = offsetMilliseconds;
    oldRangeStartSeconds = rangeStartSeconds;
    oldRangeEndSeconds = rangeEndSeconds;

    if (rangeStartSeconds <= localTimeSeconds) {
        int64_t newEndSeconds = Min(rangeEndSeconds + RangeExpansionAmount, MaxUnixTimeT);
        if (newEndSeconds >= localTimeSeconds) {
            int64_t endOffsetMilliseconds = computeDSTOffsetMilliseconds(newEndSeconds);
            if (endOffsetMilliseconds == offsetMilliseconds) {
                rangeEndSeconds = newEndSeconds;
                return offsetMilliseconds;
            }

            offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
            if (offsetMilliseconds == endOffsetMilliseconds) {
                rangeStartSeconds = localTimeSeconds;
                rangeEndSeconds = newEndSeconds;
            } else {
                rangeEndSeconds = localTimeSeconds;
            }
            return offsetMilliseconds;
        }

        offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
        rangeStartSeconds = rangeEndSeconds = localTimeSeconds;
        return offsetMilliseconds;
    }

    int64_t newStartSeconds = Max<int64_t>(rangeStartSeconds - RangeExpansionAmount, 0);
    if (newStartSeconds <= localTimeSeconds) {
        int64_t startOffsetMilliseconds = computeDSTOffsetMilliseconds(newStartSeconds);
        if (startOffsetMilliseconds == offsetMilliseconds) {
            rangeStartSeconds = newStartSeconds;
            return offsetMilliseconds;
        }

        offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
        if (offsetMilliseconds == startOffsetMilliseconds) {
            rangeStartSeconds = newStartSeconds;
            rangeEndSeconds = localTimeSeconds;
        } else {
            rangeStartSeconds = localTimeSeconds;
        }
        return offsetMilliseconds;
    }

    rangeStartSeconds = rangeEndSeconds = localTimeSeconds;
    offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
    return offsetMilliseconds;
}

void
js::DateTimeInfo::sanityCheck()
{
    MOZ_ASSERT(rangeStartSeconds <= rangeEndSeconds);
    MOZ_ASSERT_IF(rangeStartSeconds == INT64_MIN, rangeEndSeconds == INT64_MIN);
    MOZ_ASSERT_IF(rangeEndSeconds == INT64_MIN, rangeStartSeconds == INT64_MIN);
    MOZ_ASSERT_IF(rangeStartSeconds != INT64_MIN,
                  rangeStartSeconds >= 0 && rangeEndSeconds >= 0);
    MOZ_ASSERT_IF(rangeStartSeconds != INT64_MIN,
                  rangeStartSeconds <= MaxUnixTimeT && rangeEndSeconds <= MaxUnixTimeT);
}
