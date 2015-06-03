// |jit-test| --no-ggc; allow-unhandlable-oom; --no-ion
load(libdir + 'oomTest.js');
var g = newGlobal();
oomTest(function() {
    Debugger(g);
});
