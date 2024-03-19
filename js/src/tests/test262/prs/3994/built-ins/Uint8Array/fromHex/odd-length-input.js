// |reftest| shell-option(--enable-uint8array-base64) skip-if(!Uint8Array.fromBase64||!xulRuntime.shell) -- uint8array-base64 is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-uint8array.fromhex
description: Uint8Array.fromHex throws if given an odd number of input hex characters
features: [uint8array-base64, TypedArray]
---*/

assert.throws(SyntaxError, function() {
  Uint8Array.fromHex('a');
});

reportCompare(0, 0);
