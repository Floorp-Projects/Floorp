// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Temporal.Calendar.prototype.dateFromFields should throw TypeError with wrong type.
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Let result be ? ISODateFromFields(fields, options).
  7. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]], result.[[Day]], calendar).
features: [Symbol, Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601")

// Check throw for first arg
assert.throws(TypeError, () => cal.dateFromFields(),
    "Temporal.Calendar.prototype.dateFromFields called on non-object");
[undefined, true, false, 123, 456n, Symbol(), "string"].forEach(
    function(fields) {
      assert.throws(TypeError, () => cal.dateFromFields(fields),
          "Temporal.Calendar.prototype.dateFromFields called on non-object");
      assert.throws(TypeError, () => cal.dateFromFields(fields, undefined),
          "Temporal.Calendar.prototype.dateFromFields called on non-object");
      assert.throws(TypeError, () => cal.dateFromFields(fields, {overflow: "constrain"}),
          "Temporal.Calendar.prototype.dateFromFields called on non-object");
      assert.throws(TypeError, () => cal.dateFromFields(fields, {overflow: "reject"}),
          "Temporal.Calendar.prototype.dateFromFields called on non-object");
    });

assert.throws(TypeError, () => cal.dateFromFields({month: 1, day: 17}),
    "invalid_argument");
assert.throws(TypeError, () => cal.dateFromFields({year: 2021, day: 17}),
    "invalid_argument");
assert.throws(TypeError, () => cal.dateFromFields({year: 2021, month: 12}),
    "invalid_argument");

reportCompare(0, 0);
