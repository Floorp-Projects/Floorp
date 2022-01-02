// Test an empty allocation log.

const root = newGlobal({newCompartment: true});
const dbg = new Debugger();
dbg.addDebuggee(root)

dbg.memory.trackingAllocationSites = true;
const allocs = dbg.memory.drainAllocationsLog();
assertEq(allocs.length, 0);
