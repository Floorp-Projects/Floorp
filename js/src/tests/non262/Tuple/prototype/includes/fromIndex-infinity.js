// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.includes
description: handle Infinity values for fromIndex
---*/

var sample = #[42, 43, 43, 41];

assertEq(
  sample.includes(43, Infinity),
  false,
  "includes(43, Infinity)"
);
assertEq(
  sample.includes(43, -Infinity),
  true,
  "includes(43, -Infinity)"
);

reportCompare(0, 0);
