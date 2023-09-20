// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration()['wasi']

g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("Debugger(parent).onExceptionUnwind = function () {};");

function f(x) {
    if (x === 0) {
        return;
    }
    f(x - 1);
    f(x - 1);
}
timeout(0.00001);
f(100);
