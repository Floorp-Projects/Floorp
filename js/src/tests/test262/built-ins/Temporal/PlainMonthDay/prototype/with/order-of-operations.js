// |reftest| skip -- Temporal is not supported
// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.with
description: Properties on an object passed to with() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainMonthDay(5, 2);
const expected = [
  "get calendar",
  "get timeZone",
  "get day",
  "get day.valueOf",
  "call day.valueOf",
  "get month",
  "get month.valueOf",
  "call month.valueOf",
  "get monthCode",
  "get monthCode.toString",
  "call monthCode.toString",
  "get year",
  "get year.valueOf",
  "call year.valueOf",
];
const actual = [];
const fields = {
  year: 1.7,
  month: 1.7,
  monthCode: "M01",
  day: 1.7,
};
const argument = new Proxy(fields, {
  get(target, key) {
    actual.push(`get ${key}`);
    const result = target[key];
    if (result === undefined) {
      return undefined;
    }
    return TemporalHelpers.toPrimitiveObserver(actual, result, key);
  },
  has(target, key) {
    actual.push(`has ${key}`);
    return key in target;
  },
});
const result = instance.with(argument);
TemporalHelpers.assertPlainMonthDay(result, "M01", 1);
assert.compareArray(actual, expected, "order of operations");

reportCompare(0, 0);
