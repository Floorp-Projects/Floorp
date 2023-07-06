// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.drop
description: >
  Underlying iterator next returns non-object
info: |
  %Iterator.prototype%.drop ( limit )

  6.b.ii. Let next be ? IteratorStep(iterated).

  6.c.i. Let next be ? IteratorStep(iterated).

features: [iterator-helpers]
flags: []
---*/
class NonObjectIterator extends Iterator {
  next() {
    return null;
  }
}

let iterator = new NonObjectIterator().drop(0);

assert.throws(TypeError, function () {
  iterator.next();
});

iterator = new NonObjectIterator().drop(2);

assert.throws(TypeError, function () {
  iterator.next();
});

reportCompare(0, 0);
