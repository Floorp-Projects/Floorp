// Debugger.Script.prototype.displayName

var g = newGlobal();
var dbg = Debugger(g);
var name;
dbg.onDebuggerStatement = f => { name = f.callee.script.displayName; };

g.eval("(function f() { debugger; })();");
assertEq(name, "f");
g.eval("(function () { debugger; })();");
assertEq(name, undefined);
g.eval("Function('debugger;')();");
assertEq(name, "anonymous");
g.eval("var f = function() { debugger; }; f()");
assertEq(name, "f");
g.eval("var a = {}; a.f = function() { debugger; }; a.f()");
assertEq(name, "a.f");
