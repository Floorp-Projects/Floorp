// The Map constructor creates an empty Map by default.

assertEq(Map().size(), 0);
assertEq((new Map).size(), 0);
assertEq(Map(undefined).size(), 0);
assertEq(new Map(undefined).size(), 0);
