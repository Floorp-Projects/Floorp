// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Tuple.prototype.indexOf - 'length' is own data property that
    overrides an inherited data property on an Tuple
---*/

var target = #[];

Tuple.prototype.length = 0;

assertEq(#[0, target].indexOf(target), 1, '#[0, target].indexOf(target)');

reportCompare(0, 0);
