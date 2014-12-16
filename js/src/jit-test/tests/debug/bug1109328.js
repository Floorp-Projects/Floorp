try {
    gcslice(0)(""());
} catch (e) {}
g = newGlobal()
g.parent = this
g.eval("Debugger(parent).onExceptionUnwind=(function(){})");
gcparam("maxBytes", gcparam("gcBytes"));
