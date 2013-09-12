// An Environment keeps its referent alive.

var g = newGlobal();
g.eval("function f(x) { return 2 * x; }");
var dbg = Debugger(g);
var env;
dbg.onEnterFrame = function (frame) { env = frame.environment; };
assertEq(g.f(22), 44);
dbg.onEnterFrame = undefined;

assertEq(env.find("x"), env);
assertEq(env.names().join(","), "arguments,x");

gc();
g.gc(g);
gc(env);

assertEq(env.find("x"), env);
assertEq(env.names().join(","), "arguments,x");
