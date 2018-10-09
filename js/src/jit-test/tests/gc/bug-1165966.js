// |jit-test| --no-ion; skip-if: !('oomTest' in this)

var g = newGlobal();
oomTest(function() {
    Debugger(g);
});
