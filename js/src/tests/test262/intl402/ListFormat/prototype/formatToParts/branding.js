// |reftest| skip-if(!Intl.hasOwnProperty('ListFormat')) -- Intl.ListFormat is not enabled unconditionally
// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.ListFormat.prototype.formatToParts
description: Verifies the branding check for the "formatToParts" function of the ListFormat prototype object.
info: |
    Intl.ListFormat.prototype.formatToParts([ list ])

    2. If Type(lf) is not Object, throw a TypeError exception.
    3. If lf does not have an [[InitializedListFormat]] internal slot, throw a TypeError exception.
features: [Intl.ListFormat]
---*/

const fn = Intl.ListFormat.prototype.formatToParts;

assert.throws(TypeError, () => fn.call(undefined), "undefined");
assert.throws(TypeError, () => fn.call(null), "null");
assert.throws(TypeError, () => fn.call(true), "true");
assert.throws(TypeError, () => fn.call(""), "empty string");
assert.throws(TypeError, () => fn.call(Symbol()), "symbol");
assert.throws(TypeError, () => fn.call(1), "1");
assert.throws(TypeError, () => fn.call({}), "plain object");
assert.throws(TypeError, () => fn.call(Intl.ListFormat), "Intl.ListFormat");
assert.throws(TypeError, () => fn.call(Intl.ListFormat.prototype), "Intl.ListFormat.prototype");

reportCompare(0, 0);
