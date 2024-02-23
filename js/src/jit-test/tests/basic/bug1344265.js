// |jit-test| allow-unhandlable-oom; allow-oom
oomAfterAllocations(1);
newString("a", {external: true});
