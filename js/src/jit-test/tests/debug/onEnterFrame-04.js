// We detect and stop the runaway recursion caused by making onEnterFrame a wrapper of a debuggee function.

var g = newGlobal('new-compartment');
g.n = 0;
g.eval("function f(frame) { n++; return 42; }");
print('ok');
var dbg = Debugger(g);
dbg.onEnterFrame = g.f;

// Since enterFrame cannot throw, the InternalError is reported and execution proceeds.
var x = g.f();
assertEq(x, 42);
assertEq(g.n > 20, true);

// When an error is reported, the shell usually exits with a nonzero exit
// code. If we get here, the test passed, so override that behavior.
quit(0);
