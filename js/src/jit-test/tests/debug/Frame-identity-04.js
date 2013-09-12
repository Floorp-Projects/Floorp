// Test that on-stack Debugger.Frames are not GC'd even if they are only reachable
// from the js::Debugger::frames table.

var g = newGlobal();
g.eval("function f(n) { if (n) f(n - 1); debugger; }");
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    if (hits === 0) {
        for (; frame; frame = frame.older)
            frame.seen = true;
    } else {
        for (; frame; frame = frame.older)
            assertEq(frame.seen, true);
    }
    gc();
    hits++;
};
g.f(20);
assertEq(hits, 21);
