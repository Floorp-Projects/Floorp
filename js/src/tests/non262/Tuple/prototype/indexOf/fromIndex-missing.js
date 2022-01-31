// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.indexof
description: Tuple.prototype.indexOf - 'fromIndex' isn't passed
---*/

var arr = #[0, 1, 2, 3, 4];
//'fromIndex' will be set as 0 if not passed by default

assertEq(arr.indexOf(0), arr.indexOf(0, 0), 'arr.indexOf(0)');
assertEq(arr.indexOf(2), arr.indexOf(2, 0), 'arr.indexOf(2)');
assertEq(arr.indexOf(4), arr.indexOf(4, 0), 'arr.indexOf(4)');

reportCompare(0, 0);
