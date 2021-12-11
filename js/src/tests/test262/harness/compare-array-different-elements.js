// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Arrays containing different elements are not equivalent.
includes: [compareArray.js]
---*/

var first = [0, 'a', undefined];
var second = [0, 'b', undefined];

if (compareArray(first, second) !== false) {
  throw new Error('Arrays containing different elements are not equivalent.');
}

reportCompare(0, 0);
