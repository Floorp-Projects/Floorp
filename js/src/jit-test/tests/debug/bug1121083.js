// |jit-test| error:terminated
options('werror');

g = newGlobal();
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
