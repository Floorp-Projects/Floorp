// Adding a debuggee more than once is redundant.

var dbg = new Debugger;
var g = newGlobal('new-compartment');
var w = dbg.addDebuggee(g);
assertEq(w instanceof Debugger.Object, true);

function usual() {
    assertEq(dbg.hasDebuggee(g), true);
    assertEq(dbg.hasDebuggee(w), true);
    var arr = dbg.getDebuggees();
    assertEq(arr.length, 1);
    assertEq(arr[0], w);
}

usual();
assertEq(dbg.addDebuggee(g), w);
usual();
assertEq(dbg.addDebuggee(w), w);
usual();

// Removing the debuggee once is enough.
assertEq(dbg.removeDebuggee(g), undefined);
assertEq(dbg.hasDebuggee(g), false);
assertEq(dbg.getDebuggees().length, 0);
