// Test that objects in different compartments can have the same shape.

var g1 = newGlobal();
var g2 = newGlobal({sameZoneAs: g1});

g1.eval("x1 = {foo: 1}");
g2.eval("x2 = {foo: 2}");
assertEq(unwrappedObjectsHaveSameShape(g1.x1, g2.x2), true);

g1.eval("x1 = [1]");
g2.eval("x2 = [2]");
assertEq(unwrappedObjectsHaveSameShape(g1.x1, g2.x2), true);

g1.eval("x1 = function f(){}");
g2.eval("x2 = function f(){}");
assertEq(unwrappedObjectsHaveSameShape(g1.x1, g2.x2), true);

g1.eval("x1 = /abc/;");
g2.eval("x2 = /def/");
assertEq(unwrappedObjectsHaveSameShape(g1.x1, g2.x2), true);

// Now the same, but we change Array.prototype.__proto__.
// The arrays should no longer get the same Shape, as their
// proto chain is different.
g1 = newGlobal();
g2 = newGlobal({sameZoneAs: g1});
g1.eval("x1 = [1];");
g2.eval("Array.prototype.__proto__ = Math;");
g2.eval("x2 = [2];");
assertEq(unwrappedObjectsHaveSameShape(g1.x1, g2.x2), false);
