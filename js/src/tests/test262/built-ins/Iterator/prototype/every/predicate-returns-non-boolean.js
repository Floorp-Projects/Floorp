// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.every
description: >
  Iterator.prototype.every coerces predicate return value to boolean
info: |
  %Iterator.prototype%.every ( predicate )

  4.f. If ToBoolean(result) is false, return ? IteratorClose(iterated, NormalCompletion(false)).

features: [iterator-helpers]
flags: []
---*/
function* g() {
  for (let i = 4; i >= 0; --i) {
    yield i;
  }
}

let iter = g();

let predicateCalls = 0;
let result = iter.every(v => {
  ++predicateCalls;
  return v;
});

assert.sameValue(result, false);
assert.sameValue(predicateCalls, 5);

reportCompare(0, 0);
