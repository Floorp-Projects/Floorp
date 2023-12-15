// |reftest| skip -- set-methods is not supported
// Copyright (C) 2023 Anthony Frehner. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-set.prototype.union
description: Set.prototype.union RequireInternalSlot
info: |
    2. Perform ? RequireInternalSlot(O, [[SetData]])
features: [set-methods]
---*/

const union = Set.prototype.union;

assert.sameValue(typeof union, "function");

assert.throws(TypeError, () => union.call(undefined), "undefined");
assert.throws(TypeError, () => union.call(null), "null");
assert.throws(TypeError, () => union.call(true), "true");
assert.throws(TypeError, () => union.call(""), "empty string");
assert.throws(TypeError, () => union.call(Symbol()), "symbol");
assert.throws(TypeError, () => union.call(1), "1");
assert.throws(TypeError, () => union.call(1n), "1n");
assert.throws(TypeError, () => union.call({}), "plain object");
assert.throws(TypeError, () => union.call([]), "array");
assert.throws(TypeError, () => union.call(new Map()), "map");
assert.throws(TypeError, () => union.call(Set.prototype), "Set.prototype");

reportCompare(0, 0);
