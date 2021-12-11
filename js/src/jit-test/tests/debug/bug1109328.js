try {
    gcslice(0)(""());
} catch (e) {}
g = newGlobal({newCompartment: true})
g.parent = this
g.eval("Debugger(parent).onExceptionUnwind=(function(){})");
gcparam("maxBytes", gcparam("maxBytes") - 8);
gc();
