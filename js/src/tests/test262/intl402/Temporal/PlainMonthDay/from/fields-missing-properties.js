// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: >
  Basic tests for PlainMonthDay.from(object) with missing properties for non-ISO
  calendars
features: [Temporal]
---*/

assert.throws(TypeError, () => Temporal.PlainMonthDay.from({ month: 11, day: 18, calendar: "gregory" }), "month, day with non-iso8601 calendar");

reportCompare(0, 0);
