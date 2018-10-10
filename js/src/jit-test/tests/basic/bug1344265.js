// |jit-test| allow-unhandlable-oom; allow-oom; skip-if: !('oomAfterAllocations' in this)
oomAfterAllocations(1);
newExternalString("a");
