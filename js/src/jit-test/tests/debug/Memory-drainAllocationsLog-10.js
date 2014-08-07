// Test logs that contain allocations from debuggee compartments added as we are
// logging.

const dbg = new Debugger();

dbg.memory.trackingAllocationSites = true;

const root1 = newGlobal();
dbg.addDebuggee(root1);
root1.eval("this.alloc = {}");

const root2 = newGlobal();
dbg.addDebuggee(root2);
root2.eval("this.alloc = {}");

const root3 = newGlobal();
dbg.addDebuggee(root3);
root3.eval("this.alloc = {}");

const allocs = dbg.memory.drainAllocationsLog();
assertEq(allocs.length >= 3, true);
