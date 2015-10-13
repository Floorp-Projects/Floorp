// |jit-test| --no-ion
if (!('oomTest' in this))
    quit();

var g = newGlobal();
oomTest(function() {
    Debugger(g);
});
