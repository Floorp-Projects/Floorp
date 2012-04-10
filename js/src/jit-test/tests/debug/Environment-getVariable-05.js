// getVariable sees global variables.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var log = '';
dbg.onDebuggerStatement = function (frame) {
    log += frame.environment.getVariable("x") + frame.environment.getVariable("y");
};
g.eval("var x = 'a'; this.y = 'b'; debugger;");
assertEq(log, 'ab');
