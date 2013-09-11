// setVariable can set variables and arguments in functions.

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    frame.environment.setVariable("a", 100);
    frame.environment.setVariable("b", 200);
};
g.eval("function f(a) { var b = a + 1; debugger; return a + b; }");
assertEq(g.f(1), 300);
