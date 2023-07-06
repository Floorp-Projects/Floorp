// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.reduce
description: >
  Iterator.prototype.reduce supports a this value that does not inherit from Iterator.prototype but implements the iterator protocol
info: |
  %Iterator.prototype%.reduce ( reducer )

includes: [compareArray.js]
features: [iterator-helpers]
flags: []
---*/
let iter = {
  get next() {
    let count = 3;
    return function () {
      --count;
      return count >= 0 ? { done: false, value: count } : { done: true, value: undefined };
    };
  },
};

let reducerCalls = 0;
let result = Iterator.prototype.reduce.call(iter, function (memo, v) {
  ++reducerCalls;
  memo.push(v);
  return memo;
}, []);

assert.compareArray(result, [2, 1, 0]);
assert.sameValue(reducerCalls, 3);

reportCompare(0, 0);
