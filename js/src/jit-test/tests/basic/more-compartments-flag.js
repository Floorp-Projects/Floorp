// |jit-test| --more-compartments

// With --more-compartments we should default to creating a new compartment for
// new globals.

var g = newGlobal();
assertEq(isSameCompartment(this, g), false);
assertEq(isProxy(g), true);
