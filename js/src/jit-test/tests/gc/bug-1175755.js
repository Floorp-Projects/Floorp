// |jit-test| allow-oom; allow-unhandlable-oom

setGCCallback({
    action: "majorGC",
});
oomAfterAllocations(50);
