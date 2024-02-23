stackTest(function() {
    eval(`var g = newGlobal(); recomputeWrappers(this, g);`);
});
