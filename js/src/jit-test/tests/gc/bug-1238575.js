// |jit-test| allow-oom; allow-unhandlable-oom; skip-if: !('oomAfterAllocations' in this)

oomAfterAllocations(5)
gcslice(11);
evalInWorker("1");
