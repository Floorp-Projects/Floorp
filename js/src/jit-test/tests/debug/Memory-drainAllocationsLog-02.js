// Test that drainAllocationsLog fails when we aren't trackingAllocationSites.

load(libdir + 'asserts.js');

const root = newGlobal();
const dbg = new Debugger();

dbg.memory.trackingAllocationSites = true;
root.eval("this.alloc1 = {}");
dbg.memory.trackingAllocationSites = false;
root.eval("this.alloc2 = {};");

assertThrowsInstanceOf(() => dbg.memory.drainAllocationsLog(),
                       Error);
