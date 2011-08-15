// Debugger objects do not keep debuggee globals live.
var dbg = new Debugger;
for (var i = 0; i < 10; i++)
    dbg.addDebuggee(newGlobal('new-compartment'));
gc();
assertEq(dbg.getDebuggees().length <= 4, true);
