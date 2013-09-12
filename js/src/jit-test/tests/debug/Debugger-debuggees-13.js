// Removing a debuggee does not detach the debugger from a compartment if another debuggee is in it.
var g1 = newGlobal();
var g2 = g1.eval("newGlobal('same-compartment')");
var dbg = new Debugger(g1, g2);
var hits = 0;
dbg.onDebuggerStatement = function (frame) { hits++; };
dbg.removeDebuggee(g1);
g2.eval("debugger;");
assertEq(hits, 1);
