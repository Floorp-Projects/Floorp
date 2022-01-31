// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Tuple.prototype.indexOf when fromIndex is floating point number
---*/

var a = #[1,2,3];

assertEq(a.indexOf(3, 0.49), 2);
assertEq(a.indexOf(1, 0.51), 0);
assertEq(a.indexOf(1, 1.51), -1);

reportCompare(0, 0);
