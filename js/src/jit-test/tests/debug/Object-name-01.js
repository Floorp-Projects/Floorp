// Debugger.Object.prototype.name

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var name;
dbg.onDebuggerStatement = function (frame) { name = frame.callee.name; };

g.eval("(function f() { debugger; })();");
assertEq(name, "f");
g.eval("(function () { debugger; })();");
assertEq(name, undefined);
g.eval("Function('debugger;')();");
assertEq(name, "anonymous");
