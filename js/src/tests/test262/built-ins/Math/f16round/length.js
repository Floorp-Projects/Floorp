// |reftest| shell-option(--enable-float16array) skip-if(!this.hasOwnProperty('Float16Array')||!xulRuntime.shell) -- Float16Array is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-math.f16round
description: >
  Math.f16round.length is 1
features: [Float16Array]
includes: [propertyHelper.js]
---*/

verifyProperty(Math.f16round, 'length', {
  value: 1,
  enumerable: false,
  writable: false,
  configurable: true
});

reportCompare(0, 0);
