// Suspended generators keep their associated Debugger.Frames gc-alive.

var g = newGlobal({newCompartment: true});
g.eval("function* f() { debugger; yield 1; debugger; }");
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    if (hits === 0)
        frame.seen = true;
    else
        assertEq(frame.seen, true);
    gc();
    hits++;
};
var it = g.f();
gc();
assertEq(it.next().value, 1);
gc();
assertEq(it.next().done, true);
assertEq(hits, 2);
