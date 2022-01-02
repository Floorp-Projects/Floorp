// |jit-test| error: TypeError

var g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () { };");

function f() {
    [[]] = [];
}
try {
    f();
} catch (e) {};
try {
    f();
} catch (e) {};
f();
