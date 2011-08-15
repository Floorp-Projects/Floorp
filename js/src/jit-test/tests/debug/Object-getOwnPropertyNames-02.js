// obj.getOwnPropertyNames() works when obj's referent is itself a cross-compartment wrapper.

var g = newGlobal('new-compartment');
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);
g.p = {xyzzy: 8};  // makes a cross-compartment wrapper
assertEq(gobj.getOwnPropertyDescriptor("p").value.getOwnPropertyDescriptor("xyzzy").value, 8);
