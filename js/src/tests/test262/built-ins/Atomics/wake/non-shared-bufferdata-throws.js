// |reftest| skip-if(!this.hasOwnProperty('Atomics')) -- Atomics is not enabled unconditionally
// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.wake
description: >
  Throws a TypeError if typedArray.buffer is not a SharedArrayBuffer
info: |
  Atomics.wake( typedArray, index, count )

  1.Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).
    ...
      9.If IsSharedArrayBuffer(buffer) is false, throw a TypeError exception.
        ...
          4.If bufferData is a Data Block, return false.
features: [ArrayBuffer, Atomics, TypedArray]
---*/

const i32a = new Int32Array(
  new ArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(TypeError, function() {
  Atomics.wake(i32a, 0, 0);
}, '`Atomics.wake(i32a, 0, 0)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(i32a, poisoned, poisoned);
}, '`Atomics.wake(i32a, poisoned, poisoned)` throws TypeError');

reportCompare(0, 0);
