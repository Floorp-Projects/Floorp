// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test Atomics.wake on non-shared integer TypedArrays
includes: [testTypedArray.js]
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const sab = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4);

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(TypeError, function() {
  Atomics.wake(new Int16Array(sab), poisoned, poisoned);
}, '`Atomics.wake(new Int16Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(new Int8Array(sab), poisoned, poisoned);
}, '`Atomics.wake(new Int8Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(new Uint32Array(sab),  poisoned, poisoned);
}, '`Atomics.wake(new Uint32Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(new Uint16Array(sab), poisoned, poisoned);
}, '`Atomics.wake(new Uint16Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(new Uint8Array(sab), poisoned, poisoned);
}, '`Atomics.wake(new Uint8Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(new Uint8ClampedArray(sab), poisoned, poisoned);
}, '`Atomics.wake(new Uint8ClampedArray(sab), poisoned, poisoned)` throws TypeError');

reportCompare(0, 0);
