// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: get the integer value from fromIndex
*/

var obj = {
  valueOf: function() {
    return 1;
  }
};

var sample = #[42, 43];
assertEq(sample.includes(42, "1"), false, "string [0]");
assertEq(sample.includes(43, "1"), true, "string [1]");

assertEq(sample.includes(42, true), false, "true [0]");
assertEq(sample.includes(43, true), true, "true [1]");

assertEq(sample.includes(42, false), true, "false [0]");
assertEq(sample.includes(43, false), true, "false [1]");

assertEq(sample.includes(42, NaN), true, "NaN [0]");
assertEq(sample.includes(43, NaN), true, "NaN [1]");

assertEq(sample.includes(42, null), true, "null [0]");
assertEq(sample.includes(43, null), true, "null [1]");

assertEq(sample.includes(42, undefined), true, "undefined [0]");
assertEq(sample.includes(43, undefined), true, "undefined [1]");

assertEq(sample.includes(42, null), true, "null [0]");
assertEq(sample.includes(43, null), true, "null [1]");

assertEq(sample.includes(42, obj), false, "object [0]");
assertEq(sample.includes(43, obj), true, "object [1]");

reportCompare(0, 0);
