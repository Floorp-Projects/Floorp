load(libdir + "asserts.js");
var g = newGlobal('new-compartment');
var a = g.RegExp("x");
assertThrowsInstanceOf(function () { Object.defineProperty(a, "ignoreCase", {value: undefined}); },
                       g.TypeError);
a.toString();
