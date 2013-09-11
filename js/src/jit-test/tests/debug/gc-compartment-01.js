// A debugger can survive per-compartment GC.

var g = newGlobal();
var dbg = Debugger(g);
gc(g);
gc(this);
