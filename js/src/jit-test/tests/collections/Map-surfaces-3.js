// Map methods work when arguments are omitted.

var m = new Map;
assertEq(m.has(), false);
assertEq(m.get(), undefined);
assertEq(m.delete(), false);
assertEq(m.has(), false);
assertEq(m.get(), undefined);
assertEq(m.set(), undefined);
assertEq(m.has(), true);
assertEq(m.get(), undefined);
assertEq(m.delete(), true);
assertEq(m.has(), false);
assertEq(m.get(), undefined);
