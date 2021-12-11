var x = "abc";
assertEq(x.indexOf(x), 0);
assertEq(x.indexOf(x, -1), 0);
assertEq(x.indexOf(x, 1), -1);
assertEq(x.indexOf(x, 100), -1);

assertEq(x.lastIndexOf(x), 0);
assertEq(x.lastIndexOf(x, -1), 0);
assertEq(x.lastIndexOf(x, 1), 0);
assertEq(x.lastIndexOf(x, 100), 0);
