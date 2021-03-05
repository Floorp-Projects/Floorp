// Test that removing a non-dictionary property does not roll back the object's
// shape to the previous one if the protos are different.

var o = {x: 1};
o.y = 2;
Object.setPrototypeOf(o, null);
delete o.y;
assertEq(Object.getPrototypeOf(o), null);
