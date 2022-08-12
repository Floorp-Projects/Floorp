// |reftest| skip -- Temporal is not supported
// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: sub-minute offset
features: [Temporal]
---*/

var zone = new Temporal.TimeZone("Europe/Amsterdam");
var inst = Temporal.Instant.from("1900-01-01T12:00Z");
var dtm = Temporal.PlainDateTime.from("1900-01-01T12:00");
assert.sameValue(zone.id, `${ zone }`)
assert.sameValue(zone.getOffsetNanosecondsFor(inst), 1172000000000)
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "1900-01-01T11:40:28Z")
assert.sameValue(`${ zone.getNextTransition(inst) }`, "1916-04-30T23:40:28Z")
assert.sameValue(zone.getPreviousTransition(inst), null)

reportCompare(0, 0);
