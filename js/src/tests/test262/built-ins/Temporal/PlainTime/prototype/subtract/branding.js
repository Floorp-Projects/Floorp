// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.subtract
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const subtract = Temporal.PlainTime.prototype.subtract;

assert.sameValue(typeof subtract, "function");

assert.throws(TypeError, () => subtract.call(undefined), "undefined");
assert.throws(TypeError, () => subtract.call(null), "null");
assert.throws(TypeError, () => subtract.call(true), "true");
assert.throws(TypeError, () => subtract.call(""), "empty string");
assert.throws(TypeError, () => subtract.call(Symbol()), "symbol");
assert.throws(TypeError, () => subtract.call(1), "1");
assert.throws(TypeError, () => subtract.call({}), "plain object");
assert.throws(TypeError, () => subtract.call(Temporal.PlainTime), "Temporal.PlainTime");
assert.throws(TypeError, () => subtract.call(Temporal.PlainTime.prototype), "Temporal.PlainTime.prototype");

reportCompare(0, 0);
