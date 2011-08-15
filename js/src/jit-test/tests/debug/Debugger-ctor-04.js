// Repeated Debugger() arguments are ignored.

var g = newGlobal('new-compartment');
var dbg = Debugger(g, g, g);
assertEq(dbg.getDebuggees().length, 1);
