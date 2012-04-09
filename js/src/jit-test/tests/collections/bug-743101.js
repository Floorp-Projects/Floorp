load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
assertThrowsInstanceOf(function () { Map.prototype.size.apply(g, []); }, g.TypeError);
assertThrowsInstanceOf(function () { Set.prototype.size.apply(g, []); }, g.TypeError);
