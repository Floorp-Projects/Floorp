// Debugger.Object basics

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.arguments[0], frame.callee);
    assertEq(Object.getPrototypeOf(frame.arguments[0]), Debugger.Object.prototype);
    assertEq(frame.arguments[0] instanceof Debugger.Object, true);
    assertEq(frame.arguments[0] !== frame.arguments[1], true);
    assertEq(Object.getPrototypeOf(frame.arguments[1]), Debugger.Object.prototype);
    assertEq(frame.arguments[1] instanceof Debugger.Object, true);
    hits++;
};

g.eval("var obj = {}; function f(a, b) { debugger; } f(f, obj);");
assertEq(hits, 1);
