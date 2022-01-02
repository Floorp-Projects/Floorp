var x = -false;
var y = -0;
assertEq(-x === x, true);
assertEq(-x === y, true);
assertEq(-y !== y, false);

assertEq(-x == x, true);
assertEq(-x == y, true);
assertEq(-y != y, false);
