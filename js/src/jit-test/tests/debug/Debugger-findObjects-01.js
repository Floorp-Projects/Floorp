// In a debugger with no debuggees, findObjects should return no objects.

var dbg = new Debugger;
assertEq(dbg.findObjects().length, 0);
