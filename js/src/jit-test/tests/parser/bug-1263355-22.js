// |jit-test| error: ReferenceError

// Adapted from randomly chosen test: js/src/jit-test/tests/debug/Frame-onPop-error-scope-unwind-02.js
var g = newGlobal();
var dbg = new Debugger(g);
dbg.onEnterFrame = function(f) {
    (f.environment.getVariable("e") == 0);
};
g.eval("" + function f() {
    try {
        throw 42;
    } catch (e) {
        noSuchFn(e);
    }
});
g.eval("f();");
