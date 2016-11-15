// |jit-test| error:TypeError

g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function() 0;
} + ")()");
async function f() {
    t;
}
f();
