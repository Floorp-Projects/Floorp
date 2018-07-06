// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2018 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Return abrupt when symbol passed for 'count' argument to Atomics.wake
info: |
  Atomics.wake( typedArray, index, count )

  ...
  3. If count is undefined, let c be +∞.
  4. Else,
    a. Let intCount be ? ToInteger(count).
  ...

features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

assert.throws(TypeError, function() {
  Atomics.wake(i32a, 0, Symbol());
}, '`Atomics.wake(i32a, 0, Symbol())` throws TypeError');

reportCompare(0, 0);
