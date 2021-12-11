// A debugger can survive per-compartment GC.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
gc(g);
gc(this);
