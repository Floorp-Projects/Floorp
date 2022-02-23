// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.indexof
description: Tuple.prototype.indexOf returns 0 if fromIndex is null
---*/

var a = #[1, 2, 3];

// null resolves to 0
assertEq(a.indexOf(1, null), 0, 'a.indexOf(1,null)');

reportCompare(0, 0);
