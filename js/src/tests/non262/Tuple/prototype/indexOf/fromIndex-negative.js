// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Tuple.prototype.indexOf - value of 'fromIndex' is a number (value
    is negative number)
---*/

var target = #[];

assertEq(#[0, target, 2].indexOf(target, -1), -1, '[0, target, 2].indexOf(target, -1)');
assertEq(#[0, 1, target].indexOf(target, -1), 2, '[0, 1, target].indexOf(target, -1)');

reportCompare(0, 0);
