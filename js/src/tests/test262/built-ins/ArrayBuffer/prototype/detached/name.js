// |reftest| shell-option(--enable-arraybuffer-transfer) skip-if(!ArrayBuffer.prototype.transfer||!xulRuntime.shell) -- arraybuffer-transfer is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-arraybuffer.prototype.detached
description: >
  get ArrayBuffer.prototype.detached

  17 ECMAScript Standard Built-in Objects

  Functions that are specified as get or set accessor functions of built-in
  properties have "get " or "set " prepended to the property name string.

includes: [propertyHelper.js]
features: [ArrayBuffer, arraybuffer-transfer]
---*/

var desc = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'detached');

verifyProperty(desc.get, 'name', {
  value: 'get detached',
  enumerable: false,
  writable: false,
  configurable: true
});

reportCompare(0, 0);
