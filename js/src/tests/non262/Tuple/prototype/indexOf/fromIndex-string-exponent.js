// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Tuple.prototype.indexOf - value of 'fromIndex' is a string
    containing an exponential number
---*/

var target = #[];

assertEq(#[0, 1, target, 3, 4].indexOf(target, "3E0"), -1, '#[0, 1, target, 3, 4].indexOf(target, "3E0")');
assertEq(#[0, 1, 2, target, 4].indexOf(target, "3E0"), 3, '#[0, 1, 2, target, 4].indexOf(target, "3E0")');

reportCompare(0, 0);
