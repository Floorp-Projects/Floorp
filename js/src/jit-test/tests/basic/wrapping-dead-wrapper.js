// Make sure the wrapper machinery returns a dead wrapper instead of a CCW when
// attempting to wrap a dead wrapper.
var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var wrapper = g1.Math;
nukeCCW(wrapper);
assertEq(isSameCompartment(wrapper, this), true);
g2.wrapper = wrapper;
g2.eval("assertEq(isSameCompartment(wrapper, this), true)");
