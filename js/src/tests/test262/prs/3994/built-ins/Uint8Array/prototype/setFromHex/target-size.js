// |reftest| shell-option(--enable-uint8array-base64) skip-if(!Uint8Array.fromBase64||!xulRuntime.shell) -- uint8array-base64 is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-uint8array.prototype.setfromhex
description: Uint8Array.prototype.setFromHex behavior when target buffer is small
includes: [compareArray.js]
features: [uint8array-base64, TypedArray]
---*/

// buffer too small
var target = new Uint8Array([255, 255]);
var result = target.setFromHex('aabbcc');
assert.sameValue(result.read, 4);
assert.sameValue(result.written, 2);
assert.compareArray(target, [170, 187]);

// buffer exact
var target = new Uint8Array([255, 255, 255]);
var result = target.setFromHex('aabbcc');
assert.sameValue(result.read, 6);
assert.sameValue(result.written, 3);
assert.compareArray(target, [170, 187, 204]);

// buffer too large
var target = new Uint8Array([255, 255, 255, 255]);
var result = target.setFromHex('aabbcc');
assert.sameValue(result.read, 6);
assert.sameValue(result.written, 3);
assert.compareArray(target, [170, 187, 204, 255]);

reportCompare(0, 0);
