// A Debug object created with no argument initially has no debuggees.
var dbg = new Debug;
var debuggees = dbg.getDebuggees();
assertEq(Array.isArray(debuggees), true);
assertEq(debuggees.length, 0);
