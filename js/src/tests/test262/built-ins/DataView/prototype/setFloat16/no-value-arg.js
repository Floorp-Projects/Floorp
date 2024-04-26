// |reftest| shell-option(--enable-float16array) skip-if(!this.hasOwnProperty('Float16Array')||!xulRuntime.shell) -- Float16Array is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.setfloat16
description: >
  Set value as undefined (cast to NaN) when value argument is not present
features: [Float16Array]
---*/

var buffer = new ArrayBuffer(8);
var sample = new DataView(buffer, 0);

var result = sample.setFloat16(0);

assert.sameValue(sample.getFloat16(0), NaN);
assert.sameValue(result, undefined);

reportCompare(0, 0);
