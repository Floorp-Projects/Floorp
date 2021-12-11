// A Debugger object created with no argument initially has no debuggees.
var dbg = new Debugger;
var debuggees = dbg.getDebuggees();
assertEq(Array.isArray(debuggees), true);
assertEq(debuggees.length, 0);
