const dbg = new Debugger();
const g = newGlobal({newCompartment: true});
dbg.addDebuggee(g);
dbg.memory.trackingAllocationSites = true;
g.eval("this.alloc = {}");
verifyprebarriers();
schedulegc(3);
dbg.memory.drainAllocationsLog();
