// defineProperty can make a non-configurable writable property non-writable

load(libdir + "asserts.js");
var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
gw.defineProperty("p", {writable: true, value: 1});
gw.defineProperty("p", {writable: false});
g.p = 4;
assertEq(g.p, 1);
