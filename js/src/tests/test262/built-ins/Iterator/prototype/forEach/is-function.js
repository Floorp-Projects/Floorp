// |reftest| skip -- iterator-helpers is not supported
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.forEach
description: >
  Iterator.prototype.forEach is a built-in function
features: [iterator-helpers]
---*/

assert.sameValue(typeof Iterator.prototype.forEach, 'function');

reportCompare(0, 0);
