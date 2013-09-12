// The argument to Debugger.Object.prototype.getOwnPropertyDescriptor can be an object.
var g = newGlobal();
g.eval("var obj = {};");

var dbg = Debugger(g);
var obj;
dbg.onDebuggerStatement = function (frame) { obj = frame.eval("obj").return; };
g.eval("debugger;");

var nameobj = {toString: function () { return 'x'; }};
assertEq(obj.getOwnPropertyDescriptor(nameobj), undefined);
g.obj.x = 17;
var desc = obj.getOwnPropertyDescriptor(nameobj);
assertEq(desc.value, 17);
