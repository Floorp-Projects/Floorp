// |jit-test| --no-ggc; allow-unhandlable-oom
load(libdir + 'oomTest.js');
var g = newGlobal();
oomTest(function() {
    Debugger(g);
});
