// Debugger hooks fire based on debuggees.

var g1 = newGlobal();
g1.eval("var g2 = newGlobal('same-compartment')");
var g2 = g1.g2;
g1.eval("function f() { debugger; g2.g(); }");
g2.eval("function g() { debugger; }");

var log;
var dbg = new Debugger;
dbg.onDebuggerStatement = function (frame) { log += frame.callee.name; };

// No debuggees: onDebuggerStatement is not called.
log = '';
g1.f();
assertEq(log, '');

// Add a debuggee and check that the handler is called.
var g1w = dbg.addDebuggee(g1);
log = '';
g1.f();
assertEq(log, 'f');

// Two debuggees, two onDebuggerStatement calls.
dbg.addDebuggee(g2);
log = '';
g1.f();
assertEq(log, 'fg');

// After a debuggee is removed, it no longer calls hooks.
assertEq(dbg.removeDebuggee(g1w), undefined);
log = '';
g1.f();
assertEq(log, 'g');
