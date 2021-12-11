// |jit-test| skip-if: !('oomTest' in this)

enableGeckoProfiling();
oomTest(function() {
    eval("(function() {})()")
});
