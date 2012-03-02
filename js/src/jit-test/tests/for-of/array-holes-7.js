// for-of visits inherited properties in an array full of holes.

Object.prototype[1] = "O";
Array.prototype[2] = "A";
assertEq([x for (x of Array(4))].join(","), ",O,A,");
