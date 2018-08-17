// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('BigInt')) -- Atomics,BigInt is not enabled unconditionally
// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Throws a TypeError if typedArray arg is not an BigInt64Array
info: |
  Atomics.wake( typedArray, index, count )

  1.Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).
    ...
      5.If onlyInt32 is true, then
        If typeName is not "BigInt64Array", throw a TypeError exception.
features: [Atomics, BigInt, TypedArray]
---*/

const i64a = new BigUint64Array(
  new SharedArrayBuffer(BigUint64Array.BYTES_PER_ELEMENT * 8)
);

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(TypeError, function() {
  Atomics.wait(i64a, 0, 0);
}, '`Atomics.wait(i64a, 0, 0)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wait(i64a, poisoned, poisoned);
}, '`Atomics.wait(i64a, poisoned, poisoned)` throws TypeError');

reportCompare(0, 0);
