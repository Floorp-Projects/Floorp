// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.forEach
description: >
  Iterator.prototype.forEach throws TypeError when its this value is a non-object
info: |
  %Iterator.prototype%.forEach ( fn )

features: [iterator-helpers]
flags: []
---*/
assert.throws(TypeError, function () {
  Iterator.prototype.forEach.call(null, () => {});
});

Object.defineProperty(Number.prototype, 'next', {
  get: function () {
    throw new Test262Error();
  },
});
assert.throws(TypeError, function () {
  Iterator.prototype.forEach.call(0, () => {});
});

reportCompare(0, 0);
