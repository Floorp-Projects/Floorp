const dbg = new Debugger();
const g = newGlobal();
dbg.addDebuggee(g);
dbg.memory.trackingAllocationSites = true;
g.eval("this.alloc = {}");
verifyprebarriers();
schedulegc(3);
dbg.memory.drainAllocationsLog();
