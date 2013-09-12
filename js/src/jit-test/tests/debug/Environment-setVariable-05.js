// setVariable can change the types of variables and arguments in functions.

var g = newGlobal();
g.eval("function f(a) { var b = a + 1; debugger; return a + b; }");
for (var i = 0; i < 20; i++)
    assertEq(g.f(i), 2 * i + 1);

var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    frame.environment.setVariable("a", "xyz");
    frame.environment.setVariable("b", "zy");
};
for (var i = 0; i < 10; i++)
    assertEq(g.f(i), "xyzzy");
