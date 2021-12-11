// |reftest| skip-if(!xulRuntime.shell) -- needs Debugger

let g = newGlobal({ newCompartment: true });
let dbg = new Debugger(g);
dbg.memory.trackingAllocationSites = true;
g.createExternalArrayBuffer(64);

if (typeof reportCompare === 'function')
    reportCompare(true, true);
