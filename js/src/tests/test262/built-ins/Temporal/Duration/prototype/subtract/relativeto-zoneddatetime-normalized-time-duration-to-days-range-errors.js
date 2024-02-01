// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.subtract
description: >
  Abstract operation NormalizedTimeDurationToDays can throw four different
  RangeErrors.
info: |
  NormalizedTimeDurationToDays ( norm, zonedRelativeTo, timeZoneRec [ , precalculatedPlainDateTime ] )
    22. If days < 0 and sign = 1, throw a RangeError exception.
    23. If days > 0 and sign = -1, throw a RangeError exception.
    ...
    25. If NormalizedTimeDurationSign(_norm_) = 1 and sign = -1, throw a RangeError exception.
    ...
    28. If dayLength ≥ 2⁵³, throw a RangeError exception.
features: [Temporal, BigInt]
includes: [temporalHelpers.js]
---*/

const dayNs = 86_400_000_000_000;
const dayDuration = Temporal.Duration.from({ days: 1 });
const epochInstant = new Temporal.Instant(0n);

function timeZoneSubstituteValues(
  getPossibleInstantsFor,
  getOffsetNanosecondsFor
) {
  const tz = new Temporal.TimeZone("UTC");
  TemporalHelpers.substituteMethod(
    tz,
    "getPossibleInstantsFor",
    getPossibleInstantsFor
  );
  TemporalHelpers.substituteMethod(
    tz,
    "getOffsetNanosecondsFor",
    getOffsetNanosecondsFor
  );
  return tz;
}

// Step 22: days < 0 and sign = 1
let zdt = new Temporal.ZonedDateTime(
  -1n, // Set DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for first call, AddDuration step 15
      [epochInstant], // Returned in AddDuration step 16, setting _endNs_ -> DifferenceZonedDateTime _ns2_
      [epochInstant], // Returned in step 16, setting _relativeResult_
    ],
    [
      // Behave normally in 3 calls made prior to NormalizedTimeDurationToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      dayNs - 1, // Returned in step 8, setting _startDateTime_
      -dayNs + 1, // Returned in step 9, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Subtracting day from day sets largestUnit to 'day', avoids having any week/month/year components in difference
  dayDuration.subtract(dayDuration, {
    relativeTo: zdt,
  })
);

// Step 23: days > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  1n, // Set DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for first call, AddDuration step 15
      [epochInstant], // Returned in AddDuration step 16, setting _endNs_ -> DifferenceZonedDateTime _ns2_
      [epochInstant], // Returned in step 16, setting _relativeResult_
    ],
    [
      // Behave normally in 3 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      -dayNs + 1, // Returned in step 8, setting _startDateTime_
      dayNs - 1, // Returned in step 9, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Subtracting day from day sets largestUnit to 'day', avoids having any week/month/year components in difference
  dayDuration.subtract(dayDuration, {
    relativeTo: zdt,
  })
);

// Step 25: nanoseconds > 0 and sign = -1
zdt = new Temporal.ZonedDateTime(
  0n, // Set DifferenceZonedDateTime _ns1_
  timeZoneSubstituteValues(
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for first call, AddDuration step 15
      [new Temporal.Instant(-1n)], // Returned in AddDuration step 16, setting _endNs_ -> DifferenceZonedDateTime _ns2_
      [new Temporal.Instant(-2n)], // Returned in step 16, setting _relativeResult_
      [new Temporal.Instant(-4n)], // Returned in step 21.a, setting _oneDayFarther_
    ],
    [
      // Behave normally in 3 calls made prior to NanosecondsToDays
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      TemporalHelpers.SUBSTITUTE_SKIP,
      dayNs - 1, // Returned in step 8, setting _startDateTime_
      -dayNs + 1, // Returned in step 9, setting _endDateTime_
    ]
  )
);
assert.throws(RangeError, () =>
  // Subtracting day from day sets largestUnit to 'day', avoids having any week/month/year components in difference
  dayDuration.subtract(dayDuration, {
    relativeTo: zdt,
  })
);

// Step 28: day length is an unsafe integer
zdt = new Temporal.ZonedDateTime(
  0n,
  timeZoneSubstituteValues(
    [
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for AddDuration step 15
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for AddDuration step 16
      TemporalHelpers.SUBSTITUTE_SKIP, // Behave normally for step 16, setting _relativeResult_
      // Returned in step 21.a, making _oneDayFarther_ 2^53 ns later than _relativeResult_
      [new Temporal.Instant(2n ** 53n - 3n * BigInt(dayNs))],
    ],
    []
  )
);
const twoDaysDuration = new Temporal.Duration(0, 0, 0, 2);
assert.throws(RangeError, () =>
  dayDuration.subtract(twoDaysDuration, {
    relativeTo: zdt,
  }),
  "Should throw RangeError when time zone calculates an outrageous day length"
);

reportCompare(0, 0);
