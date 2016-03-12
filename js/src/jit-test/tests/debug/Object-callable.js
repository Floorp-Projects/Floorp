// Test Debugger.Object.prototype.callable.

var g = newGlobal();
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.arguments[0].callable, frame.arguments[1]);
    hits++;
};

g.eval("function f(obj, iscallable) { debugger; }");

g.eval("f({}, false);");
g.eval("f(Function.prototype, true);");
g.eval("f(f, true);");
g.eval("f(new Proxy({}, {}), false);");
g.eval("f(new Proxy(f, {}), true);");
assertEq(hits, 5);
