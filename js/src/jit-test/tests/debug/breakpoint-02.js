// Setting a breakpoint in a non-debuggee Script is an error.

load(libdir + "asserts.js");

var g1 = newGlobal('new-compartment');
var g2 = g1.eval("newGlobal('same-compartment')");
g2.eval("function f() { return 2; }");
g1.f = g2.f;

var dbg = Debugger(g1);
var s;
dbg.onDebuggerStatement = function (frame) { s = frame.eval("f").return.script; };
g1.eval("debugger;");

assertThrowsInstanceOf(function () { s.setBreakpoint(0, {}); }, Error);
