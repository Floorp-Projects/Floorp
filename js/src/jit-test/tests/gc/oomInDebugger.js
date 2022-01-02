// |jit-test| skip-if: !('oomTest' in this)

var g = newGlobal();
oomTest(() => Debugger(g));
