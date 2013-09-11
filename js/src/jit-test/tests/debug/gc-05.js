// If a Debugger survives its debuggee, its object cache must still be swept.

var g2arr = []; // non-debuggee globals
var xarr = []; // debuggee objects

var N = 4, M = 4;
for (var i = 0; i < N; i++) {
    var g1 = newGlobal();
    g1.M = M;
    var dbg = new Debugger(g1);
    var g2 = g1.eval("newGlobal('same-compartment')");
    g1.x = g2.eval("x = {};");

    dbg.onDebuggerStatement = function (frame) { xarr.push(frame.eval("x").return); };
    g1.eval("debugger;");
    g2arr.push(g2);

    g1 = null;
    gc();
}

// At least some of the debuggees have probably been collected at this
// point. It is nondeterministic, though.
assertEq(g2arr.length, N);
assertEq(xarr.length, N);

// Try to make g2arr[i].eval eventually allocate a new object in the same
// location as a previously gc'd object. If the object caches are not being
// swept, the pointer coincidence will cause a Debugger.Object to be erroneously
// reused.
for (var i = 0; i < N; i++) {
    var obj = xarr[i];
    for (j = 0; j < M; j++) {
        assertEq(obj instanceof Debugger.Object, true);
        g2arr[i].eval("x = x.prop = {};");
        obj = obj.getOwnPropertyDescriptor("prop").value;;
        assertEq("seen" in obj, false);
        obj.seen = true;
        gc();
    }
}
