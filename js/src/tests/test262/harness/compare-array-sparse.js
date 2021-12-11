// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Spares arrays are only equivalent if they have the same length.
includes: [compareArray.js]
---*/

if (compareArray([,], [,]) !== true) {
  throw new Error('Sparse arrays of the same length are equivalent.');
}

if (compareArray([,], [,,]) !== false) {
  throw new Error('Sparse arrays of differing lengths are not equivalent.');
}

if (compareArray([,,], [,]) !== false) {
  throw new Error('Sparse arrays of differing lengths are not equivalent.');
}

if (compareArray([,], []) !== false) {
  throw new Error('Sparse arrays are not equivalent to empty arrays.');
}

if (compareArray([], [,]) !== false) {
  throw new Error('Sparse arrays are not equivalent to empty arrays.');
}

reportCompare(0, 0);
