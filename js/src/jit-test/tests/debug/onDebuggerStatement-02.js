// Activity in the debugger compartment should not trigger debug hooks.

var g = newGlobal('new-compartment');
var hit = false;

var dbg = Debugger(g);
dbg.onDebuggerStatement = function (stack) { hit = true; };

debugger;
assertEq(hit, false, "raw debugger statement in debugger compartment should not hit");

g.f = function () { debugger; };
g.eval("f();");
assertEq(hit, false, "debugger statement in debugger compartment function should not hit");

g.outerEval = eval;
g.eval("outerEval('debugger;');");
assertEq(hit, false, "debugger statement in debugger compartment eval code should not hit");

var g2 = newGlobal('new-compartment');
g2.eval("debugger;");
assertEq(hit, false, "debugger statement in unrelated non-debuggee compartment should not hit");
