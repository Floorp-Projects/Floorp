// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.until
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const until = Temporal.PlainTime.prototype.until;

assert.sameValue(typeof until, "function");

assert.throws(TypeError, () => until.call(undefined), "undefined");
assert.throws(TypeError, () => until.call(null), "null");
assert.throws(TypeError, () => until.call(true), "true");
assert.throws(TypeError, () => until.call(""), "empty string");
assert.throws(TypeError, () => until.call(Symbol()), "symbol");
assert.throws(TypeError, () => until.call(1), "1");
assert.throws(TypeError, () => until.call({}), "plain object");
assert.throws(TypeError, () => until.call(Temporal.PlainTime), "Temporal.PlainTime");
assert.throws(TypeError, () => until.call(Temporal.PlainTime.prototype), "Temporal.PlainTime.prototype");

reportCompare(0, 0);
