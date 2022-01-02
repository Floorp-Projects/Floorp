// getVariable sees properties inherited from the global object's prototype chain.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var log = '';
dbg.onDebuggerStatement = function (frame) {
    log += frame.environment.parent.getVariable("x") + frame.environment.parent.getVariable("y");
};
g.eval("Object.getPrototypeOf(this).x = 'a';\n" +
       "Object.prototype.y = 'b';\n" +
       "debugger;\n");
assertEq(log, 'ab');
