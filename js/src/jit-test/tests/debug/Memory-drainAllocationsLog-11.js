// Test logs that shouldn't contain allocations from debuggee compartments
// removed as we are logging.

const dbg = new Debugger();

const root1 = newGlobal();
dbg.addDebuggee(root1);
const root2 = newGlobal();
dbg.addDebuggee(root2);
const root3 = newGlobal();
dbg.addDebuggee(root3);

dbg.memory.trackingAllocationSites = true;

dbg.removeDebuggee(root1);
root1.eval("this.alloc = {}");

dbg.removeDebuggee(root2);
root2.eval("this.alloc = {}");

dbg.removeDebuggee(root3);
root3.eval("this.alloc = {}");

const allocs = dbg.memory.drainAllocationsLog();
assertEq(allocs.length, 0);
