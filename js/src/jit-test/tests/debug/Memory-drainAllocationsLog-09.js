// Test logs that contain allocations from many debuggee compartments.

const dbg = new Debugger();

const root1 = newGlobal({newCompartment: true});
const root2 = newGlobal({newCompartment: true});
const root3 = newGlobal({newCompartment: true});

dbg.addDebuggee(root1);
dbg.addDebuggee(root2);
dbg.addDebuggee(root3);

dbg.memory.trackingAllocationSites = true;

root1.eval("this.alloc = {}");
root2.eval("this.alloc = {}");
root3.eval("this.alloc = {}");

const allocs = dbg.memory.drainAllocationsLog();
assertEq(allocs.length >= 3, true);
