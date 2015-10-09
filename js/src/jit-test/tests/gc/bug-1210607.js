// |jit-test| allow-oom

if (!('oomAfterAllocations' in this))
    quit();

var g = newGlobal();
x = Debugger(g);
selectforgc(g);
oomAfterAllocations(1);
