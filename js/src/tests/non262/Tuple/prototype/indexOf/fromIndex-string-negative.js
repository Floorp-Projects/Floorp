// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Tuple.prototype.indexOf - value of 'fromIndex' is a string
    containing a negative number
---*/

assertEq(#[0, true, 2].indexOf(true, "-1"), -1, '#[0, true, 2].indexOf(true, "-1")');
assertEq(#[0, 1, true].indexOf(true, "-1"), 2, '#[0, 1, true].indexOf(true, "-1")');

reportCompare(0, 0);
