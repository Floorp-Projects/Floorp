// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.flatMap
description: >
  Underlying iterator return is called when result iterator is closed
info: |
  %Iterator.prototype%.flatMap ( mapper )

features: [iterator-helpers]
flags: []
---*/
let returnCount = 0;

class TestIterator extends Iterator {
  next() {
    return {
      done: false,
      value: 1,
    };
  }
  return() {
    ++returnCount;
    return {};
  }
}

let iterator = new TestIterator().flatMap(() => []);
assert.sameValue(returnCount, 0);
iterator.return();
assert.sameValue(returnCount, 1);
iterator.return();
assert.sameValue(returnCount, 1);

reportCompare(0, 0);
