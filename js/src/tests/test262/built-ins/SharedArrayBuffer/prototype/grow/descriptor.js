// |reftest| shell-option(--enable-arraybuffer-resizable) skip-if(!this.hasOwnProperty('SharedArrayBuffer')||!ArrayBuffer.prototype.resize||!xulRuntime.shell) -- SharedArrayBuffer,resizable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-sharedarraybuffer.prototype.grow
description: >
  SharedArrayBuffer.prototype.grow has default data property attributes.
info: |
  SharedArrayBuffer.prototype.grow ( newLength )

  17 ECMAScript Standard Built-in Objects:
    Every other data property described in clauses 18 through 26 and in
    Annex B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [SharedArrayBuffer, resizable-arraybuffer]
---*/

verifyProperty(SharedArrayBuffer.prototype, 'grow', {
  enumerable: false,
  writable: true,
  configurable: true
});

reportCompare(0, 0);
