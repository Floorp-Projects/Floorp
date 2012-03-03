// Each Map has its own size.

var m1 = Map(), m2 = Map();
m1.set("x", 3);
assertEq(m1.size(), 1);
assertEq(m2.size(), 0);
