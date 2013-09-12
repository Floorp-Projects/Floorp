// Simple {throw:} resumption.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (stack) { return {throw: "oops"}; };

assertThrowsValue(function () { g.eval("debugger;"); }, "oops");

g.eval("function f() { debugger; }");
assertThrowsValue(function () { g.f(); }, "oops");
