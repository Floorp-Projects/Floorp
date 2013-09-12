// Environment.prototype.setVariable can set global variables.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    frame.environment.setVariable("x", 2);
};
g.eval("var x = 1; debugger;");
assertEq(g.x, 2);
