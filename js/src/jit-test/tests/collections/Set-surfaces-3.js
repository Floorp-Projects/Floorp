// Set methods work when arguments are omitted.

var s = new Set;
assertEq(s.has(), false);
assertEq(s.delete(), false);
assertEq(s.has(), false);
assertEq(s.add(), s);
assertEq(s.has(), true);
assertEq(s.delete(), true);
assertEq(s.has(), false);
