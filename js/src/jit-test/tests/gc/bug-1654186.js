// |jit-test| allow-oom; skip-if: !('oomAfterAllocations' in this)

gczeal(14, 5);
var g = newGlobal();
g.eval("(" + function() {
    oomAfterAllocations(100);
} + ")()");
f.x("");
