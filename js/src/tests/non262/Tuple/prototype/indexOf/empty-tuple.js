// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Tuple.prototype.indexOf returns -1 if 'length' is 0 (empty tuple)
---*/

var i = #[].indexOf(42);

assertEq(i, -1);

reportCompare(0, 0);
