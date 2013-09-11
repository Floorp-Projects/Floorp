var g = newGlobal();
g.eval("function f(...x) {}");
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var fw = gw.getOwnPropertyDescriptor("f").value;
assertEq(fw.parameterNames.toString(), "x");

var g = newGlobal();
g.eval("function f(...rest) { debugger; }");
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    var result = frame.eval("arguments");
    assertEq("throw" in result, true);
    var result2 = frame.evalWithBindings("exc instanceof SyntaxError", {exc: result.throw});
    assertEq(result2.return, true);
};
g.f();