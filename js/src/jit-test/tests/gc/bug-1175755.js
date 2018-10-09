// |jit-test| allow-oom; allow-unhandlable-oom; skip-if: !('oomAfterAllocations' in this)

setGCCallback({
    action: "majorGC",
});
oomAfterAllocations(50);
