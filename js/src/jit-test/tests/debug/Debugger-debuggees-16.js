// GC can turn off debug mode in a compartment.

var dbgs = [];
var nonDebugGlobals = [];
var f = gc;
for (var i = 0; i < 4; i++) {
    // Create two globals, one debuggee.
    var g1 = newGlobal('new-compartment');
    var g2 = g1.eval("newGlobal('same-compartment')");
    var dbg = Debugger(g1);
    dbg.onDebuggerStatement = function () {};

    // Thread a chain of functions through the non-debuggee globals.
    g2.eval("function f() { return g() + 1; }");
    g2.g = f;
    f = g2.f;

    // Root the Debugger objects and non-debuggee globals.
    dbgs[i] = dbg;
    nonDebugGlobals[i] = g2;
}

// Call the chain of functions. At the end of the chain is gc. This will
// collect (some or all of) the debuggee globals, leaving non-debuggee
// globals. It should disable debug mode in those debuggee compartments.
nonDebugGlobals[nonDebugGlobals.length - 1].f();

gc();
nonDebugGlobals[0].g = function () { return 0; }
assertEq(nonDebugGlobals[nonDebugGlobals.length - 1].f(), nonDebugGlobals.length);
