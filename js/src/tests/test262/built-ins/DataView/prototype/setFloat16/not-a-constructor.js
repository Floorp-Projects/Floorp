// |reftest| shell-option(--enable-float16array) skip-if(!this.hasOwnProperty('Float16Array')||!xulRuntime.shell) -- Float16Array is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-ecmascript-standard-built-in-objects
description: >
  DataView.prototype.setFloat16 does not implement [[Construct]], is not new-able
includes: [isConstructor.js]
features: [Float16Array, Reflect.construct, DataView, arrow-function, ArrayBuffer]
---*/

assert.sameValue(
  isConstructor(DataView.prototype.setFloat16),
  false,
  'isConstructor(DataView.prototype.setFloat16) must return false'
);

assert.throws(TypeError, () => {
  let dv = new DataView(new ArrayBuffer(16)); new dv.setFloat16(0, 0);
});


reportCompare(0, 0);
