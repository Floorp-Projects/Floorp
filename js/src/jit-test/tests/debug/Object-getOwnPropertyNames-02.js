// obj.getOwnPropertyNames() works when obj's referent is itself a cross-compartment wrapper.

var g = newGlobal({newCompartment: true});
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);
g.p = {xyzzy: 8};  // makes a cross-compartment wrapper
g.p[Symbol.for("plugh")] = 9;
var wp = gobj.getOwnPropertyDescriptor("p").value;
var names = wp.getOwnPropertyNames();
assertEq(names.length, 1);
assertEq(names[0], "xyzzy");
