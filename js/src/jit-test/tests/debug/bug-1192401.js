const dbg = new Debugger();
const g = evalcx("lazy");
dbg.addDebuggee(g);
dbg.memory.trackingAllocationSites = true;
g.eval("this.alloc = {}");
