// The tracejit does not prevent onEnterFrame from being called.

var g = newGlobal('new-compartment');
g.eval("function f() { return 1; }\n");
var N = g.N = RUNLOOP + 2;
g.eval("function h() {\n" +
       "    for (var i = 0; i < N; i += f()) {}\n" +
       "}");
g.h(); // record loop

var dbg = Debugger(g);
var log = '';
dbg.onEnterFrame = function (frame) { log += frame.callee.name; };
g.h();
assertEq(log, 'h' + Array(N + 1).join('f'));
