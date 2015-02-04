// The Set constructor creates an empty Set by default.

var s = new Set();
assertEq(s.size, 0);
s = new Set(undefined);
assertEq(s.size, 0);
s = new Set(null);
assertEq(s.size, 0);
