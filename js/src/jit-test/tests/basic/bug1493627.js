// |jit-test| skip-if: !('stackTest' in this)
stackTest(function() {
    eval(`var g = newGlobal(); recomputeWrappers(this, g);`);
});
