var g = newGlobal();
var dbg = new Debugger(g);

g.eval("function h() { debugger; }");
g.eval("function g() { h() }");
g.eval("function f() { var blah = 333; g() }");

dbg.onDebuggerStatement = function(frame) {
    frame = frame.older;
    g.trace = frame.older.eval("(new Error()).stack;").return;
}
g.f();

assertEq(typeof g.trace, "string");

var frames = g.trace.split("\n");
assertEq(frames[0].contains("eval code"), true);
assertEq(frames[1].startsWith("f@"), true);
assertEq(frames[2].startsWith("@"), true);
