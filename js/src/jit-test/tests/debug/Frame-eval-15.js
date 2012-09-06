var g = newGlobal();
var dbg = new Debugger(g);

g.eval("function h() { debugger }");
g.eval("function f() { h() }");
g.blah = 42;
dbg.onDebuggerStatement = function(frame) {
    frame.older.eval("var blah = 43");
    frame.older.eval("blah = 44");
    assertEq(frame.older.environment.getVariable("blah"), 44);
}
g.f();
assertEq(g.blah, 42);
