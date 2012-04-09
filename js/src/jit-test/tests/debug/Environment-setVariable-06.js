// setVariable on an argument works as expected with non-strict 'arguments'.

var g = newGlobal('new-compartment');
g.eval("function f(a) { debugger; return arguments[0]; }");
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    frame.environment.setVariable("a", 2);
};
assertEq(g.f(1), 2);
