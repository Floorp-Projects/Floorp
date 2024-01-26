// |reftest| shell-option(--enable-arraybuffer-resizable) skip-if(!this.hasOwnProperty('SharedArrayBuffer')||!ArrayBuffer.prototype.resize||!xulRuntime.shell) -- SharedArrayBuffer,resizable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-sharedarraybuffer.prototype.maxbytelength
description: Requires this value to have a [[ArrayBufferData]] internal slot
info: |
  get SharedArrayBuffer.prototype.maxByteLength

  1. Let O be the this value.
  2. Perform ? RequireInternalSlot(O, [[ArrayBufferData]]).
  [...]
features: [SharedArrayBuffer, resizable-arraybuffer]
---*/

assert.throws(TypeError, function() {
  SharedArrayBuffer.prototype.maxByteLength;
});

reportCompare(0, 0);
