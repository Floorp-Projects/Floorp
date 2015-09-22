// |jit-test| --no-threads; --no-ion
load(libdir + 'oomTest.js');
var g = newGlobal();
oomTest(function() {
    Debugger(g);
});
