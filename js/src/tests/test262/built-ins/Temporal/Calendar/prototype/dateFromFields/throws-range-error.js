// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Temporal.Calendar.prototype.dateFromFields should throw RangeError for
  input not in valid range.
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Let result be ? ISODateFromFields(fields, options).
  7. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]], result.[[Day]], calendar).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601")

assert.throws(RangeError, () => cal.dateFromFields({year: 2021, monthCode: "m1", day: 17}),
    "monthCode value is out of range.");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, monthCode: "M1", day: 17}),
    "monthCode value is out of range.");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, monthCode: "m01", day: 17}),
    "monthCode value is out of range.");

assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 12, monthCode: "M11", day: 17}),
    "monthCode value is out of range.");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, monthCode: "M00", day: 17}),
    "monthCode value is out of range.");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, monthCode: "M19", day: 17}),
    "monthCode value is out of range.");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, monthCode: "M99", day: 17}),
    "monthCode value is out of range.");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, monthCode: "M13", day: 17}),
    "monthCode value is out of range.");

assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: -1, day: 17}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: -Infinity, day: 17}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 7, day: -17}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 7, day: -Infinity}),
    "Invalid time value");

assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 12, day: 0}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 12, day: 32}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 1, day: 32}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 2, day: 29}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 6, day: 31}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 9, day: 31}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 0, day: 5}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 13, day: 5}, {overflow: "reject"}),
    "Invalid time value");

assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M12", day: 0}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M12", day: 32}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M01", day: 32}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M02", day: 29}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M06", day: 31}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M09", day: 31}, {overflow: "reject"}),
    "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M00", day: 5}, {overflow: "reject"}),
    "monthCode value is out of range.");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, monthCode: "M13", day: 5}, {overflow: "reject"}),
    "monthCode value is out of range.");

assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 12, day: 0}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 0, day: 3}), "Invalid time value");

// Check throw for the second arg
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 7, day: 13}, {overflow: "invalid"}),
    RangeError,
    "Value invalid out of range for Temporal.Calendar.prototype.dateFromFields options property overflow");

assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 1, day: 32}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 2, day: 29}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 3, day: 32}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 4, day: 31}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 5, day: 32}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 6, day: 31}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 7, day: 32}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 8, day: 32}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 9, day: 31}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 10, day: 32}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 11, day: 31}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 12, day: 32}, {overflow: "reject"}), "Invalid time value");
assert.throws(RangeError, () => cal.dateFromFields(
    {year: 2021, month: 13, day: 5}, {overflow: "reject"}), "Invalid time value");

reportCompare(0, 0);
