// The Set constructor creates an empty Set by default.

assertEq(Set().size, 0);
assertEq((new Set).size, 0);
assertEq(Set(undefined).size, 0);
assertEq(new Set(undefined).size, 0);
