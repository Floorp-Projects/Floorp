// |jit-test| allow-oom; allow-unhandlable-oom

if (!('oomAfterAllocations' in this))
    quit();

oomAfterAllocations(5)
gcslice(11);
evalInWorker("1");
