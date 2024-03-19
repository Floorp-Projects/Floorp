// |reftest| shell-option(--enable-uint8array-base64) skip-if(!Uint8Array.fromBase64||!xulRuntime.shell) -- uint8array-base64 is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-uint8array.prototype.setfromhex
description: >
  Uint8Array.prototype.setFromHex.length is 1.
includes: [propertyHelper.js]
features: [uint8array-base64, TypedArray]
---*/

verifyProperty(Uint8Array.prototype.setFromHex, 'length', {
  value: 1,
  enumerable: false,
  writable: false,
  configurable: true
});

reportCompare(0, 0);
