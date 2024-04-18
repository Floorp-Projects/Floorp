// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')||(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('arm64-simulator'))) -- Atomics,SharedArrayBuffer is not enabled unconditionally, ARM64 Simulator cannot emulate atomics
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-ecmascript-standard-built-in-objects
description: >
  Atomics.and does not implement [[Construct]], is not new-able
info: |
  ECMAScript Function Objects

  Built-in function objects that are not identified as constructors do not
  implement the [[Construct]] internal method unless otherwise specified in
  the description of a particular function.

  sec-evaluatenew

  ...
  7. If IsConstructor(constructor) is false, throw a TypeError exception.
  ...
includes: [isConstructor.js]
features: [Reflect.construct, Atomics, arrow-function, TypedArray, SharedArrayBuffer]
---*/

assert.sameValue(isConstructor(Atomics.and), false, 'isConstructor(Atomics.and) must return false');

assert.throws(TypeError, () => {
  new Atomics.and(new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT)));
}, '`new Atomics.and(new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT)))` throws TypeError');


reportCompare(0, 0);
