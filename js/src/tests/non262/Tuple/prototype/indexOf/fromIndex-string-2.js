// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Tuple.prototype.indexOf when fromIndex is string
---*/

var a = #[1, 2, 1, 2, 1, 2];

assertEq(a.indexOf(2, "2"), 3, '"2" resolves to 2');
assertEq(a.indexOf(2, "one"), 1, '"one" resolves to 0');

reportCompare(0, 0);
