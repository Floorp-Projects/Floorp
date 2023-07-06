// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.map
description: >
  Iterator.prototype.map expects to be called with a callable argument.
info: |
  %Iterator.prototype%.map ( mapper )

  3. If IsCallable(mapper) is false, throw a TypeError exception.

features: [iterator-helpers]
flags: []
---*/
let nonCallable = {};
let iterator = (function* () {})();

assert.throws(TypeError, function () {
  iterator.map(nonCallable);
});

reportCompare(0, 0);
