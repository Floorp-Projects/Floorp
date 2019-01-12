// |jit-test| --more-compartments

// With --more-compartments we should default to creating a new compartment for
// new globals.

var g = newGlobal();
assertEq(objectGlobal(g), null); // CCW
assertEq(isProxy(g), true);
