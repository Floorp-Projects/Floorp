// Basic preventExtensions test.

var g = newGlobal('new-compartment');
var obj = g.eval("({x: 1})");
assertEq(g.Object.isExtensible(obj), true);

var dbg = new Debugger;
var objw = dbg.addDebuggee(obj);
assertEq(objw.isExtensible(), true);

assertEq(objw.preventExtensions(), undefined);
assertEq(g.Object.isExtensible(obj), false);
assertEq(objw.isExtensible(), false);

// Calling preventExtensions again has no effect.
assertEq(objw.preventExtensions(), undefined);
