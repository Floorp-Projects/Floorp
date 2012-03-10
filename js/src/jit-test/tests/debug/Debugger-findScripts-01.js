// In a debugger with no debuggees, findScripts should return no scripts.

var dbg = new Debugger;
assertEq(dbg.findScripts().length, 0);
