// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Return false if fromIndex >= TupleLength
---*/

var sample = #[7, 7, 7, 7];
assertEq(sample.includes(7, 4), false, "length");
assertEq(sample.includes(7, 5), false, "length + 1");

reportCompare(0, 0);
