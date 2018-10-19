// |reftest| skip-if(!Intl.hasOwnProperty('Segmenter')) -- Intl.Segmenter is not enabled unconditionally
// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter.prototype.resolvedOptions
description: Verifies the branding check for the "resolvedOptions" function of the Segmenter prototype object.
info: |
    Intl.Segmenter.prototype.resolvedOptions ()

    2. If Type(pr) is not Object or pr does not have an [[InitializedSegmenter]] internal slot, throw a TypeError exception.
features: [Intl.Segmenter]
---*/

const fn = Intl.Segmenter.prototype.resolvedOptions;

assert.throws(TypeError, () => fn.call(undefined), "undefined");
assert.throws(TypeError, () => fn.call(null), "null");
assert.throws(TypeError, () => fn.call(true), "true");
assert.throws(TypeError, () => fn.call(""), "empty string");
assert.throws(TypeError, () => fn.call(Symbol()), "symbol");
assert.throws(TypeError, () => fn.call(1), "1");
assert.throws(TypeError, () => fn.call({}), "plain object");
assert.throws(TypeError, () => fn.call(Intl.Segmenter), "Intl.Segmenter");
assert.throws(TypeError, () => fn.call(Intl.Segmenter.prototype), "Intl.Segmenter.prototype");

reportCompare(0, 0);
