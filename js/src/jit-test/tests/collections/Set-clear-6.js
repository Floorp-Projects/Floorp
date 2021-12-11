// Clearing a Set doesn't affect expando properties.

var s = new Set();
s.x = 3;
s.clear();
assertEq(s.x, 3);
