// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Tuple.prototype.indexOf when fromIndex is boolean
---*/

let a = #[1, 2, 3];

assertEq(a.indexOf(1, true), -1);
assertEq(a.indexOf(1, false), 0);

reportCompare(0, 0);
