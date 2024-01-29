// |reftest| shell-option(--enable-arraybuffer-resizable) skip-if(!this.hasOwnProperty('SharedArrayBuffer')||!ArrayBuffer.prototype.resize||!xulRuntime.shell) -- SharedArrayBuffer,resizable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-sharedarraybuffer.prototype.grow
description: Throws a TypeError if `this` value is an ArrayBuffer
info: |
  SharedArrayBuffer.prototype.grow ( newLength )

  1. Let O be the this value.
  2. Perform ? RequireInternalSlot(O, [[ArrayBufferMaxByteLength]]).
  3. If IsSharedArrayBuffer(O) is false, throw a TypeError exception.
  [...]
features: [ArrayBuffer, SharedArrayBuffer, resizable-arraybuffer]
---*/

var ab = new ArrayBuffer(0);

assert.throws(TypeError, function() {
  SharedArrayBuffer.prototype.grow.call(ab);
}, '`this` value cannot be an ArrayBuffer');

reportCompare(0, 0);
