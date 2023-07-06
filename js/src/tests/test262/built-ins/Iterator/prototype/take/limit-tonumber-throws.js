// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.take
description: >
  Throws a RangeError exception when limit argument valueOf throws.
info: |
  %Iterator.prototype%.take ( limit )

  3. Let numLimit be ? ToNumber(limit).

features: [iterator-helpers]
---*/
let iterator = (function* () {})();

assert.throws(Test262Error, () => {
  iterator.take({
    valueOf: function () {
      throw new Test262Error();
    },
  });
});

reportCompare(0, 0);
