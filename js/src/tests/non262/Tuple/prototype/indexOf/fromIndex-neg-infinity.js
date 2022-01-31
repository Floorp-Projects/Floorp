// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Tuple.prototype.indexOf - value of 'fromIndex' is a number (value
    is -Infinity)
---*/

assertEq(#[true].indexOf(true, -Infinity), 0, '#[true].indexOf(true, -Infinity)');

reportCompare(0, 0);
