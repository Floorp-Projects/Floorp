// |jit-test| allow-oom

gczeal(15);
oomAfterAllocations(5);
gcslice(11);
