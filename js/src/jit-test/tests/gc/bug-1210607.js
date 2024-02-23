// |jit-test| allow-oom

var g = newGlobal({newCompartment: true});
x = Debugger(g);
selectforgc(g);
oomAfterAllocations(1);
