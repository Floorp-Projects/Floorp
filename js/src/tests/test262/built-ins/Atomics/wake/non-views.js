// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test Atomics.wake on view values other than TypedArrays
includes: [testAtomics.js]
features: [ArrayBuffer, Atomics, DataView, SharedArrayBuffer, Symbol, TypedArray]
---*/

testWithAtomicsNonViewValues(function(nonView) {
  assert.throws(TypeError, function() {
    Atomics.wake(nonView, 0, 0);
  }, '`Atomics.wake(nonView, 0, 0)` throws TypeError'); // Even with count == 0
});

reportCompare(0, 0);
