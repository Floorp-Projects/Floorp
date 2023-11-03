// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')||(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('arm64-simulator'))) -- Atomics,SharedArrayBuffer is not enabled unconditionally, ARM64 Simulator cannot emulate atomics
// Copyright (C) 2020 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.store
description: >
  Atomics.store calls ToInteger, which normalizes -0 to +0
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

assert(
  Object.is(
    Atomics.store(i32a, 0, -0),
    +0
  ),
  'Atomics.store(i32a, 0, -0) normalizes -0 to +0'
);
assert.sameValue(
  i32a[0],
  +0,
  'The value of i32a[0] is normalized to +0'
);

reportCompare(0, 0);
