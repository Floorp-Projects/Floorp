// When the debugger is triggered twice from the same stack frame, the same
// Debugger.Frame object is passed to the hook both times.

var g = newGlobal();
var hits, frame;
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (f) {
    if (hits++ == 0)
        frame = f;
    else
        assertEq(f, frame);
};

hits = 0;
g.evaluate("debugger; debugger;");
assertEq(hits, 2);

hits = 0;
g.evaluate("function f() { debugger; debugger; }  f();");
assertEq(hits, 2);

hits = 0;
g.evaluate("eval('debugger; debugger;');");
assertEq(hits, 2);
