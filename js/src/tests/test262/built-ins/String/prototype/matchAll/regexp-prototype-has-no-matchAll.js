// |reftest| skip -- Symbol.matchAll,String.prototype.matchAll is not supported
// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: Behavior when @@matchAll is removed from RegExp's prototype
info: |
  String.prototype.matchAll ( regexp )
    1. Let O be ? RequireObjectCoercible(this value).
    2. If regexp is neither undefined nor null, then
      a. Let matcher be ? GetMethod(regexp, @@matchAll).
      b. If matcher is not undefined, then
        [...]
    [...]
    4. Let matcher be ? RegExpCreate(R, "g").
    [...]

  21.2.3.2.3 Runtime Semantics: RegExpCreate ( P, F )
    [...]
    2. Return ? RegExpInitialize(obj, P, F).

  21.2.3.2.2 Runtime Semantics: RegExpInitialize ( obj, pattern, flags )
    1. If pattern is undefined, let P be the empty String.
    2. Else, let P be ? ToString(pattern).
    [...]
features: [Symbol.matchAll, String.prototype.matchAll]
includes: [compareArray.js, compareIterator.js, regExpUtils.js]
---*/

delete RegExp.prototype[Symbol.matchAll];
var str = '/a/g*/b/g';

assert.compareIterator(str.matchAll(/\w/g), [
  matchValidator(['/a/g'], 0, str),
  matchValidator(['/b/g'], 5, str)
]);

reportCompare(0, 0);
