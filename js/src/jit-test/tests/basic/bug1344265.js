// |jit-test| allow-unhandlable-oom; allow-oom
if (!('oomAfterAllocations' in this))
    quit();
oomAfterAllocations(1);
newExternalString("a");
