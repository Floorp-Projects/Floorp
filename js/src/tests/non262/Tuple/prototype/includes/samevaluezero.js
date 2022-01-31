// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: search element is compared using SameValueZero
---*/

var sample = #[42, 0, 1, NaN];
assertEq(sample.includes("42"), false);
assertEq(sample.includes([42]), false);
assertEq(sample.includes(#[42]), false);
assertEq(sample.includes(42.0), true);
assertEq(sample.includes(-0), true);
assertEq(sample.includes(true), false);
assertEq(sample.includes(false), false);
assertEq(sample.includes(null), false);
assertEq(sample.includes(""), false);
assertEq(sample.includes(NaN), true);

reportCompare(0, 0);
