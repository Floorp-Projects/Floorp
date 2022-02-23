// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Tuple.prototype.indexOf returns -1 if fromIndex is greater than
    Tuple length
---*/

let a = #[1, 2, 3];

assertEq(a.indexOf(1, 5), -1);
assertEq(a.indexOf(1, 3), -1);
assertEq(#[].indexOf(1, 0), -1);

reportCompare(0, 0);
