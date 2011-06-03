// Debug objects do not keep debuggee globals live.
var dbg = new Debug;
for (var i = 0; i < 4; i++)
    dbg.addDebuggee(newGlobal('new-compartment'));
gc();
assertEq(dbg.getDebuggees().length <= 1, true);
