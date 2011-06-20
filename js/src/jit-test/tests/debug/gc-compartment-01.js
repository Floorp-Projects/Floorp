// A debugger can survive per-compartment GC.

var g = newGlobal('new-compartment');
var dbg = Debug(g);
gc(g);
gc(this);
