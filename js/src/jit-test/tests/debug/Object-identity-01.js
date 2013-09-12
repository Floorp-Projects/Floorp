// Two references to the same object get the same Debugger.Object wrapper.
var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.arguments[0], frame.arguments[1]);
    hits++;
};
g.eval("var obj = {}; function f(a, b) { debugger; } f(obj, obj);");
assertEq(hits, 1);
