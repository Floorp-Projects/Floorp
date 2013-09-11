// |jit-test| debug
// Don't allow cycles in the graph of the compartment "debugs" relation.

load(libdir + "asserts.js");

// trivial cycles
var dbg = new Debugger;
assertThrowsInstanceOf(function () { dbg.addDebuggee(this); }, TypeError);
assertThrowsInstanceOf(function () { new Debugger(this); }, TypeError);

// cycles of length 2
var d1 = newGlobal();
d1.top = this;
d1.eval("var dbg = new Debugger(top)");
assertThrowsInstanceOf(function () { dbg.addDebuggee(d1); }, TypeError);
assertThrowsInstanceOf(function () { new Debugger(d1); }, TypeError);

// cycles of length 3
var d2 = newGlobal();
d2.top = this;
d2.eval("var dbg = new Debugger(top.d1)");
assertThrowsInstanceOf(function () { dbg.addDebuggee(d2); }, TypeError);
assertThrowsInstanceOf(function () { new Debugger(d2); }, TypeError);
