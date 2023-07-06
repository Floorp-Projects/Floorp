// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.filter
description: >
  Iterator.prototype.filter coerces predicate return value to boolean
info: |
  %Iterator.prototype%.filter ( predicate )

  3.b.vi. If ToBoolean(selected) is true, then

features: [iterator-helpers]
flags: []
---*/
function* g() {
  yield 0;
  yield 0;
  yield 0;
  yield 1;
}

let iter = g();

let predicateCalls = 0;
iter = iter.filter(v => {
  ++predicateCalls;
  return v;
});

assert.sameValue(predicateCalls, 0);

iter.next();

assert.sameValue(predicateCalls, 4);

iter.next();

assert.sameValue(predicateCalls, 4);

reportCompare(0, 0);
