// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.indexof
description: >
    Tuple.prototype.indexOf uses inherited valueOf method when value
    of 'fromIndex' is an object with an own toString and inherited
    valueOf methods
---*/

var toStringAccessed = false;
var valueOfAccessed = false;

var proto = {
  valueOf: function() {
    valueOfAccessed = true;
    return 1;
  }
};

var Con = function() {};
Con.prototype = proto;

var child = new Con();
child.toString = function() {
  toStringAccessed = true;
  return 2;
};

assertEq(#[0, true].indexOf(true, child), 1, '[0, true].indexOf(true, child)');
assertEq(valueOfAccessed, true);
assertEq(toStringAccessed, false, 'toStringAccessed');

reportCompare(0, 0);
