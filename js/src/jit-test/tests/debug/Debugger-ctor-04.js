// Repeated Debugger() arguments are ignored.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g, g, g);
assertEq(dbg.getDebuggees().length, 1);
