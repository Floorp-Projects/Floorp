// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.indexof
description: >
    Tuple.prototype.indexOf - value of 'fromIndex' is an Object, which
    has an own valueOf method
---*/

var fromIndex = {
  valueOf: function() {
    return 1;
  }
};


assertEq(#[0, true].indexOf(true, fromIndex), 1, '#[0, true].indexOf(true, fromIndex)');

reportCompare(0, 0);
