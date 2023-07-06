// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.flatMap
description: >
  Iterator.prototype.flatMap mapper this value is undefined
info: |
  %Iterator.prototype%.flatMap ( mapper )

  5.b.iv. Let mapped be Completion(Call(mapper, undefined, « value, 𝔽(counter) »)).

features: [iterator-helpers]
flags: []
---*/
function* g() {
  yield 0;
}

let iter = g();

let expectedThis = function () {
  return this;
}.call(undefined);

let assertionCount = 0;
iter = iter.flatMap(function (v, count) {
  assert.sameValue(this, expectedThis);
  ++assertionCount;
  return [v];
});

iter.next();
assert.sameValue(assertionCount, 1);

reportCompare(0, 0);
