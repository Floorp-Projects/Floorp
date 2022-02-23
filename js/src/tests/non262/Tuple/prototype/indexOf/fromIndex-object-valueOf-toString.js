// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Tuple.prototype.indexOf - value of 'fromIndex' is an object that
    has an own valueOf method that returns an object and toString
    method that returns a string
---*/

var toStringAccessed = false;
var valueOfAccessed = false;

var fromIndex = {
  toString: function() {
    toStringAccessed = true;
    return '1';
  },

  valueOf: function() {
    valueOfAccessed = true;
    return {};
  }
};

assertEq(#[0, true].indexOf(true, fromIndex), 1, '#[0, true].indexOf(true, fromIndex)');
assertEq(toStringAccessed, true);
assertEq(valueOfAccessed, true);

reportCompare(0, 0);
