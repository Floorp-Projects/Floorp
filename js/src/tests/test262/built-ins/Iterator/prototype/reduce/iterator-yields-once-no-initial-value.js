// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.reduce
description: >
  Iterator.prototype.reduce calls reducer once for each value yielded by the underlying iterator except the first when not passed an initial value
info: |
  %Iterator.prototype%.reduce ( reducer )

features: [iterator-helpers]
flags: []
---*/
function* g() {
  yield 'a';
}

let iter = g();

let assertionCount = 0;
let result = iter.reduce((memo, v, count) => {
  ++assertionCount;
  return v;
});

assert.sameValue(result, 'a');
assert.sameValue(assertionCount, 0);

reportCompare(0, 0);
