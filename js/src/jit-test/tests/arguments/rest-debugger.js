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
    frame.eval("args = arguments");
};
g.f(9, 8, 7);

assertEq(g.args.length, 3);
assertEq(g.args[2], 7);
