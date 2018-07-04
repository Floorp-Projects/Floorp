// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Throws a RangeError is index < 0
info: |
  Atomics.wake( typedArray, index, count )

  2.Let i be ? ValidateAtomicAccess(typedArray, index).
    ...
      2.Let accessIndex be ? ToIndex(requestIndex).
        ...
        2.b If integerIndex < 0, throw a RangeError exception
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(RangeError, function() {
  Atomics.wake(i32a, -Infinity, poisoned);
}, '`Atomics.wake(i32a, -Infinity, poisoned)` throws RangeError');
assert.throws(RangeError, function() {
  Atomics.wake(i32a, -7.999, poisoned);
}, '`Atomics.wake(i32a, -7.999, poisoned)` throws RangeError');
assert.throws(RangeError, function() {
  Atomics.wake(i32a, -1, poisoned);
}, '`Atomics.wake(i32a, -1, poisoned)` throws RangeError');
assert.throws(RangeError, function() {
  Atomics.wake(i32a, -300, poisoned);
}, '`Atomics.wake(i32a, -300, poisoned)` throws RangeError');

reportCompare(0, 0);
