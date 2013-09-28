// |jit-test| allow-oom
if (typeof oomAfterAllocations == 'function') {
    gczeal(4);
    oomAfterAllocations(1);
    var s = new Set;
}
