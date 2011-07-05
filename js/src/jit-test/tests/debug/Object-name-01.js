// Debug.Object.prototype.name

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var name;
dbg.hooks = {debuggerHandler: function (frame) { name = frame.callee.name; }};

g.eval("(function f() { debugger; })();");
assertEq(name, "f");
g.eval("(function () { debugger; })();");
assertEq(name, undefined);
g.eval("Function('debugger;')();");
assertEq(name, "anonymous");
