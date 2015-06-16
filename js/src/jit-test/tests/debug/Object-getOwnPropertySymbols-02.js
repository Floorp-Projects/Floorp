// obj.getOwnPropertySymbols() works when obj's referent is itself a cross-compartment wrapper.

var g = newGlobal();
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);
g.p = {xyzzy: 8};  // makes a cross-compartment wrapper
var sym = Symbol("plugh");
g.p[sym] = 9;
var wp = gobj.getOwnPropertyDescriptor("p").value;
var names = wp.getOwnPropertySymbols();
assertEq(names.length, 1);
assertEq(names[0], sym);
