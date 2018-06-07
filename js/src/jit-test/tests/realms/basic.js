var g1 = newGlobal({sameCompartmentAs: this});
var g2 = newGlobal({sameCompartmentAs: g1});
g2.x = g1;
gc();
assertEq(objectGlobal(Math), this);
assertEq(objectGlobal(g1.print), g1);
assertEq(objectGlobal(g2.x), g1);

// Different-compartment realms have wrappers.
assertEq(objectGlobal(newGlobal().Math), null);
