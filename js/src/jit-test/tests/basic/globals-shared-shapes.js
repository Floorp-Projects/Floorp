// Test that objects in different compartments can have the same shape if they
// have a null proto.

var g1 = newGlobal();
var g2 = newGlobal({sameZoneAs: g1});

g1.eval("x1 = {foo: 1}");
g2.eval("x2 = {foo: 2}");
assertEq(unwrappedObjectsHaveSameShape(g1.x1, g2.x2), false);

g1.eval("x1 = Object.create(null); x1.foo = 1;");
g2.eval("x2 = Object.create(null); x2.foo = 2;");
assertEq(unwrappedObjectsHaveSameShape(g1.x1, g2.x2), true);
