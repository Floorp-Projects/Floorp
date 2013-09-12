// getVariable sees properties inherited from the global object's prototype chain.

var g = newGlobal();
var dbg = Debugger(g);
var log = '';
dbg.onDebuggerStatement = function (frame) {
    log += frame.environment.getVariable("x") + frame.environment.getVariable("y");
};
g.eval("Object.getPrototypeOf(this).x = 'a';\n" +
       "Object.prototype.y = 'b';\n" +
       "debugger;\n");
assertEq(log, 'ab');
