// |reftest| shell-option(--enable-uint8array-base64) skip-if(!Uint8Array.fromBase64||!xulRuntime.shell) -- uint8array-base64 is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-uint8array.prototype.tohex
description: Uint8Array.prototype.toHex throws if called on a detached buffer
includes: [detachArrayBuffer.js]
features: [uint8array-base64, TypedArray]
---*/

var array = new Uint8Array(2);
$DETACHBUFFER(array.buffer);
assert.throws(TypeError, function() {
  array.toHex();
});


reportCompare(0, 0);
