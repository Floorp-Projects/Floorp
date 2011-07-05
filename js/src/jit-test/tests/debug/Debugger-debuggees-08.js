// addDebuggee(obj), where obj is not global, adds obj's global.
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
dbg.addDebuggee(g.Math);
usual();
dbg.addDebuggee(g.eval("(function () {})"));
usual();

// w2 is a Debugger.Object in g. Treat it like any other object in g; don't auto-unwrap it.
g.g2 = newGlobal('new-compartment');
g.eval("var w2 = new Debugger().addDebuggee(g2)");
dbg.addDebuggee(g.w2);
usual();
assertEq(!dbg.hasDebuggee(g.g2), true);
assertEq(dbg.hasDebuggee(g.w2), true);

// Removing the debuggee once is enough.
assertEq(dbg.removeDebuggee(g), undefined);
assertEq(dbg.hasDebuggee(g), false);
assertEq(dbg.getDebuggees().length, 0);
