// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')||(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('arm64-simulator'))) -- Atomics,SharedArrayBuffer is not enabled unconditionally, ARM64 Simulator cannot emulate atomics
// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.notify
description: >
  Test Atomics.notify on non-shared integer TypedArrays
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const sab = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4);

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(TypeError, function() {
  Atomics.notify(new Int16Array(sab), poisoned, poisoned);
}, '`Atomics.notify(new Int16Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.notify(new Int8Array(sab), poisoned, poisoned);
}, '`Atomics.notify(new Int8Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.notify(new Uint32Array(sab),  poisoned, poisoned);
}, '`Atomics.notify(new Uint32Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.notify(new Uint16Array(sab), poisoned, poisoned);
}, '`Atomics.notify(new Uint16Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.notify(new Uint8Array(sab), poisoned, poisoned);
}, '`Atomics.notify(new Uint8Array(sab), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.notify(new Uint8ClampedArray(sab), poisoned, poisoned);
}, '`Atomics.notify(new Uint8ClampedArray(sab), poisoned, poisoned)` throws TypeError');

reportCompare(0, 0);
