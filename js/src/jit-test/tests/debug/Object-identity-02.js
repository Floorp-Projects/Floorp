// Different objects get different Debugger.Object wrappers.
var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.arguments[0] === frame.arguments[1], false);
    hits++;
};
g.eval("function f(a, b) { debugger; } f({}, {});");
assertEq(hits, 1);
