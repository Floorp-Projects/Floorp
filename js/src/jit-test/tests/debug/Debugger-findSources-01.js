// In a debugger with no debuggees, findSources should return no scripts.

const dbg = new Debugger;
assertEq(dbg.findSources().length, 0);
