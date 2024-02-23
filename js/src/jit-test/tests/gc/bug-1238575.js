// |jit-test| allow-oom; allow-unhandlable-oom; skip-if: helperThreadCount() === 0

oomAfterAllocations(5)
gcslice(11);
evalInWorker("1");
