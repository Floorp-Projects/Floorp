// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Empty arrays of are equivalent.
includes: [compareArray.js]
---*/

if (compareArray([], []) !== true) {
  throw new Error('Empty arrays are equivalent.');
}

reportCompare(0, 0);
