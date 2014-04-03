// Arrow functions have a .length property like ordinary functions.

assertEq((a => a).hasOwnProperty("length"), true);

assertEq((a => a).length, 1);
assertEq((() => 0).length, 0);
assertEq(((a) => 0).length, 1);
assertEq(((a, b) => 0).length, 2);

assertEq(((...arr) => arr).length, 0);
assertEq(((a=1, b=2) => 0).length, 0);
