// Bug 717104 - Unreachable debuggee globals should not keep their debuggers
// alive. The loop is to defeat conservative stack scanning; if the same stack
// locations are used each time through the loop, at least three of the
// debuggers should be collected.

for (var i = 0; i < 4; i++) {
    var g = newGlobal('new-compartment');
    var dbg = new Debugger(g);
    dbg.onDebuggerStatement = function () { throw "FAIL"; };
    dbg.o = makeFinalizeObserver();
    dbg.loop = g; // make a cycle of strong references with dbg and g
}

gc();
assertEq(finalizeCount() > 0, true);
