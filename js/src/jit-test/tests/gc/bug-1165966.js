// |jit-test| --no-ion

var g = newGlobal();
oomTest(function() {
    Debugger(g);
});
