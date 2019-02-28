// |jit-test| skip-if: !('oomAtAllocation' in this) || helperThreadCount() === 0
oomAtAllocation(11, 11);
evalInWorker("");
