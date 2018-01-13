// |reftest| skip-if(!this.hasOwnProperty('SharedArrayBuffer')||!this.hasOwnProperty('Atomics')) -- SharedArrayBuffer,Atomics is not enabled unconditionally
// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.exchange
description: >
  Test Atomics.exchange on view values other than TypedArrays
includes: [testAtomics.js]
features: [SharedArrayBuffer, ArrayBuffer, DataView, Atomics, arrow-function, let, for-of]
---*/

testWithAtomicsNonViewValues(function(view) {
    assert.throws(TypeError, (() => Atomics.exchange(view, 0, 0)));
});

reportCompare(0, 0);
