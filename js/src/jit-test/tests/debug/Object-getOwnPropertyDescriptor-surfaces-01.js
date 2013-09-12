// The argument to Debugger.Object.prototype.getOwnPropertyDescriptor can be omitted.

var g = newGlobal();
g.eval("var obj = {};");

var dbg = Debugger(g);
var obj;
dbg.onDebuggerStatement = function (frame) { obj = frame.eval("obj").return; };
g.eval("debugger;");

assertEq(obj.getOwnPropertyDescriptor(), undefined);
g.obj.undefined = 17;
var desc = obj.getOwnPropertyDescriptor();
assertEq(desc.value, 17);
