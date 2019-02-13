var g1 = newGlobal({sameCompartmentAs: this});
var g2 = newGlobal({newCompartment: true});
assertEq(firstGlobalInCompartment(this), this);
assertEq(firstGlobalInCompartment(g1), this);
assertEq(firstGlobalInCompartment(g2), g2);
