// |reftest| shell-option(--enable-float16array) skip-if(!this.hasOwnProperty('Float16Array')||!xulRuntime.shell) -- Float16Array is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.setfloat16
description: >
  Set values and return undefined
features: [Float16Array]
includes: [byteConversionValues.js]
---*/

var buffer = new ArrayBuffer(2);
var sample = new DataView(buffer, 0);
var values = byteConversionValues.values;
var expectedValues = byteConversionValues.expected.Float16;

values.forEach(function(value, i) {
  var result;
  var expected = expectedValues[i];

  result = sample.setFloat16(0, value, false);

  assert.sameValue(
    sample.getFloat16(0),
    expected,
    "value: " + value
  );
  assert.sameValue(
    result,
    undefined,
    "return is undefined, value: " + value
  );
});

reportCompare(0, 0);
