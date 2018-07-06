// |jit-test| allow-oom

if (!('oomAfterAllocations' in this))
    quit();

gczeal(15);
oomAfterAllocations(5);
gcslice(11);
