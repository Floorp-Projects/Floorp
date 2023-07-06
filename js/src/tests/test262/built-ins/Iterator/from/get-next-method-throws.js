// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iterator.from
description: >
  Underlying iterator has throwing next getter
info: |
  Iterator.from ( O )

  4. Let iterated be ? GetIteratorDirect(O).

features: [iterator-helpers]
flags: []
---*/
class ThrowingIterator {
  get next() {
    throw new Test262Error();
  }
}

let iterator = new ThrowingIterator();

assert.throws(Test262Error, function () {
  Iterator.from(iterator);
});

reportCompare(0, 0);
