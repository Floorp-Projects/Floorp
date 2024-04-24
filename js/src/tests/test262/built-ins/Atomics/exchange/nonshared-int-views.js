// |reftest| shell-option(--enable-float16array) skip-if(!this.hasOwnProperty('Atomics')) -- Atomics is not enabled unconditionally
// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.exchange
description: >
  Atomics.exchange throws when operating on non-sharable integer TypedArrays
includes: [testTypedArray.js]
features: [ArrayBuffer, Atomics, TypedArray]
---*/

testWithNonAtomicsFriendlyTypedArrayConstructors(TA => {
  const buffer = new ArrayBuffer(TA.BYTES_PER_ELEMENT * 4);
  const view = new TA(buffer);

  assert.throws(TypeError, function() {
    Atomics.exchange(view, 0, 0);
  }, `Atomics.exchange(new ${TA.name}(buffer), 0, 0) throws TypeError`);
});

reportCompare(0, 0);
