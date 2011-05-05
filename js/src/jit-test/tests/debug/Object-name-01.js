// |jit-test| debug
// Debug.Object.prototype.name

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var name, hits;
dbg.hooks = {debuggerHandler: function (frame) { hits++; assertEq(frame.callee.name, name); }};

hits = 0;
name = "f";
g.eval("(function f() { debugger; })();");
name = null;
g.eval("(function () { debugger; })();");
assertEq(hits, 2);
