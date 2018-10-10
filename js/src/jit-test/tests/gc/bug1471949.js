// |jit-test| allow-oom; skip-if: !('oomAfterAllocations' in this)

gczeal(15);
oomAfterAllocations(5);
gcslice(11);
