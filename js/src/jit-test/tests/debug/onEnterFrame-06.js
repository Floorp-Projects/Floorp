// The tracejit does not prevent onEnterFrame from being called after entering
// a debuggee compartment from a non-debuggee compartment.

var g1 = newGlobal('new-compartment');
var g2 = newGlobal('new-compartment');
var dbg = Debugger(g1, g2);
dbg.removeDebuggee(g2); // turn off debug mode in g2

g1.eval("function f() { return 1; }\n");
var N = g1.N = RUNLOOP + 2;
g1.eval("function h() {\n" +
       "    for (var i = 0; i < N; i += f()) {}\n" +
       "}");
g1.h(); // record loop

var log = '';
dbg.onEnterFrame = function (frame) { log += frame.callee.name; };
g1.h();
assertEq(log, 'h' + Array(N + 1).join('f'));
