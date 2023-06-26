// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: ZonedDateTime strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const str = "1970-01-01T00:02:00.000000000+00:02[+00:01:30.987654321]";

const result = Temporal.ZonedDateTime.from(str);
assert.sameValue(result.timeZoneId, "+00:01:30.987654321", "Time zone determined from bracket name");

reportCompare(0, 0);
