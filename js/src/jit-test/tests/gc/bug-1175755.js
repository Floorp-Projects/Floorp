// |jit-test| allow-oom; allow-unhandlable-oom

if (!('oomAfterAllocations' in this))
    quit();

setGCCallback({
    action: "majorGC",
});
oomAfterAllocations(50);
