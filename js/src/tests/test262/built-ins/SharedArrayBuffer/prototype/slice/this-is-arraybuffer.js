// |reftest| skip-if(!this.hasOwnProperty('SharedArrayBuffer')) -- SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-sharedarraybuffer.prototype.slice
description: >
  Throws a TypeError if `this` is an ArrayBuffer
features: [SharedArrayBuffer]
---*/

assert.throws(TypeError, function() {
  var ab = new ArrayBuffer(0);
  SharedArrayBuffer.prototype.slice.call(ab);
}, "`this` value cannot be an ArrayBuffer");

reportCompare(0, 0);
