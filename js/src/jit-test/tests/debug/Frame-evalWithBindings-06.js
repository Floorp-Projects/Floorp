// In evalWithBindings code, assignment to any name not in the bindings works just as in eval.
var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.evalWithBindings("y = z; x = w;", {z: 2, w: 3}).return, 3);
};
g.eval("function f(x) { debugger; assertEq(x, 3); }");
g.eval("var y = 0; f(0);");
assertEq(g.y, 2);
