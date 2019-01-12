// |jit-test| allow-oom; skip-if: !('oomAfterAllocations' in this)

var g = newGlobal({newCompartment: true});
x = Debugger(g);
selectforgc(g);
oomAfterAllocations(1);
