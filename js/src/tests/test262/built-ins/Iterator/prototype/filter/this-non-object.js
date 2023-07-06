// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.filter
description: >
  Iterator.prototype.filter throws TypeError when its this value is a non-object
info: |
  %Iterator.prototype%.filter ( predicate )

  1. Let iterated be ? GetIteratorDirect(this value).

features: [iterator-helpers]
flags: []
---*/
assert.throws(TypeError, function () {
  Iterator.prototype.filter.call(null, () => true);
});

Object.defineProperty(Number.prototype, 'next', {
  get: function () {
    throw new Test262Error();
  },
});
assert.throws(TypeError, function () {
  Iterator.prototype.filter.call(0, () => true);
});

reportCompare(0, 0);
