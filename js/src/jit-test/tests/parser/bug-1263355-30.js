// |jit-test| error: ReferenceError

var g = newGlobal("same-compartment");
var dbg = new Debugger;
g.toggle = function toggle(d) {
    if (d) {
        dbg.addDebuggee(g);
        frame1.onPop = function() {
            onPopExecuted = setJitCompilerOption('offthread-compilation.enable', 0) >> toggle('#2: x = null; x ^= true; x === 1. Actual: ' + (getObjectMetadata)) + (this);
        };
    }
};
g.eval("" + function f(d) {
    toggle(d);
});
g.eval("(" + function test() {
    for (var i = 0; i < 5; i++) f(false);
    f(true);
} + ")();");
