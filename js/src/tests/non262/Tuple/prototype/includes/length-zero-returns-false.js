// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Returns false if length is 0
  ...
---*/

var calls = 0;
var fromIndex = {
  valueOf: function() {
    calls++;
  }
};

var sample = #[];
assertEq(sample.includes(0), false, "returns false");
assertEq(sample.includes(), false, "returns false - no arg");
assertEq(sample.includes(0, fromIndex), false, "using fromIndex");
assertEq(calls, 0, "length is checked before ToInteger(fromIndex)");

reportCompare(0, 0);
