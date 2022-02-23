// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
  22.1.3.11 Tuple.prototype.includes ( searchElement [ , fromIndex ] )
---*/

var sample = #["a", "b", "c"];
assertEq(sample.includes("a", 0), true, "includes('a', 0)");
assertEq(sample.includes("a", 1), false, "includes('a', 1)");
assertEq(sample.includes("a", 2), false, "includes('a', 2)");

assertEq(sample.includes("b", 0), true, "includes('b', 0)");
assertEq(sample.includes("b", 1), true, "includes('b', 1)");
assertEq(sample.includes("b", 2), false, "includes('b', 2)");

assertEq(sample.includes("c", 0), true, "includes('c', 0)");
assertEq(sample.includes("c", 1), true, "includes('c', 1)");
assertEq(sample.includes("c", 2), true, "includes('c', 2)");

assertEq(sample.includes("a", -1), false, "includes('a', -1)");
assertEq(sample.includes("a", -2), false, "includes('a', -2)");
assertEq(sample.includes("a", -3), true, "includes('a', -3)");
assertEq(sample.includes("a", -4), true, "includes('a', -4)");

assertEq(sample.includes("b", -1), false, "includes('b', -1)");
assertEq(sample.includes("b", -2), true, "includes('b', -2)");
assertEq(sample.includes("b", -3), true, "includes('b', -3)");
assertEq(sample.includes("b", -4), true, "includes('b', -4)");

assertEq(sample.includes("c", -1), true, "includes('c', -1)");
assertEq(sample.includes("c", -2), true, "includes('c', -2)");
assertEq(sample.includes("c", -3), true, "includes('c', -3)");
assertEq(sample.includes("c", -4), true, "includes('c', -4)");

reportCompare(0, 0);
