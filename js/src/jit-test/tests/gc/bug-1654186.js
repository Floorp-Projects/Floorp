// |jit-test| allow-oom; skip-if: !hasFunction.oomAfterAllocations

gczeal(14, 5);
var g = newGlobal();
g.eval("(" + function() {
    oomAfterAllocations(100);
} + ")()");
f.x("");
